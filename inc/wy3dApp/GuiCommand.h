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

#ifndef WY3DAPP_GUI_CMDS_H
#define WY3DAPP_GUI_CMDS_H

#include <set>
#include <cassert>

#include <QString>
#include <QCoreApplication>
#include <QLabel>

#include <wyVector2.h>
#include <wyVector3.h>
#include <wy3dVector2.h>
#include <wy3dVector3.h>
#include <wydbElementId.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapCmdExecution.h>
#include "snap/SnapSystemBase.h"
#include <wy3dPlane.h>
#include <wy3dSelectionType.h>
#include <wy3dApp/ContextMenu.h>
#include <wy3dApp/GuiCmdTransient.h>
#include <wy3dApp/KeyCode.h>
#include "widgets/CursorType.h"
#include <wy3dApp/Snap2dContext.h>
#include <wy3dApp/Snap2dResult.h>
#include "scene/nodes/ElementNodeType.h"

#include "select/SelectMode.h"
#include "select/SelectFilterFunctors.h"
#include <wy3dApp/SelectPreview.h>
#include "select/SelectionSetHighlightor.h"

#include <wy3dApp/GuiCommandUtil.h>

class GuiCmdOsgEventHandler;

#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class GuiCommand;
class Snap2dSystem;
class GuiEventDispatcher;

// Mouse event
struct MouseEvent
{
    // Window coordinates: origin bottom left, device pixels.
    float x, y;
    // Seconds since the program started.
    double time;
};

// Keyboard event.
struct KeyEvent
{
    // Key code; see KeyCode.
    int key;
    // Modifier key state; see KeyModifier.
    int modifiers;
    // Seconds since the program started.
    double time;
};

class WY3DAPP_EXPORT GuiCommand : public wyap::InteractiveCmdExecution
{
    WYRX_DECLARE_ABSTRACT_MEMBERS(GuiCommand, wy3dApp::GuiCommand, wyap::InteractiveCmdExecution)
protected:
    GuiCommand();
    virtual ~GuiCommand();

    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;

    // Command selection options.
    struct SelectOptions
    {
        // Whether point selection is enabled.
        bool pointSelect = false;
        // Whether box selection is enabled.
        bool boxSelect = false;
        // Whether hover preview is enabled (point selection only).
        bool preview = true;
        // Allowed element types (ElementNodeType bit set).
        unsigned int allowedElemTypes = ElementNodeType::All;
        // Selection type.
        wy3d::SelectionType selectionType = wy3d::SelectionType::Element;
        // Pre-filter functor.
        SelectPreFilterFunctorSPtr preFilter = nullptr;
        // Post-filter functor.
        SelectPostFilterFunctorSPtr postFilter = nullptr;
        // Selection mode: replace or add.
        SelectMode selectMode = SelectMode::Replace;
    };
    // Fills the command's selection options; called at command start.
    virtual void initializeSelectOptions(SelectOptions& selectOptions)
    {
        selectOptions.pointSelect = false;
        selectOptions.boxSelect = false;
        selectOptions.preview = true;
        selectOptions.allowedElemTypes = ElementNodeType::All;
        selectOptions.selectionType = wy3d::SelectionType::Element;
        selectOptions.preFilter = nullptr;
        selectOptions.postFilter = nullptr;
        selectOptions.selectMode = SelectMode::Replace;
    }

    // Clean up temporary command resources,
    // called on command termination (onEnd / onAbort).
    virtual void cleanUp() {}

    // Frame update event
    // time: seconds since the program started.
    virtual void onFrame(double time) {}

    // Mouse event handlers
    virtual void onLeftMouseDown(const MouseEvent& event) {}
    virtual void onLeftMouseUp(const MouseEvent& event) {}
    virtual void onMiddleMouseDown(const MouseEvent& event) {}
    virtual void onMiddleMouseUp(const MouseEvent& event) {}
    virtual void onRightMouseDown(const MouseEvent& event) {}
    virtual void onRightMouseUp(const MouseEvent& event);
    virtual void onMouseMove(const MouseEvent& event) {}
    virtual void onLeftMouseDoubleClicked(const MouseEvent& event) {}
    virtual void onRightMouseDoubleClicked(const MouseEvent& event) {}

    // Keyboard event handlers
    virtual void onKeyDown(const KeyEvent& event);
    virtual void onEscapeKey() { this->requestAbort(AbortCause::UserCancel); }
    virtual void onEnterKey() {}
    virtual void onSpaceKey() {}
    virtual void onKeyUp(const KeyEvent& event) {}

    // Triggered when an item in the feature tree is clicked.
    virtual void onFeatureTreeItemClicked(const wydb::ElementId& id) { return; }

    // Configures the command's context menu; called before the standard items are
    // appended, so custom items added here stay at the top.
    virtual void configureContextMenu(ContextMenu& menu) {}
    // The Complete Selection menu item's handler.
    virtual void onCompleteSelectionMenuItem() {}
    // The Clear Selection menu item's handler.
    virtual void onClearSelectionMenuItem() {}

    // Applies the options to the selection handler.
    void applySelectOptions(SelectOptions options);
    // Reads the options back from the selection handler.
    SelectOptions getSelectOptions();

    // Converts desktop screen coordinates (top-left origin, QCursor::pos) to window
    // coordinates (bottom-left origin, device pixels), the same system as MouseEvent.
    virtual void screenToWindow(
        double screenX, double screenY,
        double& windowX, double& windowY) const = 0;

    // Command point-pick options.
    struct PointPickOptions
    {
        // Allowed element types (ElementNodeType bit set).
        unsigned int allowedElemTypes = ElementNodeType::All;
        // Selection type.
        wy3d::SelectionType selType = wy3d::SelectionType::Element;
        // Pre-filter functor.
        SelectPreFilterFunctorSPtr pPreFilter = nullptr;
        // Post-filter functor.
        SelectPostFilterFunctorSPtr pPostFilter = nullptr;
    };

    // Picks an element at window coordinates (x, y), the same system as MouseEvent.
    // Returns a Selection with a null element id if nothing matches option.
    virtual wyap::Selection pointPick(
        double x, double y,
        const PointPickOptions& option) = 0;

    // 3D point on the given plane for window coordinates (x, y), using the same
    // coordinate system as MouseEvent.
    //
    // When snap is true, object snapping runs first; on a hit the returned point is
    // the snap position projected onto workPlane. Otherwise the view ray is
    // intersected with workPlane. excludeIds are skipped by the snap.
    //
    // Returns the point and the snap result, the latter being null when snap is
    // false or nothing was snapped.
    virtual std::pair<wy::Vector3, wyap::SnapResultSPtr> computePoint(
        double x, double y,
        const wy3d::Plane& workPlane,
        bool snap = true,
        const std::set<wydb::ElementId>& snapExcludeIds = {}) = 0;

    // 2D point on the given plane; the 2D counterpart of computePoint.
    //
    // Takes the same arguments and snapping behavior as computePoint; the point is
    // expressed in the plane's UV coordinates (Plane::uv). Returns the point and the
    // snap result, the latter being null when snap is false or nothing was snapped.
    virtual std::pair<wy::Vector2, wyap::SnapResultSPtr> computePoint2d(
        double x, double y,
        const wy3d::Plane& workPlane,
        bool snap = true,
        const std::set<wydb::ElementId>& snapExcludeIds = {}) = 0;

    // 2D point in the snap system's plane for window coordinates (x, y), using the
    // same coordinate system as MouseEvent.
    //
    // Snaps through pSnap2dSystem, passing pSnapContext to the registered snap rules;
    // snapExcludeIds are skipped by the snap. When nothing snaps, the cursor position
    // projected onto the plane is returned. Returns the point and the snap result, the
    // latter being null when nothing was snapped. A null system or context yields
    // (0, 0) and a null result.
    virtual std::pair<wy::Vector2, Snap2dResultSPtr> computePoint2d(
        double x, double y,
        Snap2dSystem* pSnap2dSystem,
        std::shared_ptr<Snap2dContext> pSnapContext,
        const std::set<wydb::ElementId>& snapExcludeIds = {}) = 0;

    // Point on the axis axisOrigin + t*axisDirection closest to the cursor ray
    // (window coordinates, same as MouseEvent). Object snapping runs first; on a
    // hit the snapped point is projected onto the axis. Returns axisOrigin when
    // the view is unavailable or the ray is parallel to the axis.
    virtual wy::Vector3 computePointOnAxis(double x, double y,
        const wy::Vector3& axisOrigin,
        const wy::Vector3& axisDirection,
        const std::set<wydb::ElementId>& excludeIds = {}) = 0;

private:
    friend class FeatureTreeWidget;
};

} // namespace wy3dApp

#endif // WY3DAPP_GUI_CMDS_H
