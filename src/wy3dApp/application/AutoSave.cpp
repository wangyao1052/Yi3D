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
}

AutoSave::AutoSave() :
    _initialized(false)
{
    // Repeating timer with a fixed 60s tick
    _timer.setInterval(kTickMs);
    QObject::connect(&_timer, &QTimer::timeout, this, &AutoSave::onTimerTimeout);
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
}

void AutoSave::onDocumentSaved(wyap::Document* pDoc)
{
    assert(pDoc);

    // Refresh the "last attempt" time so autosave does not immediately
    // rewrite the copy right after a manual save.
    this->touchLastAttempt(pDoc);

    // Delete the .autosave recorded for this document. The recorded path is
    // looked up by document, not derived from the current file name: after a
    // save-as the name has changed and the recorded copy belongs to the old
    // name.
    auto iterAutosave = _knownAutosaves.find(pDoc);
    if (iterAutosave != _knownAutosaves.end())
    {
        this->deleteAutosaveFile(iterAutosave->second);
        _knownAutosaves.erase(iterAutosave);
    }
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
    // onDocumentStatusChanged, which starts the autosave heartbeat).
    wy::ErrorStatus error = pDoc->getDatabase()->readFile(
        autosavePath.toStdString(), {wydb::FileType::Binary});
    if (wy::ErrorStatus::Ok != error)
    {
        MessageBoxUtil::showOpenFileError(error);
        // Remove the empty shell so that the normal open below can proceed.
        pDocMgr->closeDocument(pDoc);
        return false; // Keep the .autosave so it can be retried next time.
    }

    // Take over the leftover .autosave (created by a previous session, so it
    // is not in _knownAutosaves yet): without this, saving/closing/exiting
    // could not delete it.
    _knownAutosaves[pDoc] = autosavePath;

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
        if (!FileCmdsUtil::saveFileToPath(pDoc, u8FileFullPath))
        {
            MessageBoxUtil::showError(tr("Failed to save the recovered document!"));
            // Fall back to the save-as dialog so the user can pick another path.
            bool isUserCanceled(false);
            FileCmdsUtil::uiSaveFile(pDoc, true, isUserCanceled);
        }
    }

    // The open has been handled by the recovery flow; the caller must skip
    // opening the original file.
    return true;
}

void AutoSave::cleanupOnExit()
{
    _timer.stop();

    for (const auto& entry : _knownAutosaves)
    {
        this->deleteAutosaveFile(entry.second);
    }
    _knownAutosaves.clear();
    _lastAttempt.clear();
}

void AutoSave::onDocumentStatusChanged(wyap::Document* pDoc, unsigned int oldStatus)
{
    (void)oldStatus;
    assert(pDoc);

    if (pDoc->hasStatus(wyap::DocumentStatus::Modified))
    {
        this->startTimerIfNeeded();
    }
    else
    {
        this->stopTimerIfIdle();
    }
}

void AutoSave::onDocumentToBeDestroyed(wyap::Document* pDocToDestroy)
{
    assert(pDocToDestroy);

    // The pointer is still valid before destruction: look up the recorded
    // .autosave path (keyed by the pointer), delete the file, then clear
    // the records.
    auto iterAutosave = _knownAutosaves.find(pDocToDestroy);
    if (iterAutosave != _knownAutosaves.end())
    {
        this->deleteAutosaveFile(iterAutosave->second);
        _knownAutosaves.erase(iterAutosave);
    }

    _lastAttempt.erase(pDocToDestroy);

    this->stopTimerIfIdle();
}

void AutoSave::onTimerTimeout()
{
    if (!this->isCycleAllowed())
    {
        // The timer still fires inside the nested event loop of a modal
        // dialog; defer to the next tick.
        return;
    }

    const std::vector<wyap::Document*> documents = this->getEligibleModifiedDocuments();
    if (documents.empty())
    {
        this->stopTimerIfIdle();
        return;
    }

    const Config* pConfig = Application::instance().getConfig();
    assert(pConfig);
    const qint64 intervalMs = static_cast<qint64>(pConfig->autoSave.intervalMinutes) * 60 * 1000;

    // Per-document due check: write only when the time since the last attempt
    // reached the configured interval.
    for (wyap::Document* pDoc : documents)
    {
        auto iterLastAttempt = _lastAttempt.find(pDoc);
        const bool isDue = (iterLastAttempt == _lastAttempt.end())
            || !iterLastAttempt->second.isValid()
            || iterLastAttempt->second.elapsed() >= intervalMs;
        if (isDue)
        {
            // autosaveDocument refreshes the "last attempt" time internally on
            // success and failure; a defer (leaf transaction) does not, so the
            // next tick retries.
            this->autosaveDocument(pDoc);
        }
    }
}

void AutoSave::startTimerIfNeeded()
{
    if (!_initialized)
    {
        return;
    }

    const Config* pConfig = Application::instance().getConfig();
    assert(pConfig);
    // 0 disables auto save (config.ini autoSave/intervalMinutes).
    if (pConfig->autoSave.intervalMinutes <= 0)
    {
        return;
    }

    if (!_timer.isActive() && !this->getEligibleModifiedDocuments().empty())
    {
        _timer.start();
    }
}

void AutoSave::stopTimerIfIdle()
{
    if (this->getEligibleModifiedDocuments().empty())
    {
        _timer.stop();
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

bool AutoSave::isCycleAllowed() const
{
    // Defer the whole cycle while a modal dialog (file dialog, message box,
    // etc.) is open.
    return QApplication::activeModalWidget() == nullptr;
}

bool AutoSave::autosaveDocument(wyap::Document* pDoc)
{
    assert(pDoc);

    wydb::Database* pDb = pDoc->getDatabase();
    if (!pDb)
    {
        assert(false);
        // Should-not-happen state: refresh the "last attempt" time so
        // retries are rate-limited to the configured interval.
        this->touchLastAttempt(pDoc);
        return false;
    }

    // Strict rule: defer while ANY transaction is active (including a
    // top-level transaction group such as a sketch session). Chosen over the
    // sketch-group exception for maximum safety: getActiveTransaction()
    // returns the innermost active transaction.
    wydb::Transaction* pActiveTrans = pDb->getTransactionManager()->getActiveTransaction();
    if (pActiveTrans)
    {
        // Defer WITHOUT refreshing the "last attempt" time, so the due check
        // on the next tick (<=60s) retries as soon as the transaction has
        // ended.
        return false;
    }

    // A real write attempt starts here: refresh the "last attempt" time so
    // success and failure alike are rate-limited to the configured interval
    // (no retry on every tick).
    this->touchLastAttempt(pDoc);

    // Write a .tmp file first, then atomically replace the .autosave with it,
    // so a crash mid-save can never corrupt the .autosave.
    const QString autosavePath = this->autosavePathFor(pDoc);
    const QString tmpPath = autosavePath + QStringLiteral(".tmp");

    wy::ErrorStatus error = pDb->writeFile(tmpPath.toStdString(), {wydb::FileType::Binary});
    if (wy::ErrorStatus::Ok != error)
    {
        qWarning() << "AutoSave: writeFile failed, code" << static_cast<unsigned int>(error)
                   << "path" << tmpPath;
        return false;
    }

    // Atomically replace the previous .autosave: no remove-then-rename gap
    // where a crash would leave neither the old copy nor the new one.
    if (!replaceFileAtomically(tmpPath, autosavePath))
    {
        qWarning() << "AutoSave: replace failed" << tmpPath << "->" << autosavePath;
        QFile::remove(tmpPath);
        return false;
    }

    _knownAutosaves[pDoc] = autosavePath;
    return true;
}

QString AutoSave::autosavePathFor(wyap::Document* pDoc) const
{
    assert(pDoc);

    return QString::fromStdString(pDoc->getFileName()) + kAutosaveSuffix;
}

void AutoSave::deleteAutosaveFile(const QString& autosavePath)
{
    // Callers must only pass paths recorded in _knownAutosaves (created or
    // taken over by this session), so unrelated user files are never touched.
    QFile::remove(autosavePath);
    QFile::remove(autosavePath + QStringLiteral(".tmp"));
}

void AutoSave::touchLastAttempt(wyap::Document* pDoc)
{
    assert(pDoc);

    QElapsedTimer& timer = _lastAttempt[pDoc];
    if (!timer.isValid())
    {
        timer.start();
    }
    else
    {
        timer.restart();
    }
}
