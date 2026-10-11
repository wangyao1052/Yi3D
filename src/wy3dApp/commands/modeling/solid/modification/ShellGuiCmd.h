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

#ifndef WY3DAPP_SHELL_GUI_CMD_H
#define WY3DAPP_SHELL_GUI_CMD_H

#include <wy3dApp/OsgGuiCommand.h>
#include <wy3dShell.h>
#include <wy3dApp/SelectPreview.h>
#include "select/SelectionSetHighlightor.h"
#include "commands/GuiCommandMenu.h"

class ShellGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(ShellGuiCmd, wy3dApp::ShellGuiCmd, OsgGuiCommand)
public:
    ShellGuiCmd();
    virtual ~ShellGuiCmd();

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;

protected:
    enum class Step
    {
        Undefined = 0,
        SelectFaces = 1,
        InputThickness = 2,
    };
    virtual void reset();
    bool finishStep(Step step);
    void gotoStep(Step step);

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseUp(const MouseEvent& event) override;

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
    // 创建抽壳
    bool createShell(const wyap::SelectionSet& sels, double thickness, unsigned int& errorCode);

private:
    Step _step;
    wyap::SelectionSet _sels;
    double _thickness;

    // 点选选项
    PointPickOption _pointPickOption;

    // 预览
    SelectPreview _preview;
    // 高亮
    SelectionSetHighlightorSPtr _pSelSetHighlightor;

    friend class ShellGuiCmdMenu;
};

#endif // WY3DAPP_SHELL_GUI_CMD_H