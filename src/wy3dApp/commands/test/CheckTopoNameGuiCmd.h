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

#ifndef WY3DAPP_CHECK_TOPONAME_GUI_CMD_H
#define WY3DAPP_CHECK_TOPONAME_GUI_CMD_H

#include <vector>
#include <string>
#include <map>
#include <TopAbs_ShapeEnum.hxx>

#include "commands/OsgGuiCommand.h"
#include "select/SelectPreview.h"
#include "commands/GuiCommandMenu.h"

class CheckTopoNameGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(CheckTopoNameGuiCmd, wy3dApp::CheckTopoNameGuiCmd, OsgGuiCommand)
public:
    CheckTopoNameGuiCmd();
    virtual ~CheckTopoNameGuiCmd();

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;

protected:
    virtual void reset();

    virtual void onMouseMove(const MouseEvent& event) override;

    // 上下文菜单
    virtual GuiCmdMenu* initContextMenu() override;

private:
    void checkTopoName(const wyap::Selection& sel);

private:
    // 点选选项
    PointPickOption _pointPickOption;
    // 预览
    SelectPreviewSPtr _pPreview;
    //
    std::map<wydb::ElementId, std::vector<std::string>> _id2CheckInfo;

    friend class CheckTopoNameGuiCmdMenu;
};

class CheckTopoNameGuiCmdMenu : public GuiCmdMenu
{
    Q_OBJECT
public:
    explicit CheckTopoNameGuiCmdMenu(CheckTopoNameGuiCmd* pCmd) : GuiCmdMenu(pCmd) {}

protected:
    // 初始化客制化菜单项
    virtual bool initCustomHeaderActions(QMenu* menu) override;

private:
    void onCheckAll();
};

#endif // WY3DAPP_CHECK_TOPONAME_GUI_CMD_H