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

#ifndef WY3DAPP_SKETCH_DRAW_POINT_GUI_CMD_H
#define WY3DAPP_SKETCH_DRAW_POINT_GUI_CMD_H

#include <wy3dApp/OsgGuiCommand.h>
#include <cfloat>
#include <memory>
#include <set>
#include <wyVector2.h>
#include <wy3dVector2.h>
#include <wydbElementId.h>
#include <wy3dSketchPoint.h>
#include "wy3dAppDefs.h"

namespace wy3dApp {

class GuiCmdHoverInputPopup2;

class SketchDrawPointGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(SketchDrawPointGuiCmd, wy3dApp::SketchDrawPointGuiCmd, OsgGuiCommand)
public:
    SketchDrawPointGuiCmd();

protected:
    GuiCmdSketchInfo _sketchInfo;
    virtual wyap::CmdExecution::StartResult onStart() override;

protected:
    enum class Step
    {
        Undefined = 0,
        SpecifyPosition = 1,
    };
    virtual void cleanup() override;
    virtual void reset();
    virtual void onEscapeKey() override;
    bool finishStep(Step step);
    void gotoStep(Step step);

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseDown(const MouseEvent& event) override;
    void onFrame(double time) override;

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
        // 鼠标信息
        double lastMouseX;
        double lastMouseY;
        double lastMouseMoveTime;

        // 值信息
        wy::Vector2 point;

        HoverPopupState() :
            lastMouseX(DBL_MAX),
            lastMouseY(DBL_MAX),
            lastMouseMoveTime(-1.0),
            point()
        {
            this->resetValue();
        }

        void resetValue()
        {
            point.set(0.0, 0.0);
        }
    };

private:
    // 步骤
    Step _step;
    // 位置
    wy::Vector2 _position;
    // 草图捕捉上下文
    SketchSnapContextSPtr _pSnapContext;
    // 悬停浮窗
    std::unique_ptr<GuiCmdHoverInputPopup2> _pXYPopup;
    HoverPopupState _hoverPopupState;
};

class MakeSketchPoint : public GuiCmdMakeElement
{
public:
    MakeSketchPoint(GuiCommand* pGuiCmd) : GuiCmdMakeElement(pGuiCmd), _pSketchPoint(nullptr) {}

    // 收集创建的元素ID
    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

    // 创建
    bool perform(const wy::Vector2& position, wydb::ElementId sketchId);

private:
    wy3d::SketchPoint* _pSketchPoint;
};

}

#endif // WY3DAPP_SKETCH_DRAW_POINT_GUI_CMD_H
