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

#include "SketchDrawEllipseArcGuiCmd.h"

#include <QCoreApplication>
#include <QCursor>
#include <QString>
#include <cmath>

#include <wyVector2.h>
#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapSelManager.h>
#include <wy3dImpl.h>
#include <wy3dSketch.h>
#include <wy3dSketchEllipseArc.h>

#include "application/Application.h"
#include "commands/sketch/dialogs/GuiCmdHoverInputPopup.h"
#include "utils/MathUtils.h"
#include "commands/transient/BasicTransient.h"
#include "commands/transient/SketchBasicTransient.h"
#include "snap/SketchSnapSystem.h"
#include "widgets/frame/MainWindow.h"


static bool parseDoubleText(const QString& text, double& value)
{
    bool ok(false);
    value = text.trimmed().toDouble(&ok);
    return ok;
}

SketchDrawEllipseArcGuiCmd::SketchDrawEllipseArcGuiCmd()
    : OsgGuiCommand(),
    _step(Step::Undefined), _centerPnt(), _majorAxis(), _otherRadius(0.0), _startAngle(0.0), _sweepAngle(0.0),
    _pXYPopup(nullptr), _pLengthAnglePopup(nullptr), _pRadiusPopup(nullptr), _pAnglePopup(nullptr), _pSweepAnglePopup(nullptr), _hoverPopupState()
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

SketchDrawEllipseArcGuiCmd::~SketchDrawEllipseArcGuiCmd()
{
}

wyap::CmdExecution::StartResult SketchDrawEllipseArcGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = GuiCommand::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);
 
    _sketchInfo = GuiCommandUtil::initSketchInfo();
    if (_sketchInfo.pSketchSnapSys) _sketchInfo.pSketchSnapSys->clearSnapResult();

    // 清空选择集
    Application::instance().getSelManager()->beginChange();
    Application::instance().getSelManager()->clearSelections();
    Application::instance().getSelManager()->endChange();

    // 初始化
    _pLineTransient = std::make_shared<LineTransient>();
    _pLineTransient->hide();
    _pEllipseTransient = std::make_shared<SketchEllipseTransient>(_sketchInfo.sketchPlane);
    _pEllipseTransient->hide();
    this->gotoStep(Step::SpecifyCenterPnt);

    // 鼠标样式
    Application::instance().setCursor(CursorType::Locate);

    return wyap::CmdExecution::StartResult::Succeeded;
}
void SketchDrawEllipseArcGuiCmd::onEnd()
{
    GuiCommand::onEnd();

    // 放弃当前绘制的SketchEllipseArc
    if (_pMakeSketchEllipseArc)
    {
        _pMakeSketchEllipseArc = nullptr;
    }

}
void SketchDrawEllipseArcGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    GuiCommand::onAbort(cause);

    // 放弃当前绘制的SketchEllipseArc
    if (_pMakeSketchEllipseArc)
    {
        _pMakeSketchEllipseArc = nullptr;
    }

}

void SketchDrawEllipseArcGuiCmd::reset()
{
    this->cleanup();
    this->gotoStep(Step::SpecifyCenterPnt);
}

void SketchDrawEllipseArcGuiCmd::cleanup()
{
    this->hidePopup();

    _step = Step::Undefined;
    _centerPnt.set(0.0, 0.0);
    _majorAxis.set(0.0, 0.0);
    _otherRadius = 0.0;
    _startAngle = 0.0;
    _sweepAngle = 0.0;
    _pSnapContext = nullptr;
    _hoverPopupState.resetValue();
    _pMakeSketchEllipseArc = nullptr;
    if (_pLineTransient) _pLineTransient->hide();
    if (_pEllipseTransient) _pEllipseTransient->hide();
}

void SketchDrawEllipseArcGuiCmd::onEscapeKey()
{
    this->hidePopup();

    if (_step == Step::SpecifyCenterPnt || _step == Step::Undefined)
    {
        this->requestAbort(AbortCause::UserCancel);
    }
    else
    {
        this->reset();
        this->simulateMouseMoveFromPopup();
    }
}

bool SketchDrawEllipseArcGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SpecifyCenterPnt:
    {
        if (_pLineTransient)
        {
            wy::Vector3 pnt = _sketchInfo.sketchPlane.value(_centerPnt);
            _pLineTransient->update(pnt, pnt);
            _pLineTransient->show();
        }

        // next step
        this->gotoStep(Step::SpecifyAxisEndPoint);
        return true;
    }
    break;

    case Step::SpecifyAxisEndPoint:
    {
        if (_majorAxis.length() < wy3d::kMinValue || _majorAxis.length() > wy3d::kMaxValue)
        {
            return false;
        }

        if (_pLineTransient)
        {
            wy::Vector3 centerPnt = _sketchInfo.sketchPlane.value(_centerPnt);
            wy::Vector3 endPnt = _sketchInfo.sketchPlane.value(_centerPnt + _majorAxis);
            _pLineTransient->update(centerPnt, endPnt);
        }

        if (_pEllipseTransient)
        {
            _pEllipseTransient->update(_centerPnt, _majorAxis, 1.0);
            _pEllipseTransient->show();
        }

        // next step
        this->gotoStep(Step::SpecifyOtherRadius);
        return true;
    }
    break;

    case Step::SpecifyOtherRadius:
    {
        if (_otherRadius < wy3d::kMinValue || _otherRadius > wy3d::kMaxValue)
        {
            return false;
        }
        if (_otherRadius / _majorAxis.length() < 1e-10 || _otherRadius / _majorAxis.length() > 1e10)
        {
            return false;
        }

        if (_pEllipseTransient)
        {
            _pEllipseTransient->update(_centerPnt, _majorAxis, _otherRadius / _majorAxis.length());
            _pEllipseTransient->show();
        }

        // 如果短轴半径大于长轴半径则要互换长短轴
        if (_otherRadius > _majorAxis.length())
        {
            wy::Vector2 newMajorAxis = MathUtils::rotateAround(_majorAxis, wy::Vector2::kZero, wy3d::PI_2);
            newMajorAxis = (_otherRadius / _majorAxis.length()) * newMajorAxis;
            _otherRadius = _majorAxis.length();
            _majorAxis = newMajorAxis;
        }

        // next step
        this->gotoStep(Step::SpecifyStartAngle);
        return true;
    }
    break;

    case Step::SpecifyStartAngle:
    {
        wydb::Database* pDb = Application::instance().getActiveDatabase();
        if (!pDb)
        {
            this->reset();
            return false;
        }

        _pMakeSketchEllipseArc = std::make_shared<MakeSketchEllipseArc>(this);
        if (!_pMakeSketchEllipseArc->init(_centerPnt, _majorAxis, _otherRadius / _majorAxis.length(), _startAngle, _startAngle, _sketchInfo.sketchId))
        {
            _pMakeSketchEllipseArc = nullptr;
            return false;
        }

        if (_pEllipseTransient) _pEllipseTransient->hide();

        // next step
        this->gotoStep(Step::SpecifyEndAngle);
        return true;
    }
    break;

    case Step::SpecifyEndAngle:
    {
        if (_pMakeSketchEllipseArc)
        {
            if (!_pMakeSketchEllipseArc->update(_sweepAngle))
            {
                return false;
            }
            _pMakeSketchEllipseArc->commit();
            _pMakeSketchEllipseArc = nullptr;
        }

        if (_pEllipseTransient) _pEllipseTransient->hide();
        if (_pLineTransient) _pLineTransient->hide();

        // next step
        this->gotoStep(Step::SpecifyCenterPnt);
        return true;
    }
    break;

    default:
    {
        assert(false);
    }
    break;
    }

    return false;
}

void SketchDrawEllipseArcGuiCmd::gotoStep(Step step)
{
    _step = step;
    this->hidePopup();
    _hoverPopupState.resetValue();

    switch (step)
    {
    case Step::SpecifyCenterPnt:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawEllipseArc",
            "Specify the center point; you can directly input the coordinate values."));

        if (_sketchInfo.pSketchSnapSys)
        {
            _sketchInfo.pSketchSnapSys->partiallyUpdate(Application::instance().getActiveDatabase());
        }
        _pSnapContext = std::make_shared<SketchLocateContext>(wydb::ElementId::kNull);
    }
    break;

    case Step::SpecifyAxisEndPoint:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawEllipseArc",
            "Specify the axis vector; you can directly input the values."));

        _pSnapContext = std::make_shared<SketchDrawLineContext>(wydb::ElementId::kNull, _centerPnt);
    }
    break;

    case Step::SpecifyOtherRadius:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawEllipseArc",
            "Specify the other radius; you can directly input the value."));

        _pSnapContext = std::make_shared<SketchLocateContext>(wydb::ElementId::kNull);
    }
    break;

    case Step::SpecifyStartAngle:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawEllipseArc",
            "Specify the start angle; you can directly input the value."));
    }
    break;

    case Step::SpecifyEndAngle:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawEllipseArc",
            "Specify the sweep angle; you can directly input the value."));
    }
    break;

    default:
    {
        Application::instance().getStatusBar()->setTips("");
        assert(false);
    }
    break;
    }
}

void SketchDrawEllipseArcGuiCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

void SketchDrawEllipseArcGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (event.x != _hoverPopupState.lastMouseX ||
        event.y != _hoverPopupState.lastMouseY)
    {
        this->hidePopup();
        _hoverPopupState.lastMouseX = event.x;
        _hoverPopupState.lastMouseY = event.y;
        _hoverPopupState.lastMouseMoveTime = event.time;
    }

    switch (_step)
    {
    case Step::SpecifyCenterPnt:
    {
        wy::Vector2 centerPnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, this->getSnapExcludeIds(), _pSnapContext, _sketchInfo.pSketchSnapSys);
        _hoverPopupState.point = centerPnt;
    }
    break;

    case Step::SpecifyAxisEndPoint:
    {
        wy::Vector2 axisEndPnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, this->getSnapExcludeIds(), _pSnapContext, _sketchInfo.pSketchSnapSys);
        wy::Vector2 majorAxis = axisEndPnt - _centerPnt;
        _hoverPopupState.point = axisEndPnt;
        _hoverPopupState.majorAxisLength = majorAxis.length();
        _hoverPopupState.majorAxisAngleDeg = wy3d::radiansToDegrees(
            wy::Vector2::rotationAngle(wy::Vector2::kXAxis, majorAxis));
        {
            if (_pLineTransient)
                _pLineTransient->update(_sketchInfo.sketchPlane.value(_centerPnt), _sketchInfo.sketchPlane.value(axisEndPnt));
        }
    }
    break;

    case Step::SpecifyOtherRadius:
    {
        wy::Vector2 axisEndPnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, this->getSnapExcludeIds(), _pSnapContext, _sketchInfo.pSketchSnapSys);
        double otherRadius = (axisEndPnt - _centerPnt).length();
        _hoverPopupState.point = axisEndPnt;
        _hoverPopupState.otherRadius = otherRadius;
        {
            if (_pLineTransient) _pLineTransient->update(_sketchInfo.sketchPlane.value(_centerPnt), _sketchInfo.sketchPlane.value(axisEndPnt));
            if (_pEllipseTransient)
                _pEllipseTransient->update(_centerPnt, _majorAxis, otherRadius / _majorAxis.length());
        }
    }
    break;

    case Step::SpecifyStartAngle:
    {
        wy::Vector2 pnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, this->getSnapExcludeIds(), _pSnapContext, _sketchInfo.pSketchSnapSys);
        wy::Vector2 vec = pnt - _centerPnt;
        double startAngle = wy::Vector2::rotationAngle(_majorAxis, vec);
        if (std::isnan(startAngle)) return;
        _hoverPopupState.point = pnt;
        _hoverPopupState.angleDeg = wy3d::radiansToDegrees(startAngle);
        {
            if (_pLineTransient) _pLineTransient->update(_sketchInfo.sketchPlane.value(_centerPnt), _sketchInfo.sketchPlane.value(pnt));
        }
    }
    break;

    case Step::SpecifyEndAngle:
    {
        wy::Vector2 pnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, this->getSnapExcludeIds(), _pSnapContext, _sketchInfo.pSketchSnapSys);
        wy::Vector2 vec = pnt - _centerPnt;
        double endAngle = wy::Vector2::rotationAngle(_majorAxis, vec);
        if (std::isnan(endAngle)) return;
        double sweepAngle = endAngle - _startAngle;
        double twoPI = wy3d::PI * 2.0;
        while (sweepAngle < 0.0)
        {
            sweepAngle += twoPI;
        }
        while (sweepAngle >= twoPI)
        {
            sweepAngle -= twoPI;
        }
        _hoverPopupState.point = pnt;
        _hoverPopupState.angleDeg = wy3d::radiansToDegrees(sweepAngle);
        {
            if (_pLineTransient) _pLineTransient->update(_sketchInfo.sketchPlane.value(_centerPnt), _sketchInfo.sketchPlane.value(pnt));
            if (_pMakeSketchEllipseArc) _pMakeSketchEllipseArc->update(sweepAngle);
        }
    }
    break;

    default:
    {
    }
    break;
    }

    return;
}

void SketchDrawEllipseArcGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    this->hidePopup();
    _hoverPopupState.lastMouseX = event.x;
    _hoverPopupState.lastMouseY = event.y;
    _hoverPopupState.lastMouseMoveTime = event.time;

    switch (_step)
    {
    case Step::SpecifyCenterPnt:
    {
        _centerPnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, this->getSnapExcludeIds(), _pSnapContext, _sketchInfo.pSketchSnapSys);
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
    }
    break;

    case Step::SpecifyAxisEndPoint:
    {
        wy::Vector2 axisEndPoint = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, this->getSnapExcludeIds(), _pSnapContext, _sketchInfo.pSketchSnapSys);
        _majorAxis = axisEndPoint - _centerPnt;
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
    }
    break;

    case Step::SpecifyOtherRadius:
    {
        wy::Vector2 axisEndPnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, this->getSnapExcludeIds(), _pSnapContext, _sketchInfo.pSketchSnapSys);
        _otherRadius = (axisEndPnt - _centerPnt).length();
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
    }
    break;

    case Step::SpecifyStartAngle:
    {
        wy::Vector2 pnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, this->getSnapExcludeIds(), _pSnapContext, _sketchInfo.pSketchSnapSys);
        _startAngle = wy::Vector2::rotationAngle(_majorAxis, pnt - _centerPnt);
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
    }
    break;

    case Step::SpecifyEndAngle:
    {
        wy::Vector2 pnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, this->getSnapExcludeIds(), _pSnapContext, _sketchInfo.pSketchSnapSys);
        _sweepAngle = wy::Vector2::rotationAngle(_majorAxis, pnt - _centerPnt) - _startAngle;
        double twoPI = wy3d::PI * 2.0;
        while (_sweepAngle < 0.0)
        {
            _sweepAngle += twoPI;
        }
        while (_sweepAngle >= twoPI)
        {
            _sweepAngle -= twoPI;
        }
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
    }
    break;

    default:
    {
        assert(false);
    }
    break;
    }

    return;
}

void SketchDrawEllipseArcGuiCmd::initializePopups()
{
    MainWindow* pMainWindow = Application::instance().getMainWindow();
    if (!_pXYPopup)
    {
        _pXYPopup = std::make_unique<GuiCmdHoverInputPopup2>(
            QStringLiteral("X"),
            QStringLiteral("Y"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pXYPopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pXYPopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pXYPopup->hide();
    }
    if (!_pLengthAnglePopup)
    {
        _pLengthAnglePopup = std::make_unique<GuiCmdHoverInputPopup2>(
            QCoreApplication::translate("SketchDrawEllipseArc", "Length"),
            QCoreApplication::translate("SketchDrawEllipseArc", "Angle"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pLengthAnglePopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pLengthAnglePopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pLengthAnglePopup->hide();
    }
    if (!_pRadiusPopup)
    {
        _pRadiusPopup = std::make_unique<GuiCmdHoverInputPopup1>(
            QCoreApplication::translate("SketchDrawEllipseArc", "Radius"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pRadiusPopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pRadiusPopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pRadiusPopup->hide();
    }
    if (!_pAnglePopup)
    {
        _pAnglePopup = std::make_unique<GuiCmdHoverInputPopup1>(
            QCoreApplication::translate("SketchDrawEllipseArc", "StartAngle"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pAnglePopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pAnglePopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pAnglePopup->hide();
    }
    if (!_pSweepAnglePopup)
    {
        _pSweepAnglePopup = std::make_unique<GuiCmdHoverInputPopup1>(
            QCoreApplication::translate("SketchDrawEllipseArc", "SweepAngle"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pSweepAnglePopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pSweepAnglePopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pSweepAnglePopup->hide();
    }
}

void SketchDrawEllipseArcGuiCmd::showPopup()
{
    if (!_pXYPopup || !_pLengthAnglePopup || !_pRadiusPopup || !_pAnglePopup || !_pSweepAnglePopup)
    {
        this->initializePopups();
    }

    if (_step == Step::SpecifyCenterPnt)
    {
        _pXYPopup->setValues(
            _hoverPopupState.point.x(),
            _hoverPopupState.point.y());
        _pXYPopup->showAtGlobal(QCursor::pos());
    }
    else if (_step == Step::SpecifyAxisEndPoint)
    {
        _pLengthAnglePopup->setValues(
            _hoverPopupState.majorAxisLength,
            _hoverPopupState.majorAxisAngleDeg);
        _pLengthAnglePopup->showAtGlobal(QCursor::pos());
    }
    else if (_step == Step::SpecifyOtherRadius)
    {
        _pRadiusPopup->setValue(_hoverPopupState.otherRadius);
        _pRadiusPopup->showAtGlobal(QCursor::pos());
    }
    else if (_step == Step::SpecifyStartAngle)
    {
        _pAnglePopup->setValue(_hoverPopupState.angleDeg);
        _pAnglePopup->showAtGlobal(QCursor::pos());
    }
    else if (_step == Step::SpecifyEndAngle)
    {
        _pSweepAnglePopup->setValue(_hoverPopupState.angleDeg);
        _pSweepAnglePopup->showAtGlobal(QCursor::pos());
    }
}

void SketchDrawEllipseArcGuiCmd::hidePopup()
{
    if (_pXYPopup && _pXYPopup->isVisible())
    {
        _pXYPopup->hide();
    }
    if (_pLengthAnglePopup && _pLengthAnglePopup->isVisible())
    {
        _pLengthAnglePopup->hide();
    }
    if (_pRadiusPopup && _pRadiusPopup->isVisible())
    {
        _pRadiusPopup->hide();
    }
    if (_pAnglePopup && _pAnglePopup->isVisible())
    {
        _pAnglePopup->hide();
    }
    if (_pSweepAnglePopup && _pSweepAnglePopup->isVisible())
    {
        _pSweepAnglePopup->hide();
    }
}

void SketchDrawEllipseArcGuiCmd::tryShowPopupOnHover(double time)
{
    if (_step != Step::SpecifyCenterPnt &&
        _step != Step::SpecifyAxisEndPoint &&
        _step != Step::SpecifyOtherRadius &&
        _step != Step::SpecifyStartAngle &&
        _step != Step::SpecifyEndAngle)
    {
        return;
    }
    if (_hoverPopupState.lastMouseMoveTime < 0.0)
    {
        return;
    }
    if ((_pXYPopup && _pXYPopup->isVisible()) ||
        (_pLengthAnglePopup && _pLengthAnglePopup->isVisible()) ||
        (_pRadiusPopup && _pRadiusPopup->isVisible()) ||
        (_pAnglePopup && _pAnglePopup->isVisible()) ||
        (_pSweepAnglePopup && _pSweepAnglePopup->isVisible()))
    {
        return;
    }
    if (time - _hoverPopupState.lastMouseMoveTime >= 0.45)
    {
        this->showPopup();
    }
}

void SketchDrawEllipseArcGuiCmd::onPopupEnterKey()
{
    if (_step == Step::SpecifyCenterPnt)
    {
        if (!_pXYPopup) return;
        double x(0.0), y(0.0);
        if (!parseDoubleText(_pXYPopup->getRow1Text(), x) ||
            !parseDoubleText(_pXYPopup->getRow2Text(), y))
        {
            return;
        }
        _centerPnt.set(x, y);
    }
    else if (_step == Step::SpecifyAxisEndPoint)
    {
        if (!_pLengthAnglePopup) return;
        double length(0.0), angleDeg(0.0);
        if (!parseDoubleText(_pLengthAnglePopup->getRow1Text(), length) ||
            !parseDoubleText(_pLengthAnglePopup->getRow2Text(), angleDeg))
        {
            return;
        }
        double angle = wy3d::degreesToRadians(angleDeg);
        _majorAxis = length * wy::Vector2(std::cos(angle), std::sin(angle));
    }
    else if (_step == Step::SpecifyOtherRadius)
    {
        if (!_pRadiusPopup) return;
        double radius(0.0);
        if (!parseDoubleText(_pRadiusPopup->getRowText(), radius))
        {
            return;
        }
        _otherRadius = radius;
    }
    else if (_step == Step::SpecifyStartAngle)
    {
        if (!_pAnglePopup) return;
        double angle(0.0);
        if (!parseDoubleText(_pAnglePopup->getRowText(), angle))
        {
            return;
        }
        _startAngle = wy3d::degreesToRadians(angle);
    }
    else if (_step == Step::SpecifyEndAngle)
    {
        if (!_pSweepAnglePopup) return;
        double angle(0.0);
        if (!parseDoubleText(_pSweepAnglePopup->getRowText(), angle))
        {
            return;
        }
        _sweepAngle = wy3d::degreesToRadians(angle);
    }
    else
    {
        return;
    }

    if (this->finishStep(_step))
    {
        this->simulateMouseMoveFromPopup();
    }
}

void SketchDrawEllipseArcGuiCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void SketchDrawEllipseArcGuiCmd::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX)
        return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}

std::set<wydb::ElementId> SketchDrawEllipseArcGuiCmd::getSnapExcludeIds() const
{
    std::set<wydb::ElementId> snapExcludeIds;
    snapExcludeIds.insert(_sketchInfo.sketchId);
    if (_pMakeSketchEllipseArc) _pMakeSketchEllipseArc->collectElements(snapExcludeIds);
    return snapExcludeIds;
}

void MakeSketchEllipseArc::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pSketchEllipseArc) idSet.insert(_pSketchEllipseArc->getId());
}

bool MakeSketchEllipseArc::init(
    const wy::Vector2& centerPnt,
    const wy::Vector2& majorAxis,
    double radiusRatio,
    double startAngle, double endAngle,
    wydb::ElementId sketchId)
{
    if (!_pDb || !_pTopTrans || _pSketchEllipseArc || _isFinished)
    {
        return false;
    }

    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    wy3d::Sketch* pSketch = nullptr;
    wy3d::SketchEllipseArc* pSketchEllipseArc = nullptr;
    wydb::Element* pSketchElem = pTrans->getElementForWrite(sketchId);
    if (!pSketchElem) goto ABORT_TRANS;
    pSketch = wy3d::Sketch::cast(pSketchElem);
    if (!pSketch) goto ABORT_TRANS;
    if (wy::ErrorStatus::Ok != wy3d::SketchEllipseArc::create(pTrans, centerPnt, majorAxis,
        radiusRatio, startAngle, endAngle, pSketchEllipseArc) || !pSketchEllipseArc)
    {
        goto ABORT_TRANS;
    }
    _pSketchEllipseArc = pSketchEllipseArc;
    if (wy::ErrorStatus::Ok != pSketch->addEntity(pSketchEllipseArc))
    {
        goto ABORT_TRANS;
    }
    _pDb->getTransactionManager()->endTransaction();
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    _pSketchEllipseArc = nullptr;
    return false;
}

bool MakeSketchEllipseArc::update(double sweepAngle)
{
    if (!_pDb || !_pTopTrans || !_pSketchEllipseArc || _isFinished)
    {
        return false;
    }

    double twoPI = wy3d::PI * 2.0;
    while (sweepAngle >= twoPI)
    {
        sweepAngle -= twoPI;
    }
    if (sweepAngle < wy3d::kMinValue)
    {
        return false;
    }

    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    {
        wy::ErrorStatus error = _pSketchEllipseArc->upgradeForWrite();
        if (wy::ErrorStatus::Ok != error)
        {
            _pDb->getTransactionManager()->abortTransaction();
            return false;
        }

        error = _pSketchEllipseArc->setEndAngle(_pSketchEllipseArc->getStartAngle() + sweepAngle);
        if (wy::ErrorStatus::Ok != error)
        {
            _pDb->getTransactionManager()->abortTransaction();
            return false;
        }
    }
    if (wy::ErrorStatus::Ok == _pDb->getTransactionManager()->endTransaction())
    {
        wydb::TransactionManager* pTransMgr = _pDb->getTransactionManager();
        pTransMgr->mergeTransaction();
    }
    return true;
}
