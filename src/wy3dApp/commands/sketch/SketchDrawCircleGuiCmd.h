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

#ifndef WY3DAPP_SKETCH_DRAW_CIRCLE_GUI_CMD_H
#define WY3DAPP_SKETCH_DRAW_CIRCLE_GUI_CMD_H

#include <wy3dApp/OsgGuiCommand.h>
#include <cfloat>
#include <memory>
#include <set>

#include <wyVector2.h>
#include <wy3dVector2.h>
#include <wydbElementId.h>
#include <wy3dSketchCircle.h>

#include "commands/transient/BasicTransient.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class MakeSketchCircle;
class GuiCmdHoverInputPopup1;
class GuiCmdHoverInputPopup2;

class SketchDrawCircleGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(SketchDrawCircleGuiCmd, wy3dApp::SketchDrawCircleGuiCmd, OsgGuiCommand)
public:
    SketchDrawCircleGuiCmd();
    virtual ~SketchDrawCircleGuiCmd();

protected:
    GuiCmdSketchInfo _sketchInfo;
    virtual wyap::CmdExecution::StartResult onStart() override;

protected:
    enum class Step
    {
        Undefined = 0,
        SpecifyCenterPnt = 1,
        SpecifyRadius = 2,
    };

    virtual void cleanUp() override;
    virtual void reset();
    virtual void onEscapeKey() override;

    bool finishStep(Step step);
    void gotoStep(Step step);

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseDown(const MouseEvent& event) override;
    void onFrame(double time) override;

private:
    std::set<wydb::ElementId> getSnapExcludeIds() const;

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
        double radius;

        HoverPopupState()
            : lastMouseX(DBL_MAX)
            , lastMouseY(DBL_MAX)
            , lastMouseMoveTime(-1.0)
            , point()
            , radius(0.0)
        {
            this->resetValue();
        }

        void resetValue()
        {
            point.set(0.0, 0.0);
            radius = 0.0;
        }
    };

private:
    Step _step;
    wy::Vector2 _centerPnt;
    double _radius;
    Snap2dContextSPtr _pSnapContext;

    CenterPointTransientSPtr _pCenterPointTransient;
    std::shared_ptr<MakeSketchCircle> _pMakeSketchCircle;

    std::unique_ptr<GuiCmdHoverInputPopup2> _pXYPopup;
    std::unique_ptr<GuiCmdHoverInputPopup1> _pRadiusPopup;
    HoverPopupState _hoverPopupState;
};

class MakeSketchCircle : public GuiCmdMakeElement
{
public:
    MakeSketchCircle(GuiCommand* pGuiCmd)
        : GuiCmdMakeElement(pGuiCmd)
        , _pSketchCircle(nullptr)
    {}

    ~MakeSketchCircle() {}

    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

    wydb::ElementId getId() const
    {
        return _pSketchCircle ? _pSketchCircle->getId() : wydb::ElementId::kNull;
    }

    bool init(const wy::Vector2& startPnt, wydb::ElementId sketchId);
    bool update(double radius);

private:
    wy3d::SketchCircle* _pSketchCircle;
};

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_DRAW_CIRCLE_GUI_CMD_H
