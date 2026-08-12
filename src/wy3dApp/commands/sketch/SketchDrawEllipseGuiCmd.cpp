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

#include "SketchDrawEllipseGuiCmd.h"

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
#include <wy3dSketchEllipse.h>
#include <wy3dSketchEllipse.h>

#include "application/Application.h"
#include "commands/sketch/dialogs/GuiCmdHoverInputPopup.h"
#include "commands/transient/BasicTransient.h"
#include "snap/SketchSnapSystem.h"
#include "widgets/frame/MainWindow.h"


static bool parseDoubleText(const QString& text, double& value)
{
    bool ok(false);
    value = text.trimmed().toDouble(&ok);
    return ok;
}

SketchDrawEllipseGuiCmd::SketchDrawEllipseGuiCmd()
    : OsgGuiCommand(),
    _step(Step::Undefined), _centerPnt(), _majorAxis(), _otherRadius(0.0),
    _pXYPopup(nullptr), _pLengthAnglePopup(nullptr), _pRadiusPopup(nullptr), _hoverPopupState()
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

SketchDrawEllipseGuiCmd::~SketchDrawEllipseGuiCmd()
{
}

wyap::CmdExecution::StartResult SketchDrawEllipseGuiCmd::onStart()
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
    this->gotoStep(Step::SpecifyCenterPnt);

    // 鼠标样式
    Application::instance().setCursor(CursorType::Locate);

    return wyap::CmdExecution::StartResult::Succeeded;
}

void SketchDrawEllipseGuiCmd::reset()
{
    this->cleanup();
    this->gotoStep(Step::SpecifyCenterPnt);
}

void SketchDrawEllipseGuiCmd::cleanup()
{
    this->hidePopup();

    _step = Step::Undefined;
    _centerPnt.set(0.0, 0.0);
    _majorAxis.set(0.0, 0.0);
    _otherRadius = 0.0;
    _pSnapContext = nullptr;
    _hoverPopupState.resetValue();
    _pMakeSketchEllipse = nullptr;
    if (_pLineTransient) _pLineTransient->hide();

}

void SketchDrawEllipseGuiCmd::onEscapeKey()
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

bool SketchDrawEllipseGuiCmd::finishStep(Step step)
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
        wydb::Database* pDb = Application::instance().getActiveDatabase();
        if (pDb)
        {
            _pMakeSketchEllipse = std::make_shared<MakeSketchEllipse>(this);
            if (!_pMakeSketchEllipse->init(_centerPnt, _majorAxis, 1.0, _sketchInfo.sketchId))
            {
                _pMakeSketchEllipse = nullptr;
                return false;
            }
        }
        else
        {
            this->reset();
            return false;
        }

        // next step
        this->gotoStep(Step::SpecifyOtherRadius);
        return true;
    }
    break;

    case Step::SpecifyOtherRadius:
    {
        if (_pMakeSketchEllipse)
        {
            if (!_pMakeSketchEllipse->update(_otherRadius, true))
            {
                return false;
            }
            _pMakeSketchEllipse->commit();
            _pMakeSketchEllipse = nullptr;
        }

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

void SketchDrawEllipseGuiCmd::gotoStep(Step step)
{
    _step = step;
    this->hidePopup();
    _hoverPopupState.resetValue();

    switch (step)
    {
    case Step::SpecifyCenterPnt:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawEllipse",
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
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawEllipse",
            "Specify the axis vector; you can directly input the values."));

        _pSnapContext = std::make_shared<SketchDrawLineContext>(wydb::ElementId::kNull, _centerPnt);
    }
    break;

    case Step::SpecifyOtherRadius:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawEllipse",
            "Specify the other radius; you can directly input the value."));

        // TODO
        // 捕捉这一块的工作量太大了,暂时不处理绘制椭圆这一块的逻辑,使用空的草图绘制上下文
        _pSnapContext = std::make_shared<SketchLocateContext>(wydb::ElementId::kNull);
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

void SketchDrawEllipseGuiCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

void SketchDrawEllipseGuiCmd::onMouseMove(const MouseEvent& event)
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
            {
                _pLineTransient->update(_sketchInfo.sketchPlane.value(_centerPnt), _sketchInfo.sketchPlane.value(axisEndPnt));
            }
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
            if (_pMakeSketchEllipse) _pMakeSketchEllipse->update(otherRadius);
            if (_pLineTransient)
            {
                _pLineTransient->update(_sketchInfo.sketchPlane.value(_centerPnt), _sketchInfo.sketchPlane.value(axisEndPnt));
            }
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

void SketchDrawEllipseGuiCmd::onLeftMouseDown(const MouseEvent& event)
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

    default:
    {
        assert(false);
    }
    break;
    }

    return;
}

void SketchDrawEllipseGuiCmd::initializePopups()
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
            QCoreApplication::translate("SketchDrawEllipse", "Length"),
            QCoreApplication::translate("SketchDrawEllipse", "Angle"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pLengthAnglePopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pLengthAnglePopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pLengthAnglePopup->hide();
    }
    if (!_pRadiusPopup)
    {
        _pRadiusPopup = std::make_unique<GuiCmdHoverInputPopup1>(
            QCoreApplication::translate("SketchDrawEllipse", "Radius"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pRadiusPopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pRadiusPopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pRadiusPopup->hide();
    }
}

void SketchDrawEllipseGuiCmd::showPopup()
{
    if (!_pXYPopup || !_pLengthAnglePopup || !_pRadiusPopup)
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
}

void SketchDrawEllipseGuiCmd::hidePopup()
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
}

void SketchDrawEllipseGuiCmd::tryShowPopupOnHover(double time)
{
    if (_step != Step::SpecifyCenterPnt &&
        _step != Step::SpecifyAxisEndPoint &&
        _step != Step::SpecifyOtherRadius)
    {
        return;
    }
    if (_hoverPopupState.lastMouseMoveTime < 0.0)
    {
        return;
    }
    if ((_pXYPopup && _pXYPopup->isVisible()) ||
        (_pLengthAnglePopup && _pLengthAnglePopup->isVisible()) ||
        (_pRadiusPopup && _pRadiusPopup->isVisible()))
    {
        return;
    }
    if (time - _hoverPopupState.lastMouseMoveTime >= 0.45)
    {
        this->showPopup();
    }
}

void SketchDrawEllipseGuiCmd::onPopupEnterKey()
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
    else
    {
        return;
    }

    if (this->finishStep(_step))
    {
        this->simulateMouseMoveFromPopup();
    }
}

void SketchDrawEllipseGuiCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void SketchDrawEllipseGuiCmd::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX)
        return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}

std::set<wydb::ElementId> SketchDrawEllipseGuiCmd::getSnapExcludeIds() const
{
    std::set<wydb::ElementId> snapExcludeIds;
    snapExcludeIds.insert(_sketchInfo.sketchId);
    if (_pMakeSketchEllipse) _pMakeSketchEllipse->collectElements(snapExcludeIds);
    return snapExcludeIds;
}

void MakeSketchEllipse::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pSketchEllipse) idSet.insert(_pSketchEllipse->getId());
}

bool MakeSketchEllipse::init(const wy::Vector2& centerPnt, const wy::Vector2& majorAxis, double radiusRatio, wydb::ElementId sketchId)
{
    if (!_pDb || !_pTopTrans || _pSketchEllipse || _isFinished)
    {
        return false;
    }

    // 创建SketchArc
    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    wy3d::Sketch* pSketch = nullptr;
    wy3d::SketchEllipse* pSketchEllipse = nullptr;
    wydb::Element* pSketchElem = pTrans->getElementForWrite(sketchId);
    if (!pSketchElem) goto ABORT_TRANS;
    pSketch = wy3d::Sketch::cast(pSketchElem);
    if (!pSketch) goto ABORT_TRANS;
    if (wy::ErrorStatus::Ok != wy3d::SketchEllipse::create(pTrans, centerPnt, majorAxis, radiusRatio, pSketchEllipse)
        || !pSketchEllipse)
    {
        goto ABORT_TRANS;
    }
    _pSketchEllipse = pSketchEllipse;
    if (wy::ErrorStatus::Ok != pSketch->addEntity(pSketchEllipse))
    {
        goto ABORT_TRANS;
    }
    _pDb->getTransactionManager()->endTransaction();
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    _pSketchEllipse = nullptr;
    return false;
}

bool MakeSketchEllipse::update(double otherRadius, bool autoAdjustMajorMirorAxis)
{
    if (!_pDb || !_pTopTrans || !_pSketchEllipse || _isFinished)
    {
        return false;
    }

    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    {
        _pSketchEllipse->upgradeForWrite();
        double radiusRatio = otherRadius / _pSketchEllipse->getMajorRadius();        if (autoAdjustMajorMirorAxis && radiusRatio > 1.0)
        {
            wy::Vector2 newMajorAxis = _pSketchEllipse->getMinorAxis();
            newMajorAxis.normalize();
            newMajorAxis = otherRadius * newMajorAxis;
            wy::ErrorStatus error = _pSketchEllipse->setMajorAxis(newMajorAxis);
            if (wy::ErrorStatus::Ok != error)
            {
                _pDb->getTransactionManager()->abortTransaction();
                return false;
            }
            error = _pSketchEllipse->setRadiusRatio(1.0 / radiusRatio);
            if (wy::ErrorStatus::Ok != error)
            {
                _pDb->getTransactionManager()->abortTransaction();
                return false;
            }
        }
        else
        {
            wy::ErrorStatus error = _pSketchEllipse->setRadiusRatio(radiusRatio, true);
            if (wy::ErrorStatus::Ok != error)
            {
                _pDb->getTransactionManager()->abortTransaction();
                return false;
            }
        }
    }
    if (wy::ErrorStatus::Ok == _pDb->getTransactionManager()->endTransaction())
    {
        wydb::TransactionManager* pTransMgr = _pDb->getTransactionManager();
        pTransMgr->mergeTransaction();
    }
    return true;
}
