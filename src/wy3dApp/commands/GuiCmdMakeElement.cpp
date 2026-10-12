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

#include "GuiCmdMakeElement.h"

#include <cassert>

#include <wy3dApp/Application.h>
#include <wy3dApp/GuiCommand.h>
#include "wy3dAppDefs.h"

namespace wy3dApp {

GuiCmdMakeElement::GuiCmdMakeElement(GuiCommand* pGuiCmd)
    : _pGuiCmd(pGuiCmd), _pTopTrans(nullptr), _isFinished(false)
{
    _pDb = Application::instance().getActiveDatabase();
    assert(_pDb);
    if (_pDb)
    {
        _pTopTrans = _pDb->getTransactionManager()->startTransactionGroup();
        assert(_pTopTrans);
    }
}

GuiCmdMakeElement::~GuiCmdMakeElement()
{
    assert(_pDb);
    assert(_pTopTrans);
    if (_pDb && _pTopTrans && !_isFinished)
    {
        wy::ErrorStatus error = _pDb->getTransactionManager()->abortTransaction();
        assert(wy::ErrorStatus::Ok == error);
    }
}

bool GuiCmdMakeElement::commit()
{
    if (!_pDb || !_pTopTrans || _isFinished)
    {
        return false;
    }
    wy::ErrorStatus error = _pDb->getTransactionManager()->endTransaction();
    _isFinished = true;
    if (wy::ErrorStatus::Ok == error)
    {
        return true;
    }
    else
    {
        assert(false);
        return false;
    }
}

// 开启事务
wydb::Transaction* GuiCmdMakeElement::startTransaction(wydb::Database* pDb,
    wydb::ChainUpdateScope scope)
{
    wydb::TransactionOption option;
    option.chainUpdateScope = scope;
    return pDb->getTransactionManager()->startTransaction("", option);
}

}
