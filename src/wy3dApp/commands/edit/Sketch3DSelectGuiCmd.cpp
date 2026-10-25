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

#include "Sketch3DSelectGuiCmd.h"

#include <cmath>

#include <QCoreApplication>

#include <wy3dDatumPlane.h>
#include <wy3dSelectionType.h>
#include <wy3dSheet.h>
#include <wy3dSketch3D.h>
#include <wy3dSketchEntity3D.h>
#include <wy3dSolid.h>

#include <wy3dApp/Application.h>
#include <wyapGizmoManager.h>
#include "application/ApplicationImpl.h"
#include "commands/GuiEventDispatcher.h"
#include "environments/sketch3d/Sketch3DEnvironmentImpl.h"
#include "scene/nodes/ElementNodeType.h"
#include "select/SelectHandler.h"
#include <wy3dApp/CommonSelFilters.h>
#include <wy3dApp/GuiCommandUtil.h>
#include "view/BaseView.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

namespace
{

// Faces and datum planes in one pick. A datum plane has no sub entities, so it is only ever
// offered as a whole element and the pick has to accept whole elements; that same coarse pass
// turns a click on a solid into a whole body selection and would swallow every face pick, so
// body selections are turned down here and the ray falls through to the face pass.
// Curved faces are deliberately let through: the command turns them down with a tip, which a
// filter cannot do.
class WorkPlaneSourceSelFilter : public SelectPostFilterFunctor
{
public:
    virtual SelectFilterStatus operator()(
        const wydb::Database* pDb, const wyap::Selection& sel, SelectAction) const override
    {
        assert(pDb);
        if (!pDb) return SelectFilterStatus::Continue;

        const wydb::Element* pElement = pDb->getElement(sel.getElementId());
        if (!pElement) return SelectFilterStatus::Continue;

        switch (wy3d::UIntToSelectionType(sel.getSelectionType()))
        {
        case wy3d::SelectionType::Face:
            if (sel.getSubPath().empty()) return SelectFilterStatus::Continue;
            return (wy3d::Solid::cast(pElement) || wy3d::Sheet::cast(pElement))
                ? SelectFilterStatus::Ok : SelectFilterStatus::Continue;

        case wy3d::SelectionType::Element:
            return wy3d::DatumPlane::cast(pElement) ? SelectFilterStatus::Ok : SelectFilterStatus::Continue;

        default:
            return SelectFilterStatus::Continue;
        }
    }
};

} // namespace

Sketch3DSelectGuiCmd::Sketch3DSelectGuiCmd() : SelectGuiCmd(),
    _leftDownX(0.0f), _leftDownY(0.0f), _isOverCurvedFace(false)
{
}

Sketch3DSelectGuiCmd::~Sketch3DSelectGuiCmd()
{
}

wyap::CmdExecution::StartResult Sketch3DSelectGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = __baseClass::onStart();
    if (!GuiCommandUtil::initSketch3DInfo(_sketch3DInfo))
    {
        assert(false);
        return wyap::CmdExecution::StartResult::Failed;
    }

    // Faces and datum planes are picked by this command itself, so they never reach the
    // selection set. The Element bit has to stay on, see the filter above.
    _planePickOption.allowedElemTypes = ElementNodeType::Solid |
        ElementNodeType::Sheet |
        ElementNodeType::DatumPlane;
    _planePickOption.selType = wy3d::SelectionType::Face | wy3d::SelectionType::Element;
    _planePickOption.postFilter = std::make_shared<WorkPlaneSourceSelFilter>();

    _planePreview.clear();
    _pCustomWorkPlaneHighlight = nullptr;
    _leftDownX = 0.0f;
    _leftDownY = 0.0f;
    this->setOverCurvedFace(false);

    return ret;
}

void Sketch3DSelectGuiCmd::initializeSelectOptions(SelectOptions& selectOptions)
{
    SelectGuiCmd::initializeSelectOptions(selectOptions);
    selectOptions.allowedElemTypes = ElementNodeType::Sketch3DEntity;
    selectOptions.postFilter = SingleClassSelPostFilter::create<wy3d::SketchEntity3D>();
}

void Sketch3DSelectGuiCmd::onStart_EnvSpecific()
{
    // 3D草图环境不需要启用特征树可选择
}

void Sketch3DSelectGuiCmd::selectAll_Impl(wyap::SelectionSet& ss)
{
    wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    if (!pDb) return;
    const wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pDb->getElement(_sketch3DInfo.sketch3dId));
    if (pSketch3D)
    {
        for (auto iter = pSketch3D->createIterator(); !iter.isDone(); iter.moveNext())
        {
            ss.add(wyap::Selection(iter.current()));
        }
    }
}

// The verdict comes from the selection channel itself, so "an entity in front wins" can never
// drift away from what a click would select.
bool Sketch3DSelectGuiCmd::isEntityUnderPointer(double x, double y)
{
    SelectHandler* pSelectHandler = this->getSelectHandler();
    return pSelectHandler && !pSelectHandler->querySelectionAt(x, y).getElementId().isNull();
}

SelectHandler* Sketch3DSelectGuiCmd::getSelectHandler() const
{
    BaseView* pView = ApplicationImpl::instance().getActiveView();
    if (!pView) return nullptr;
    GuiEventDispatcher* pGuiEventDispatcher = pView->getGuiEventDispatcher();
    return pGuiEventDispatcher ? pGuiEventDispatcher->getSelectHandler() : nullptr;
}

void Sketch3DSelectGuiCmd::onMouseMove(const MouseEvent& event)
{
    __baseClass::onMouseMove(event);

    wyap::Selection sel = this->pointPick(event.x, event.y, _planePickOption);
    _planePreview.update(sel);

    bool isOverCurvedFace = false;
    if (!_planePreview.isNull())
    {
        wy3d::Plane plane;
        if (!GuiCommandUtil::getWorkingPlane(_planePreview.getSelection(), plane))
        {
            // A curved face cannot be a work plane. The preview object is the highlight itself,
            // so dropping it here means nothing is drawn for it: the frame comes after this.
            _planePreview.clear();
            isOverCurvedFace = true;
        }
        else if (this->isEntityUnderPointer(event.x, event.y))
        {
            // An entity in front is what the click would select.
            _planePreview.clear();
        }
    }

    this->setOverCurvedFace(isOverCurvedFace);
}

void Sketch3DSelectGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    __baseClass::onLeftMouseDown(event);

    _leftDownX = event.x;
    _leftDownY = event.y;
}

void Sketch3DSelectGuiCmd::onLeftMouseUp(const MouseEvent& event)
{
    __baseClass::onLeftMouseUp(event);

    // Same threshold as the selection handler: anything longer was a box select.
    const float kClickThreshold = 5.0f;
    if (std::abs(event.x - _leftDownX) + std::abs(event.y - _leftDownY) >= kClickThreshold)
    {
        return;
    }

    // An entity under the pointer is an ordinary selection, the pending plane is left alone.
    if (this->isEntityUnderPointer(event.x, event.y))
    {
        return;
    }

    if (!this->tryPickWorkPlane(event.x, event.y))
    {
        // Both picks came back empty: the click landed on nothing. Same as an empty selection.
        this->clearPendingWorkPlane();
    }

    _planePreview.clear();
}

// Returns true when the click was taken by a face or a datum plane, usable or not. Only a click
// on empty space is left for the caller to act on.
bool Sketch3DSelectGuiCmd::tryPickWorkPlane(double x, double y)
{
    wyap::Selection sel = this->pointPick(x, y, _planePickOption);
    if (sel.getElementId().isNull())
    {
        return false;
    }

    wy3d::Plane plane;
    if (!GuiCommandUtil::getWorkingPlane(sel, plane))
    {
        ApplicationImpl::instance().getStatusBar()->setTips(QCoreApplication::translate("Sketch3DSelectGuiCmd",
            "Only planar faces can be used as the sketch plane."));
        return true;
    }

    if (Sketch3DEnvironmentImpl* pSketch3DEnv = dynamic_cast<Sketch3DEnvironmentImpl*>(
        GuiCommandUtil::getActiveSketch3DEnvironment()))
    {
        pSketch3DEnv->setPendingWorkPlane(plane, sel);
    }

    if (!_pCustomWorkPlaneHighlight)
    {
        _pCustomWorkPlaneHighlight = std::make_shared<SelectionSetHighlightor>();
    }
    else
    {
        _pCustomWorkPlaneHighlight->clearSelections();
    }
    _pCustomWorkPlaneHighlight->addSelection(sel);

    ApplicationImpl::instance().getStatusBar()->setTips(QCoreApplication::translate("Sketch3DSelectGuiCmd",
        "Sketch plane set. It will be used by the next drawing command."));

    return true;
}

void Sketch3DSelectGuiCmd::clearPendingWorkPlane()
{
    if (Sketch3DEnvironmentImpl* pSketch3DEnv = dynamic_cast<Sketch3DEnvironmentImpl*>(
        GuiCommandUtil::getActiveSketch3DEnvironment()))
    {
        pSketch3DEnv->clearPendingWorkPlane();
    }

    _pCustomWorkPlaneHighlight = nullptr;
    _planePreview.clear();
}

void Sketch3DSelectGuiCmd::setOverCurvedFace(bool value)
{
    if (_isOverCurvedFace == value)
    {
        return;
    }

    _isOverCurvedFace = value;
    ApplicationImpl::instance().setCursor(value ? CursorType::Forbid : CursorType::Select);
}

void Sketch3DSelectGuiCmd::onEscapeKey()
{
    __baseClass::onEscapeKey();

    this->clearPendingWorkPlane();
}

void Sketch3DSelectGuiCmd::onEnd()
{
    // The pending plane itself has to outlive this command: it is consumed by the drawing
    // command that is about to start.
    this->setOverCurvedFace(false);
    _planePreview.clear();
    _pCustomWorkPlaneHighlight = nullptr;

    __baseClass::onEnd();
}

void Sketch3DSelectGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    this->setOverCurvedFace(false);
    _planePreview.clear();
    _pCustomWorkPlaneHighlight = nullptr;

    __baseClass::onAbort(cause);
}

void Sketch3DSelectGuiCmd::updateSelectTipAndLabel()
{
    if (!_pPasteOp)
    {
        ApplicationImpl::instance().getStatusBar()->setTips(QCoreApplication::translate("Sketch3DSelectGuiCmd",
            "Select elements. Pick a planar face or a datum plane to set the sketch plane."));
    }
}

void Sketch3DSelectGuiCmd::onSelectionChanged(
    const wyap::SelectionSet&,
    const wyap::SelectionSet&,
    const wyap::SelectionSet&)
{
    // 3D 草图图元不支持 Gizmo：清空可能残留的 Gizmo，只更新提示。
    if (wyap::GizmoManager* pGizmoMgr = ApplicationImpl::instance().getGizmoManager())
    {
        pGizmoMgr->beginChange();
        pGizmoMgr->clearGizmos();
        pGizmoMgr->endChange();
    }
    this->updateSelectTipAndLabel();
}

void Sketch3DSelectGuiCmd::onKeyDown(const KeyEvent& event)
{
    // 吞掉Ctrl+C/Ctrl+V(3D草图环境不支持复制/粘贴)
    if (event.key == KeyCode::CtrlC || event.key == KeyCode::CtrlV)
    {
        return;
    }
    __baseClass::onKeyDown(event);
}

} // namespace wy3dApp
