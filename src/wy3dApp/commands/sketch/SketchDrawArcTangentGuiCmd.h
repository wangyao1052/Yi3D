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

#ifndef WY3DAPP_SKETCH_DRAW_ARC_TANGENT_GUI_CMD_H
#define WY3DAPP_SKETCH_DRAW_ARC_TANGENT_GUI_CMD_H

#include <wy3dApp/OsgGuiCommand.h>
#include <cfloat>
#include <memory>
#include <set>
#include <wyVector2.h>
#include <wy3dVector2.h>
#include <wydbElementId.h>
#include "commands/transient/BasicTransient.h"
#include "commands/transient/SketchBasicTransient.h"
#include "commands/dialogs/SketchArcTangentModePanel.h"
#include "snap/SketchSnapContext.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class GuiCmdHoverInputPopupBase;
class GuiCmdHoverInputPopup1;
class GuiCmdHoverInputPopup2;

class SketchDrawArcTangentGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(SketchDrawArcTangentGuiCmd, wy3dApp::SketchDrawArcTangentGuiCmd, OsgGuiCommand)
public:
    SketchDrawArcTangentGuiCmd();
    virtual ~SketchDrawArcTangentGuiCmd();

protected:
    GuiCmdSketchInfo _sketchInfo;
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;

protected:
    enum class Step
    {
        Undefined = 0,
        SpecifyStartPoint = 1,
        SpecifyEndPoint = 2,   // ArcTangentMode::PassPoint
        SpecifyRadius = 3,     // ArcTangentMode::Radius, first step
        SpecifySweepAngle = 4  // ArcTangentMode::Radius, second step
    };
    virtual void cleanup() override;
    virtual void reset();
    virtual void onEscapeKey() override;
    bool finishStep(Step step);
    void gotoStep(Step step);
    // Step the current mode enters once the start point is anchored.
    Step entryStepForMode() const;

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseDown(const MouseEvent& event) override;
    void onFrame(double time) override;

private:
    std::set<wydb::ElementId> getSnapExcludeIds() const;

    // Mode switch panel: created on first use, shown for the second step only.
    void showModePanel();
    void hideModePanel();
    void destroyModePanel();
    void onModeChanged(ArcTangentMode mode);

    // Hover input popup.
    void initializePopups();
    void showPopup();
    void hidePopup();
    GuiCmdHoverInputPopupBase* getActivePopup() const;
    void tryShowPopupOnHover(double time);
    void onPopupEnterKey();
    void onPopupEscapeKey();
    void simulateMouseMoveFromPopup();

    // Get the arc start point and the outward unit tangent of the reference curve at that point.
    bool getEndpointSnap(wy::Vector2& startPnt, wy::Vector2& tangent) const;
    // Freeze the radius: the bend side is the one the cursor last hovered on.
    void commitRadius(double radius);
    // Sweep step: the end point on the frozen circle turned from the start point by sweepAngle in
    // the frozen bend direction.
    void setSweepPreview(double sweepAngle);
    // Update the arc preview for the current end point.
    void updatePreview();
    // Create the arc for the current end point.
    bool createArc();

private:
    struct HoverPopupState
    {
        double lastMouseX;
        double lastMouseY;
        double lastMouseMoveTime;

        // Current end point (PassPoint and Radius modes).
        wy::Vector2 point;
        // Current live radius in the Radius mode; 0 when the cursor has no solution.
        double radius;
        // Current live sweep angle in degrees in the sweep step.
        double sweepAngleDeg;

        HoverPopupState() :
            lastMouseX(DBL_MAX),
            lastMouseY(DBL_MAX),
            lastMouseMoveTime(-1.0),
            point(),
            radius(0.0),
            sweepAngleDeg(0.0)
        {
            this->resetValue();
        }

        void resetValue()
        {
            point.set(0.0, 0.0);
            radius = 0.0;
            sweepAngleDeg = 0.0;
        }
    };

private:
    Step _step;
    // Mode of the second step; kept for the whole command session.
    ArcTangentMode _mode;
    // Arc start point and the outward unit tangent of the reference curve at that point.
    wy::Vector2 _startPnt;
    wy::Vector2 _startTangent;
    // Current end point: the cursor point (PassPoint mode) or, on the frozen circle, the point the
    // arc is committed to (Radius mode).
    wy::Vector2 _endPnt;
    SketchSnapContextSPtr _pSnapContext;

    // Radius mode: the circle the committed radius and the frozen bend side define.
    wy::Vector2 _center;
    double _radius;

    // Arc preview.
    SketchArcTransientSPtr _pArcTransient;

    // Center marker, shown whenever the preview arc has a solution.
    CenterPointTransientSPtr _pCenterPointTransient;

    // Mode switch panel.
    SketchArcTangentModePanel* _pModePanel;

    // Hover input popups.
    std::unique_ptr<GuiCmdHoverInputPopup2> _pXYPopup;
    std::unique_ptr<GuiCmdHoverInputPopup1> _pRadiusPopup;
    std::unique_ptr<GuiCmdHoverInputPopup1> _pSweepPopup;
    HoverPopupState _hoverPopupState;
};

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_DRAW_ARC_TANGENT_GUI_CMD_H
