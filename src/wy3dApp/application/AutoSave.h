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

#ifndef WY3DAPP_AUTO_SAVE_H
#define WY3DAPP_AUTO_SAVE_H

#include <map>
#include <string>
#include <vector>

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>

#include <wyapDocManager.h>

// Auto save module.
// <1> Periodically (10 minutes by default) writes modified documents to
//     <original-name>.autosave (tmp file + rename, never overwrites the original).
// <2> Deletes the .autosave copy on normal exit / document close / manual save.
// <3> When opening a file that has a .autosave next to it, asks the user to
//     recover or discard (Krita style, no manifest). Recovery creates an empty
//     document named with the original path and reads the .autosave content
//     into it, so the document identity is correct from the start.
class AutoSave : public QObject, public wyap::DocManagerReactor
{
    Q_OBJECT
public:
    AutoSave();
    ~AutoSave() override;

    // Register as a document manager reactor (idempotent).
    void initialize();

    // Called before opening a file; returns true if the recovery flow already
    // handled the open, in which case the caller must skip opening the original.
    bool checkRecoveryForOpen(const std::string& u8FileFullPath);

    // Normal exit: delete all .autosave files produced by this session (idempotent).
    void cleanupOnExit();

    // DocManagerReactor overrides.
    void onDocumentStatusChanged(wyap::Document* pDoc, unsigned int oldStatus) override;
    // Called before the document is destroyed (pointer still valid): clears the
    // pointer-keyed records and deletes the document's .autosave.
    // onDocumentDestroyed(fileName) is NOT used: after destruction the pointer
    // is dangling and the pointer-keyed maps cannot be cleaned.
    void onDocumentToBeDestroyed(wyap::Document* pDocToDestroy) override;

private:
    void onTimer();
    std::vector<wyap::Document*> getEligibleModifiedDocuments() const;
    // Autosave one document; returns false when deferred (any active
    // transaction), skipped (nothing changed since the last copy), or on
    // failure. A defer does NOT restart the stopwatch so the next tick
    // retries; skip, success and failure do restart it (rate-limited to the
    // configured interval).
    bool autosaveDocument(wyap::Document* pDoc);

private:
    QTimer _timer;
    bool _initialized;
    int64_t _intervalMs;

    // Per-document autosave info.
    struct AutosaveInfo
    {
        QElapsedTimer modifiedTimer;
        unsigned long long lastAutoSaveIndex = 0;
        QString lastAutoSaveFilePath;
    };
    std::map<wyap::Document*, AutosaveInfo> _autoSaveInfoMap;
};

#endif // WY3DAPP_AUTO_SAVE_H
