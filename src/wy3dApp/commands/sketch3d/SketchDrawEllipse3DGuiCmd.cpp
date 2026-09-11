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

#include "SketchDrawEllipse3DGuiCmd.h"

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
#include <wy3dImpl.h>

#include "application/Application.h"
#include "commands/sketch/dialogs/GuiCmdHoverInputPopup.h"
#include "utils/GuiCommandUtil.h"
#include "widgets/frame/MainWindow.h"

static bool parseDoubleText(const QString& text, double& value)
{
    bool ok(false);
    value = text.trimmed().toDouble(&ok);
    return ok;
}

SketchDrawEllipse3DGuiCmd::SketchDrawEllipse3DGuiCmd() : Sketch3DDrawGuiCmd(),
    _step(0), _centerPnt(), _majorAxisDir(wy::Vector3::kXAxis), _majorRadius(0.0), _otherRadius(0.0),
    _pMakeSketchEllipse3D(nullptr), _pXYZPopup(nullptr), _pLengthAnglePopup(nullptr),
    _pRadiusPopup(nullptr), _hoverPopupState(), _pAxisTransient(nullptr)
{
}

SketchDrawEllipse3DGuiCmd::~SketchDrawEllipse3DGuiCmd()
{
}

wyap::CmdExecution::StartResult SketchDrawEllipse3DGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = Sketch3DDrawGuiCmd::onStart();
    if (wyap::CmdExecution::StartResult::Succeeded != ret)
    {
        return ret;
    }

    this->gotoStep(static_cast<unsigned int>(Step::SpecifyCenterPnt));
    return ret;
}

void SketchDrawEllipse3DGuiCmd::cleanup()
{
    _step = 0;
    _centerPnt.set(0.0, 0.0, 0.0);
    _majorAxisDir = wy::Vector3::kXAxis;
    _majorRadius = 0.0;
    _otherRadius = 0.0;
    _pMakeSketchEllipse3D = nullptr;
    this->destroyAxisTransient();

    this->hidePopup();
    _hoverPopupState.resetValue();
}

void SketchDrawEllipse3DGuiCmd::onEscapeKey()
{
    this->hidePopup();

    if (_step == static_cast<unsigned int>(Step::SpecifyCenterPnt))
    {
        this->requestAbort(AbortCause::UserCancel);
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyAxisEndPoint) ||
        _step == static_cast<unsigned int>(Step::SpecifyOtherRadius))
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

bool SketchDrawEllipse3DGuiCmd::finishStep(unsigned int step)
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
        // 先让工作平面原点落在中心,再建图元:切平面时中心不动、椭圆绕中心转
        this->moveWorkPlaneOriginTo(_centerPnt);

        const wy3d::SketchPlane& plane = this->getWorkingPlane();
        _majorAxisDir = plane.getXDir();
        _pMakeSketchEllipse3D = std::make_shared<MakeSketchEllipse3D>(this);
        if (!_pMakeSketchEllipse3D->init(_centerPnt, plane.getNormal(), plane.getXDir(),
            wy3d::kMinValue, 1.0, _sketch3DInfo.sketch3dId))
        {
            _pMakeSketchEllipse3D = nullptr;
            return false;
        }

        _pAxisTransient = std::make_shared<LineTransient>();
        this->updateAxisTransient(_centerPnt);

        this->gotoStep(static_cast<unsigned int>(Step::SpecifyAxisEndPoint));
        return true;
    }
    break;

    case Step::SpecifyAxisEndPoint:
    {
        if (!_pMakeSketchEllipse3D)
        {
            assert(false);
            return false;
        }
        // 弹窗输入的长轴立即生效
        if (!this->applyToEntity(this->getWorkingPlane()))
        {
            return false;
        }
        this->gotoStep(static_cast<unsigned int>(Step::SpecifyOtherRadius));
        return true;
    }
    break;

    case Step::SpecifyOtherRadius:
    {
        if (!_pMakeSketchEllipse3D)
        {
            assert(false);
            return false;
        }

        // 另一半轴大于长半轴时互换长短轴(预览期间不互换,与2D一致)
        const wy3d::SketchPlane& plane = this->getWorkingPlane();
        double majorRadius = _majorRadius;
        double radiusRatio = (_majorRadius > wy3d::TOL) ? (_otherRadius / _majorRadius) : 1.0;
        wy::Vector3 majorDir = _majorAxisDir;
        if (radiusRatio > 1.0)
        {
            const double majorAngle = this->angleFromPlaneDir(plane, _majorAxisDir);
            majorDir = this->planeDirFromAngle(plane, majorAngle + wy3d::PI_2);
            majorRadius = _otherRadius;
            radiusRatio = (_otherRadius > wy3d::TOL) ? (_majorRadius / _otherRadius) : 1.0;
        }
        if (!_pMakeSketchEllipse3D->update(plane.getNormal(), majorDir, majorRadius, radiusRatio))
        {
            return false;
        }

        _pMakeSketchEllipse3D->commit();
        _pMakeSketchEllipse3D = nullptr;
        this->destroyAxisTransient();

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

void SketchDrawEllipse3DGuiCmd::gotoStep(unsigned int step)
{
    _step = step;
    this->hidePopup();
    _hoverPopupState.resetValue();

    switch (static_cast<Step>(step))
    {
    case Step::SpecifyCenterPnt:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawEllipse3DGuiCmd",
            "Specify the center point; you can directly input the coordinate values. "
            "Press Space to switch the drawing plane."));
    }
    break;

    case Step::SpecifyAxisEndPoint:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawEllipse3DGuiCmd",
            "Specify the axis vector; you can directly input the values. "
            "Press Space to switch the drawing plane."));
    }
    break;

    case Step::SpecifyOtherRadius:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawEllipse3DGuiCmd",
            "Specify the other radius; you can directly input the value. "
            "Press Space to switch the drawing plane."));
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

void SketchDrawEllipse3DGuiCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

double SketchDrawEllipse3DGuiCmd::currentRatio() const
{
    if (_majorRadius <= wy3d::TOL) return 1.0;
    double ratio = _otherRadius / _majorRadius;
    if (ratio > 1.0) ratio = 1.0; // 预览期间不互换长短轴
    if (ratio < 1e-5) ratio = 1e-5;
    return ratio;
}

wy::Vector3 SketchDrawEllipse3DGuiCmd::planeDirFromAngle(const wy3d::SketchPlane& plane, double angle) const
{
    return plane.getXDir() * std::cos(angle) + plane.getYDir() * std::sin(angle);
}

double SketchDrawEllipse3DGuiCmd::angleFromPlaneDir(const wy3d::SketchPlane& plane, const wy::Vector3& dir) const
{
    return std::atan2(dir.dot(plane.getYDir()), dir.dot(plane.getXDir()));
}

bool SketchDrawEllipse3DGuiCmd::applyToEntity(const wy3d::SketchPlane& plane)
{
    if (!_pMakeSketchEllipse3D)
    {
        return false;
    }
    return _pMakeSketchEllipse3D->update(plane.getNormal(), _majorAxisDir, _majorRadius, this->currentRatio());
}

void SketchDrawEllipse3DGuiCmd::onSpaceKey()
{
    Sketch3DDrawGuiCmd::onSpaceKey();

    if (_step != static_cast<unsigned int>(Step::SpecifyAxisEndPoint) &&
        _step != static_cast<unsigned int>(Step::SpecifyOtherRadius))
    {
        return;
    }
    if (!_pMakeSketchEllipse3D)
    {
        return;
    }

    // 保中心/半轴,长轴方向投影到新平面后整体重发(椭圆绕中心刚性转向)
    const wy3d::SketchPlane& plane = this->getWorkingPlane();
    const wy::Vector3 normal = plane.getNormal();
    wy::Vector3 xDir = _majorAxisDir - normal * _majorAxisDir.dot(normal);
    if (xDir.length() < wy3d::EPS)
    {
        xDir = plane.getXDir();
    }
    else
    {
        xDir.normalize();
    }
    _majorAxisDir = xDir;
    if (!_pMakeSketchEllipse3D->updatePlane(plane.getNormal(), xDir))
    {
        return;
    }

    // 同步悬停显示值(角度相对新平面xDir)
    _hoverPopupState.majorLength = _majorRadius;
    _hoverPopupState.majorAngleDeg = wy3d::radiansToDegrees(this->angleFromPlaneDir(plane, _majorAxisDir));
    _hoverPopupState.otherRadius = _otherRadius;
    this->hidePopup();
}

void SketchDrawEllipse3DGuiCmd::onMouseMove(const MouseEvent& event)
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

    const wy3d::SketchPlane& plane = this->getWorkingPlane();
    const wy::Vector3 delta = ret.first - _centerPnt;

    if (_step == static_cast<unsigned int>(Step::SpecifyAxisEndPoint))
    {
        // 长轴:向量 = 端点 - 中心(长度即长半轴,方向即长轴方向)
        const wy::Vector2 uv = plane.uv(ret.first) - plane.uv(_centerPnt);
        const double majorRadius = uv.length();
        const double majorAngle = std::atan2(uv.y(), uv.x());
        _hoverPopupState.majorLength = majorRadius;
        _hoverPopupState.majorAngleDeg = wy3d::radiansToDegrees(majorAngle);
        this->updateAxisTransient(ret.first);
        if (_pMakeSketchEllipse3D && majorRadius >= wy3d::kMinValue)
        {
            _majorRadius = majorRadius; // 切平面时要用它做长轴方向投影
            _majorAxisDir = this->planeDirFromAngle(plane, majorAngle);
            _pMakeSketchEllipse3D->update(plane.getNormal(), _majorAxisDir, majorRadius, 1.0);
        }
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyOtherRadius))
    {
        // 另一半轴:到中心的距离(方向无关)
        const double otherRadius = (plane.uv(ret.first) - plane.uv(_centerPnt)).length();
        _hoverPopupState.otherRadius = otherRadius;
        _otherRadius = otherRadius; // 预览与提交都以它为准
        this->updateAxisTransient(ret.first);
        if (_pMakeSketchEllipse3D && otherRadius >= wy3d::kMinValue && _majorRadius > wy3d::TOL)
        {
            _pMakeSketchEllipse3D->update(plane.getNormal(), _majorAxisDir, _majorRadius, this->currentRatio());
        }
    }
    else
    {
        assert(false);
    }

    return;
}

void SketchDrawEllipse3DGuiCmd::onLeftMouseDown(const MouseEvent& event)
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
    else if (_step == static_cast<unsigned int>(Step::SpecifyAxisEndPoint))
    {
        const wy3d::SketchPlane& plane = this->getWorkingPlane();
        const wy::Vector3 pnt = this->computePoint3d(event.x, event.y).first;
        const wy::Vector2 uv = plane.uv(pnt) - plane.uv(_centerPnt);
        _majorRadius = uv.length();
        if (_majorRadius < wy3d::kMinValue)
        {
            return;
        }
        _majorAxisDir = this->planeDirFromAngle(plane, std::atan2(uv.y(), uv.x()));
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyOtherRadius))
    {
        const wy3d::SketchPlane& plane = this->getWorkingPlane();
        _otherRadius = (plane.uv(this->computePoint3d(event.x, event.y).first) - plane.uv(_centerPnt)).length();
        if (_otherRadius < wy3d::kMinValue)
        {
            return;
        }
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

void SketchDrawEllipse3DGuiCmd::updateAxisTransient(const wy::Vector3& endPnt)
{
    if (!_pAxisTransient)
    {
        return;
    }
    _pAxisTransient->update(_centerPnt, endPnt);
}

void SketchDrawEllipse3DGuiCmd::destroyAxisTransient()
{
    _pAxisTransient = nullptr;
}

void SketchDrawEllipse3DGuiCmd::initializePopups()
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

    if (!_pLengthAnglePopup)
    {
        _pLengthAnglePopup = std::make_unique<GuiCmdHoverInputPopup2>(
            QCoreApplication::translate("SketchDrawEllipse3DGuiCmd", "Length"),
            QCoreApplication::translate("SketchDrawEllipse3DGuiCmd", "Angle"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pLengthAnglePopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pLengthAnglePopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pLengthAnglePopup->setSpaceKeyHandler([this]() { this->onPopupSpaceKey(); });
        _pLengthAnglePopup->hide();
    }

    if (!_pRadiusPopup)
    {
        _pRadiusPopup = std::make_unique<GuiCmdHoverInputPopup1>(
            QCoreApplication::translate("SketchDrawEllipse3DGuiCmd", "Radius"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pRadiusPopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pRadiusPopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pRadiusPopup->setSpaceKeyHandler([this]() { this->onPopupSpaceKey(); });
        _pRadiusPopup->hide();
    }
}

void SketchDrawEllipse3DGuiCmd::showPopup()
{
    if (!_pXYZPopup || !_pLengthAnglePopup || !_pRadiusPopup)
    {
        this->initializePopups();
    }

    if (_step == static_cast<unsigned int>(Step::SpecifyCenterPnt))
    {
        if (!_pXYZPopup) return;
        _pXYZPopup->setValues(
            _hoverPopupState.point.x(),
            _hoverPopupState.point.y(),
            _hoverPopupState.point.z());
        _pXYZPopup->showAtGlobal(QCursor::pos());
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyAxisEndPoint))
    {
        if (!_pLengthAnglePopup) return;
        _pLengthAnglePopup->setValues(
            _hoverPopupState.majorLength,
            _hoverPopupState.majorAngleDeg);
        _pLengthAnglePopup->showAtGlobal(QCursor::pos());
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyOtherRadius))
    {
        if (!_pRadiusPopup) return;
        _pRadiusPopup->setValue(_hoverPopupState.otherRadius);
        _pRadiusPopup->showAtGlobal(QCursor::pos());
    }
}

void SketchDrawEllipse3DGuiCmd::hidePopup()
{
    if (_pXYZPopup && _pXYZPopup->isVisible())
    {
        _pXYZPopup->hide();
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

void SketchDrawEllipse3DGuiCmd::tryShowPopupOnHover(double time)
{
    if (_step != static_cast<unsigned int>(Step::SpecifyCenterPnt) &&
        _step != static_cast<unsigned int>(Step::SpecifyAxisEndPoint) &&
        _step != static_cast<unsigned int>(Step::SpecifyOtherRadius))
    {
        return;
    }
    if (_hoverPopupState.lastMouseMoveTime < 0.0)
    {
        return;
    }
    if ((_pXYZPopup && _pXYZPopup->isVisible()) ||
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

void SketchDrawEllipse3DGuiCmd::onPopupEnterKey()
{
    const wy3d::SketchPlane& plane = this->getWorkingPlane();

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
    else if (_step == static_cast<unsigned int>(Step::SpecifyAxisEndPoint))
    {
        if (!_pLengthAnglePopup)
        {
            return;
        }

        double length(0.0);
        double angleDeg(0.0);
        if (!parseDoubleText(_pLengthAnglePopup->getRow1Text(), length) ||
            !parseDoubleText(_pLengthAnglePopup->getRow2Text(), angleDeg))
        {
            return;
        }
        if (length < wy3d::kMinValue)
        {
            return;
        }
        _majorRadius = length;
        _majorAxisDir = this->planeDirFromAngle(plane, wy3d::degreesToRadians(angleDeg));
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyOtherRadius))
    {
        if (!_pRadiusPopup)
        {
            return;
        }

        double radius(0.0);
        if (!parseDoubleText(_pRadiusPopup->getRowText(), radius))
        {
            return;
        }
        if (radius < wy3d::kMinValue)
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

void SketchDrawEllipse3DGuiCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void SketchDrawEllipse3DGuiCmd::onPopupSpaceKey()
{
    this->onSpaceKey();
    this->hidePopup();
}

void SketchDrawEllipse3DGuiCmd::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX)
        return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}

std::pair<wy::Vector3, bool> SketchDrawEllipse3DGuiCmd::computePoint3d(double x, double y)
{
    auto ret = this->computePosition3d(x, y, this->getWorkingPlane(), this->getSnapExcludeIds(), true);
    if (ret.second)
    {
        return std::make_pair(ret.second->getPosition(), true);
    }
    return std::make_pair(ret.first, false);
}

std::set<wydb::ElementId> SketchDrawEllipse3DGuiCmd::getSnapExcludeIds() const
{
    std::set<wydb::ElementId> snapExcludeIds;
    // 只排除正在预览的图元,已提交的3D草图图元可作捕捉源(与2D草绘行为一致)
    if (_pMakeSketchEllipse3D)
    {
        _pMakeSketchEllipse3D->collectElements(snapExcludeIds);
    }
    return snapExcludeIds;
}

void MakeSketchEllipse3D::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pSketchEllipse3D) idSet.insert(_pSketchEllipse3D->getId());
}

bool MakeSketchEllipse3D::init(const wy::Vector3& centerPnt, const wy::Vector3& normal, const wy::Vector3& xDir,
    double majorRadius, double radiusRatio, wydb::ElementId sketch3dId)
{
    if (!_pDb || !_pTopTrans || _pSketchEllipse3D || _isFinished)
    {
        return false;
    }

    // 创建SketchEllipse3D
    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    wy3d::Sketch3D* pSketch3D = nullptr;
    wy3d::SketchEllipse3D* pSketchEllipse3D = nullptr;
    wydb::Element* pSketchElem = pTrans->getElementForWrite(sketch3dId);
    if (!pSketchElem) goto ABORT_TRANS;
    pSketch3D = wy3d::Sketch3D::cast(pSketchElem);
    if (!pSketch3D) goto ABORT_TRANS;

    if (wy::ErrorStatus::Ok != wy3d::SketchEllipse3D::create(pTrans, centerPnt, normal, xDir,
        majorRadius, radiusRatio, pSketchEllipse3D) || !pSketchEllipse3D)
    {
        goto ABORT_TRANS;
    }
    _pSketchEllipse3D = pSketchEllipse3D;
    if (wy::ErrorStatus::Ok != pSketch3D->addEntity(pSketchEllipse3D))
    {
        goto ABORT_TRANS;
    }
    _pDb->getTransactionManager()->endTransaction();
    _pSketchEllipse3D = pSketchEllipse3D;
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    _pSketchEllipse3D = nullptr;
    return false;
}

bool MakeSketchEllipse3D::update(const wy::Vector3& normal, const wy::Vector3& xDir,
    double majorRadius, double radiusRatio)
{
    if (!_pDb || !_pTopTrans || !_pSketchEllipse3D || _isFinished)
    {
        return false;
    }
    if (majorRadius < wy3d::kMinValue || radiusRatio < 1e-5 || radiusRatio > 1.0)
    {
        return false;
    }

    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    {
        _pSketchEllipse3D->upgradeForWrite();
        if (wy::ErrorStatus::Ok != _pSketchEllipse3D->setPlane(normal, xDir) ||
            wy::ErrorStatus::Ok != _pSketchEllipse3D->setMajorRadius(majorRadius) ||
            wy::ErrorStatus::Ok != _pSketchEllipse3D->setRadiusRatio(radiusRatio))
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

bool MakeSketchEllipse3D::updatePlane(const wy::Vector3& normal, const wy::Vector3& xDir)
{
    if (!_pSketchEllipse3D)
    {
        return false;
    }
    // 半轴取实体现值,避免命令侧缓存过期导致切平面无效
    return this->update(normal, xDir, _pSketchEllipse3D->getMajorRadius(), _pSketchEllipse3D->getRadiusRatio());
}
