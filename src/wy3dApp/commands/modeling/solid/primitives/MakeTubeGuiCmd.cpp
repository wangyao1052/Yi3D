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

#include "commands/modeling/solid/primitives/MakeTubeGuiCmd.h"

#include <QCoreApplication>
#include <QCursor>
#include <QString>
#include <cmath>
#include <osg/LineSegment>
#include <osg/ref_ptr>

#include <wyVector2.h>
#include <wyVector3.h>
#include <wy3dMath.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapDocManager.h>
#include <wyapDocument.h>
#include <wy3dBox.h>
#include <wy3dCylinder.h>
#include <wy3dSphere.h>
#include <wy3dImpl.h>

#include "application/Application.h"
#include "commands/sketch/dialogs/GuiCmdHoverInputPopup.h"
#include "scene/Scene.h"
#include "commands/transient/SketchBasicTransient.h"
#include "utils/MathUtils.h"
#include "widgets/frame/MainWindow.h"

static bool parseDoubleText(const QString& text, double& value)
{
    bool ok(false);
    value = text.trimmed().toDouble(&ok);
    return ok;
}

constexpr double kHoverPopupDelaySeconds = 0.45;


MakeTubeGuiCmd::MakeTubeGuiCmd() : MakePrimitiveGuiCmd(),
    _uv1(), _uv2(), _uv3(), _height(0.0),
    _pXYPopup(nullptr),
    _pOuterRadiusPopup(nullptr),
    _pInnerRadiusPopup(nullptr),
    _pHeightPopup(nullptr),
    _hoverPopupState()
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

MakeTubeGuiCmd::~MakeTubeGuiCmd()
{
}
void MakeTubeGuiCmd::onEnd()
{
    __baseClass::onEnd();

    // 放弃当前绘制的Tube
    if (_pMakeTube)
    {
        _pMakeTube = nullptr;
    }

}
void MakeTubeGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    __baseClass::onAbort(cause);

    // 放弃当前绘制的Tube
    if (_pMakeTube)
    {
        _pMakeTube = nullptr;
    }

}

void MakeTubeGuiCmd::reset()
{
    __baseClass::reset();
    this->cleanup();
}

void MakeTubeGuiCmd::cleanup()
{
    __baseClass::cleanup();
    this->hidePopup();

    _uv1.set(0.0, 0.0);
    _uv2.set(0.0, 0.0);
    _uv3.set(0.0, 0.0);
    _height = 0.0;

    _pMakeTube = nullptr;
    _pCircleTransientOuter = nullptr;
    _pCircleTransientInner = nullptr;
    _hoverPopupState.resetValue();
}

bool MakeTubeGuiCmd::finishStep(unsigned int step)
{
    switch (step)
    {
    case Step::SpecifyWorkingPlane: // 工作平面
    {
        return MakePrimitiveGuiCmd::finishStep(step);
    }
    break;

    case Step::SpecifyPnt1: // 确定底面中心
    {
        // 绘制外圆
        _pCircleTransientOuter = std::make_shared<SketchCircleTransient>(_workPln);
        _pCircleTransientOuter->update(_uv1, 0.0f);

        // next step
        this->gotoStep(Step::SpecifyPnt2);
        return true;
    }
    break;

    case Step::SpecifyPnt2: // 确定外径
    {
        // 校验半径
        double radius = (_uv2 - _uv1).length();
        if (radius < wy3d::kMinValue)
        {
            return false;
        }
        
        // 绘制内圆
        _pCircleTransientInner = std::make_shared<SketchCircleTransient>(_workPln);
        _pCircleTransientInner->update(_uv1, 0.0f);

        // next step
        this->gotoStep(Step::SpecifyPnt3);
        return true;
    }
    break;

    case Step::SpecifyPnt3: // 确定内径
    {
        // 创建MakeTube
        double outerRadius = (_uv2 - _uv1).length();
        double innerRadius = (_uv3 - _uv1).length();
        if (innerRadius > outerRadius)
        {
            _uv3 = _uv1 + wy::Vector2(outerRadius, 0.0);
        }
        _pMakeTube = std::make_shared<MakeTube>(this);
        if (!_pMakeTube->init(_workPln, _uv1, _uv2, _uv3)) // 半径过小时会返回false(比如输入0)
        {
            _pMakeTube = nullptr;
            return false;
        }
        _pMakeTube->collectElements(_excludeIds);

        // 销毁圆
        _pCircleTransientOuter = nullptr;
        _pCircleTransientInner = nullptr;

        // next step
        this->gotoStep(Step::SpecifyPnt4);
        return true;
    }
    break;

    case Step::SpecifyPnt4: // 确定高度
    {
        // 更新高度
        if (_pMakeTube)
        {
            if (!_pMakeTube->update(_height)) // 高度过小时会返回false(比如输入0)
            {
                return false;
            }

            // 提交
            _pMakeTube->commit();
            _pMakeTube = nullptr;
        }

        // 退出
        this->requestEnd();
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

void MakeTubeGuiCmd::gotoStepImpl(unsigned int step)
{
    this->hidePopup();
    _hoverPopupState.resetValue();

    switch (step)
    {
    case Step::SpecifyWorkingPlane: // 工作平面
    {
        return MakePrimitiveGuiCmd::gotoStepImpl(step);
    }
    break;

    case Step::SpecifyPnt1: // 确定底面中心
    {
        // 允许输入
        // 提示
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("MakeTubeGuiCmd",
            "Specify the bottom center point; you can directly input the coordinate values."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::Locate);
    }
    break;

    case Step::SpecifyPnt2: // 确定外径
    {
        // 允许输入
        // 提示
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("MakeTubeGuiCmd",
            "Specify the outer radius; you can directly input the value."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::Locate);
    }
    break;

    case Step::SpecifyPnt3: // 确定内径
    {
        // 允许输入
        // 提示
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("MakeTubeGuiCmd",
            "Specify the inner radius; you can directly input the value."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::Locate);
    }
    break;

    case Step::SpecifyPnt4: // 确定高度
    {
        // 允许输入
        // 提示
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("MakeTubeGuiCmd",
            "Specify the height; you can directly input the value."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::Locate);
    }
    break;

    default:
    {
        assert(false);
        Application::instance().getStatusBar()->setTips("");
        assert(false);
    }
    break;
    }
}

void MakeTubeGuiCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

void MakeTubeGuiCmd::onMouseMove(const MouseEvent& event)
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
    case Step::SpecifyWorkingPlane: // 工作平面
    {
        return MakePrimitiveGuiCmd::onMouseMove(event);
    }
    break;

    case Step::SpecifyPnt1: // 确定底面中心
    {
        wy::Vector3 pnt = this->computePosition3d(event.x, event.y, _workPln, _excludeIds).first;
        wy::Vector2 uv = _workPln.uv(pnt);
        _hoverPopupState.point = uv;
        return;
    }
    break;

    case Step::SpecifyPnt2: // 确定外径
    {
        wy::Vector3 pnt = this->computePosition3d(event.x, event.y, _workPln, _excludeIds).first;
        wy::Vector2 uv = _workPln.uv(pnt);
        double radius = (uv - _uv1).length();
        _hoverPopupState.outerRadius = radius;
        {
            if (_pCircleTransientOuter) _pCircleTransientOuter->update(_uv1, radius);
        }
        return;
    }
    break;

    case Step::SpecifyPnt3: // 确定内径
    {
        wy::Vector3 pnt = this->computePosition3d(event.x, event.y, _workPln, _excludeIds).first;
        wy::Vector2 uv = _workPln.uv(pnt);
        double radius = (uv - _uv1).length();
        double outerRadius = (_uv2 - _uv1).length();
        if (outerRadius > 0.0 && radius > outerRadius)
        {
            radius = outerRadius;
        }
        _hoverPopupState.innerRadius = radius;
        {
            if (_pCircleTransientInner) _pCircleTransientInner->update(_uv1, radius);
        }
        return;
    }
    break;

    case Step::SpecifyPnt4: // 确定高度
    {
        double height(0.0);
        if (this->computeHeight2(event.x, event.y, _workPln, _uv3, _excludeIds, height)) // height可以小于0
        {
            _hoverPopupState.heightSign = height < 0.0 ? -1 : 1;
            _hoverPopupState.height = std::fabs(height);
            {
                if (_pMakeTube) _pMakeTube->update(height);
            }
        }
        else
        {
            assert(false);
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

void MakeTubeGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    this->hidePopup();
    _hoverPopupState.lastMouseX = event.x;
    _hoverPopupState.lastMouseY = event.y;
    _hoverPopupState.lastMouseMoveTime = event.time;

    switch (_step)
    {
    case Step::SpecifyWorkingPlane: // 工作平面
    {
        unsigned int prevStep = _step;
    MakePrimitiveGuiCmd::onLeftMouseDown(event);
    if (prevStep != _step)
        this->simulateMouseMoveFromPopup();
    }
    break;

    case Step::SpecifyPnt1: // 确定底面中心
    {
        wy::Vector3 pnt = this->computePosition3d(event.x, event.y, _workPln, _excludeIds).first;
        _uv1 = _workPln.uv(pnt);
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
        return;
    }
    break;

    case Step::SpecifyPnt2: // 确定外径
    {
        wy::Vector3 pnt = this->computePosition3d(event.x, event.y, _workPln, _excludeIds).first;
        _uv2 = _workPln.uv(pnt);
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
        return;
    }
    break;

    case Step::SpecifyPnt3: // 确定内径
    {
        wy::Vector3 pnt = this->computePosition3d(event.x, event.y, _workPln, _excludeIds).first;
        _uv3 = _workPln.uv(pnt);
        double outerRadius = (_uv2 - _uv1).length();
        double innerRadius = (_uv3 - _uv1).length();
        if (outerRadius > 0.0 && innerRadius > outerRadius)
        {
            _uv3 = _uv1 + wy::Vector2(outerRadius, 0.0);
        }
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
        return;
    }
    break;

    case Step::SpecifyPnt4: // 确定高度
    {
        double height(0.0);
        if (this->computeHeight2(event.x, event.y, _workPln, _uv3, _excludeIds, height))
        {
            _hoverPopupState.heightSign = height < 0.0 ? -1 : 1;
            _height = height;
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

void MakeTubeGuiCmd::initializePopups()
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
    if (!_pOuterRadiusPopup)
    {
        _pOuterRadiusPopup = std::make_unique<GuiCmdHoverInputPopup1>(
            QCoreApplication::translate("MakeTubeGuiCmd", "Outer radius"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pOuterRadiusPopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pOuterRadiusPopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pOuterRadiusPopup->hide();
    }
    if (!_pInnerRadiusPopup)
    {
        _pInnerRadiusPopup = std::make_unique<GuiCmdHoverInputPopup1>(
            QCoreApplication::translate("MakeTubeGuiCmd", "Inner radius"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pInnerRadiusPopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pInnerRadiusPopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pInnerRadiusPopup->hide();
    }
    if (!_pHeightPopup)
    {
        _pHeightPopup = std::make_unique<GuiCmdHoverInputPopup1>(
            QCoreApplication::translate("MakeTubeGuiCmd", "Height"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pHeightPopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pHeightPopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pHeightPopup->hide();
    }
}

void MakeTubeGuiCmd::showPopup()
{
    if (_step != Step::SpecifyPnt1 &&
        _step != Step::SpecifyPnt2 &&
        _step != Step::SpecifyPnt3 &&
        _step != Step::SpecifyPnt4)
    {
        return;
    }
    if (!_pXYPopup || !_pOuterRadiusPopup || !_pInnerRadiusPopup || !_pHeightPopup)
    {
        this->initializePopups();
    }

    if (_step == Step::SpecifyPnt1)
    {
        if (!_pXYPopup)
        {
            return;
        }
        _pXYPopup->setValues(_hoverPopupState.point.x(), _hoverPopupState.point.y());
        _pXYPopup->showAtGlobal(QCursor::pos());
    }
    else if (_step == Step::SpecifyPnt2)
    {
        if (!_pOuterRadiusPopup)
        {
            return;
        }
        _pOuterRadiusPopup->setValue(_hoverPopupState.outerRadius);
        _pOuterRadiusPopup->showAtGlobal(QCursor::pos());
    }
    else if (_step == Step::SpecifyPnt3)
    {
        if (!_pInnerRadiusPopup)
        {
            return;
        }
        _pInnerRadiusPopup->setValue(_hoverPopupState.innerRadius);
        _pInnerRadiusPopup->showAtGlobal(QCursor::pos());
    }
    else
    {
        if (!_pHeightPopup)
        {
            return;
        }
        _pHeightPopup->setValue(_hoverPopupState.height);
        _pHeightPopup->showAtGlobal(QCursor::pos());
    }
}

void MakeTubeGuiCmd::hidePopup()
{
    if (_pXYPopup && _pXYPopup->isVisible())
    {
        _pXYPopup->hide();
    }
    if (_pOuterRadiusPopup && _pOuterRadiusPopup->isVisible())
    {
        _pOuterRadiusPopup->hide();
    }
    if (_pInnerRadiusPopup && _pInnerRadiusPopup->isVisible())
    {
        _pInnerRadiusPopup->hide();
    }
    if (_pHeightPopup && _pHeightPopup->isVisible())
    {
        _pHeightPopup->hide();
    }
}

void MakeTubeGuiCmd::tryShowPopupOnHover(double time)
{
    if (_step != Step::SpecifyPnt1 &&
        _step != Step::SpecifyPnt2 &&
        _step != Step::SpecifyPnt3 &&
        _step != Step::SpecifyPnt4)
    {
        return;
    }
    if (_hoverPopupState.lastMouseMoveTime < 0.0)
    {
        return;
    }
    if ((_pXYPopup && _pXYPopup->isVisible()) ||
        (_pOuterRadiusPopup && _pOuterRadiusPopup->isVisible()) ||
        (_pInnerRadiusPopup && _pInnerRadiusPopup->isVisible()) ||
        (_pHeightPopup && _pHeightPopup->isVisible()))
    {
        return;
    }
    if (time - _hoverPopupState.lastMouseMoveTime >= kHoverPopupDelaySeconds)
    {
        this->showPopup();
    }
}

void MakeTubeGuiCmd::onPopupEnterKey()
{
    if (_step == Step::SpecifyPnt1)
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
        _uv1.set(x, y);
    }
    else if (_step == Step::SpecifyPnt2)
    {
        if (!_pOuterRadiusPopup)
        {
            return;
        }
        double radius(0.0);
        if (!parseDoubleText(_pOuterRadiusPopup->getRowText(), radius))
        {
            return;
        }
        radius = std::fabs(radius);
        _uv2 = _uv1 + wy::Vector2(radius, 0.0);
        if (_pCircleTransientOuter) _pCircleTransientOuter->update(_uv1, radius);
    }
    else if (_step == Step::SpecifyPnt3)
    {
        if (!_pInnerRadiusPopup)
        {
            return;
        }
        double radius(0.0);
        if (!parseDoubleText(_pInnerRadiusPopup->getRowText(), radius))
        {
            return;
        }
        radius = std::fabs(radius);
        double outerRadius = (_uv2 - _uv1).length();
        if (outerRadius > 0.0 && radius > outerRadius)
        {
            radius = outerRadius;
        }
        _uv3 = _uv1 + wy::Vector2(radius, 0.0);
    }
    else if (_step == Step::SpecifyPnt4)
    {
        if (!_pHeightPopup)
        {
            return;
        }
        double height(0.0);
        if (!parseDoubleText(_pHeightPopup->getRowText(), height))
        {
            return;
        }
        height = std::fabs(height);
        _height = static_cast<double>(_hoverPopupState.heightSign) * height;
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

void MakeTubeGuiCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void MakeTubeGuiCmd::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX)
        return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}

void MakeTube::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pTube) idSet.insert(_pTube->getId());
}

bool MakeTube::init(const wy3d::SketchPlane& workPln,
    const wy::Vector2& pnt1,
    const wy::Vector2& pnt2,
    const wy::Vector2& pnt3)
{
    if (!_pDb || !_pTopTrans || _pTube || _isFinished)
    {
        return false;
    }

    // 计算半径
    double outerRadius = (pnt2 - pnt1).length();
    if (outerRadius < wy3d::kMinValue || outerRadius > wy3d::kMaxValue)
    {
        return false;
    }
    double innerRadius = (pnt3 - pnt1).length();
    if (innerRadius < wy3d::kMinValue || innerRadius > wy3d::kMaxValue)
    {
        return false;
    }
    if (innerRadius > outerRadius)
    {
        std::swap(outerRadius, innerRadius);
    }
    if (std::fabs(outerRadius - innerRadius) < wy3d::kMinValue)
    {
        return false;
    }

    // 计算旋转欧拉角
    if (!workPln.isValid())
    {
        return false;
    }
    wy::Vector3 rotation = MathUtils::computeEulerZXY(workPln);
   
    // 确定起始坐标
    wy::Vector3 initOrigin = workPln.value(pnt1);

    // 最小高度
    double height(wy3d::kMinValue);

    // 创建圆环体
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction();
    if (!pTrans) return false;
    wy3d::Tube* pTube(nullptr);
    if (wy::ErrorStatus::Ok != wy3d::Tube::create(pTrans, outerRadius, innerRadius, height, pTube) || !pTube)
    {
        goto ABORT_TRANS;
    }
    if (wy::ErrorStatus::Ok != pTube->setRotation(rotation)) goto ABORT_TRANS;
    if (wy::ErrorStatus::Ok != pTube->setPosition(initOrigin)) goto ABORT_TRANS;
    _pDb->getTransactionManager()->endTransaction();
    _pTube = pTube;
    _initOrigin = pTube->getPosition();
    _zAxis = workPln.getNormal();
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    _pTube = nullptr;
    return false;
}

bool MakeTube::update(double height)
{
    if (!_pDb || !_pTopTrans || !_pTube || _isFinished)
    {
        return false;
    }
    if (std::fabs(height) < wy3d::kMinValue || std::fabs(height) > wy3d::kMaxValue)
    {
        return false;
    }

    wydb::TransactionManager* pTransMgr = _pDb->getTransactionManager();
    wydb::Transaction* pTrans = pTransMgr->startTransaction();
    if (!pTrans) return false;
    {
        _pTube->upgradeForWrite();
        if (height < 0) _pTube->setPosition(_initOrigin + height * _zAxis);
        else _pTube->setPosition(_initOrigin);
        _pTube->setHeight(std::fabs(height));
    }
    if (wy::ErrorStatus::Ok == pTransMgr->endTransaction())
    {
        pTransMgr->mergeTransaction();
    }
    return true;
}
