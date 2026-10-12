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

#ifndef WY3DAPP_GUI_CMD_MAKE_ELEMENT_H
#define WY3DAPP_GUI_CMD_MAKE_ELEMENT_H

#include <set>

#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include "wy3dAppDefs.h"

namespace wy3dApp {

class GuiCommand;

class GuiCmdMakeElement
{
public:
    GuiCmdMakeElement(GuiCommand* pGuiCmd);
    virtual ~GuiCmdMakeElement();

    // 收集创建的元素ID
    virtual void collectElements(std::set<wydb::ElementId>& idSet) const {}

    // 获取工作平面法向
    virtual wy::Vector3 getWorkingPlaneNormal() const
    {
        return wy::Vector3(0.0, 0.0, 1.0);
    }

    // 提交
    bool commit();

protected:
    // 开启事务
    wydb::Transaction* startTransaction(wydb::Database* pDb,
        wydb::ChainUpdateScope scope = wydb::ChainUpdateScope::Cascade);

protected:
    wydb::Database* _pDb;
    GuiCommand* _pGuiCmd;
    wydb::Transaction* _pTopTrans;
    bool _isFinished;
};

}

#endif // WY3DAPP_GUI_CMD_MAKE_ELEMENT_H
