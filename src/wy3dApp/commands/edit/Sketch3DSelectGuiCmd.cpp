///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 Wang Yao <wangyao1052@163.com>
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

#include "Sketch3DSelectGuiCmd.h"

#include <wy3dSketch3D.h>
#include <wy3dSketchEntity3D.h>

#include "application/Application.h"
#include "scene/nodes/ElementNodeType.h"
#include "select/filters/CommonSelFilters.h"
#include "utils/GuiCommandUtil.h"

Sketch3DSelectGuiCmd::Sketch3DSelectGuiCmd() : SelectGuiCmd()
{
}

Sketch3DSelectGuiCmd::~Sketch3DSelectGuiCmd()
{
}

GuiCmdMenu* Sketch3DSelectGuiCmd::initContextMenu()
{
    // 3D草图环境不支持复制/粘贴
    return nullptr;
}

wyap::CmdExecution::StartResult Sketch3DSelectGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = __baseClass::onStart();
    if (!GuiCommandUtil::initSketch3DInfo(_sketch3DInfo))
    {
        assert(false);
        return wyap::CmdExecution::StartResult::Failed;
    }
    return ret;
}

void Sketch3DSelectGuiCmd::configureSelectOptions(GuiCmdSelectOptions& options)
{
    options.pickMask = static_cast<unsigned int>(ElementNodeType::Sketch3DEntity);
    options.filter = std::make_shared<SingleClassSelFilter>(wy3d::SketchEntity3D::classInfo());
}

void Sketch3DSelectGuiCmd::onStart_EnvSpecific()
{
    // 3D草图环境不需要启用特征树可选择
}

void Sketch3DSelectGuiCmd::selectAll_Impl(wyap::SelectionSet& ss)
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return;
    const wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pDb->getElement(_sketch3DInfo.sketch3dId));
    if (pSketch3D)
    {
        for (auto iter = pSketch3D->createIterator(); !iter.isDone(); iter.moveNext())
        {
            ss.add(wyap::Selection(iter.current()));
        }
    }
}

bool Sketch3DSelectGuiCmd::tryAddPositionGizmo_Impl(const wyap::SelectionSet& sels, std::list<wyap::GizmoSPtr>& gizmos)
{
    // 3D草图图元不支持Gizmo(仅属性面板修改)
    return false;
}

void Sketch3DSelectGuiCmd::onKeyDown(const KeyEvent& event)
{
    // 吞掉Ctrl+C/Ctrl+V(3D草图环境不支持复制/粘贴)
    if (event.key == KeyCode::CtrlC || event.key == KeyCode::CtrlV)
    {
        return;
    }
    __baseClass::onKeyDown(event);
}
