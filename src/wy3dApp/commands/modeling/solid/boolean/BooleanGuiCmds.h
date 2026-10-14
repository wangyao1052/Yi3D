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

#ifndef WY3DAPP_BOOLEAN_GUI_CMDS_H
#define WY3DAPP_BOOLEAN_GUI_CMDS_H

#include <set>
#include <memory>
#include <wydbElementId.h>
#include <wyapSelManager.h>
#include <wy3dBoolean.h>

#include <wy3dApp/OsgGuiCommand.h>
#include "commands/GuiCommandMenu.h"
#include "commands/transient/ElementNodeHighlightor.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class BooleanGuiCmd : public OsgGuiCommand, public wyap::SelManagerReactor
{
    WYRX_DECLARE_MEMBERS(BooleanGuiCmd, wy3dApp::BooleanGuiCmd, OsgGuiCommand)
public:
    BooleanGuiCmd();
    explicit BooleanGuiCmd(wy3d::BooleanType boolType);
    virtual ~BooleanGuiCmd();

    // 选择集变更
    virtual void onSelectionChanged(
        const wyap::SelectionSet& addedSS,
        const wyap::SelectionSet& removedSS,
        const wyap::SelectionSet& currSS) override;

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;

protected:
    enum class Step
    {
        Undefined = 0,
        SpecifyTarget = 1,
        SpecifyTools = 2,
    };
    virtual void reset();
    bool finishStep(Step step);
    void gotoStep(Step step);

    // 特征树节点单击事件
    virtual void onFeatureTreeItemClicked(const wydb::ElementId& id) override;

    // Enter键响应
    virtual void onEnterKey() override;
    // Space键响应
    virtual void onSpaceKey() override;

    // 上下文菜单
    virtual bool isContextMenuActionVisible_CompleteSelection() const override;
    virtual void onContextMenuAction_CompleteSelection() override;
    virtual bool isContextMenuActionVisible_ClearSelection() const override;
    virtual void onContextMenuAction_ClearSelection() override;

private:
    // 新增布尔特征
    bool newBooleanFeature();
    // 执行
    bool perform();

private:
    // 步骤
    Step _step;
    // 布尔类型
    wy3d::BooleanType _boolType;
    // 目标体
    wydb::ElementId _targetId;
    // 参与体
    std::set<wydb::ElementId> _toolIds;
    // 目标体临时显示对象
    SelectionSetHighlightor _targetHighlightor;
};

class UnionGuiCmd : public BooleanGuiCmd
{
    WYRX_DECLARE_MEMBERS(UnionGuiCmd, wy3dApp::UnionGuiCmd, BooleanGuiCmd);
public:
    UnionGuiCmd() : BooleanGuiCmd(wy3d::BooleanType::Union) {}
    ~UnionGuiCmd() {}
};

class SubtractGuiCmd : public BooleanGuiCmd
{
    WYRX_DECLARE_MEMBERS(SubtractGuiCmd, wy3dApp::SubtractGuiCmd, BooleanGuiCmd);
public:
    SubtractGuiCmd() : BooleanGuiCmd(wy3d::BooleanType::Difference) {}
    ~SubtractGuiCmd() {}
};

class IntersectGuiCmd : public BooleanGuiCmd
{
    WYRX_DECLARE_MEMBERS(IntersectGuiCmd, wy3dApp::IntersectGuiCmd, BooleanGuiCmd);
public:
    IntersectGuiCmd() : BooleanGuiCmd(wy3d::BooleanType::Intersection) {}
    ~IntersectGuiCmd() {}
};

} // namespace wy3dApp

#endif // WY3DAPP_BOOLEAN_GUI_CMDS_H
