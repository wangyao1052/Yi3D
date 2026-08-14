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

#ifndef WY3DAPP_DOCUMENT_H
#define WY3DAPP_DOCUMENT_H

#include <wyapDocument.h>


class Document : public wyap::Document
{
public:
    Document();

    // 获取是否响应DbChanged
    bool getDbChangedResponseEnabled() const
    {
        return _respondToDbChanged;
    }
    // 设置是否响应DbChanged
    void setDbChangedResponseEnabled(bool enabled)
    {
        _respondToDbChanged = enabled;
    }

    // 获取数据库修改计数:onDatabaseChanged 每被通知一次 +1(事务结束/撤销/重做/中止)。
    // 自动保存用它做变更检测(计数没变 = 内容没变,跳过写盘)。
    unsigned long long getChangeCount() const
    {
        return _changeCount;
    }

    virtual void onDatabaseChanged(
        const wydb::Database* pDatabase,
        const wydb::Transaction* pTransaction,
        const wydb::DatabaseChangeInfo& changeInfo) override;

private:
    // 是否响应DbChanged
    bool _respondToDbChanged;
    // 数据库修改计数:每次 onDatabaseChanged 通知 +1
    unsigned long long _changeCount;
};

#endif // WY3DAPP_DOCUMENT_H