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

#ifndef WY3DAPP_TOPONAME_GUI_CMD_H
#define WY3DAPP_TOPONAME_GUI_CMD_H

#include <TopAbs_ShapeEnum.hxx>

#include "commands/OsgGuiCommand.h"
#include "select/SelectPreview.h"
#include "select/SelectionSetHighlightor.h"
#include "commands/GuiCommandMenu.h"

class TopoNameGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(TopoNameGuiCmd, wy3dApp::TopoNameGuiCmd, OsgGuiCommand)
public:
    TopoNameGuiCmd();
    virtual ~TopoNameGuiCmd();

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;

protected:
    virtual void reset();

    virtual void onMouseMove(const MouseEvent& event) override;

    // 上下文菜单
    virtual GuiCmdMenu* initContextMenu() override;
    virtual bool isContextMenuActionVisible_CompleteSelection() const override;
    virtual void onContextMenuAction_CompleteSelection() override;
    virtual bool isContextMenuActionVisible_ClearSelection() const override;
    virtual void onContextMenuAction_ClearSelection() override;

private:
    void showTopoName(const wyap::Selection& sel);

private:
    // 点选选项
    PointPickOption _pointPickOption;

    // 预览
    SelectPreviewSPtr _pPreview;
    // 高亮
    SelectionSetHighlightorSPtr _pSelSetHighlightor;

    friend class TopoNameGuiCmdMenu;
};

class TopoNameGuiCmdMenu : public GuiCmdMenu
{
    Q_OBJECT
public:
    explicit TopoNameGuiCmdMenu(TopoNameGuiCmd* pCmd) : GuiCmdMenu(pCmd) {}

protected:
    // 初始化客制化菜单项
    virtual bool initCustomHeaderActions(QMenu* menu) override;

    // 通过拓扑名称查找边
    void onFindEdgesByTopoName();
    // 通过拓扑名称查找面
    void onFindFacesByTopoName();

private:
    void addSelection(TopAbs_ShapeEnum shapeType, const std::string& topoName);
};

#endif // WY3DAPP_TOPONAME_GUI_CMD_H
