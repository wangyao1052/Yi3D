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

#ifndef WY3DAPP_COPY_GUI_CMD_H
#define WY3DAPP_COPY_GUI_CMD_H

#include <cfloat>
#include <wyVector2.h>
#include <wyVector3.h>
#include <wyapSelManager.h>
#include <wyapSelection.h>
#include <wy3dApp/OsgGuiCommand.h>
#include "commands/GuiCommandMenu.h"
#include "snap/SketchSnapSystem.h"
#include "commands/edit/CopyElements.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class CopyElemens;
class LineTransient;
class GuiCmdHoverInputPopup2;

class CopyGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(CopyGuiCmd, wy3dApp::CopyGuiCmd, OsgGuiCommand)
public:
    CopyGuiCmd();
    virtual ~CopyGuiCmd();

protected:
    GuiCmdSketchInfo _sketchInfo;
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;

protected:
    enum class Step
    {
        Undefined = 0,
        Step1_SelectElements  = 1,
        Step2_SpecifyStartPnt = 2,
        Step3_SpecifyEndPnt   = 3,
    };
    bool finishStep(Step step);
    void gotoStep(Step step);

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseDown(const MouseEvent& event) override;
    void onFrame(double time) override;

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
    void initializePopups();
    void showPopup();
    void hidePopup();
    void tryShowPopupOnHover(double time);
    void onPopupEnterKey();
    void onPopupEscapeKey();
    void simulateMouseMoveFromPopup();

private:
    struct HoverPopupState
    {
        double lastMouseX;
        double lastMouseY;
        double lastMouseMoveTime;
        wy::Vector3 point;
        wy::Vector3 vector;

        HoverPopupState()
            : lastMouseX(DBL_MAX),
              lastMouseY(DBL_MAX),
              lastMouseMoveTime(-1.0),
              point(0.0, 0.0, 0.0),
              vector(0.0, 0.0, 0.0)
        {
            this->resetValue();
        }

        void resetValue()
        {
            point.set(0.0, 0.0, 0.0);
            vector.set(0.0, 0.0, 0.0);
        }
    };

private:
    Step _step;
    wyap::SelectionSet _sels;
    wy::Vector2 _startPnt2d;
    wy::Vector3 _startPnt;
    wy::Vector2 _moveVec2d;
    wy::Vector3 _moveVec;
    SketchSnapContextSPtr _pSnapContext;

    std::shared_ptr<CopyElemens> _pCopyElements;
    std::shared_ptr<LineTransient> _pLineTransient;
    std::unique_ptr<GuiCmdHoverInputPopup2> _pXYPopup;
    std::unique_ptr<GuiCmdHoverInputPopup2> _pLengthAnglePopup;
    HoverPopupState _hoverPopupState;
};

}

#endif // WY3DAPP_MOVE_GUI_CMD_H
