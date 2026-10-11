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

#include "EditCommands.h"
#include <cassert>
#include <wydbDatabase.h>
#include <wydbElement.h>
#include <wydbTransaction.h>
#include <wyapSelManager.h>
#include <wyapSelection.h>
#include <wy3dApp/Application.h>
#include "utils/CopyPasteUtil.h"


int ShowCommand::run()
{
    const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
    if (ss.isEmpty()) return 0;

    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return 0;
    }
    wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
    if (!pTrans)
    {
        assert(false);
        return 0;
    }
    wy::ErrorStatus error(wy::ErrorStatus::Ok);
    for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
    {
        wydb::Element* pElem = pTrans->getElementForWrite(iter.current().getElementId());
        if (!pElem)
        {
            assert(false);
            continue;
        }
        error = pElem->hide(false);
        assert(wy::ErrorStatus::Ok == error);
    }
    error = pDb->getTransactionManager()->endTransaction();
    assert(wy::ErrorStatus::Ok == error);

    return 0;
}


int HideCommand::run()
{
    const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
    if (ss.isEmpty()) return 0;

    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return 0;
    }
    wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
    if (!pTrans)
    {
        assert(false);
        return 0;
    }
    wy::ErrorStatus error(wy::ErrorStatus::Ok);
    for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
    {
        wydb::Element* pElem = pTrans->getElementForWrite(iter.current().getElementId());
        if (!pElem)
        {
            assert(false);
            continue;
        }
        error = pElem->hide();
        assert(wy::ErrorStatus::Ok == error);
    }
    error = pDb->getTransactionManager()->endTransaction();
    assert(wy::ErrorStatus::Ok == error);

    // 清空选择集
    Application::instance().getSelManager()->beginChange();
    Application::instance().getSelManager()->clearSelections();
    Application::instance().getSelManager()->endChange();

    return 0;
}


int CopyClipCommand::run()
{
    CopyPasteUtil::CopyReturn ret = CopyPasteUtil::copy();
    if (ret != CopyPasteUtil::CopyReturn::Ok)
    {
        CopyPasteUtil::showCopyErrorMsgBox(ret);
    }
    return 0;
}


int PasteClipCommand::run()
{
    CopyPasteUtil::PasteReturn ret = CopyPasteUtil::paste();
    if (ret != CopyPasteUtil::PasteReturn::Ok)
    {
        CopyPasteUtil::showPasteErrorMsgBox(ret);
    }
    return 0;
}