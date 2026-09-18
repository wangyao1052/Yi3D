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

#ifndef WY3DAPP_MOVE_ROTATE_GUI_CMD_UTIL_H
#define WY3DAPP_MOVE_ROTATE_GUI_CMD_UTIL_H

#include <wyapSelManager.h>
#include <wydbDatabase.h>
#include <wy3dSolid.h>
#include <wy3dSelectionType.h>

#include "application/Application.h"
#include "utils/GuiCommandUtil.h"

// 获取当前选择集中有效的形体元素(实体或片体、顶层、无父级)
// 供 Move 和 Rotate 建模环境命令共用
inline wyap::SelectionSet getValidSSFromCurrentSelSet_MoveRotateGuiCmd()
{
    wyap::SelectionSet filterSS;
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return filterSS;

    const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
    for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
    {
        const wyap::Selection& sel = iter.current();
        if (sel.getSelectionType() != static_cast<unsigned int>(wy3d::SelectionType::Element))
        {
            continue;
        }
        if (!GuiCommandUtil::isTopLevelBody(pDb->getElement(sel.getElementId()))) continue;
        filterSS.add(sel);
    }

    return filterSS;
}

#endif // WY3DAPP_MOVE_ROTATE_GUI_CMD_UTIL_H
