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

#include "ModelingMoveGuiCmd.h"

#include <QCoreApplication>
#include <QCursor>

#include <wyVector2.h>
#include <wyVector3.h>
#include <wy3dSolid.h>
#include <wy3dSheet.h>
#include <wy3dPrimitive.h>
#include <wy3dMove.h>

#include "application/Application.h"
#include "scene/Scene.h"
#include "scene/nodes/ElementNodeType.h"
#include "commands/sketch/dialogs/GuiCmdHoverInputPopup.h"
#include "commands/transient/BasicTransient.h"
#include "commands/edit/MoveRotateGuiCmdUtil.h"
#include "select/filters/CommonSelFilters.h"
#include "snap/SketchSnapSystem.h"
#include "snap/SnapObject.h"
#include "utils/GuiCommandUtil.h"
#include "widgets/frame/MainWindow.h"

static bool parseDoubleText(const QString& text, double& value)
{
    bool ok(false);
    value = text.trimmed().toDouble(&ok);
    return ok;
}


class MoveGuiCmdPreFilter_Modeling : public SelectPreFilterFunctor
{
public:
    virtual SelectFilterStatus operator()(
        const wydb::Database* pDb,
        const wydb::ElementId& id,
        SelectAction selectAction) const override
    {
        assert(pDb);
        if (id.isNull())
        {
            return SelectFilterStatus::Break;
        }

        if (!GuiCommandUtil::isTopLevelBody(pDb->getElement(id)))
        {
            return SelectFilterStatus::Continue;
        }

        return SelectFilterStatus::Ok;
    }
};


ModelingMoveGuiCmd::ModelingMoveGuiCmd() : MoveGuiCmd()
{
}

ModelingMoveGuiCmd::~ModelingMoveGuiCmd()
{
}

void ModelingMoveGuiCmd::onStart_EnvSpecific()
{
    wyap::SelectionSet filterSS = getValidSSFromCurrentSelSet_MoveRotateGuiCmd();
    Application::instance().getSelManager()->beginChange();
    Application::instance().getSelManager()->setSelections(filterSS);
    Application::instance().getSelManager()->endChange();
}

void ModelingMoveGuiCmd::cleanupEnvSpecific()
{
    // 隐藏工作平面坐标系
    if (Scene* pScene = Application::instance().getActiveScene())
    {
        pScene->hideSketchCSYS();
    }
    else
    {
        assert(false);
    }

    // 移除工作平面原点捕捉对象
    if (_pWorkPlnOriginSnapObject)
    {
        wyap::SnapSystem* pSnapSys = Application::instance().getSnapSystem();
        pSnapSys->beginChange();
        {
            pSnapSys->removeResidentSnapObject(_pWorkPlnOriginSnapObject);
        }
        pSnapSys->endChange();
    }
}

void ModelingMoveGuiCmd::gotoNextStepAfterSelectElements()
{
    this->gotoStep(Step::SelectWorkingPlane);
}

void ModelingMoveGuiCmd::configureSelectElementOptions(GuiCmdSelectOptions& options)
{
    options.pickMask = static_cast<unsigned int>(ElementNodeType::Solid | ElementNodeType::Sheet);
    options.preFilter = std::make_shared<MoveGuiCmdPreFilter_Modeling>();
    options.filter = std::make_shared<MultiClassSelFilter>(
        std::vector<wyrx::ClassInfo*>{wy3d::Solid::classInfo(), wy3d::Sheet::classInfo()});
}

void ModelingMoveGuiCmd::onMouseMove_SpecifyStartPnt(double x, double y)
{
    auto ret = this->computePosition3d(x, y, _workPln, {});
    wy::Vector3 pnt3d;
    if (ret.second)
    {
        pnt3d = ret.second->getPosition();
    }
    else
    {
        pnt3d = this->snapInWorkPlane(x, y, ret.first);
    }
    _hoverPopupState.point = pnt3d;
}

void ModelingMoveGuiCmd::onMouseMove_SpecifyEndPnt(double x, double y)
{
    auto ret = this->computePosition3d(x, y, _workPln, {});
    wy::Vector3 endPnt;
    if (ret.second)
    {
        endPnt = ret.second->getPosition();
    }
    else
    {
        endPnt = this->snapInWorkPlane(x, y, ret.first);
    }
    wy::Vector3 moveVec = endPnt - _startPnt;
    _hoverPopupState.vector = moveVec;
    {
        if (_pLineTransient) _pLineTransient->update(_startPnt, endPnt);
        if (_pMoveElements) _pMoveElements->update(moveVec);
    }
}

void ModelingMoveGuiCmd::onLeftMouseDown_SpecifyStartPnt(double x, double y)
{
    auto ret = this->computePosition3d(x, y, _workPln, {});
    if (ret.second) // 捕捉对象
    {
        _startPnt = ret.second->getPosition();
    }
    else
    {
        _startPnt = this->snapInWorkPlane(x, y, ret.first);
    }
}

void ModelingMoveGuiCmd::onLeftMouseDown_SpecifyEndPnt(double x, double y)
{
    auto ret = this->computePosition3d(x, y, _workPln, {});
    if (ret.second) // 捕捉对象
    {
        _moveVec = ret.second->getPosition() - _startPnt;
    }
    else
    {
        _moveVec = this->snapInWorkPlane(x, y, ret.first) - _startPnt;
    }
}

void ModelingMoveGuiCmd::syncStartPntData()
{
    _startPnt2d = _workPln.uv(_startPnt);
}

void ModelingMoveGuiCmd::syncEndPntData()
{
    _moveVec2d = _workPln.uv(_moveVec) - _workPln.uv(wy::Vector3::kZero);
}

void ModelingMoveGuiCmd::showCoordinatePopup()
{
    if (!_pXYZPopup) return;
    if (_step == Step::SpecifyStartPnt)
    {
        _pXYZPopup->setValues(
            _hoverPopupState.point.x(),
            _hoverPopupState.point.y(),
            _hoverPopupState.point.z());
    }
    else
    {
        _pXYZPopup->setValues(
            _hoverPopupState.vector.x(),
            _hoverPopupState.vector.y(),
            _hoverPopupState.vector.z());
    }
    _pXYZPopup->showAtGlobal(QCursor::pos());
}

void ModelingMoveGuiCmd::handlePopupEnterKey()
{
    if (!_pXYZPopup) return;
    double x(0.0), y(0.0), z(0.0);
    if (!parseDoubleText(_pXYZPopup->getRow1Text(), x) ||
        !parseDoubleText(_pXYZPopup->getRow2Text(), y) ||
        !parseDoubleText(_pXYZPopup->getRow3Text(), z))
    {
        return;
    }

    if (_step == Step::SpecifyStartPnt)
    {
        _startPnt.set(x, y, z);
    }
    else
    {
        _moveVec.set(x, y, z);
    }
}

bool ModelingMoveGuiCmd::executeMove()
{
    return _pMoveElements->perform(_sels, _moveVec, GuiCmdEnvType::Modeling);
}

wy::Vector3 ModelingMoveGuiCmd::snapInWorkPlane(double x, double y, const wy::Vector3& pos3d) const
{
    if (_pWorkPlnSnapSystem)
    {
        SketchSnapResultSPtr pSketchSnapRet = _pWorkPlnSnapSystem->snap(x, y, _pOsgView.get(), _workPln.uv(pos3d), _pSnapContext);
        if (pSketchSnapRet)
        {
            return _workPln.value(pSketchSnapRet->getPosition());
        }
        else
        {
            return pos3d;
        }
    }
    else
    {
        return pos3d;
    }
}
