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

#include "RotateGuiCmd.h"

#include <QCoreApplication>
#include <QCursor>
#include <QString>
#include <cmath>

#include <gp_Trsf.hxx>
#include <gp_Ax1.hxx>
#include <gp_Quaternion.hxx>
#include <gp_Mat.hxx>

#include <wyVector2.h>
#include <wyVector3.h>
#include <wy3dMath.h>
#include <wy3dPrimitive.h>
#include <wy3dSketch.h>
#include <wy3dSketchEntity.h>
#include <wy3dSketchEntity.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>
#include <wy3dRotate.h>
#include <wy3dSketchSpline.h>
#include <wy3dSketchSpline.h>

#include "application/Application.h"
#include "scene/Scene.h"
#include "view/OsgView.h"
#include "commands/OsgGuiEventDispatcher.h"
#include "scene/nodes/ElementNode.h"
#include "utils/MathUtils.h"
#include "environments/sketch/SketchEnvironment.h"
#include "select/filters/CommonSelFilters.h"
#include "select/SketchPlaneSelFilter.h"
#include "utils/GuiCommandUtil.h"
#include "snap/SnapObject.h"
#include "snap/SketchSnapSystem.h"
#include "commands/sketch/dialogs/GuiCmdHoverInputPopup.h"
#include "widgets/frame/MainWindow.h"
#include "commands/edit/MoveRotateGuiCmdUtil.h"

static bool parseDoubleText(const QString& text, double& value)
{
    bool ok(false);
    value = text.trimmed().toDouble(&ok);
    return ok;
}

constexpr double kHoverPopupDelaySeconds = 0.45;


RotateGuiCmd::RotateGuiCmd()
    : OsgGuiCommand(),
    _step(Step::Undefined), _sels(), _pickWorkPlnOption(), _workPln(),
    _basePnt(), _basePnt2(), _baseLineEndPnt(), _baseLineEndPnt2(),
    _rotateAngle(0.0),
    _pXYPopup(nullptr),
    _pLengthAnglePopup(nullptr),
    _pAnglePopup(nullptr),
    _hoverPopupState()
{
    _options.pointSelect = true;
    _options.boxSelect = true;
}

RotateGuiCmd::~RotateGuiCmd()
{
}

wyap::CmdExecution::StartResult RotateGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = __baseClass::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    // 建模环境
    onStart_EnvSpecific();

    // 初始化
    _pRotateTransient = std::make_shared<RotateTransient>();
    _pRotateTransient->hide();
    if (Application::instance().getSelManager()->getSelections().isEmpty())
    {
        this->gotoStep(Step::SelectElements);
    }
    else
    {
        this->finishStep(Step::SelectElements);
    }

    return wyap::CmdExecution::StartResult::Succeeded;
}
void RotateGuiCmd::onEnd()
{
    this->hidePopup();

    // 取消旋转
    if (_pRotateElements)
    {
        _pRotateElements = nullptr;
    }

    __baseClass::onEnd();

    // 建模环境
    cleanupEnvSpecific();

    // 清空工作平面捕捉对象
    if (_pWorkPlnSnapSystem) _pWorkPlnSnapSystem->clearSnapResult();

}
void RotateGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    this->hidePopup();

    // 取消移动
    if (_pRotateElements)
    {
        _pRotateElements = nullptr;
    }

    __baseClass::onAbort(cause);

    // 建模环境
    cleanupEnvSpecific();

    // 清空工作平面捕捉对象
    if (_pWorkPlnSnapSystem) _pWorkPlnSnapSystem->clearSnapResult();
}

void RotateGuiCmd::reset()
{
    this->hidePopup();

    if (_pRotateElements) _pRotateElements = nullptr;
    if (_pRotateTransient) _pRotateTransient->hide();

    _step = Step::Undefined;
    _sels.clear();
    _basePnt2.set(0.0, 0.0);
    _basePnt.set(0.0, 0.0, 0.0);
    _baseLineEndPnt2.set(0.0, 0.0);
    _baseLineEndPnt.set(0.0, 0.0, 0.0);
    _rotateAngle = 0.0;
    _rid.isCCW = true;
    _rid.lastRadian = 0.0;
    _pSnapContext = nullptr;
    _hoverPopupState.resetValue();

    this->gotoStep(Step::SelectElements);
}

bool RotateGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SelectElements:
    {
        // 选择集
        const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
        if (ss.isEmpty())
        {
            assert(false);
            return false;
        }
        _sels = ss;

        // 下一步
        gotoNextStepAfterSelectElements();
        return true;
    }
    break;

    case Step::SelectWorkingPlane:
    {
        // 工作平面
        if (!_pWorkPlnPreview)
        {
            assert(false);
            return false;
        }
        if (!GuiCommandUtil::getWorkingPlane(_pWorkPlnPreview->getSelection(), _workPln))
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }
        _pWorkPlnPreview = nullptr;

        // 添加工作平面坐标原点为常驻捕捉对象
        wyap::SnapSystem* pSnapSys = Application::instance().getSnapSystem();
        assert(pSnapSys);
        pSnapSys->beginChange();
        {
            _pWorkPlnOriginSnapObject = std::make_shared<SnapCoordinatePoint>(_workPln.getOrigin());
            pSnapSys->addResidentSnapObject(_pWorkPlnOriginSnapObject);
        }
        pSnapSys->endChange();

        // 显示工作平面坐标系
        if (Scene* pScene = Application::instance().getActiveScene())
        {
            pScene->showSketchCSYS(_workPln);
        }
        else
        {
            assert(false);
        }

        // 工作平面捕捉系统
        _pWorkPlnSnapSystem = std::make_shared<SketchSnapSystem>(_workPln);

        // 下一步
        this->gotoStep(Step::SpecifyBasePnt);
        return true;
    }
    break;

    case Step::SpecifyBasePnt:
    {
        // 旋转临时对象
        if (_pRotateTransient)
        {
            _pRotateTransient->updateBaseLine(_basePnt, _basePnt);
            _pRotateTransient->showBaseLine();
        }

        // 下一步
        this->gotoStep(Step::SpecifyBaseLineEndPnt);
        return true;
    }
    break;

    case Step::SpecifyBaseLineEndPnt:
    {
        // 旋转
        _pRotateElements = std::make_shared<RotateElements>(this);
        wy3d::SketchPlane plane = getActivePlane();
        if (!_pRotateElements->init(_sels, _basePnt, _basePnt2, plane.getNormal()))
        {
            _pRotateElements = nullptr;
            return false;
        }

        // 旋转临时对象
        if (_pRotateTransient)
        {
            _pRotateTransient->updateBaseLine(_basePnt, _baseLineEndPnt);
            _pRotateTransient->updateRotateLine(_basePnt, _basePnt);
            _pRotateTransient->showRotateLine();
        }

        // 下一步
        this->gotoStep(Step::SpecifyRotateAngle);
        return true;
    }
    break;

    case Step::SpecifyRotateAngle:
    {
        if (!_pRotateElements)
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }

        // 执行旋转
        bool ret = _pRotateElements->perform(_sels, _rotateAngle, getEnvType());
        if (ret)
        {
            _pRotateElements->commit();
        }
        else
        {
            assert(false);
        }
        _pRotateElements = nullptr;

        // 隐藏旋转临时对象
        if (_pRotateTransient) _pRotateTransient->hide();

        // 清空捕捉结果
        Application::instance().getSnapSystem()->clearSnapResult();
        // 清空草图捕捉结果
        if (getSketchSnapSys()) getSketchSnapSys()->clearSnapResult();
        if (_pWorkPlnSnapSystem) _pWorkPlnSnapSystem->clearSnapResult();

        // 结束
        this->requestEnd();
        return ret;
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

void RotateGuiCmd::gotoStep(Step step)
{
    _step = step;
    this->hidePopup();
    _hoverPopupState.resetValue();

    // 清空捕捉结果
    Application::instance().getSnapSystem()->clearSnapResult();
    // 清空草图捕捉结果
    if (getSketchSnapSys()) getSketchSnapSys()->clearSnapResult();
    if (_pWorkPlnSnapSystem) _pWorkPlnSnapSystem->clearSnapResult();

    switch (step)
    {
    case Step::SelectElements:
    {
        // 选择配置项:<1>建模环境实体 <2>草图环境草绘图元
        GuiCmdSelectOptions selOptions;
        selOptions.pointSelect = true;
        selOptions.boxSelect = true;
        selOptions.selectionType = wy3d::SelectionType::Element;
        configureSelectElementOptions(selOptions);
        selOptions.preview = true;
        selOptions.selectMode = SelectMode::Incremental;
        this->configSelect(selOptions);

        // 禁用文本输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("RotateGuiCmd",
            "Select elements to rotate; press Enter or Spacebar to confirm; press Esc to cancel."));
        // 鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);
    }
    break;

    case Step::SelectWorkingPlane:
    {
        // 选择配置项:禁用选择
        GuiCmdSelectOptions selOptions;
        selOptions.pointSelect = false;
        selOptions.boxSelect = false;
        this->configSelect(selOptions);

        // 禁用文本输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("RotateGuiCmd",
            "Select datum plane as working plane."));
        // 鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);
    }
    break;

    case Step::SpecifyBasePnt:
    {
        // 选择配置项:禁用选择
        GuiCmdSelectOptions selOptions;
        selOptions.pointSelect = false;
        selOptions.boxSelect = false;
        this->configSelect(selOptions);

        // 允许文本输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("RotateGuiCmd",
            "Specify the baseline start point; you can directly input the coordinate values."));
        // 鼠标样式
        Application::instance().setCursor(CursorType::Locate);

        // 局部更新草图捕捉系统
        if (getSketchSnapSys())
        {
            getSketchSnapSys()->partiallyUpdate(Application::instance().getActiveDatabase());
        }
        // 草图捕捉上下文
        _pSnapContext = std::make_shared<SketchLocateContext>(wydb::ElementId::kNull);
    }
    break;

    case Step::SpecifyBaseLineEndPnt:
    {
        // 选择配置项:禁用选择
        GuiCmdSelectOptions selOptions;
        selOptions.pointSelect = false;
        selOptions.boxSelect = false;
        this->configSelect(selOptions);

        // 允许文本输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("RotateGuiCmd",
            "Specify the baseline end point; you can directly input length and angle values."));
        // 鼠标样式
        Application::instance().setCursor(CursorType::Locate);

        // 草图捕捉上下文:绘制线
        _pSnapContext = std::make_shared<SketchDrawLineContext>(wydb::ElementId::kNull, _basePnt2);
    }
    break;

    case Step::SpecifyRotateAngle:
    {
        // 选择配置项:禁用选择
        GuiCmdSelectOptions selOptions;
        selOptions.pointSelect = false;
        selOptions.boxSelect = false;
        this->configSelect(selOptions);

        // 允许文本输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("RotateGuiCmd",
            "Specify the rotation angle; you can directly input the value."));
        // 鼠标样式
        Application::instance().setCursor(CursorType::Locate);

        // 草图捕捉上下文:绘制线
        _pSnapContext = std::make_shared<SketchDrawLineContext>(wydb::ElementId::kNull, _basePnt2);

        // added by wangyao 2025.08.26 {
        // 起始旋转角度为0度
        _rotateAngle = 0.0;
        // 默认逆时针
        _rid.isCCW = true;
        _rid.lastRadian = 0.0;
        // }
    }
    break;

    default:
    {
        assert(false);

        GuiCmdSelectOptions selOptions;
        selOptions.pointSelect = false;
        selOptions.boxSelect = false;
        this->configSelect(selOptions);

        Application::instance().getStatusBar()->setTips("");
        Application::instance().setCursor(CursorType::Select);
    }
    break;
    }
}

void RotateGuiCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

void RotateGuiCmd::onMouseMove(const MouseEvent& event)
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
    case Step::SelectElements:
    {
        return;
    }
    break;

    case Step::SelectWorkingPlane:
    {
        wyap::Selection sel = this->pointPick(event.x, event.y, _pickWorkPlnOption);
        if (_pWorkPlnPreview)
        {
            if (sel.getElementId().isNull())
            {
                _pWorkPlnPreview = nullptr;
            }
            else
            {
                if (!_pWorkPlnPreview->isEqual(sel))
                {
                    _pWorkPlnPreview = std::make_shared<SelectPreview>(sel);
                }
            }
        }
        else
        {
            if (!sel.getElementId().isNull())
            {
                _pWorkPlnPreview = std::make_shared<SelectPreview>(sel);
            }
        }
        return;
    }
    break;

    case Step::SpecifyBasePnt:
    {
        const wy3d::SketchPlane& plane = getActivePlane();
        SketchSnapSystem* pSnapSystem = getActiveSnapSystem();
        assert(pSnapSystem);
        wy::Vector2 pnt2d = this->computePosition2d(event.x, event.y, plane, {}, _pSnapContext, pSnapSystem);
        _hoverPopupState.point = pnt2d;
        return;
    }
    break;

    case Step::SpecifyBaseLineEndPnt:
    {
        const wy3d::SketchPlane& plane = getActivePlane();
        SketchSnapSystem* pSnapSystem = getActiveSnapSystem();
        assert(pSnapSystem);
        wy::Vector2 pnt2d = this->computePosition2d(event.x, event.y, plane, {}, _pSnapContext, pSnapSystem);
        wy::Vector2 vec = pnt2d - _basePnt2;
        double length = vec.length();
        double angleRad = wy3d::normalizeRadian(std::atan2(vec.y(), vec.x()));
        double angleDeg = wy3d::radiansToDegrees(angleRad);
        _hoverPopupState.length = length;
        _hoverPopupState.angleDeg = angleDeg;
        {
            if (_pRotateTransient) _pRotateTransient->updateBaseLine(_basePnt, plane.value(pnt2d));
        }
        return;
    }
    break;

    case Step::SpecifyRotateAngle:
    {
        const wy3d::SketchPlane& plane = getActivePlane();
        SketchSnapSystem* pSnapSystem = getActiveSnapSystem();
        assert(pSnapSystem);
        wy::Vector2 endPnt2 = this->computePosition2d(event.x, event.y, plane, {}, _pSnapContext, pSnapSystem);
        if ((endPnt2 - _basePnt2).length() < 1e-5)
        {
            return;
        }
        wy::Vector3 endPnt = plane.value(endPnt2);
        wy::Vector3 baseLineVec = _baseLineEndPnt - _basePnt;
        wy::Vector3 rotateLineVec = endPnt - _basePnt;
        double rotateRadian = MathUtils::getRotateAngle(baseLineVec, rotateLineVec, plane.getNormal());
        double rotateAngle = wy3d::radiansToDegrees(rotateRadian);

        // 判断是否需要重新设置顺逆时针
        if (_rid.lastRadian == 0.0 ||
            rotateRadian == 0.0 ||
            std::fabs(rotateRadian - _rid.lastRadian) > wy3d::PI)
        {
            if (rotateRadian <= wy3d::PI) // 逆时针
            {
                _rid.isCCW = true;
            }
            else
            {
                _rid.isCCW = false;
            }
        }
        _rid.lastRadian = rotateRadian;

        // 用于显示的角度
        double uiRotAngle = rotateAngle;
        if (!_rid.isCCW) // 顺时针
        {
            uiRotAngle = 360.0 - rotateAngle;
        }

        _hoverPopupState.angleDeg = uiRotAngle;
        {
            if (_pRotateTransient) _pRotateTransient->updateRotateLine(_basePnt, endPnt);
            if (_pRotateElements) _pRotateElements->update(rotateRadian);
        }
        return;
    }
    break;

    default:
    {
        assert(false);
        return;
    }
    break;
    }

    return;
}

void RotateGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    this->hidePopup();
    _hoverPopupState.lastMouseX = event.x;
    _hoverPopupState.lastMouseY = event.y;
    _hoverPopupState.lastMouseMoveTime = event.time;

    switch (_step)
    {
    case Step::SelectElements:
    {
        return;
    }
    break;

    case Step::SelectWorkingPlane:
    {
        if (_pWorkPlnPreview && this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }

        return;
    }
    break;

    case Step::SpecifyBasePnt:
    {
        const wy3d::SketchPlane& plane = getActivePlane();
        SketchSnapSystem* pSnapSystem = getActiveSnapSystem();
        assert(pSnapSystem);
        _basePnt2 = this->computePosition2d(event.x, event.y, plane, {}, _pSnapContext, pSnapSystem);
        _basePnt = plane.value(_basePnt2);
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
        return;
    }
    break;

    case Step::SpecifyBaseLineEndPnt:
    {
        const wy3d::SketchPlane& plane = getActivePlane();
        SketchSnapSystem* pSnapSystem = getActiveSnapSystem();
        assert(pSnapSystem);
        wy::Vector2 endPnt2 = this->computePosition2d(event.x, event.y, plane, {}, _pSnapContext, pSnapSystem);
        if ((endPnt2 - _basePnt2).length() < 1e-5)
        {
            return;
        }
        _baseLineEndPnt2 = endPnt2;
        _baseLineEndPnt = plane.value(endPnt2);
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
    }
    break;

    case Step::SpecifyRotateAngle:
    {
        const wy3d::SketchPlane& plane = getActivePlane();
        SketchSnapSystem* pSnapSystem = getActiveSnapSystem();
        assert(pSnapSystem);
        wy::Vector2 pnt2 = this->computePosition2d(event.x, event.y, plane, {}, _pSnapContext, pSnapSystem);
        if ((pnt2 - _basePnt2).length() < 1e-5)
        {
            return;
        }
        wy::Vector3 pnt = plane.value(pnt2);
        wy::Vector3 baseLineVec = _baseLineEndPnt - _basePnt;
        wy::Vector3 rotateLineVec = pnt - _basePnt;
        double rotateRadian = MathUtils::getRotateAngle(baseLineVec, rotateLineVec, plane.getNormal());
        _rotateAngle = rotateRadian;
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
    }
    break;

    default:
    {
        assert(false);
        return;
    }
    break;
    }

    return;
}

void RotateGuiCmd::initializePopups()
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
            QCoreApplication::translate("RotateGuiCmd", "Length"),
            QCoreApplication::translate("RotateGuiCmd", "Angle"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pLengthAnglePopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pLengthAnglePopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pLengthAnglePopup->hide();
    }
    if (!_pAnglePopup)
    {
        _pAnglePopup = std::make_unique<GuiCmdHoverInputPopup1>(
            QCoreApplication::translate("RotateGuiCmd", "Angle"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pAnglePopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pAnglePopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pAnglePopup->hide();
    }
}

void RotateGuiCmd::showPopup()
{
    if (_step != Step::SpecifyBasePnt &&
        _step != Step::SpecifyBaseLineEndPnt &&
        _step != Step::SpecifyRotateAngle)
    {
        return;
    }

    if (!_pXYPopup || !_pLengthAnglePopup || !_pAnglePopup)
    {
        this->initializePopups();
    }

    GuiCmdHoverInputPopupBase* pActivePopup = this->getActivePopup();
    if (!pActivePopup)
    {
        return;
    }

    if (_step == Step::SpecifyRotateAngle)
    {
        if (_pAnglePopup)
        {
            _pAnglePopup->setValue(_hoverPopupState.angleDeg);
        }
    }
    else if (_step == Step::SpecifyBaseLineEndPnt)
    {
        if (_pLengthAnglePopup)
        {
            _pLengthAnglePopup->setValues(_hoverPopupState.length, _hoverPopupState.angleDeg);
        }
    }
    else
    {
        if (_pXYPopup)
        {
            _pXYPopup->setValues(_hoverPopupState.point.x(), _hoverPopupState.point.y());
        }
    }
    pActivePopup->showAtGlobal(QCursor::pos());
}

void RotateGuiCmd::hidePopup()
{
    if (_pXYPopup && _pXYPopup->isVisible())
    {
        _pXYPopup->hide();
    }
    if (_pLengthAnglePopup && _pLengthAnglePopup->isVisible())
    {
        _pLengthAnglePopup->hide();
    }
    if (_pAnglePopup && _pAnglePopup->isVisible())
    {
        _pAnglePopup->hide();
    }
}

GuiCmdHoverInputPopupBase* RotateGuiCmd::getActivePopup() const
{
    if (_step == Step::SpecifyBasePnt)
    {
        return _pXYPopup.get();
    }
    if (_step == Step::SpecifyBaseLineEndPnt)
    {
        return _pLengthAnglePopup.get();
    }
    if (_step == Step::SpecifyRotateAngle)
    {
        return _pAnglePopup.get();
    }
    return nullptr;
}

void RotateGuiCmd::tryShowPopupOnHover(double time)
{
    if (_step != Step::SpecifyBasePnt &&
        _step != Step::SpecifyBaseLineEndPnt &&
        _step != Step::SpecifyRotateAngle)
    {
        return;
    }
    if (_hoverPopupState.lastMouseMoveTime < 0.0)
    {
        return;
    }
    if ((_pXYPopup && _pXYPopup->isVisible()) ||
        (_pLengthAnglePopup && _pLengthAnglePopup->isVisible()) ||
        (_pAnglePopup && _pAnglePopup->isVisible()))
    {
        return;
    }
    if (time - _hoverPopupState.lastMouseMoveTime >= kHoverPopupDelaySeconds)
    {
        this->showPopup();
    }
}

void RotateGuiCmd::onPopupEnterKey()
{
    if (_step == Step::SpecifyBasePnt)
    {
        if (!_pXYPopup)
        {
            return;
        }
        double x(0.0), y(0.0);
        if (!parseDoubleText(_pXYPopup->getRow1Text(), x) ||
            !parseDoubleText(_pXYPopup->getRow2Text(), y))
        {
            return;
        }

        const wy3d::SketchPlane& plane = getActivePlane();
        _basePnt2.set(x, y);
        _basePnt = plane.value(_basePnt2);
    }
    else if (_step == Step::SpecifyBaseLineEndPnt)
    {
        if (!_pLengthAnglePopup)
        {
            return;
        }
        double length(0.0), angleDeg(0.0);
        if (!parseDoubleText(_pLengthAnglePopup->getRow1Text(), length) ||
            !parseDoubleText(_pLengthAnglePopup->getRow2Text(), angleDeg))
        {
            return;
        }

        double angleRad = wy3d::degreesToRadians(angleDeg);
        wy::Vector2 moveVec2(
            length * std::cos(angleRad),
            length * std::sin(angleRad));
        wy::Vector2 baseLineEndPnt2 = _basePnt2 + moveVec2;
        if (moveVec2.length() < 1e-5)
        {
            return;
        }
        const wy3d::SketchPlane& plane = getActivePlane();
        _baseLineEndPnt2 = baseLineEndPnt2;
        _baseLineEndPnt = plane.value(_baseLineEndPnt2);
    }
    else if (_step == Step::SpecifyRotateAngle)
    {
        if (!_pAnglePopup)
        {
            return;
        }
        double angle(0.0);
        if (!parseDoubleText(_pAnglePopup->getRowText(), angle))
        {
            return;
        }

        double radian = wy3d::degreesToRadians(angle);
        radian = wy3d::normalizeRadian(radian);
        if (!_rid.isCCW)
        {
            radian = wy3d::TWO_PI - radian;
        }
        _rotateAngle = radian;
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

void RotateGuiCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void RotateGuiCmd::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX)
        return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}

void RotateGuiCmd::onEnterKey()
{
    if (Step::SelectElements == _step)
    {
        const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
        if (!ss.isEmpty())
        {
            if (this->finishStep(_step))
            {
                this->simulateMouseMoveFromPopup();
            }
        }
    }
}

void RotateGuiCmd::onSpaceKey()
{
    this->onEnterKey();
}

bool RotateGuiCmd::isContextMenuActionVisible_CompleteSelection() const
{
    return Step::SelectElements == _step;
}

void RotateGuiCmd::onContextMenuAction_CompleteSelection()
{
    this->onEnterKey();
}

bool RotateGuiCmd::isContextMenuActionVisible_ClearSelection() const
{
    return Step::SelectElements == _step;
}

void RotateGuiCmd::onContextMenuAction_ClearSelection()
{
    if (Step::SelectElements == _step)
    {
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();
    }
}
