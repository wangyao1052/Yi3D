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

#ifndef WY3DAPP_SKETCH_TEXT_COMMAND_H
#define WY3DAPP_SKETCH_TEXT_COMMAND_H

#include <wy3dApp/OsgGuiCommand.h>
#include <cfloat>
#include <map>
#include <memory>
#include <osg/PositionAttitudeTransform>
#include <wyVector2.h>
#include <wy3dMakeSketchText.h>
#include "commands/sketch/MakeText.h"
#include "wy3dAppDefs.h"

namespace wy3dApp {

class MakeText;
class GuiCmdHoverInputPopup2;

struct SketchTextHistory
{
    wy3d::MakeSketchText::Data data;
    QFont font;
};

class SketchTextCommand : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(SketchTextCommand, wy3dApp::SketchTextCommand, OsgGuiCommand)
public:
    SketchTextCommand();
    virtual ~SketchTextCommand();

protected:
    GuiCmdSketchInfo _sketchInfo;
    virtual wyap::CmdExecution::StartResult onStart() override;

protected:
    enum class Step
    {
        Undefined = 0,
        MakeText = 1,
        SpecifyPosition = 2,
    };
    bool finishStep(Step step);
    void gotoStep(Step step);

    virtual void onEscapeKey() override;

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
        double lastMouseX;
        double lastMouseY;
        double lastMouseMoveTime;
        wy::Vector2 point;

        HoverPopupState()
            : lastMouseX(DBL_MAX),
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
    Step _step;
    wy3d::MakeSketchText::Data _data;
    wy::Vector2 _position;
    SketchSnapContextSPtr _pSnapContext;
    std::unique_ptr<GuiCmdHoverInputPopup2> _pXYPopup;
    HoverPopupState _hoverPopupState;
    std::shared_ptr<MakeText> _pMakeText;
    bool _isRevising; // 是否在修订状态,在第二步SpecifyPosition中按住Esc键回退到上一步修改时会设置该值为true

    static std::shared_ptr<SketchTextHistory> _pHistory;
};

}

#endif // WY3DAPP_SKETCH_TEXT_COMMAND_H
