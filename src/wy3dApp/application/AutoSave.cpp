///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2024-2026 Wang Yao <wangyao1052@163.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
///////////////////////////////////////////////////////////////////////////////

#include "AutoSave.h"

#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QPushButton>

#if defined(_WIN32)
#include <windows.h>
#endif

#include <wyapDocument.h>
#include <wydbTransaction.h>
#include <wyapEnums.h>

#include "application/Application.h"
#include "application/Config.h"
#include "application/Document.h"
#include "commands/FileCommands.h"
#include "utils/MessageBoxUtil.h"
#include "widgets/frame/MainWindow.h"

namespace
{
    // .autosave suffix
    const QString kAutosaveSuffix = QStringLiteral(".autosave");
    // Timer tick interval (ms): 60s
    const int kTickMs = 60 * 1000;

    // Atomically replace an existing file (tmp and target are in the same
    // directory, hence on the same volume). On failure the target keeps its
    // previous content, so the caller can safely retry next cycle.
    bool replaceFileAtomically(const QString& fromPath, const QString& toPath)
    {
#if defined(_WIN32)
        // QFile::rename's replace semantics on Windows are unreliable (Qt's
        // documented contract says it fails when the target exists, and the
        // underlying implementation has varied across versions); MoveFileEx
        // with REPLACE_EXISTING is the documented atomic replace.
        return MoveFileExW(
            reinterpret_cast<const wchar_t*>(fromPath.utf16()),
            reinterpret_cast<const wchar_t*>(toPath.utf16()),
            MOVEFILE_REPLACE_EXISTING) != 0;
#else
        // POSIX rename(2) atomically replaces an existing target.
        return QFile::rename(fromPath, toPath);
#endif
    }

    void deleteAutosaveFile(const QString& autosavePath)
    {
        if (autosavePath.isEmpty())
        {
            return;
        }
        QFile::remove(autosavePath);
        QFile::remove(autosavePath + QStringLiteral(".tmp"));
    }
}

AutoSave::AutoSave() : _initialized(false), _intervalMs(INT_MAX)
{
    _timer.setInterval(kTickMs);
    QObject::connect(&_timer, &QTimer::timeout, this, &AutoSave::onTimer);
}

AutoSave::~AutoSave()
{
}

void AutoSave::initialize()
{
    if (_initialized)
    {
        return;
    }
    wyap::DocManager* pDocMgr = Application::instance().getDocManager();
    if (!pDocMgr)
    {
        assert(false);
        return;
    }
    wy::ErrorStatus error = pDocMgr->addReactor(this);
    if (wy::ErrorStatus::Ok != error)
    {
        assert(false);
        return;
    }
    _initialized = true;

    const Config* pConfig = Application::instance().getConfig();
    assert(pConfig);
    _intervalMs = static_cast<int64_t>(pConfig->autoSave.intervalMinutes) * 60 * 1000;

    _timer.start();
}

bool AutoSave::checkRecoveryForOpen(const std::string& u8FileFullPath)
{
    wyap::DocManager* pDocMgr = Application::instance().getDocManager();
    const QString originalPath = QString::fromStdString(u8FileFullPath);
    const QString autosavePath = originalPath + kAutosaveSuffix;
    if (!QFile::exists(autosavePath))
    {
        return false;
    }

    // The original is already open (its recovery was handled when it was
    // opened); skip the prompt. openDocument would return the existing
    // document anyway, and newDocument would fail on the duplicate name.
    for (wyap::Document* pOpenDoc : pDocMgr->getDocuments())
    {
        if (pOpenDoc && pOpenDoc->getFileName() == u8FileFullPath)
        {
            return false;
        }
    }

    // Ask: recover or discard.
    const QString originalTime = QFileInfo(originalPath).lastModified().toString("yyyy-MM-dd HH:mm:ss");
    const QString autosaveTime = QFileInfo(autosavePath).lastModified().toString("yyyy-MM-dd HH:mm:ss");
    const QString text = tr("An autosave file was found for:\n%1\n\n"
        "Original file: %2\n"
        "Autosave file: %3\n\n"
        "Do you want to recover the unsaved changes?")
        .arg(originalPath, originalTime, autosaveTime);
    QMessageBox messageBox(
        QMessageBox::Warning,
        tr("Recovery"),
        text,
        QMessageBox::NoButton,
        Application::instance().getMainWindow());
    QPushButton* pRecoverButton = messageBox.addButton(
        tr("Recover"), QMessageBox::AcceptRole);
    QPushButton* pDiscardButton = messageBox.addButton(
        tr("Discard"), QMessageBox::DestructiveRole);
    messageBox.setDefaultButton(qobject_cast<QPushButton*>(pRecoverButton));
    messageBox.exec();

    // Discard: delete the .autosave and open the original normally.
    if (messageBox.clickedButton() == pDiscardButton)
    {
        QFile::remove(autosavePath);
        return false;
    }

    // Recover: create an empty shell named with the original path (identity
    // correct from the start, status NewlyCreated; the old original content
    // is never read), then load the recovered content into its database.
    // Note: use the pointer-returning overload; the Debug wyap library
    // predates the two-argument overload.
    wyap::Document* pDoc = pDocMgr->newDocument(u8FileFullPath);
    if (!pDoc)
    {
        // Should not happen: the name is unique (checked above).
        assert(false);
        return false;
    }

    // readFile marks the document Modified natively (verified by spike:
    // status becomes NewlyCreated | Modified, and the SDK fires
    // onDocumentStatusChanged, which starts the autosave heartbeat). It also
    // fires onDatabaseChanged notifications (verified by log), so the change
    // counter reflects the loaded content.
    wy::ErrorStatus error = pDoc->getDatabase()->readFile(
        autosavePath.toStdString(), {wydb::FileType::Binary});
    if (wy::ErrorStatus::Ok != error)
    {
        MessageBoxUtil::showOpenFileError(error);
        // Remove the empty shell so that the normal open below can proceed.
        pDocMgr->closeDocument(pDoc);
        return false; // Keep the .autosave so it can be retried next time.
    }

    Document* pAppDoc = dynamic_cast<Document*>(pDoc);
    assert(pAppDoc);
    assert(1 == pAppDoc->getChangeCount());
    assert(_autoSaveInfoMap.find(pDoc) != _autoSaveInfoMap.cend());
    AutosaveInfo& autosaveInfo = _autoSaveInfoMap[pDoc];
    autosaveInfo.modifiedTimer.restart();
    autosaveInfo.lastAutoSaveIndex = pAppDoc->getChangeCount();
    autosaveInfo.lastAutoSaveFilePath = autosavePath;

    // Activate the recovered document.
    pDocMgr->activateDocument(pDoc, wyap::ExecutionMode::Async);

    // Fit the view to the recovered model (same as the normal open flow).
    FileCmdsUtil::fitViewToAll(pDoc);

    // Ask whether to save the recovered document now (target is clear: the
    // original path; save directly without a dialog).
    const int ret = QMessageBox::question(
        Application::instance().getMainWindow(),
        tr("Recovery"),
        tr("The document has been recovered.\n"
            "Do you want to save it to the original file now?\n%1").arg(originalPath),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);
    if (QMessageBox::Yes == ret)
    {
        const wydb::FileType fileType = FileCmdsUtil::inferFileType(u8FileFullPath);
        wy::ErrorStatus error = Application::instance().getDocManager()->saveDocument(pDoc, u8FileFullPath, { fileType });
        if (wy::ErrorStatus::Ok != error)
        {
            MessageBoxUtil::showError(tr("Failed to save the recovered document!"));
        }
    }

    // The open has been handled by the recovery flow; the caller must skip
    // opening the original file.
    return true;
}

void AutoSave::cleanupOnExit()
{
    _timer.stop();

    for (const auto& kvp : _autoSaveInfoMap)
    {
        deleteAutosaveFile(kvp.second.lastAutoSaveFilePath);
    }
    _autoSaveInfoMap.clear();
}

void AutoSave::onDocumentStatusChanged(wyap::Document* pDoc, unsigned int oldStatus)
{
    assert(pDoc);

    if (pDoc->hasStatus(wyap::DocumentStatus::Modified))
    {
        auto iter = _autoSaveInfoMap.find(pDoc);
        if (_autoSaveInfoMap.cend() == iter)
        {
            AutosaveInfo& info = _autoSaveInfoMap[pDoc];
            info.modifiedTimer.restart();
            info.lastAutoSaveIndex = 0;
            info.lastAutoSaveFilePath = "";
        }
        else
        {
            if (iter->second.modifiedTimer.isValid())
            {
                assert(false);
            }
            else
            {
                iter->second.modifiedTimer.restart();
            }
        }
    }
    else
    {
        auto iter = _autoSaveInfoMap.find(pDoc);
        if (_autoSaveInfoMap.cend() == iter)
        {
            assert(false);
            return;
        }
        iter->second.modifiedTimer.invalidate();
        deleteAutosaveFile(iter->second.lastAutoSaveFilePath);
        iter->second.lastAutoSaveIndex = 0;
        iter->second.lastAutoSaveFilePath = "";
    }
}

void AutoSave::onDocumentToBeDestroyed(wyap::Document* pDocToDestroy)
{
    assert(pDocToDestroy);

    auto iter = _autoSaveInfoMap.find(pDocToDestroy);
    if (_autoSaveInfoMap.cend() == iter)
    {
        return;
    }

    if (!iter->second.lastAutoSaveFilePath.isEmpty())
    {
        deleteAutosaveFile(iter->second.lastAutoSaveFilePath);
    }
    _autoSaveInfoMap.erase(iter);
}

void AutoSave::onTimer()
{
    if (QApplication::activeModalWidget() != nullptr)
    {
        return;
    }

    if (_intervalMs <= 0)
    {
        // Auto save disabled: the tick does nothing.
        return;
    }

    const std::vector<wyap::Document*> documents = this->getEligibleModifiedDocuments();
    if (documents.empty())
    {
        return;
    }

    for (wyap::Document* pDoc : documents)
    {
        auto iter = _autoSaveInfoMap.find(pDoc);
        if (_autoSaveInfoMap.cend() == iter)
        {
            continue;
        }
        if (!iter->second.modifiedTimer.isValid())
        {
            continue;
        }
        if (iter->second.modifiedTimer.elapsed() >= _intervalMs)
        {
            this->autosaveDocument(pDoc);
        }
    }
}

std::vector<wyap::Document*> AutoSave::getEligibleModifiedDocuments() const
{
    std::vector<wyap::Document*> eligibleDocs;
    wyap::DocManager* pDocMgr = Application::instance().getDocManager();
    if (!pDocMgr)
    {
        assert(false);
        return eligibleDocs;
    }

    const std::vector<wyap::Document*> documents = pDocMgr->getDocuments();
    for (wyap::Document* pDoc : documents)
    {
        if (!pDoc)
        {
            assert(false);
            continue;
        }
        // Only save documents that were saved at least once (absolute path);
        // NewlyCreated documents have a relative name with no "original
        // directory" to write next to.
        if (!pDoc->hasStatus(wyap::DocumentStatus::Modified))
        {
            continue;
        }
        const QString fileName = QString::fromStdString(pDoc->getFileName());
        if (!QFileInfo(fileName).isAbsolute())
        {
            continue;
        }
        eligibleDocs.emplace_back(pDoc);
    }
    return eligibleDocs;
}

bool AutoSave::autosaveDocument(wyap::Document* pDoc)
{
    assert(pDoc);
    wydb::Database* pDb = pDoc->getDatabase();
    if (!pDb)
    {
        assert(false);
        return false;
    }

    wydb::Transaction* pActiveTrans = pDb->getTransactionManager()->getActiveTransaction();
    if (pActiveTrans)
    {
        return false;
    }

    auto iter = _autoSaveInfoMap.find(pDoc);
    if (_autoSaveInfoMap.cend() == iter)
    {
        assert(false);
        return false;
    }

    Document* pAppDoc = dynamic_cast<Document*>(pDoc);
    if (!pAppDoc)
    {
        assert(false);
        return false;
    }
    unsigned long long changeCount = pAppDoc->getChangeCount();
    if (iter->second.lastAutoSaveIndex != 0
        && changeCount <= iter->second.lastAutoSaveIndex)
    {
        assert(changeCount == iter->second.lastAutoSaveIndex);
        iter->second.modifiedTimer.restart();
        return false;
    }

    // Write a .tmp file first, then atomically replace the .autosave with it,
    // so a crash mid-save can never corrupt the .autosave.
    const QString autosavePath = QString::fromStdString(pDoc->getFileName()) + kAutosaveSuffix;
    const QString tmpPath = autosavePath + QStringLiteral(".tmp");
    wy::ErrorStatus error = pDb->writeFile(tmpPath.toStdString(), {wydb::FileType::Binary});
    if (wy::ErrorStatus::Ok != error)
    {
        assert(false);
        QFile::remove(tmpPath);
        iter->second.modifiedTimer.restart();
        return false;
    }

    // Atomically replace the previous .autosave: no remove-then-rename gap
    // where a crash would leave neither the old copy nor the new one.
    if (!replaceFileAtomically(tmpPath, autosavePath))
    {
        assert(false);
        QFile::remove(tmpPath);
        iter->second.modifiedTimer.restart();
        return false;
    }

    iter->second.modifiedTimer.restart();
    iter->second.lastAutoSaveIndex = changeCount;
    iter->second.lastAutoSaveFilePath = autosavePath;
    return true;
}
