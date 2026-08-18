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

#ifndef WY3DAPP_SEWN_SHEET_GUI_CMD_H
#define WY3DAPP_SEWN_SHEET_GUI_CMD_H

#include <set>
#include <wydbElementId.h>
#include <wyapSelManager.h>
#include <wy3dSewnSheet.h>

#include "commands/OsgGuiCommand.h"

class SewnSheetGuiCmd : public OsgGuiCommand, public wyap::SelManagerReactor
{
    WYRX_DECLARE_MEMBERS(SewnSheetGuiCmd, wy3dApp::SewnSheetGuiCmd, OsgGuiCommand)
public:
    SewnSheetGuiCmd();
    virtual ~SewnSheetGuiCmd();

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
        SelectSources = 1,
    };
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
    // 新增缝合片体特征（默认容差）
    bool newSewnSheet();

private:
    // 步骤
    Step _step;
    // 源片体
    std::set<wydb::ElementId> _sourceIds;
};

#endif // WY3DAPP_SEWN_SHEET_GUI_CMD_H
