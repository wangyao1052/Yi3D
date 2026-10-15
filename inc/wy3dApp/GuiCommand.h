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
#include "snap/SketchSnapContext.h"
#include "scene/nodes/ElementNodeType.h"

#include "select/SelectMode.h"
#include <wy3dApp/PointPickOption.h>
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
class SketchSnapSystem;
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

    // Converts desktop screen coordinates (top-left origin, QCursor::pos) to window
    // coordinates (bottom-left origin, device pixels), the same system as MouseEvent.
    virtual void screenToWindow(
        double screenX, double screenY,
        double& windowX, double& windowY) const = 0;

protected:
    // Command selection options.
    struct SelectOptions
    {
        // Whether point selection is enabled.
        bool pointSelect;
        // Whether box selection is enabled.
        bool boxSelect;
        // Allowed element types (ElementNodeType bit set).
        unsigned int allowedElemTypes;
        // Selection type.
        wy3d::SelectionType selectionType;
        // Pre-filter functor.
        SelectPreFilterFunctorSPtr preFilter;
        // Post-filter functor.
        SelectPostFilterFunctorSPtr postFilter;
        // Whether hover preview is enabled (point selection only).
        bool preview;
        // Selection mode: full or incremental.
        SelectMode selectMode;

        SelectOptions() : pointSelect(false), boxSelect(false), allowedElemTypes(ElementNodeType::All),
            selectionType(wy3d::SelectionType::Element), preview(true), selectMode(SelectMode::Full)
        {}
    };
    void configSelect(SelectOptions options);

    virtual wyap::Selection pointPick(
        double x, double y,
        const PointPickOption& option) = 0;

    virtual std::pair<wy::Vector3, wyap::SnapResultSPtr> computePosition3d(
        double x, double y,
        const wy3d::Plane& sketchPlane,
        const std::set<wydb::ElementId>& excludeIds,
        bool snap = true) = 0;

    virtual wy::Vector2 computePosition2d(
        double x, double y,
        const wy3d::Plane& sketchPlane,
        const std::set<wydb::ElementId>& excludeIds,
        std::shared_ptr<SketchSnapContext> pSnapContext = nullptr,
        SketchSnapSystem* pSketchSnapSys = nullptr,
        bool snap3d = true) = 0;

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

protected:
    // 命令选项
    SelectOptions _options;
    // 捕捉排除项
    std::set<wydb::ElementId> _snapExcludeIds;

private:
    friend class FeatureTreeWidget;
};

} // namespace wy3dApp

#endif // WY3DAPP_GUI_CMDS_H
