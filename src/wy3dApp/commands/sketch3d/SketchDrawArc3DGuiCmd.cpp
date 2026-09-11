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

#include "SketchDrawArc3DGuiCmd.h"

#include <cassert>
#include <cmath>
#include <utility>

#include <QCoreApplication>
#include <QCursor>
#include <QString>

#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dMath.h>
#include <wy3dSketch3D.h>
#include <wy3dSketchArc3D.h>
#include <wy3dImpl.h>

#include "application/Application.h"
#include "commands/sketch/dialogs/GuiCmdHoverInputPopup.h"
#include "widgets/frame/MainWindow.h"
#include "utils/GuiCommandUtil.h"

// 起点悬停时的预览弧为近整圆(整圆使用SketchCircle3D)
static const double kPreviewTotalAngle = wy3d::TWO_PI - wy3d::kMinValue;

static bool parseDoubleText(const QString& text, double& value)
{
    bool ok(false);
    value = text.trimmed().toDouble(&ok);
    return ok;
}

SketchDrawArc3DGuiCmd::SketchDrawArc3DGuiCmd() : Sketch3DDrawGuiCmd(),
    _step(0), _centerPnt(), _radius(0.0), _startAngle(0.0), _totalAngle(0.0),
    _pMakeSketchArc3D(nullptr), _pXYZPopup(nullptr), _pRadiusStartAnglePopup(nullptr),
    _pSweepAnglePopup(nullptr), _hoverPopupState()
{
}

SketchDrawArc3DGuiCmd::~SketchDrawArc3DGuiCmd()
{
}

wyap::CmdExecution::StartResult SketchDrawArc3DGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = Sketch3DDrawGuiCmd::onStart();
    if (wyap::CmdExecution::StartResult::Succeeded != ret)
    {
        return ret;
    }

    this->gotoStep(static_cast<unsigned int>(Step::SpecifyCenterPnt));
    return ret;
}

void SketchDrawArc3DGuiCmd::cleanup()
{
    _step = 0;
    _centerPnt.set(0.0, 0.0, 0.0);
    _radius = 0.0;
    _startAngle = 0.0;
    _totalAngle = 0.0;
    _pMakeSketchArc3D = nullptr;

    this->hidePopup();
    _hoverPopupState.resetValue();
}

void SketchDrawArc3DGuiCmd::onEscapeKey()
{
    this->hidePopup();

    if (_step == static_cast<unsigned int>(Step::SpecifyCenterPnt))
    {
        this->requestAbort(AbortCause::UserCancel);
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyStartPoint) ||
        _step == static_cast<unsigned int>(Step::SpecifyEndPoint))
    {
        this->cleanup();
        this->gotoStep(static_cast<unsigned int>(Step::SpecifyCenterPnt));
        this->simulateMouseMoveFromPopup();
    }
    else
    {
        assert(false);
        this->requestAbort(AbortCause::ForceTerminate);
    }
}

bool SketchDrawArc3DGuiCmd::finishStep(unsigned int step)
{
    switch (static_cast<Step>(step))
    {
    case Step::SpecifyCenterPnt:
    {
        wydb::Database* pDb = Application::instance().getActiveDatabase();
        if (!pDb)
        {
            assert(false);
            return false;
        }
        _pMakeSketchArc3D = std::make_shared<MakeSketchArc3D>(this);
        if (!_pMakeSketchArc3D->init(_centerPnt, this->getWorkingPlane().getNormal(),
            this->getWorkingPlane().getXDir(), _sketch3DInfo.sketch3dId))
        {
            _pMakeSketchArc3D = nullptr;
            return false;
        }

        this->moveWorkPlaneOriginTo(_centerPnt);
        this->gotoStep(static_cast<unsigned int>(Step::SpecifyStartPoint));
        return true;
    }
    break;

    case Step::SpecifyStartPoint:
    {
        if (!_pMakeSketchArc3D)
        {
            assert(false);
            return false;
        }
        if (!_pMakeSketchArc3D->updateRadiusAndStartAngle(_radius, _startAngle))
        {
            return false;
        }
        // 收缩为退化圆弧,等待终点确定扫角
        if (!_pMakeSketchArc3D->updateTotalAngle(wy3d::kMinValue))
        {
            return false;
        }

        this->gotoStep(static_cast<unsigned int>(Step::SpecifyEndPoint));
        return true;
    }
    break;

    case Step::SpecifyEndPoint:
    {
        if (!_pMakeSketchArc3D)
        {
            assert(false);
            return false;
        }
        if (!_pMakeSketchArc3D->updateTotalAngle(_totalAngle))
        {
            return false;
        }
        _pMakeSketchArc3D->commit();
        _pMakeSketchArc3D = nullptr;

        this->gotoStep(static_cast<unsigned int>(Step::SpecifyCenterPnt));
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

void SketchDrawArc3DGuiCmd::gotoStep(unsigned int step)
{
    _step = step;
    this->hidePopup();
    _hoverPopupState.resetValue();

    switch (static_cast<Step>(step))
    {
    case Step::SpecifyCenterPnt:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawArc3DGuiCmd",
            "Specify the center point; you can directly input the coordinate values. "
            "Press Space to switch the drawing plane."));
    }
    break;

    case Step::SpecifyStartPoint:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawArc3DGuiCmd",
            "Specify the start point; you can directly input the radius and the start angle. "
            "Press Space to switch the drawing plane."));
    }
    break;

    case Step::SpecifyEndPoint:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawArc3DGuiCmd",
            "Specify the total angle; you can directly input the value."));
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

void SketchDrawArc3DGuiCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

void SketchDrawArc3DGuiCmd::onSpaceKey()
{
    // 起点确定后锁定工作平面:已点的圆心与起点不再随切面漂移
    if (_step == static_cast<unsigned int>(Step::SpecifyEndPoint))
    {
        return;
    }

    Sketch3DDrawGuiCmd::onSpaceKey();

    if (_step != static_cast<unsigned int>(Step::SpecifyStartPoint))
    {
        return;
    }
    if (!_pMakeSketchArc3D)
    {
        return;
    }

    const wy3d::SketchPlane& plane = this->getWorkingPlane();
    if (!_pMakeSketchArc3D->updatePlane(plane.getNormal(), plane.getXDir()))
    {
        return;
    }

    // 同步缓存:起始角相对新平面xDir定义
    _radius = _pMakeSketchArc3D->getRadius();
    _startAngle = _pMakeSketchArc3D->getStartAngle();
}

void SketchDrawArc3DGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (event.x != _hoverPopupState.lastMouseX ||
        event.y != _hoverPopupState.lastMouseY)
    {
        this->hidePopup();
        _hoverPopupState.lastMouseX = event.x;
        _hoverPopupState.lastMouseY = event.y;
        _hoverPopupState.lastMouseMoveTime = event.time;
    }

    std::pair<wy::Vector3, bool> ret = this->computePoint3d(event.x, event.y);
    _hoverPopupState.point = ret.first;

    if (_step == static_cast<unsigned int>(Step::SpecifyCenterPnt))
    {
        return;
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyStartPoint))
    {
        double radius(0.0);
        double angle(0.0);
        if (!this->computeRadiusAndAngle(ret.first, radius, angle))
        {
            return;
        }
        _hoverPopupState.radius = radius;
        _hoverPopupState.startAngleDeg = wy3d::radiansToDegrees(angle);
        if (_pMakeSketchArc3D)
        {
            _pMakeSketchArc3D->updateRadiusAndStartAngle(radius, angle);
        }
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyEndPoint))
    {
        double radius(0.0);
        double angle(0.0);
        if (!this->computeRadiusAndAngle(ret.first, radius, angle))
        {
            return;
        }
        double totalAngle = wy3d::normalizeRadian(angle - _startAngle);
        if (_pMakeSketchArc3D && _pMakeSketchArc3D->updateTotalAngle(totalAngle))
        {
            _totalAngle = totalAngle;
        }
        _hoverPopupState.totalAngleDeg = wy3d::radiansToDegrees(totalAngle);
    }
    else
    {
        assert(false);
    }

    return;
}

void SketchDrawArc3DGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    this->hidePopup();
    _hoverPopupState.lastMouseX = event.x;
    _hoverPopupState.lastMouseY = event.y;
    _hoverPopupState.lastMouseMoveTime = event.time;

    if (_step == static_cast<unsigned int>(Step::SpecifyCenterPnt))
    {
        _centerPnt = this->computePoint3d(event.x, event.y).first;
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyStartPoint))
    {
        double radius(0.0);
        double angle(0.0);
        if (!this->computeRadiusAndAngle(this->computePoint3d(event.x, event.y).first, radius, angle))
        {
            return;
        }
        _radius = radius;
        _startAngle = angle;
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyEndPoint))
    {
        double radius(0.0);
        double angle(0.0);
        if (!this->computeRadiusAndAngle(this->computePoint3d(event.x, event.y).first, radius, angle))
        {
            return;
        }
        _totalAngle = wy3d::normalizeRadian(angle - _startAngle);
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
    }
    else
    {
        assert(false);
    }

    return;
}

void SketchDrawArc3DGuiCmd::initializePopups()
{
    MainWindow* pMainWindow = Application::instance().getMainWindow();
    if (!pMainWindow)
    {
        assert(false);
        return;
    }

    if (!_pXYZPopup)
    {
        _pXYZPopup = std::make_unique<GuiCmdHoverInputPopup3>(
            QStringLiteral("X"),
            QStringLiteral("Y"),
            QStringLiteral("Z"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pXYZPopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pXYZPopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pXYZPopup->setSpaceKeyHandler([this]() { this->onPopupSpaceKey(); });
        _pXYZPopup->hide();
    }

    if (!_pRadiusStartAnglePopup)
    {
        _pRadiusStartAnglePopup = std::make_unique<GuiCmdHoverInputPopup2>(
            QCoreApplication::translate("SketchDrawArc3DGuiCmd", "Radius"),
            QCoreApplication::translate("SketchDrawArc3DGuiCmd", "StartAngle"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pRadiusStartAnglePopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pRadiusStartAnglePopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pRadiusStartAnglePopup->setSpaceKeyHandler([this]() { this->onPopupSpaceKey(); });
        _pRadiusStartAnglePopup->hide();
    }

    if (!_pSweepAnglePopup)
    {
        _pSweepAnglePopup = std::make_unique<GuiCmdHoverInputPopup1>(
            QCoreApplication::translate("SketchDrawArc3DGuiCmd", "SweepAngle"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pSweepAnglePopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pSweepAnglePopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pSweepAnglePopup->setSpaceKeyHandler([this]() { this->onPopupSpaceKey(); });
        _pSweepAnglePopup->hide();
    }
}

void SketchDrawArc3DGuiCmd::showPopup()
{
    if (!_pXYZPopup || !_pRadiusStartAnglePopup || !_pSweepAnglePopup)
    {
        this->initializePopups();
    }

    if (_step == static_cast<unsigned int>(Step::SpecifyCenterPnt))
    {
        if (!_pXYZPopup)
        {
            return;
        }
        _pXYZPopup->setValues(
            _hoverPopupState.point.x(),
            _hoverPopupState.point.y(),
            _hoverPopupState.point.z());
        _pXYZPopup->showAtGlobal(QCursor::pos());
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyStartPoint))
    {
        if (!_pRadiusStartAnglePopup)
        {
            return;
        }
        _pRadiusStartAnglePopup->setValues(
            _hoverPopupState.radius,
            _hoverPopupState.startAngleDeg);
        _pRadiusStartAnglePopup->showAtGlobal(QCursor::pos());
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyEndPoint))
    {
        if (!_pSweepAnglePopup)
        {
            return;
        }
        _pSweepAnglePopup->setValue(_hoverPopupState.totalAngleDeg);
        _pSweepAnglePopup->showAtGlobal(QCursor::pos());
    }
}

void SketchDrawArc3DGuiCmd::hidePopup()
{
    if (_pXYZPopup && _pXYZPopup->isVisible())
    {
        _pXYZPopup->hide();
    }
    if (_pRadiusStartAnglePopup && _pRadiusStartAnglePopup->isVisible())
    {
        _pRadiusStartAnglePopup->hide();
    }
    if (_pSweepAnglePopup && _pSweepAnglePopup->isVisible())
    {
        _pSweepAnglePopup->hide();
    }
}

void SketchDrawArc3DGuiCmd::tryShowPopupOnHover(double time)
{
    if (_step != static_cast<unsigned int>(Step::SpecifyCenterPnt) &&
        _step != static_cast<unsigned int>(Step::SpecifyStartPoint) &&
        _step != static_cast<unsigned int>(Step::SpecifyEndPoint))
    {
        return;
    }
    if (_hoverPopupState.lastMouseMoveTime < 0.0)
    {
        return;
    }
    if ((_pXYZPopup && _pXYZPopup->isVisible()) ||
        (_pRadiusStartAnglePopup && _pRadiusStartAnglePopup->isVisible()) ||
        (_pSweepAnglePopup && _pSweepAnglePopup->isVisible()))
    {
        return;
    }
    if (time - _hoverPopupState.lastMouseMoveTime >= 0.45)
    {
        this->showPopup();
    }
}

void SketchDrawArc3DGuiCmd::onPopupEnterKey()
{
    if (_step == static_cast<unsigned int>(Step::SpecifyCenterPnt))
    {
        if (!_pXYZPopup)
        {
            return;
        }

        double x(0.0);
        double y(0.0);
        double z(0.0);
        if (!parseDoubleText(_pXYZPopup->getRow1Text(), x) ||
            !parseDoubleText(_pXYZPopup->getRow2Text(), y) ||
            !parseDoubleText(_pXYZPopup->getRow3Text(), z))
        {
            return;
        }
        _centerPnt.set(x, y, z);
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyStartPoint))
    {
        if (!_pRadiusStartAnglePopup)
        {
            return;
        }

        double radius(0.0);
        double startAngleDeg(0.0);
        if (!parseDoubleText(_pRadiusStartAnglePopup->getRow1Text(), radius) ||
            !parseDoubleText(_pRadiusStartAnglePopup->getRow2Text(), startAngleDeg))
        {
            return;
        }
        _radius = radius;
        _startAngle = wy3d::normalizeRadian(wy3d::degreesToRadians(startAngleDeg));
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyEndPoint))
    {
        if (!_pSweepAnglePopup)
        {
            return;
        }

        double sweepAngleDeg(0.0);
        if (!parseDoubleText(_pSweepAnglePopup->getRowText(), sweepAngleDeg))
        {
            return;
        }
        _totalAngle = wy3d::degreesToRadians(sweepAngleDeg);
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

void SketchDrawArc3DGuiCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void SketchDrawArc3DGuiCmd::onPopupSpaceKey()
{
    this->onSpaceKey();
    this->hidePopup();
    this->simulateMouseMoveFromPopup();
}

void SketchDrawArc3DGuiCmd::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX)
        return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}

std::pair<wy::Vector3, bool> SketchDrawArc3DGuiCmd::computePoint3d(double x, double y)
{
    auto ret = this->computePosition3d(x, y, this->getWorkingPlane(), this->getSnapExcludeIds(), true);
    if (ret.second)
    {
        return std::make_pair(ret.second->getPosition(), true);
    }
    return std::make_pair(ret.first, false);
}

bool SketchDrawArc3DGuiCmd::computeRadiusAndAngle(const wy::Vector3& pnt, double& outRadius, double& outAngle) const
{
    const wy3d::SketchPlane& plane = this->getWorkingPlane();
    const wy::Vector3 normal = plane.getNormal();
    const wy::Vector3 xDir = plane.getXDir();
    const wy::Vector3 yDir = normal.cross(xDir);

    wy::Vector3 vec = pnt - _centerPnt;
    vec = vec - normal * vec.dot(normal); // 投影到工作平面内
    const double radius = vec.length();
    if (radius < wy3d::kMinValue)
    {
        return false;
    }

    outRadius = radius;
    outAngle = wy3d::normalizeRadian(std::atan2(vec.dot(yDir), vec.dot(xDir)));
    return true;
}

std::set<wydb::ElementId> SketchDrawArc3DGuiCmd::getSnapExcludeIds() const
{
    std::set<wydb::ElementId> snapExcludeIds;
    // 只排除正在预览的图元,已提交的3D草图图元可作捕捉源(与2D草绘行为一致)
    if (_pMakeSketchArc3D)
    {
        _pMakeSketchArc3D->collectElements(snapExcludeIds);
    }
    return snapExcludeIds;
}

void MakeSketchArc3D::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pSketchArc3D) idSet.insert(_pSketchArc3D->getId());
}

bool MakeSketchArc3D::init(const wy::Vector3& centerPnt, const wy::Vector3& normal, const wy::Vector3& xDir, wydb::ElementId sketch3dId)
{
    if (!_pDb || !_pTopTrans || _pSketchArc3D || _isFinished)
    {
        return false;
    }

    // 创建SketchArc3D
    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    wy3d::Sketch3D* pSketch3D = nullptr;
    wy3d::SketchArc3D* pSketchArc3D = nullptr;
    wydb::Element* pSketchElem = pTrans->getElementForWrite(sketch3dId);
    if (!pSketchElem) goto ABORT_TRANS;
    pSketch3D = wy3d::Sketch3D::cast(pSketchElem);
    if (!pSketch3D) goto ABORT_TRANS;

    if (wy::ErrorStatus::Ok != wy3d::SketchArc3D::create(pTrans, centerPnt, normal, xDir, wy3d::kMinValue, 0.0, wy3d::kMinValue, pSketchArc3D)
        || !pSketchArc3D)
    {
        goto ABORT_TRANS;
    }
    _pSketchArc3D = pSketchArc3D;
    if (wy::ErrorStatus::Ok != pSketch3D->addEntity(pSketchArc3D))
    {
        goto ABORT_TRANS;
    }
    _pDb->getTransactionManager()->endTransaction();
    _pSketchArc3D = pSketchArc3D;
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    _pSketchArc3D = nullptr;
    return false;
}

bool MakeSketchArc3D::updateRadiusAndStartAngle(double radius, double startAngle)
{
    if (!_pDb || !_pTopTrans || !_pSketchArc3D || _isFinished)
    {
        return false;
    }
    if (radius < wy3d::kMinValue)
    {
        return false;
    }

    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    {
        _pSketchArc3D->upgradeForWrite();
        if (wy::ErrorStatus::Ok != _pSketchArc3D->setRadius(radius) ||
            wy::ErrorStatus::Ok != _pSketchArc3D->setStartAngle(startAngle) ||
            wy::ErrorStatus::Ok != _pSketchArc3D->setEndAngle(startAngle + kPreviewTotalAngle))
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

bool MakeSketchArc3D::updateTotalAngle(double totalAngle)
{
    if (!_pDb || !_pTopTrans || !_pSketchArc3D || _isFinished)
    {
        return false;
    }
    while (totalAngle >= wy3d::TWO_PI)
    {
        totalAngle -= wy3d::TWO_PI;
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
        _pSketchArc3D->upgradeForWrite();
        if (wy::ErrorStatus::Ok != _pSketchArc3D->setEndAngle(_pSketchArc3D->getStartAngle() + totalAngle))
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

bool MakeSketchArc3D::updatePlane(const wy::Vector3& normal, const wy::Vector3& xDir)
{
    if (!_pDb || !_pTopTrans || !_pSketchArc3D || _isFinished)
    {
        return false;
    }

    const double radius = _pSketchArc3D->getRadius();
    const double totalAngle = _pSketchArc3D->getTotalAngle();

    // 起点方向取原世界起点在新平面上的投影;与原平面垂直时回退到新xDir
    const wy::Vector3 yDir = normal.cross(xDir);
    wy::Vector3 vec = _pSketchArc3D->getStartPoint() - _pSketchArc3D->getCenter();
    vec = vec - normal * vec.dot(normal);
    double startAngle(0.0);
    if (vec.length() >= wy3d::kMinValue)
    {
        startAngle = wy3d::normalizeRadian(std::atan2(vec.dot(yDir), vec.dot(xDir)));
    }

    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    {
        _pSketchArc3D->upgradeForWrite();
        if (wy::ErrorStatus::Ok != _pSketchArc3D->setPlane(normal, xDir) ||
            wy::ErrorStatus::Ok != _pSketchArc3D->setRadius(radius) ||
            wy::ErrorStatus::Ok != _pSketchArc3D->setStartAngle(startAngle) ||
            wy::ErrorStatus::Ok != _pSketchArc3D->setEndAngle(startAngle + totalAngle))
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

double MakeSketchArc3D::getRadius() const
{
    return _pSketchArc3D ? _pSketchArc3D->getRadius() : 0.0;
}

double MakeSketchArc3D::getStartAngle() const
{
    return _pSketchArc3D ? _pSketchArc3D->getStartAngle() : 0.0;
}
