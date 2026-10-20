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
#include "scene/nodes/ElementNodeType.h"

#include "select/SelectMode.h"
#include "select/SelectFilterFunctors.h"
#include <wy3dApp/SelectPreview.h>
#include "select/SelectionSetHighlightor.h"

#include <wy3dApp/GuiCommandUtil.h>

class GuiCmdOsgEventHandler;

#include "commands/GuiCmdMakeElement.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class GuiCmdMakeElement;
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

    // Command snap options.
    struct SnapOptions
    {
        // Elements to exclude from snapping.
        std::set<wydb::ElementId> excludeIds;
        // Whether to apply object snapping.
        bool snap = true;
    };

    virtual std::pair<wy::Vector3, wyap::SnapResultSPtr> computePoint(
        double x, double y,
        const wy3d::Plane& sketchPlane,
        const SnapOptions& options = {}) = 0;

    virtual wy::Vector2 computePosition2d(
        double x, double y,
        const wy3d::Plane& sketchPlane,
        const std::set<wydb::ElementId>& excludeIds,
        std::shared_ptr<Snap2dContext> pSnapContext = nullptr,
        Snap2dSystem* pSnap2dSys = nullptr) = 0;

    virtual wy::Vector2 computePosition2dWithoutSnap(
        double x, double y,
        const wy3d::Plane& sketchPlane) = 0;

    virtual bool computeHeight(double x, double y,
        const wy::Vector3& basePnt, double& height,
        const GuiCmdMakeElement* pMakeElement = nullptr) = 0;
    virtual bool computeHeight(double x, double y,
        const osg::Vec3d& basePnt, double& height,
        const GuiCmdMakeElement* pMakeElement = nullptr) = 0;
    virtual bool computeHeight2(double x, double y,
        const wy3d::Plane& workPln,
        const wy::Vector2& basePnt,
        const std::set<wydb::ElementId>& excludeIds,
        double& height) = 0;

    virtual bool computeRotationAngle(double x, double y,
        const wy3d::Plane& workPln,
        const wy::Vector2& basis,
        const std::set<wydb::ElementId>& excludeIds,
        double& rotationAngle) = 0;

private:
    friend class FeatureTreeWidget;
};

} // namespace wy3dApp

#endif // WY3DAPP_GUI_CMDS_H
