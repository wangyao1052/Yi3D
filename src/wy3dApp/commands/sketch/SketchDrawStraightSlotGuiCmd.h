///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 Wang Yao <wangyao1052@163.com>
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

#ifndef WY3DAPP_COMMANDS_SKETCH_SKETCH_DRAW_STRAIGHT_SLOT_GUI_CMD_H
#define WY3DAPP_COMMANDS_SKETCH_SKETCH_DRAW_STRAIGHT_SLOT_GUI_CMD_H

#include <limits>
#include <memory>
#include <set>

#include <wyVector2.h>
#include <wydbElementId.h>
#include <wy3dApp/GuiCommandUtil.h>
#include <wy3dApp/OsgGuiCommand.h>
#include <wy3dApp/Snap2dContext.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wydb
{
class Transaction;
}

namespace wy3dApp {

class GuiCmdHoverInputPopup1;
class GuiCmdHoverInputPopup2;
class GuiCmdHoverInputPopupBase;
class SlotGuideTransient;
class StatusBarHelper;

// IDs of the four real sketch entities created for the in-progress slot.
struct SlotElements
{
    wydb::ElementId rightArc;
    wydb::ElementId upperLine;
    wydb::ElementId leftArc;
    wydb::ElementId lowerLine;

    bool isValid() const
    {
        return !rightArc.isNull() && !upperLine.isNull()
            && !leftArc.isNull() && !lowerLine.isNull();
    }

    void reset()
    {
        rightArc = wydb::ElementId::kNull;
        upperLine = wydb::ElementId::kNull;
        leftArc = wydb::ElementId::kNull;
        lowerLine = wydb::ElementId::kNull;
    }
};

// Three-click SolidWorks-style straight slot:
//   1. first end-center
//   2. second end-center
//   3. move to set width, click to commit
// The command stays active after a slot is completed so several slots can be
// drawn in a row.
class SketchDrawStraightSlotGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(
        SketchDrawStraightSlotGuiCmd,
        wy3dApp::SketchDrawStraightSlotGuiCmd,
        wy3dApp::OsgGuiCommand)

public:
    SketchDrawStraightSlotGuiCmd();
    ~SketchDrawStraightSlotGuiCmd() override;

protected:
    wyap::CmdExecution::StartResult onStart() override;
    void cleanUp() override;
    void onFrame(double time) override;
    void onMouseMove(const MouseEvent& event) override;
    void onLeftMouseDown(const MouseEvent& event) override;
    void onEscapeKey() override;

private:
    enum class Step
    {
        FirstCenter = 0,
        SecondCenter = 1,
        Width = 2,
    };

    void resetToFirstCenter();
    void updateStatusTip();
    wy::Vector2 computePoint(const MouseEvent& event);
    std::set<wydb::ElementId> getSnapExcludeIds() const;
    double computeWidth(const wy::Vector2& cursor) const;

    bool startSlotPreview(double width);
    bool updateSlotPreview(double width);
    bool commitSlot(double width);
    void abortSlotPreview();

    void initializePopups();
    void hidePopups();
    GuiCmdHoverInputPopupBase* getActivePopup() const;
    void showPopup();
    void tryShowPopupOnHover(double time);
    void onPopupAccept();
    void onPopupCancel();
    void confirmFirstCenter(const wy::Vector2& point);
    bool confirmSecondCenter(const wy::Vector2& point);

private:
    struct HoverPopupState
    {
        double lastMouseX = std::numeric_limits<double>::max();
        double lastMouseY = std::numeric_limits<double>::max();
        double lastMouseMoveTime = -1.0;
        wy::Vector2 point;
        double length = 0.0;
        double angleDeg = 0.0;

        // Keeps the mouse fields so the next step's popup can be shown
        // immediately after a step transition, without waiting for a move.
        void resetValue()
        {
            point = wy::Vector2();
            length = 0.0;
            angleDeg = 0.0;
        }
    };

    Step _step = Step::FirstCenter;
    wy::Vector2 _firstCenter;
    wy::Vector2 _secondCenter;
    double _currentWidth = 0.0;
    GuiCmdSketchInfo _sketchInfo;
    Snap2dContextSPtr _snapContext;
    std::unique_ptr<SlotGuideTransient> _guide;
    StatusBarHelper* _statusBar = nullptr;
    HoverPopupState _hover;
    std::unique_ptr<GuiCmdHoverInputPopup2> _pXYPopup;
    std::unique_ptr<GuiCmdHoverInputPopup2> _pLengthAnglePopup;
    std::unique_ptr<GuiCmdHoverInputPopup1> _pWidthPopup;

    // Outer transaction group for the in-progress slot, and the four real
    // entities currently standing in for the slot outline.
    wydb::Transaction* _pTransGroup = nullptr;
    SlotElements _slotElements;
};

} // namespace wy3dApp

#endif // WY3DAPP_COMMANDS_SKETCH_SKETCH_DRAW_STRAIGHT_SLOT_GUI_CMD_H
