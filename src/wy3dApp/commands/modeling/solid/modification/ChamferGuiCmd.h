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

#ifndef WY3DAPP_CHAMFER_GUI_CMD_H
#define WY3DAPP_CHAMFER_GUI_CMD_H

#include "commands/OsgGuiCommand.h"
#include <wy3dChamfer.h>
#include "select/SelectPreview.h"
#include "select/SelectionSetHighlightor.h"
#include "commands/GuiCommandMenu.h"

class ChamferGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(ChamferGuiCmd, wy3dApp::ChamferGuiCmd, OsgGuiCommand)
public:
    ChamferGuiCmd();
    virtual ~ChamferGuiCmd();

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;

    virtual void cleanup() override { this->reset(); }

protected:
    enum class Step
    {
        Undefined = 0,
        SelectEdges = 1,
        InputChamferDistance = 2,
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
    // 创建倒角
    bool createChamfer(unsigned int& errorCode);

private:
    Step _step;
    wyap::SelectionSet _sels;
    double _distance;

    // 点选选项
    PointPickOption _pointPickOption;

    // 预览
    SelectPreviewSPtr _pPreview;
    // 高亮
    SelectionSetHighlightorSPtr _pSelSetHighlightor;

    friend class ChamferGuiCmdMenu;
};

#endif // WY3DAPP_CHAMFER_GUI_CMD_H
