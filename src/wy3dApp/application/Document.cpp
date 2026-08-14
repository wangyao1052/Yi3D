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

#include "Document.h"

Document::Document() :
    wyap::Document(),
    _changeCount(0)
{
}

void Document::onDatabaseChanged(
    const wydb::Database* pDatabase,
    const wydb::Transaction* pTransaction,
    const wydb::DatabaseChangeInfo& changeInfo)
{
    // Count every change notification (transaction end, undo, redo, abort,
    // and content loads such as readFile — verified by log): the counter
    // feeds autosave change detection. Notifications that do not change the
    // content (e.g. an aborted transaction) may at worst cause one redundant
    // write, never a skipped one.
    ++_changeCount;

    if (_respondToDbChanged)
    {
        wyap::Document::onDatabaseChanged(pDatabase, pTransaction, changeInfo);
    }
}