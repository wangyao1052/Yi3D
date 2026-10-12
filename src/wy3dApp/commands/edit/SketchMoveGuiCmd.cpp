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

#include "SketchMoveGuiCmd.h"

#include <QCoreApplication>
#include <QCursor>
#include <cmath>

#include <wyVector2.h>
#include <wy3dSketchEntity.h>

#include <wy3dApp/Application.h>
#include "commands/sketch/dialogs/GuiCmdHoverInputPopup.h"
#include "commands/transient/BasicTransient.h"
#include "scene/nodes/ElementNodeType.h"
#include <wy3dApp/CommonSelFilters.h>
#include "snap/SketchSnapSystem.h"
#include "widgets/frame/MainWindow.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

static bool parseDoubleText(const QString& text, double& value)
{
    bool ok(false);
    value = text.trimmed().toDouble(&ok);
    return ok;
}


SketchMoveGuiCmd::SketchMoveGuiCmd() : MoveGuiCmd()
{
}

SketchMoveGuiCmd::~SketchMoveGuiCmd()
{
}

wyap::CmdExecution::StartResult SketchMoveGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = __baseClass::onStart();
    _sketchInfo = GuiCommandUtil::initSketchInfo();
    if (_sketchInfo.pSketchSnapSys) _sketchInfo.pSketchSnapSys->clearSnapResult();
    return ret;
}

void SketchMoveGuiCmd::gotoNextStepAfterSelectElements()
{
    this->gotoStep(Step::SpecifyStartPnt);
}

void SketchMoveGuiCmd::configureSelectElementOptions(SelectOptions& options)
{
    options.pickMask = ElementNodeType::SketchEntity;
    options.preFilter = nullptr;
    options.filter = std::make_shared<SingleClassSelFilter>(wy3d::SketchEntity::classInfo());
}

void SketchMoveGuiCmd::onMouseMove_SpecifyStartPnt(double x, double y)
{
    wy::Vector2 pnt2d = this->computePosition2d(x, y, _sketchInfo.sketchPlane, {}, _pSnapContext, _sketchInfo.pSketchSnapSys);
    _hoverPopupState.point.set(pnt2d.x(), pnt2d.y(), 0.0);
}

void SketchMoveGuiCmd::onMouseMove_SpecifyEndPnt(double x, double y)
{
    wy::Vector2 endPnt2d = this->computePosition2d(x, y, _sketchInfo.sketchPlane, {}, _pSnapContext, _sketchInfo.pSketchSnapSys);
    wy::Vector2 moveVec2d = endPnt2d - _startPnt2d;
    _hoverPopupState.vector.set(moveVec2d.x(), moveVec2d.y(), 0.0);
    const double length = moveVec2d.length();
    const double angleRadian = std::atan2(moveVec2d.y(), moveVec2d.x());
    const double normalizedRadian = wy3d::normalizeRadian(angleRadian);
    const double angleDeg = wy3d::radiansToDegrees(normalizedRadian);
    {
        if (_pLineTransient) _pLineTransient->update(_sketchInfo.sketchPlane, _startPnt2d, endPnt2d);
        if (_pMoveElements) _pMoveElements->update(_sketchInfo.sketchPlane, moveVec2d);
    }
}

void SketchMoveGuiCmd::onLeftMouseDown_SpecifyStartPnt(double x, double y)
{
    _startPnt2d = this->computePosition2d(x, y, _sketchInfo.sketchPlane, {}, _pSnapContext, _sketchInfo.pSketchSnapSys);
}

void SketchMoveGuiCmd::onLeftMouseDown_SpecifyEndPnt(double x, double y)
{
    wy::Vector2 endPnt2d = this->computePosition2d(x, y, _sketchInfo.sketchPlane, {}, _pSnapContext, _sketchInfo.pSketchSnapSys);
    _moveVec2d = endPnt2d - _startPnt2d;
}

void SketchMoveGuiCmd::syncStartPntData()
{
    _startPnt = _sketchInfo.sketchPlane.value(_startPnt2d);
}

void SketchMoveGuiCmd::syncEndPntData()
{
    _moveVec.set(_moveVec2d.x(), _moveVec2d.y(), 0.0);
}

void SketchMoveGuiCmd::showCoordinatePopup()
{
    if (_step == Step::SpecifyStartPnt)
    {
        if (!_pXYPopup) return;
        _pXYPopup->setValues(
            _hoverPopupState.point.x(),
            _hoverPopupState.point.y());
        _pXYPopup->showAtGlobal(QCursor::pos());
    }
    else
    {
        if (!_pLengthAnglePopup) return;
        const wy::Vector2 vec2d(_hoverPopupState.vector.x(), _hoverPopupState.vector.y());
        const double length = vec2d.length();
        const double angleRadian = std::atan2(vec2d.y(), vec2d.x());
        const double normalizedRadian = wy3d::normalizeRadian(angleRadian);
        const double angleDeg = wy3d::radiansToDegrees(normalizedRadian);
        _pLengthAnglePopup->setValues(length, angleDeg);
        _pLengthAnglePopup->showAtGlobal(QCursor::pos());
    }
}

void SketchMoveGuiCmd::handlePopupEnterKey()
{
    if (_step == Step::SpecifyStartPnt)
    {
        if (!_pXYPopup) return;
        double x(0.0), y(0.0);
        if (!parseDoubleText(_pXYPopup->getRow1Text(), x) ||
            !parseDoubleText(_pXYPopup->getRow2Text(), y))
        {
            return;
        }
        _startPnt2d.set(x, y);
    }
    else
    {
        if (!_pLengthAnglePopup) return;
        double length(0.0), angleDeg(0.0);
        if (!parseDoubleText(_pLengthAnglePopup->getRow1Text(), length) ||
            !parseDoubleText(_pLengthAnglePopup->getRow2Text(), angleDeg))
        {
            return;
        }
        const double angleRad = wy3d::degreesToRadians(angleDeg);
        _moveVec2d.set(
            length * std::cos(angleRad),
            length * std::sin(angleRad));
    }
}

bool SketchMoveGuiCmd::executeMove()
{
    return _pMoveElements->perform(_sels, _moveVec);
}

} // namespace wy3dApp
