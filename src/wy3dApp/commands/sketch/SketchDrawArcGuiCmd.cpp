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

#include "SketchDrawArcGuiCmd.h"

#include <QCoreApplication>
#include <QCursor>
#include <QString>
#include <cmath>

#include <wyVector2.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapSelManager.h>
#include <wy3dImpl.h>
#include <wy3dSketch.h>
#include <wy3dSketchArc.h>

#include "application/Application.h"
#include "commands/sketch/dialogs/GuiCmdHoverInputPopup.h"
#include "commands/transient/SketchBasicTransient.h"
#include "snap/SketchSnapSystem.h"
#include "widgets/frame/MainWindow.h"


static bool parseDoubleText(const QString& text, double& value)
{
    bool ok(false);
    value = text.trimmed().toDouble(&ok);
    return ok;
}

SketchDrawArcGuiCmd::SketchDrawArcGuiCmd()
    : OsgGuiCommand(),
    _step(Step::Undefined), _centerPnt(), _startPnt(), _radius(0.0), _startAngle(0.0), _totalAngle(0.0),
    _pXYPopup(nullptr), _pRadiusAnglePopup(nullptr), _pSweepAnglePopup(nullptr), _hoverPopupState()
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

SketchDrawArcGuiCmd::~SketchDrawArcGuiCmd()
{
}

wyap::CmdExecution::StartResult SketchDrawArcGuiCmd::onStart()
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
    this->gotoStep(Step::SpecifyCenterPnt);

    // 鼠标样式
    Application::instance().setCursor(CursorType::Locate);

    return wyap::CmdExecution::StartResult::Succeeded;
}

void SketchDrawArcGuiCmd::cleanup()
{
    this->hidePopup();

    _step = Step::Undefined;
    _centerPnt.set(0.0, 0.0);
    _startPnt.set(0.0, 0.0);
    _radius = 0.0;
    _startAngle = 0.0;
    _totalAngle = 0.0;
    _pSnapContext = nullptr;
    _hoverPopupState.resetValue();

    _pCircleTransient = nullptr;
    _pCenterPointTransient = nullptr;
    _pMakeSketchArc = nullptr;
}

void SketchDrawArcGuiCmd::reset()
{
    this->cleanup();
    this->gotoStep(Step::SpecifyCenterPnt);
}

void SketchDrawArcGuiCmd::onEscapeKey()
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

bool SketchDrawArcGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SpecifyCenterPnt:
    {
        // 圆
        _pCircleTransient = std::make_shared<SketchCircleTransient>(_sketchInfo.sketchPlane);
        _pCircleTransient->update(_centerPnt, 0.0);
        _pCircleTransient->show();

        // 圆心标记
        _pCenterPointTransient = std::make_shared<CenterPointTransient>();
        _pCenterPointTransient->update(_sketchInfo.sketchPlane, _centerPnt);
        _pCenterPointTransient->show();

        // next step
        this->gotoStep(Step::SpecifyStartPoint);
        return true;
    }
    break;

    case Step::SpecifyStartPoint:
    {
        wydb::Database* pDb = Application::instance().getActiveDatabase();
        if (pDb)
        {
            _pMakeSketchArc = std::make_shared<MakeSketchArc>(this);
            if (!_pMakeSketchArc->init(_centerPnt, _radius, _startAngle, _sketchInfo.sketchId))
            {
                _pMakeSketchArc = nullptr;
                return false;
            }
        }
        else
        {
            this->reset();
            return false;
        }

        // 圆
        _pCircleTransient = nullptr;

        // next step
        this->gotoStep(Step::SpecifyEndPoint);
        return true;
    }
    break;

    case Step::SpecifyEndPoint:
    {
        if (_pMakeSketchArc)
        {
            if (!_pMakeSketchArc->update(_totalAngle))
            {
                return false;
            }
            _pMakeSketchArc->commit();
            _pMakeSketchArc = nullptr;
        }

        // 圆心标记
        _pCenterPointTransient = nullptr;

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

void SketchDrawArcGuiCmd::gotoStep(Step step)
{
    _step = step;
    this->hidePopup();
    _hoverPopupState.resetValue();

    switch (step)
    {
    case Step::SpecifyCenterPnt:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawArc",
            "Specify the center point; you can directly input the coordinate values."));

        if (_sketchInfo.pSketchSnapSys)
        {
            _sketchInfo.pSketchSnapSys->partiallyUpdate(Application::instance().getActiveDatabase());
        }
        _pSnapContext = std::make_shared<SketchLocateContext>(wydb::ElementId::kNull);
    }
    break;

    case Step::SpecifyStartPoint:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawArc",
            "Specify the start point; you can directly input the values."));

        SketchDrawCircleContextSPtr pDrawCircleContext = std::make_shared<SketchDrawCircleContext>(
            wydb::ElementId::kNull, _centerPnt);
        pDrawCircleContext->setIsForDrawArc(true);
        _pSnapContext = pDrawCircleContext;
    }
    break;

    case Step::SpecifyEndPoint:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawArc",
            "Specify the total angle; you can directly input the value."));

        _pSnapContext = std::make_shared<SketchDrawArcContext>(
            _pMakeSketchArc ? _pMakeSketchArc->getId() : wydb::ElementId::kNull, _centerPnt, _radius, _startPnt);
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

void SketchDrawArcGuiCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

void SketchDrawArcGuiCmd::onMouseMove(const MouseEvent& event)
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

    case Step::SpecifyStartPoint:
    {
        wy::Vector2 startPnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, this->getSnapExcludeIds(), _pSnapContext, _sketchInfo.pSketchSnapSys);
        wy::Vector2 vec = startPnt - _centerPnt;
        double radius = vec.length();
        double angle = wy3d::radiansToDegrees(wy::Vector2::rotationAngle(wy::Vector2::kXAxis, vec));
        _hoverPopupState.point = startPnt;
        _hoverPopupState.radius = radius;
        _hoverPopupState.startAngleDeg = angle;
        {
            if (_pCircleTransient)
            {
                _pCircleTransient->update(_centerPnt, radius);
            }
        }
    }
    break;

    case Step::SpecifyEndPoint:
    {
        wy::Vector2 endPnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, this->getSnapExcludeIds(), _pSnapContext, _sketchInfo.pSketchSnapSys, false);
        double totalAngle = wy::Vector2::rotationAngle(_startPnt - _centerPnt, endPnt - _centerPnt);
        double totalAngleInDegree = wy3d::radiansToDegrees(totalAngle);
        _hoverPopupState.point = endPnt;
        _hoverPopupState.totalAngleDeg = totalAngleInDegree;
        {
            if (_pMakeSketchArc) _pMakeSketchArc->update(totalAngle);
        }
    }
    break;

    default:
    {
        //assert(false);
    }
    break;
    }

    return;
}

void SketchDrawArcGuiCmd::onLeftMouseDown(const MouseEvent& event)
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

    case Step::SpecifyStartPoint:
    {
        _startPnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, this->getSnapExcludeIds(), _pSnapContext, _sketchInfo.pSketchSnapSys);
        _radius = (_startPnt - _centerPnt).length();
        _startAngle = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), _startPnt - _centerPnt);
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
    }
    break;

    case Step::SpecifyEndPoint:
    {
        wy::Vector2 endPnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, this->getSnapExcludeIds(), _pSnapContext, _sketchInfo.pSketchSnapSys, false);
        _totalAngle = wy::Vector2::rotationAngle(_startPnt - _centerPnt, endPnt - _centerPnt);
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

void SketchDrawArcGuiCmd::initializePopups()
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
    if (!_pRadiusAnglePopup)
    {
        _pRadiusAnglePopup = std::make_unique<GuiCmdHoverInputPopup2>(
            QCoreApplication::translate("SketchDrawArc", "Radius"),
            QCoreApplication::translate("SketchDrawArc", "StartAngle"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pRadiusAnglePopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pRadiusAnglePopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pRadiusAnglePopup->hide();
    }
    if (!_pSweepAnglePopup)
    {
        _pSweepAnglePopup = std::make_unique<GuiCmdHoverInputPopup1>(
            QCoreApplication::translate("SketchDrawArc", "SweepAngle"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pSweepAnglePopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pSweepAnglePopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pSweepAnglePopup->hide();
    }
}

void SketchDrawArcGuiCmd::showPopup()
{
    if (!_pXYPopup || !_pRadiusAnglePopup || !_pSweepAnglePopup)
    {
        this->initializePopups();
    }
    GuiCmdHoverInputPopupBase* pActivePopup = this->getActivePopup();
    if (!pActivePopup)
    {
        return;
    }

    if (_step == Step::SpecifyCenterPnt)
    {
        _pXYPopup->setValues(
            _hoverPopupState.point.x(),
            _hoverPopupState.point.y());
    }
    else if (_step == Step::SpecifyStartPoint)
    {
        _pRadiusAnglePopup->setValues(
            _hoverPopupState.radius,
            _hoverPopupState.startAngleDeg);
    }
    else if (_step == Step::SpecifyEndPoint)
    {
        _pSweepAnglePopup->setValue(_hoverPopupState.totalAngleDeg);
    }
    else
    {
        return;
    }
    pActivePopup->showAtGlobal(QCursor::pos());
}

void SketchDrawArcGuiCmd::hidePopup()
{
    if (_pXYPopup && _pXYPopup->isVisible())
    {
        _pXYPopup->hide();
    }
    if (_pRadiusAnglePopup && _pRadiusAnglePopup->isVisible())
    {
        _pRadiusAnglePopup->hide();
    }
    if (_pSweepAnglePopup && _pSweepAnglePopup->isVisible())
    {
        _pSweepAnglePopup->hide();
    }
}

GuiCmdHoverInputPopupBase* SketchDrawArcGuiCmd::getActivePopup() const
{
    if (_step == Step::SpecifyCenterPnt) return _pXYPopup.get();
    if (_step == Step::SpecifyStartPoint) return _pRadiusAnglePopup.get();
    if (_step == Step::SpecifyEndPoint) return _pSweepAnglePopup.get();
    return nullptr;
}

void SketchDrawArcGuiCmd::tryShowPopupOnHover(double time)
{
    if (_step != Step::SpecifyCenterPnt &&
        _step != Step::SpecifyStartPoint &&
        _step != Step::SpecifyEndPoint)
    {
        return;
    }
    if (_hoverPopupState.lastMouseMoveTime < 0.0)
    {
        return;
    }
    if ((_pXYPopup && _pXYPopup->isVisible()) ||
        (_pRadiusAnglePopup && _pRadiusAnglePopup->isVisible()) ||
        (_pSweepAnglePopup && _pSweepAnglePopup->isVisible()))
    {
        return;
    }
    if (time - _hoverPopupState.lastMouseMoveTime >= 0.45)
    {
        this->showPopup();
    }
}

void SketchDrawArcGuiCmd::onPopupEnterKey()
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
    else if (_step == Step::SpecifyStartPoint)
    {
        if (!_pRadiusAnglePopup) return;
        double radius(0.0), angleDeg(0.0);
        if (!parseDoubleText(_pRadiusAnglePopup->getRow1Text(), radius) ||
            !parseDoubleText(_pRadiusAnglePopup->getRow2Text(), angleDeg))
        {
            return;
        }
        _radius = radius;
        _startAngle = wy3d::degreesToRadians(angleDeg);
        _startPnt = _centerPnt + _radius * wy::Vector2(std::cos(_startAngle), std::sin(_startAngle));
    }
    else if (_step == Step::SpecifyEndPoint)
    {
        if (!_pSweepAnglePopup) return;
        double totalAngleDeg(0.0);
        if (!parseDoubleText(_pSweepAnglePopup->getRowText(), totalAngleDeg))
        {
            return;
        }
        _totalAngle = wy3d::degreesToRadians(totalAngleDeg);
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

void SketchDrawArcGuiCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void SketchDrawArcGuiCmd::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX)
        return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}

std::set<wydb::ElementId> SketchDrawArcGuiCmd::getSnapExcludeIds() const
{
    std::set<wydb::ElementId> snapExcludeIds;
    snapExcludeIds.insert(_sketchInfo.sketchId);
    if (_pMakeSketchArc) _pMakeSketchArc->collectElements(snapExcludeIds);
    return snapExcludeIds;
}

void MakeSketchArc::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pSketchArc) idSet.insert(_pSketchArc->getId());
}

bool MakeSketchArc::init(const wy::Vector2& centerPnt, double radius, double startAngle, wydb::ElementId sketchId)
{
    if (!_pDb || !_pTopTrans || _pSketchArc || _isFinished)
    {
        return false;
    }

    // 创建SketchArc
    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    wy3d::Sketch* pSketch = nullptr;
    wy3d::SketchArc* pSketchArc = nullptr;
    wydb::Element* pSketchElem = pTrans->getElementForWrite(sketchId);
    if (!pSketchElem) goto ABORT_TRANS;
    pSketch = wy3d::Sketch::cast(pSketchElem);
    if (!pSketch) goto ABORT_TRANS;
    if (wy::ErrorStatus::Ok != wy3d::SketchArc::create(pTrans, centerPnt, radius, startAngle, startAngle + wy3d::kMinValue, pSketchArc)
        || !pSketchArc)
    {
        goto ABORT_TRANS;
    }
    _pSketchArc = pSketchArc;
    if (wy::ErrorStatus::Ok != pSketch->addEntity(pSketchArc))
    {
        goto ABORT_TRANS;
    }
    _pDb->getTransactionManager()->endTransaction();
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    _pSketchArc = nullptr;
    return false;
}

bool MakeSketchArc::update(double totalAngle)
{
    if (!_pDb || !_pTopTrans || !_pSketchArc || _isFinished)
    {
        return false;
    }

    double twoPI = wy3d::PI * 2;
    while (totalAngle >= twoPI)
    {
        totalAngle -= twoPI;
    }
    if (totalAngle < wy3d::kMinValue)
    {
        return false;
    }

    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    {
        _pSketchArc->upgradeForWrite();
        _pSketchArc->setEndAngle(_pSketchArc->getStartAngle() + totalAngle);
    }
    if (wy::ErrorStatus::Ok == _pDb->getTransactionManager()->endTransaction())
    {
        wydb::TransactionManager* pTransMgr = _pDb->getTransactionManager();
        pTransMgr->mergeTransaction();
    }
    return true;
}
