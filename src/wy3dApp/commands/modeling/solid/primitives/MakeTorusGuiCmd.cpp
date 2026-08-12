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

#include "commands/modeling/solid/primitives/MakeTorusGuiCmd.h"

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


MakeTorusGuiCmd::MakeTorusGuiCmd() : MakePrimitiveGuiCmd(),
    _uv1(), _uv2(), _minorRadius(0.0),
    _pXYPopup(nullptr),
    _pMajorRadiusPopup(nullptr),
    _pMinorRadiusPopup(nullptr),
    _hoverPopupState()
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

MakeTorusGuiCmd::~MakeTorusGuiCmd()
{
}

void MakeTorusGuiCmd::reset()
{
    __baseClass::reset();
    this->cleanup();
}

void MakeTorusGuiCmd::cleanup()
{
    __baseClass::cleanup();
    this->hidePopup();

    _uv1.set(0.0, 0.0);
    _uv2.set(0.0, 0.0);
    _minorRadius = 0.0;

    _pMakeTorus = nullptr;
    _pCircleTransient = nullptr;
    _pRadiusTransient = nullptr;
    _hoverPopupState.resetValue();
}

bool MakeTorusGuiCmd::finishStep(unsigned int step)
{
    switch (step)
    {
    case Step::SpecifyWorkingPlane: // 工作平面
    {
        return MakePrimitiveGuiCmd::finishStep(step);
    }
    break;

    case Step::SpecifyPnt1: // 确定圆心
    {
        // 绘制圆
        _pCircleTransient = std::make_shared<SketchCircleTransient>(_workPln);
        _pCircleTransient->update(_uv1, 0.0f);

        // next step
        this->gotoStep(Step::SpecifyPnt2);
        return true;
    }
    break;

    case Step::SpecifyPnt2: // 确定主半径
    {
        // 创建MakeTorus
        _pMakeTorus = std::make_shared<MakeTorus>(this);
        if (!_pMakeTorus->init(_workPln, _uv1, _uv2)) // 半径过小时会返回false(比如输入0)
        {
            _pMakeTorus = nullptr;
            return false;
        }
        _pMakeTorus->collectElements(_excludeIds);

        // 销毁圆
        _pCircleTransient = nullptr;

        // 绘制半径
        _pRadiusTransient = std::make_shared<RadiusTransient>();
        wy::Vector3 pnt = _workPln.value(_uv1);
        _pRadiusTransient->update(pnt, pnt);

        // next step
        this->gotoStep(Step::SpecifyPnt3);
        return true;
    }
    break;

    case Step::SpecifyPnt3: // 确定管径
    {
        // 更新管径
        if (_pMakeTorus)
        {
            if (!_pMakeTorus->update(_minorRadius)) // 管径过小时会返回false(比如输入0)
            {
                return false;
            }

            // 提交
            _pMakeTorus->commit();
            _pMakeTorus = nullptr;
        }

        // 销毁绘制的半径
        _pRadiusTransient = nullptr;

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

void MakeTorusGuiCmd::gotoStepImpl(unsigned int step)
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

    case Step::SpecifyPnt1: // 确定圆心
    {
        // 允许输入
        // 提示
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("MakeTorusGuiCmd",
            "Specify the center point; you can directly input the coordinate values."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::Locate);
    }
    break;

    case Step::SpecifyPnt2: // 确定主半径
    {
        // 允许输入
        // 提示
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("MakeTorusGuiCmd",
            "Specify the major radius; you can directly input the value."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::Locate);
    }
    break;

    case Step::SpecifyPnt3: // 确定管径
    {
        // 允许输入
        // 提示
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("MakeTorusGuiCmd",
            "Specify the minor radius; you can directly input the value."));

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

void MakeTorusGuiCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

void MakeTorusGuiCmd::onMouseMove(const MouseEvent& event)
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

    case Step::SpecifyPnt1: // 确定圆心
    {
        wy::Vector3 pnt = this->computePosition3d(event.x, event.y, _workPln, _excludeIds).first;
        wy::Vector2 uv = _workPln.uv(pnt);
        _hoverPopupState.point = uv;
        return;
    }
    break;

    case Step::SpecifyPnt2: // 确定主半径
    {
        wy::Vector3 pnt = this->computePosition3d(event.x, event.y, _workPln, _excludeIds).first;
        wy::Vector2 uv = _workPln.uv(pnt);
        double radius = (uv - _uv1).length();
        _hoverPopupState.majorRadius = radius;
        {
            if (_pCircleTransient) _pCircleTransient->update(_uv1, radius);
        }
        return;
    }
    break;

    case Step::SpecifyPnt3: // 确定管径
    {
        wy::Vector3 pnt = this->computePosition3d(event.x, event.y, _workPln, _excludeIds).first;
        wy::Vector2 uv = _workPln.uv(pnt);
        double minorRadius = (uv - _uv2).length();
        _hoverPopupState.minorRadius = minorRadius;
        {
            if (_pMakeTorus) _pMakeTorus->update(minorRadius);
            if (_pRadiusTransient) _pRadiusTransient->update(_workPln.value(_uv2), pnt);
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

void MakeTorusGuiCmd::onLeftMouseDown(const MouseEvent& event)
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

    case Step::SpecifyPnt1: // 确定圆心
    {
        wy::Vector3 pnt1 = this->computePosition3d(event.x, event.y, _workPln, _excludeIds).first;
        _uv1 = _workPln.uv(pnt1);
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
        return;
    }
    break;

    case Step::SpecifyPnt2: // 确定主半径
    {
        wy::Vector3 pnt2 = this->computePosition3d(event.x, event.y, _workPln, _excludeIds).first;
        _uv2 = _workPln.uv(pnt2);
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
        return;
    }
    break;

    case Step::SpecifyPnt3: // 确定管径
    {
        wy::Vector3 pnt = this->computePosition3d(event.x, event.y, _workPln, _excludeIds).first;
        double minorRadius = (_uv2 - _workPln.uv(pnt)).length();
        _minorRadius = minorRadius;
        if (_pMakeTorus)
        {
            double majorRadius = _pMakeTorus->getMajorRadius();
            if (_minorRadius > std::fabs(majorRadius))
            {
                _minorRadius = std::fabs(majorRadius);
            }
        }
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
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

void MakeTorusGuiCmd::initializePopups()
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
    if (!_pMajorRadiusPopup)
    {
        _pMajorRadiusPopup = std::make_unique<GuiCmdHoverInputPopup1>(
            QCoreApplication::translate("MakeTorusGuiCmd", "Major radius"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pMajorRadiusPopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pMajorRadiusPopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pMajorRadiusPopup->hide();
    }
    if (!_pMinorRadiusPopup)
    {
        _pMinorRadiusPopup = std::make_unique<GuiCmdHoverInputPopup1>(
            QCoreApplication::translate("MakeTorusGuiCmd", "Minor radius"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pMinorRadiusPopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pMinorRadiusPopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pMinorRadiusPopup->hide();
    }
}

void MakeTorusGuiCmd::showPopup()
{
    if (_step != Step::SpecifyPnt1 &&
        _step != Step::SpecifyPnt2 &&
        _step != Step::SpecifyPnt3)
    {
        return;
    }
    if (!_pXYPopup || !_pMajorRadiusPopup || !_pMinorRadiusPopup)
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
        if (!_pMajorRadiusPopup)
        {
            return;
        }
        _pMajorRadiusPopup->setValue(_hoverPopupState.majorRadius);
        _pMajorRadiusPopup->showAtGlobal(QCursor::pos());
    }
    else
    {
        if (!_pMinorRadiusPopup)
        {
            return;
        }
        _pMinorRadiusPopup->setValue(_hoverPopupState.minorRadius);
        _pMinorRadiusPopup->showAtGlobal(QCursor::pos());
    }
}

void MakeTorusGuiCmd::hidePopup()
{
    if (_pXYPopup && _pXYPopup->isVisible())
    {
        _pXYPopup->hide();
    }
    if (_pMajorRadiusPopup && _pMajorRadiusPopup->isVisible())
    {
        _pMajorRadiusPopup->hide();
    }
    if (_pMinorRadiusPopup && _pMinorRadiusPopup->isVisible())
    {
        _pMinorRadiusPopup->hide();
    }
}

void MakeTorusGuiCmd::tryShowPopupOnHover(double time)
{
    if (_step != Step::SpecifyPnt1 &&
        _step != Step::SpecifyPnt2 &&
        _step != Step::SpecifyPnt3)
    {
        return;
    }
    if (_hoverPopupState.lastMouseMoveTime < 0.0)
    {
        return;
    }
    if ((_pXYPopup && _pXYPopup->isVisible()) ||
        (_pMajorRadiusPopup && _pMajorRadiusPopup->isVisible()) ||
        (_pMinorRadiusPopup && _pMinorRadiusPopup->isVisible()))
    {
        return;
    }
    if (time - _hoverPopupState.lastMouseMoveTime >= kHoverPopupDelaySeconds)
    {
        this->showPopup();
    }
}

void MakeTorusGuiCmd::onPopupEnterKey()
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
        if (!_pMajorRadiusPopup)
        {
            return;
        }
        double radius(0.0);
        if (!parseDoubleText(_pMajorRadiusPopup->getRowText(), radius))
        {
            return;
        }
        radius = std::fabs(radius);
        _uv2 = _uv1 + wy::Vector2(radius, 0.0);
    }
    else if (_step == Step::SpecifyPnt3)
    {
        if (!_pMinorRadiusPopup)
        {
            return;
        }
        double minorRadius(0.0);
        if (!parseDoubleText(_pMinorRadiusPopup->getRowText(), minorRadius))
        {
            return;
        }
        _minorRadius = std::fabs(minorRadius);
        if (_pMakeTorus)
        {
            double majorRadius = _pMakeTorus->getMajorRadius();
            if (_minorRadius > std::fabs(majorRadius))
            {
                _minorRadius = std::fabs(majorRadius);
            }
        }
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

void MakeTorusGuiCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void MakeTorusGuiCmd::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX)
        return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}

void MakeTorus::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pTorus) idSet.insert(_pTorus->getId());
}

bool MakeTorus::init(const wy3d::SketchPlane& workPln, const wy::Vector2& pnt1, const wy::Vector2& pnt2)
{
    if (!_pDb || !_pTopTrans || _pTorus || _isFinished)
    {
        return false;
    }

    // 计算主半径
    double majorRadius = (pnt2 - pnt1).length();
    if (majorRadius < wy3d::kMinValue || majorRadius > wy3d::kMaxValue)
    {
        return false;
    }
    double minorRadius(wy3d::kMinValue);

    // 计算旋转欧拉角
    if (!workPln.isValid())
    {
        return false;
    }
    wy::Vector3 rotation = MathUtils::computeEulerZXY(workPln);

    // 确定起始坐标
    wy::Vector3 initOrigin = workPln.value(pnt1);

    // 创建圆环体
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction();
    if (!pTrans) return false;
    wy3d::Torus* pTorus(nullptr);
    if (wy::ErrorStatus::Ok != wy3d::Torus::create(pTrans, majorRadius, minorRadius, pTorus) || !pTorus)
    {
        goto ABORT_TRANS;
    }
    if (wy::ErrorStatus::Ok != pTorus->setRotation(rotation)) goto ABORT_TRANS;
    if (wy::ErrorStatus::Ok != pTorus->setPosition(initOrigin)) goto ABORT_TRANS;
    _pDb->getTransactionManager()->endTransaction();
    _pTorus = pTorus;
    _initOrigin = pTorus->getPosition();
    _majorRadius = pTorus->getMajorRadius();
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    _pTorus = nullptr;
    return false;
}

bool MakeTorus::update(double minorRadius)
{
    if (!_pDb || !_pTopTrans || !_pTorus || _isFinished)
    {
        return false;
    }
    if (std::fabs(minorRadius) < wy3d::kMinValue || std::fabs(minorRadius) > wy3d::kMaxValue)
    {
        return false;
    }
    minorRadius = std::fabs(minorRadius);

    bool ret(true);
    wydb::TransactionManager* pTransMgr = _pDb->getTransactionManager();
    wydb::Transaction* pTrans = pTransMgr->startTransaction();
    if (!pTrans) return false;
    {
        _pTorus->upgradeForWrite();
        double majorRadius = _pTorus->getMajorRadius();
        if (minorRadius > majorRadius)
        {
            _pTorus->setMinorRadius(majorRadius);
            ret = false;
        }
        else
        {
            _pTorus->setMinorRadius(minorRadius);
        }   
    }
    if (wy::ErrorStatus::Ok == pTransMgr->endTransaction())
    {
        pTransMgr->mergeTransaction();
    }
    return ret;
}
