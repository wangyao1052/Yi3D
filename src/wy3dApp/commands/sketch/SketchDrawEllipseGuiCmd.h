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

#ifndef WY3DAPP_SKETCH_DRAW_ELLIPSE_GUI_CMD_H
#define WY3DAPP_SKETCH_DRAW_ELLIPSE_GUI_CMD_H

#include <wy3dApp/OsgGuiCommand.h>
#include <cfloat>
#include <memory>
#include <set>
#include <wyVector2.h>
#include <wy3dVector2.h>
#include <wydbElementId.h>
#include <wy3dSketchEllipse.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class MakeSketchEllipse;
class LineTransient;
class GuiCmdHoverInputPopup1;
class GuiCmdHoverInputPopup2;

class SketchDrawEllipseGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(SketchDrawEllipseGuiCmd, wy3dApp::SketchDrawEllipseGuiCmd, OsgGuiCommand)
public:
    SketchDrawEllipseGuiCmd();
    virtual ~SketchDrawEllipseGuiCmd();

protected:
    GuiCmdSketchInfo _sketchInfo;
    virtual wyap::CmdExecution::StartResult onStart() override;

protected:
    enum class Step
    {
        Undefined = 0,
        SpecifyCenterPnt = 1,
        SpecifyAxisEndPoint = 2,
        SpecifyOtherRadius = 3
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
        double majorAxisLength;
        double majorAxisAngleDeg;
        double otherRadius;

        HoverPopupState() :
            lastMouseX(DBL_MAX),
            lastMouseY(DBL_MAX),
            lastMouseMoveTime(-1.0),
            point(),
            majorAxisLength(0.0),
            majorAxisAngleDeg(0.0),
            otherRadius(0.0)
        {
            this->resetValue();
        }

        void resetValue()
        {
            point.set(0.0, 0.0);
            majorAxisLength = 0.0;
            majorAxisAngleDeg = 0.0;
            otherRadius = 0.0;
        }
    };

private:
    Step _step;
    wy::Vector2 _centerPnt;
    wy::Vector2 _majorAxis;
    double _otherRadius;
    SketchSnapContextSPtr _pSnapContext;

    // 创建SketchEllipse
    std::shared_ptr<MakeSketchEllipse> _pMakeSketchEllipse;
    std::shared_ptr<LineTransient> _pLineTransient;
    std::unique_ptr<GuiCmdHoverInputPopup2> _pXYPopup;
    std::unique_ptr<GuiCmdHoverInputPopup2> _pLengthAnglePopup;
    std::unique_ptr<GuiCmdHoverInputPopup1> _pRadiusPopup;
    HoverPopupState _hoverPopupState;
};

class MakeSketchEllipse : public GuiCmdMakeElement
{
public:
    MakeSketchEllipse(GuiCommand* pGuiCmd) : GuiCmdMakeElement(pGuiCmd), _pSketchEllipse(nullptr) {}
    ~MakeSketchEllipse() {}

    // 收集创建的元素ID
    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

    wydb::ElementId getId() const
    {
        return _pSketchEllipse ? _pSketchEllipse->getId() : wydb::ElementId::kNull;
    }

    // 创建
    bool init(const wy::Vector2& centerPnt, const wy::Vector2& majorAxis, double radiusRatio, wydb::ElementId sketchId);
    // 更新
    bool update(double otherRadius, bool autoAdjustMajorMirorAxis = false);

private:
    wy3d::SketchEllipse* _pSketchEllipse;
};

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_DRAW_ELLIPSE_GUI_CMD_H
