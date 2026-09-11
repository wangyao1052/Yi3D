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

#include "SketchDrawEllipseArc3DGuiCmd.h"

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

// 起点角度预览时的近整圈(整椭圆使用SketchEllipse3D)
static const double kPreviewSweepAngle = wy3d::TWO_PI - wy3d::kMinValue;

static bool parseDoubleText(const QString& text, double& value)
{
    bool ok(false);
    value = text.trimmed().toDouble(&ok);
    return ok;
}

SketchDrawEllipseArc3DGuiCmd::SketchDrawEllipseArc3DGuiCmd() : Sketch3DDrawGuiCmd(),
    _step(0), _centerPnt(), _majorAxisDir(wy::Vector3::kXAxis), _majorRadius(0.0), _otherRadius(0.0),
    _startAngle(0.0), _sweepAngle(0.0), _pMakeSketchEllipseArc3D(nullptr),
    _pXYZPopup(nullptr), _pLengthAnglePopup(nullptr), _pRadiusPopup(nullptr),
    _pStartAnglePopup(nullptr), _pSweepAnglePopup(nullptr), _hoverPopupState(), _pAxisTransient(nullptr)
{
}

SketchDrawEllipseArc3DGuiCmd::~SketchDrawEllipseArc3DGuiCmd()
{
}

wyap::CmdExecution::StartResult SketchDrawEllipseArc3DGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = Sketch3DDrawGuiCmd::onStart();
    if (wyap::CmdExecution::StartResult::Succeeded != ret)
    {
        return ret;
    }

    this->gotoStep(static_cast<unsigned int>(Step::SpecifyCenterPnt));
    return ret;
}

void SketchDrawEllipseArc3DGuiCmd::cleanup()
{
    _step = 0;
    _centerPnt.set(0.0, 0.0, 0.0);
    _majorAxisDir = wy::Vector3::kXAxis;
    _majorRadius = 0.0;
    _otherRadius = 0.0;
    _startAngle = 0.0;
    _sweepAngle = 0.0;
    _pMakeSketchEllipseArc3D = nullptr;
    this->destroyAxisTransient();

    this->hidePopup();
    _hoverPopupState.resetValue();
}

void SketchDrawEllipseArc3DGuiCmd::onEscapeKey()
{
    this->hidePopup();

    if (_step == static_cast<unsigned int>(Step::SpecifyCenterPnt))
    {
        this->requestAbort(AbortCause::UserCancel);
    }
    else if (_step != 0)
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

double SketchDrawEllipseArc3DGuiCmd::currentRatio() const
{
    if (_majorRadius <= wy3d::TOL) return 1.0;
    double ratio = _otherRadius / _majorRadius;
    if (ratio > 1.0) ratio = 1.0;
    if (ratio < 1e-5) ratio = 1e-5;
    return ratio;
}

wy::Vector3 SketchDrawEllipseArc3DGuiCmd::planeDirFromAngle(const wy3d::SketchPlane& plane, double angle) const
{
    return plane.getXDir() * std::cos(angle) + plane.getYDir() * std::sin(angle);
}

double SketchDrawEllipseArc3DGuiCmd::angleFromPlaneDir(const wy3d::SketchPlane& plane, const wy::Vector3& dir) const
{
    return std::atan2(dir.dot(plane.getYDir()), dir.dot(plane.getXDir()));
}

double SketchDrawEllipseArc3DGuiCmd::majorAngle(const wy3d::SketchPlane& plane) const
{
    return this->angleFromPlaneDir(plane, _majorAxisDir);
}

double SketchDrawEllipseArc3DGuiCmd::polarAngleOfPnt(const wy3d::SketchPlane& plane, const wy::Vector3& pnt) const
{
    const wy::Vector2 uv = plane.uv(pnt) - plane.uv(_centerPnt);
    return wy3d::normalizeRadian(std::atan2(uv.y(), uv.x()) - this->majorAngle(plane));
}

bool SketchDrawEllipseArc3DGuiCmd::applyToEntity(const wy3d::SketchPlane& plane, double startAngle, double endAngle)
{
    if (!_pMakeSketchEllipseArc3D)
    {
        return false;
    }
    return _pMakeSketchEllipseArc3D->update(plane.getNormal(), _majorAxisDir, _majorRadius,
        this->currentRatio(), startAngle, endAngle);
}

bool SketchDrawEllipseArc3DGuiCmd::finishStep(unsigned int step)
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
        _pMakeSketchEllipseArc3D = std::make_shared<MakeSketchEllipseArc3D>(this);
        // 退化占位弧:后续各步都靠实时更新它预览
        if (!_pMakeSketchEllipseArc3D->init(_centerPnt, plane.getNormal(), plane.getXDir(),
            wy3d::kMinValue, 1.0, 0.0, kPreviewSweepAngle, _sketch3DInfo.sketch3dId))
        {
            _pMakeSketchEllipseArc3D = nullptr;
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
        // 弹窗输入的长轴立即生效
        if (!this->applyToEntity(this->getWorkingPlane(), 0.0, kPreviewSweepAngle))
        {
            return false;
        }
        this->gotoStep(static_cast<unsigned int>(Step::SpecifyOtherRadius));
        return true;
    }
    break;

    case Step::SpecifyOtherRadius:
    {
        // 另一半轴大于长半轴时互换长短轴(与2D一致,在命令里归一化)
        const wy3d::SketchPlane& plane = this->getWorkingPlane();
        if (_otherRadius > _majorRadius)
        {
            _majorAxisDir = this->planeDirFromAngle(plane, this->majorAngle(plane) + wy3d::PI_2);
            const double majorRadius = _otherRadius;
            _otherRadius = _majorRadius;
            _majorRadius = majorRadius;
        }

        // 建占位弧(起点角先给0),起点角度步再更新预览
        wydb::Database* pDb = Application::instance().getActiveDatabase();
        if (!pDb || !_pMakeSketchEllipseArc3D)
        {
            assert(false);
            return false;
        }
        if (!this->applyToEntity(plane, 0.0, kPreviewSweepAngle))
        {
            return false;
        }

        this->gotoStep(static_cast<unsigned int>(Step::SpecifyStartAngle));
        return true;
    }
    break;

    case Step::SpecifyStartAngle:
    {
        if (!_pMakeSketchEllipseArc3D)
        {
            assert(false);
            return false;
        }
        this->gotoStep(static_cast<unsigned int>(Step::SpecifyEndAngle));
        return true;
    }
    break;

    case Step::SpecifyEndAngle:
    {
        if (!_pMakeSketchEllipseArc3D)
        {
            assert(false);
            return false;
        }
        const wy3d::SketchPlane& plane = this->getWorkingPlane();
        if (_sweepAngle < wy3d::kMinValue)
        {
            return false;
        }
        if (!this->applyToEntity(plane, _startAngle, _startAngle + _sweepAngle))
        {
            return false;
        }

        _pMakeSketchEllipseArc3D->commit();
        _pMakeSketchEllipseArc3D = nullptr;
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

void SketchDrawEllipseArc3DGuiCmd::gotoStep(unsigned int step)
{
    _step = step;
    this->hidePopup();
    _hoverPopupState.resetValue();

    switch (static_cast<Step>(step))
    {
    case Step::SpecifyCenterPnt:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawEllipseArc3DGuiCmd",
            "Specify the center point; you can directly input the coordinate values. "
            "Press Space to switch the drawing plane."));
    }
    break;

    case Step::SpecifyAxisEndPoint:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawEllipseArc3DGuiCmd",
            "Specify the axis vector; you can directly input the values. "
            "Press Space to switch the drawing plane."));
    }
    break;

    case Step::SpecifyOtherRadius:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawEllipseArc3DGuiCmd",
            "Specify the other radius; you can directly input the value. "
            "Press Space to switch the drawing plane."));
    }
    break;

    case Step::SpecifyStartAngle:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawEllipseArc3DGuiCmd",
            "Specify the start angle; you can directly input the value. "
            "Press Space to switch the drawing plane."));
    }
    break;

    case Step::SpecifyEndAngle:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawEllipseArc3DGuiCmd",
            "Specify the sweep angle; you can directly input the value. "
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

void SketchDrawEllipseArc3DGuiCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

void SketchDrawEllipseArc3DGuiCmd::onSpaceKey()
{
    Sketch3DDrawGuiCmd::onSpaceKey();

    if (_step == static_cast<unsigned int>(Step::SpecifyCenterPnt) || _step == 0)
    {
        return;
    }

    // 保中心/半轴/起止角,长轴方向投影到新平面后整体重发(椭圆绕中心刚性转向)
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
    if (!_pMakeSketchEllipseArc3D)
    {
        return;
    }
    // 半轴与起止角取实体现值:1/2/3 步实体里存的是近整圈预览,4/5 步是实际角度
    if (!_pMakeSketchEllipseArc3D->updatePlane(plane.getNormal(), xDir))
    {
        return;
    }

    // 同步悬停显示值(角度相对新平面xDir)
    _hoverPopupState.majorLength = _majorRadius;
    _hoverPopupState.majorAngleDeg = wy3d::radiansToDegrees(this->majorAngle(plane));
    _hoverPopupState.otherRadius = _otherRadius;
    _hoverPopupState.startAngleDeg = wy3d::radiansToDegrees(_startAngle);
    _hoverPopupState.sweepAngleDeg = wy3d::radiansToDegrees(this->currentSweepForPreview());
    this->hidePopup();
}

void SketchDrawEllipseArc3DGuiCmd::onMouseMove(const MouseEvent& event)
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

    if (_step == static_cast<unsigned int>(Step::SpecifyAxisEndPoint))
    {
        const wy::Vector2 uv = plane.uv(ret.first) - plane.uv(_centerPnt);
        const double majorRadius = uv.length();
        const double angle = std::atan2(uv.y(), uv.x());
        _hoverPopupState.majorLength = majorRadius;
        _hoverPopupState.majorAngleDeg = wy3d::radiansToDegrees(angle);
        this->updateAxisTransient(ret.first);
        if (_pMakeSketchEllipseArc3D && majorRadius >= wy3d::kMinValue)
        {
            _majorRadius = majorRadius; // 切平面时要用它做长轴方向投影
            _majorAxisDir = this->planeDirFromAngle(plane, angle);
            _pMakeSketchEllipseArc3D->update(plane.getNormal(), _majorAxisDir,
                majorRadius, this->currentRatio(), 0.0, kPreviewSweepAngle);
        }
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyOtherRadius))
    {
        const double otherRadius = (plane.uv(ret.first) - plane.uv(_centerPnt)).length();
        _hoverPopupState.otherRadius = otherRadius;
        _otherRadius = otherRadius; // 预览与提交都以它为准
        this->updateAxisTransient(ret.first);
        if (_pMakeSketchEllipseArc3D && otherRadius >= wy3d::kMinValue && _majorRadius > wy3d::TOL)
        {
            this->applyToEntity(plane, 0.0, kPreviewSweepAngle);
        }
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyStartAngle))
    {
        // 起点角度相对长轴方向,预览近整圈
        _startAngle = this->polarAngleOfPnt(plane, ret.first);
        _hoverPopupState.startAngleDeg = wy3d::radiansToDegrees(_startAngle);
        _hoverPopupState.sweepAngleDeg = wy3d::radiansToDegrees(kPreviewSweepAngle);
        _hoverPopupState.otherRadius = _otherRadius;
        this->updateAxisTransient(ret.first);
        this->applyToEntity(plane, _startAngle, _startAngle + kPreviewSweepAngle);
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyEndAngle))
    {
        _sweepAngle = wy3d::normalizeRadian(this->polarAngleOfPnt(plane, ret.first) - _startAngle);
        _hoverPopupState.startAngleDeg = wy3d::radiansToDegrees(_startAngle);
        _hoverPopupState.sweepAngleDeg = wy3d::radiansToDegrees(_sweepAngle);
        if (_sweepAngle >= wy3d::kMinValue)
        {
            this->applyToEntity(plane, _startAngle, _startAngle + _sweepAngle);
        }
    }
    else
    {
        assert(false);
    }

    return;
}

void SketchDrawEllipseArc3DGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    this->hidePopup();
    _hoverPopupState.lastMouseX = event.x;
    _hoverPopupState.lastMouseY = event.y;
    _hoverPopupState.lastMouseMoveTime = event.time;

    const wy3d::SketchPlane& plane = this->getWorkingPlane();

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
        const wy::Vector2 uv = plane.uv(this->computePoint3d(event.x, event.y).first) - plane.uv(_centerPnt);
        _majorRadius = uv.length();
        if (_majorRadius < wy3d::kMinValue || _majorRadius > wy3d::kMaxValue)
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
    else if (_step == static_cast<unsigned int>(Step::SpecifyStartAngle))
    {
        _startAngle = this->polarAngleOfPnt(plane, this->computePoint3d(event.x, event.y).first);
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyEndAngle))
    {
        _sweepAngle = wy3d::normalizeRadian(
            this->polarAngleOfPnt(plane, this->computePoint3d(event.x, event.y).first) - _startAngle);
        if (_sweepAngle < wy3d::kMinValue)
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

double SketchDrawEllipseArc3DGuiCmd::currentSweepForPreview() const
{
    // 只有最后的扫掠角度步用实际扫角,之前各步都预览近整圈
    if (_step == static_cast<unsigned int>(Step::SpecifyEndAngle))
    {
        return _sweepAngle;
    }
    return kPreviewSweepAngle;
}

void SketchDrawEllipseArc3DGuiCmd::updateAxisTransient(const wy::Vector3& endPnt)
{
    if (!_pAxisTransient)
    {
        return;
    }
    _pAxisTransient->update(_centerPnt, endPnt);
}

void SketchDrawEllipseArc3DGuiCmd::destroyAxisTransient()
{
    _pAxisTransient = nullptr;
}

void SketchDrawEllipseArc3DGuiCmd::initializePopups()
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
            QCoreApplication::translate("SketchDrawEllipseArc3DGuiCmd", "Length"),
            QCoreApplication::translate("SketchDrawEllipseArc3DGuiCmd", "Angle"),
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
            QCoreApplication::translate("SketchDrawEllipseArc3DGuiCmd", "Radius"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pRadiusPopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pRadiusPopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pRadiusPopup->setSpaceKeyHandler([this]() { this->onPopupSpaceKey(); });
        _pRadiusPopup->hide();
    }

    if (!_pStartAnglePopup)
    {
        _pStartAnglePopup = std::make_unique<GuiCmdHoverInputPopup1>(
            QCoreApplication::translate("SketchDrawEllipseArc3DGuiCmd", "StartAngle"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pStartAnglePopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pStartAnglePopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pStartAnglePopup->setSpaceKeyHandler([this]() { this->onPopupSpaceKey(); });
        _pStartAnglePopup->hide();
    }

    if (!_pSweepAnglePopup)
    {
        _pSweepAnglePopup = std::make_unique<GuiCmdHoverInputPopup1>(
            QCoreApplication::translate("SketchDrawEllipseArc3DGuiCmd", "SweepAngle"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pSweepAnglePopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pSweepAnglePopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pSweepAnglePopup->setSpaceKeyHandler([this]() { this->onPopupSpaceKey(); });
        _pSweepAnglePopup->hide();
    }
}

void SketchDrawEllipseArc3DGuiCmd::showPopup()
{
    if (!_pXYZPopup || !_pLengthAnglePopup || !_pRadiusPopup || !_pStartAnglePopup || !_pSweepAnglePopup)
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
    else if (_step == static_cast<unsigned int>(Step::SpecifyStartAngle))
    {
        if (!_pStartAnglePopup) return;
        _pStartAnglePopup->setValue(_hoverPopupState.startAngleDeg);
        _pStartAnglePopup->showAtGlobal(QCursor::pos());
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyEndAngle))
    {
        if (!_pSweepAnglePopup) return;
        _pSweepAnglePopup->setValue(_hoverPopupState.sweepAngleDeg);
        _pSweepAnglePopup->showAtGlobal(QCursor::pos());
    }
}

void SketchDrawEllipseArc3DGuiCmd::hidePopup()
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
    if (_pStartAnglePopup && _pStartAnglePopup->isVisible())
    {
        _pStartAnglePopup->hide();
    }
    if (_pSweepAnglePopup && _pSweepAnglePopup->isVisible())
    {
        _pSweepAnglePopup->hide();
    }
}

void SketchDrawEllipseArc3DGuiCmd::tryShowPopupOnHover(double time)
{
    if (_step == 0 || _step > static_cast<unsigned int>(Step::SpecifyEndAngle))
    {
        return;
    }
    if (_hoverPopupState.lastMouseMoveTime < 0.0)
    {
        return;
    }
    if ((_pXYZPopup && _pXYZPopup->isVisible()) ||
        (_pLengthAnglePopup && _pLengthAnglePopup->isVisible()) ||
        (_pRadiusPopup && _pRadiusPopup->isVisible()) ||
        (_pStartAnglePopup && _pStartAnglePopup->isVisible()) ||
        (_pSweepAnglePopup && _pSweepAnglePopup->isVisible()))
    {
        return;
    }
    if (time - _hoverPopupState.lastMouseMoveTime >= 0.45)
    {
        this->showPopup();
    }
}

void SketchDrawEllipseArc3DGuiCmd::onPopupEnterKey()
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
        if (length < wy3d::kMinValue || length > wy3d::kMaxValue)
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
    else if (_step == static_cast<unsigned int>(Step::SpecifyStartAngle))
    {
        if (!_pStartAnglePopup)
        {
            return;
        }

        double angleDeg(0.0);
        if (!parseDoubleText(_pStartAnglePopup->getRowText(), angleDeg))
        {
            return;
        }
        _startAngle = wy3d::normalizeRadian(wy3d::degreesToRadians(angleDeg));
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyEndAngle))
    {
        if (!_pSweepAnglePopup)
        {
            return;
        }

        double angleDeg(0.0);
        if (!parseDoubleText(_pSweepAnglePopup->getRowText(), angleDeg))
        {
            return;
        }
        double sweep = wy3d::degreesToRadians(angleDeg);
        while (sweep >= wy3d::TWO_PI)
        {
            sweep -= wy3d::TWO_PI;
        }
        _sweepAngle = sweep;
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

void SketchDrawEllipseArc3DGuiCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void SketchDrawEllipseArc3DGuiCmd::onPopupSpaceKey()
{
    this->onSpaceKey();
    this->hidePopup();
}

void SketchDrawEllipseArc3DGuiCmd::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX)
        return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}

std::pair<wy::Vector3, bool> SketchDrawEllipseArc3DGuiCmd::computePoint3d(double x, double y)
{
    auto ret = this->computePosition3d(x, y, this->getWorkingPlane(), this->getSnapExcludeIds(), true);
    if (ret.second)
    {
        return std::make_pair(ret.second->getPosition(), true);
    }
    return std::make_pair(ret.first, false);
}

std::set<wydb::ElementId> SketchDrawEllipseArc3DGuiCmd::getSnapExcludeIds() const
{
    std::set<wydb::ElementId> snapExcludeIds;
    // 只排除正在预览的图元,已提交的3D草图图元可作捕捉源(与2D草绘行为一致)
    if (_pMakeSketchEllipseArc3D)
    {
        _pMakeSketchEllipseArc3D->collectElements(snapExcludeIds);
    }
    return snapExcludeIds;
}

void MakeSketchEllipseArc3D::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pSketchEllipseArc3D) idSet.insert(_pSketchEllipseArc3D->getId());
}

bool MakeSketchEllipseArc3D::init(const wy::Vector3& centerPnt, const wy::Vector3& normal, const wy::Vector3& xDir,
    double majorRadius, double radiusRatio, double startAngle, double endAngle, wydb::ElementId sketch3dId)
{
    if (!_pDb || !_pTopTrans || _pSketchEllipseArc3D || _isFinished)
    {
        return false;
    }

    // 创建SketchEllipseArc3D
    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    wy3d::Sketch3D* pSketch3D = nullptr;
    wy3d::SketchEllipseArc3D* pSketchEllipseArc3D = nullptr;
    wydb::Element* pSketchElem = pTrans->getElementForWrite(sketch3dId);
    if (!pSketchElem) goto ABORT_TRANS;
    pSketch3D = wy3d::Sketch3D::cast(pSketchElem);
    if (!pSketch3D) goto ABORT_TRANS;

    if (wy::ErrorStatus::Ok != wy3d::SketchEllipseArc3D::create(pTrans, centerPnt, normal, xDir,
        majorRadius, radiusRatio, startAngle, endAngle, pSketchEllipseArc3D) || !pSketchEllipseArc3D)
    {
        goto ABORT_TRANS;
    }
    _pSketchEllipseArc3D = pSketchEllipseArc3D;
    if (wy::ErrorStatus::Ok != pSketch3D->addEntity(pSketchEllipseArc3D))
    {
        goto ABORT_TRANS;
    }
    _pDb->getTransactionManager()->endTransaction();
    _pSketchEllipseArc3D = pSketchEllipseArc3D;
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    _pSketchEllipseArc3D = nullptr;
    return false;
}

bool MakeSketchEllipseArc3D::update(const wy::Vector3& normal, const wy::Vector3& xDir, double majorRadius,
    double radiusRatio, double startAngle, double endAngle)
{
    if (!_pDb || !_pTopTrans || !_pSketchEllipseArc3D || _isFinished)
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
        _pSketchEllipseArc3D->upgradeForWrite();
        if (wy::ErrorStatus::Ok != _pSketchEllipseArc3D->setPlane(normal, xDir) ||
            wy::ErrorStatus::Ok != _pSketchEllipseArc3D->setMajorRadius(majorRadius) ||
            wy::ErrorStatus::Ok != _pSketchEllipseArc3D->setRadiusRatio(radiusRatio) ||
            wy::ErrorStatus::Ok != _pSketchEllipseArc3D->setStartAngle(startAngle) ||
            wy::ErrorStatus::Ok != _pSketchEllipseArc3D->setEndAngle(endAngle))
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

bool MakeSketchEllipseArc3D::updatePlane(const wy::Vector3& normal, const wy::Vector3& xDir)
{
    if (!_pSketchEllipseArc3D)
    {
        return false;
    }
    // 半轴与起止角取实体现值,避免命令侧缓存过期导致切平面无效
    return this->update(normal, xDir, _pSketchEllipseArc3D->getMajorRadius(), _pSketchEllipseArc3D->getRadiusRatio(),
        _pSketchEllipseArc3D->getStartAngle(), _pSketchEllipseArc3D->getEndAngle());
}
