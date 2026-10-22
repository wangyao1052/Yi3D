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

#include "TransactionUtil.h"
#include <cassert>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dApp/Application.h>
#include "application/ApplicationImpl.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

bool TransactionUtil::hasActiveTransaction()
{
    wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    if (!pDb)
    {
        return false;
    }
    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();
    assert(pTransMgr);
    return pTransMgr->getActiveTransaction();
}

wydb::Transaction* TransactionUtil::startSolitaryTransaction(wydb::Database* pDb)
{
    if (!pDb) return nullptr;
    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    return pDb->getTransactionManager()->startTransaction("", option);
}

} // namespace wy3dApp
