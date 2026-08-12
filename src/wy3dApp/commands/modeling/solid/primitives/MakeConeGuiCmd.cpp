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

#include "commands/modeling/solid/primitives/MakeConeGuiCmd.h"

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
#include <wy3dCone.h>
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


MakeConeGuiCmd::MakeConeGuiCmd() : MakePrimitiveGuiCmd(),
    _uv1(), _uv2(), _height(0.0),
    _pXYPopup(nullptr),
    _pRadiusPopup(nullptr),
    _pHeightPopup(nullptr),
    _hoverPopupState()
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

MakeConeGuiCmd::~MakeConeGuiCmd()
{
}

void MakeConeGuiCmd::reset()
{
    __baseClass::reset();
    this->cleanup();
}

void MakeConeGuiCmd::cleanup()
{
    __baseClass::cleanup();
    this->hidePopup();

    _uv1.set(0.0, 0.0);
    _uv2.set(0.0, 0.0);
    _height = 0.0;

    _pMakeCone = nullptr;
    _pCircleTransient = nullptr;
    _hoverPopupState.resetValue();
}

bool MakeConeGuiCmd::finishStep(unsigned int step)
{
    switch (step)
    {
    case Step::SpecifyWorkingPlane: // 工作平面
    {
        return MakePrimitiveGuiCmd::finishStep(step);
    }
    break;

    case Step::SpecifyPnt1: // 确定底面圆心
    {
        // 绘制圆
        _pCircleTransient = std::make_shared<SketchCircleTransient>(_workPln);
        _pCircleTransient->update(_uv1, 0.0f);

        // next step
        this->gotoStep(Step::SpecifyPnt2);
        return true;
    }
    break;

    case Step::SpecifyPnt2: // 确定半径
    {
        // 创建MakeCone
        _pMakeCone = std::make_shared<MakeCone>(this);
        if (!_pMakeCone->init(_workPln, _uv1, _uv2)) // 半径过小时会返回false(比如输入0)
        {
            _pMakeCone = nullptr;
            return false;
        }
        _pMakeCone->collectElements(_excludeIds);

        // 销毁圆
        _pCircleTransient = nullptr;

        // next step
        this->gotoStep(Step::SpecifyPnt3);
        return true;
    }
    break;

    case Step::SpecifyPnt3:
    {
        // 更新高度
        if (_pMakeCone)
        {
            if (!_pMakeCone->update(_height)) // 高度过小时会返回false(比如输入0)
            {
                return false;
            }

            // 提交
            _pMakeCone->commit();
            _pMakeCone = nullptr;
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

void MakeConeGuiCmd::gotoStepImpl(unsigned int step)
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
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("MakeConeGuiCmd",
            "Specify the bottom center point; you can directly input the coordinate values."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::Locate);
    }
    break;

    case Step::SpecifyPnt2: // 确定半径
    {
        // 允许输入
        // 提示
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("MakeConeGuiCmd",
            "Specify the bottom radius; you can directly input the value."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::Locate);
    }
    break;

    case Step::SpecifyPnt3: // 确定高度
    {
        // 允许输入
        // 提示
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("MakeConeGuiCmd",
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

void MakeConeGuiCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

void MakeConeGuiCmd::onMouseMove(const MouseEvent& event)
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

    case Step::SpecifyPnt2: // 确定半径
    {
        wy::Vector3 pnt = this->computePosition3d(event.x, event.y, _workPln, _excludeIds).first;
        wy::Vector2 uv = _workPln.uv(pnt);
        double radius = (uv - _uv1).length();
        _hoverPopupState.radius = radius;
        {
            if (_pCircleTransient) _pCircleTransient->update(_uv1, radius);
        }
        return;
    }
    break;

    case Step::SpecifyPnt3: // 确定高度
    {
        double height(0.0);
        if (this->computeHeight2(event.x, event.y, _workPln, _uv2, _excludeIds, height)) // height可以小于0
        {
            _hoverPopupState.heightSign = height < 0.0 ? -1 : 1;
            _hoverPopupState.height = std::fabs(height);
            {
                if (_pMakeCone) _pMakeCone->update(height);
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

void MakeConeGuiCmd::onLeftMouseDown(const MouseEvent& event)
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

    case Step::SpecifyPnt2:
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

    case Step::SpecifyPnt3:
    {
        double height(0.0);
        if (this->computeHeight2(event.x, event.y, _workPln, _uv2, _excludeIds, height))
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

void MakeConeGuiCmd::initializePopups()
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
    if (!_pRadiusPopup)
    {
        _pRadiusPopup = std::make_unique<GuiCmdHoverInputPopup1>(
            QCoreApplication::translate("MakeConeGuiCmd", "Radius"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pRadiusPopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pRadiusPopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pRadiusPopup->hide();
    }
    if (!_pHeightPopup)
    {
        _pHeightPopup = std::make_unique<GuiCmdHoverInputPopup1>(
            QCoreApplication::translate("MakeConeGuiCmd", "Height"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pHeightPopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pHeightPopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pHeightPopup->hide();
    }
}

void MakeConeGuiCmd::showPopup()
{
    if (_step != Step::SpecifyPnt1 &&
        _step != Step::SpecifyPnt2 &&
        _step != Step::SpecifyPnt3)
    {
        return;
    }
    if (!_pXYPopup || !_pRadiusPopup || !_pHeightPopup)
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
        if (!_pRadiusPopup)
        {
            return;
        }
        _pRadiusPopup->setValue(_hoverPopupState.radius);
        _pRadiusPopup->showAtGlobal(QCursor::pos());
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

void MakeConeGuiCmd::hidePopup()
{
    if (_pXYPopup && _pXYPopup->isVisible())
    {
        _pXYPopup->hide();
    }
    if (_pRadiusPopup && _pRadiusPopup->isVisible())
    {
        _pRadiusPopup->hide();
    }
    if (_pHeightPopup && _pHeightPopup->isVisible())
    {
        _pHeightPopup->hide();
    }
}

void MakeConeGuiCmd::tryShowPopupOnHover(double time)
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
        (_pRadiusPopup && _pRadiusPopup->isVisible()) ||
        (_pHeightPopup && _pHeightPopup->isVisible()))
    {
        return;
    }
    if (time - _hoverPopupState.lastMouseMoveTime >= kHoverPopupDelaySeconds)
    {
        this->showPopup();
    }
}

void MakeConeGuiCmd::onPopupEnterKey()
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
        if (!_pRadiusPopup)
        {
            return;
        }
        double radius(0.0);
        if (!parseDoubleText(_pRadiusPopup->getRowText(), radius))
        {
            return;
        }
        radius = std::fabs(radius);
        _uv2 = _uv1 + wy::Vector2(radius, 0.0);
    }
    else if (_step == Step::SpecifyPnt3)
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

void MakeConeGuiCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void MakeConeGuiCmd::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX)
        return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}

void MakeCone::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pCone) idSet.insert(_pCone->getId());
}

bool MakeCone::init(const wy3d::SketchPlane& workPln, const wy::Vector2& pnt1, const wy::Vector2& pnt2)
{
    if (!_pDb || !_pTopTrans || _pCone || _isFinished)
    {
        return false;
    }

    // 计算圆锥体半径
    double radius = (pnt2 - pnt1).length();
    if (radius < wy3d::kMinValue || radius > wy3d::kMaxValue)
    {
        return false;
    }
    double height(wy3d::kMinValue);

    // 计算旋转欧拉角
    if (!workPln.isValid())
    {
        return false;
    }
    wy::Vector3 rotation = MathUtils::computeEulerZXY(workPln);


    // 确定起始坐标
    wy::Vector3 initOrigin = workPln.value(pnt1);

    // 创建圆锥体
    wy3d::SketchPlane oppositeWorkPln;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction();
    if (!pTrans) return false;
    wy3d::Cone* pCone(nullptr);
    if (wy::ErrorStatus::Ok != wy3d::Cone::create(pTrans, radius, height, pCone) || !pCone)
    {
        goto ABORT_TRANS;
    }
    if (wy::ErrorStatus::Ok != pCone->setRotation(rotation)) goto ABORT_TRANS;
    if (wy::ErrorStatus::Ok != pCone->setPosition(initOrigin)) goto ABORT_TRANS;
    _pDb->getTransactionManager()->endTransaction();
    _pCone = pCone;
    _rotation = rotation;
    oppositeWorkPln = wy3d::SketchPlane(workPln.getOrigin(), -workPln.getNormal(), workPln.getXDir());
    _oppositeRot = MathUtils::computeEulerZXY(oppositeWorkPln);
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    _pCone = nullptr;
    return false;
}

bool MakeCone::update(double height)
{
    if (!_pDb || !_pTopTrans || !_pCone || _isFinished)
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
        _pCone->upgradeForWrite();
        if (height > 0)
        {
            _pCone->setRotation(_rotation);
            _pCone->setHeight(height);
        }
        else
        {
            _pCone->setRotation(_oppositeRot);
            _pCone->setHeight(std::fabs(height));
        }
    }
    if (wy::ErrorStatus::Ok == pTransMgr->endTransaction())
    {
        pTransMgr->mergeTransaction();
    }
    return true;
}
