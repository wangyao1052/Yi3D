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

#include "SketchDrawArcBy3Points3DGuiCmd.h"

#include <cassert>
#include <utility>

#include <QCoreApplication>
#include <QCursor>
#include <QString>

#include <wyVector2.h>
#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dMath.h>
#include <wy3dSketch3D.h>
#include <wy3dSketchArc3D.h>
#include <wy3dImpl.h>

#include "application/Application.h"
#include "commands/sketch/dialogs/GuiCmdHoverInputPopup.h"
#include "utils/MathUtils.h"
#include "widgets/frame/MainWindow.h"
#include "utils/GuiCommandUtil.h"

static bool parseDoubleText(const QString& text, double& value)
{
    bool ok(false);
    value = text.trimmed().toDouble(&ok);
    return ok;
}

SketchDrawArcBy3Points3DGuiCmd::SketchDrawArcBy3Points3DGuiCmd() : Sketch3DDrawGuiCmd(),
    _step(0), _pnt1st(), _pnt2nd(), _pnt3rd(), _pMakeSketchArc(nullptr),
    _pXYZPopup(nullptr), _hoverPopupState()
{
}

SketchDrawArcBy3Points3DGuiCmd::~SketchDrawArcBy3Points3DGuiCmd()
{
}

wyap::CmdExecution::StartResult SketchDrawArcBy3Points3DGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = Sketch3DDrawGuiCmd::onStart();
    if (wyap::CmdExecution::StartResult::Succeeded != ret)
    {
        return ret;
    }

    this->gotoStep(static_cast<unsigned int>(Step::SpecifyPoint1st));
    return ret;
}

void SketchDrawArcBy3Points3DGuiCmd::cleanup()
{
    _step = 0;
    _pnt1st.set(0.0, 0.0, 0.0);
    _pnt2nd.set(0.0, 0.0, 0.0);
    _pnt3rd.set(0.0, 0.0, 0.0);
    _pMakeSketchArc = nullptr;

    this->hidePopup();
    _hoverPopupState.resetValue();
}

void SketchDrawArcBy3Points3DGuiCmd::onEscapeKey()
{
    this->hidePopup();

    if (_step == static_cast<unsigned int>(Step::SpecifyPoint1st))
    {
        this->requestAbort(AbortCause::UserCancel);
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyPoint2nd) ||
        _step == static_cast<unsigned int>(Step::SpecifyPoint3rd))
    {
        this->cleanup();
        this->gotoStep(static_cast<unsigned int>(Step::SpecifyPoint1st));
        this->simulateMouseMoveFromPopup();
    }
    else
    {
        assert(false);
        this->requestAbort(AbortCause::ForceTerminate);
    }
}

bool SketchDrawArcBy3Points3DGuiCmd::finishStep(unsigned int step)
{
    switch (static_cast<Step>(step))
    {
    case Step::SpecifyPoint1st:
    {
        wydb::Database* pDb = Application::instance().getActiveDatabase();
        if (!pDb)
        {
            assert(false);
            return false;
        }
        _pMakeSketchArc = std::make_shared<MakeSketchArcBy3Points3D>(this);
        if (!_pMakeSketchArc->init(_pnt1st, this->getWorkingPlane(), _sketch3DInfo.sketch3dId))
        {
            _pMakeSketchArc = nullptr;
            return false;
        }

        this->gotoStep(static_cast<unsigned int>(Step::SpecifyPoint2nd));
        return true;
    }
    break;

    case Step::SpecifyPoint2nd:
    {
        if (!_pMakeSketchArc)
        {
            assert(false);
            return false;
        }
        // 仅有两个端点时预览半圆
        if (!_pMakeSketchArc->update(_pnt1st, _pnt2nd, wy::Vector3(),
            this->getWorkingPlane().getNormal(), false))
        {
            return false;
        }

        this->gotoStep(static_cast<unsigned int>(Step::SpecifyPoint3rd));
        return true;
    }
    break;

    case Step::SpecifyPoint3rd:
    {
        if (!_pMakeSketchArc)
        {
            assert(false);
            return false;
        }
        // 三点重合或共线时停留在当前步骤
        if (!_pMakeSketchArc->update(_pnt1st, _pnt2nd, _pnt3rd,
            this->getWorkingPlane().getNormal(), true))
        {
            return false;
        }
        _pMakeSketchArc->commit();
        _pMakeSketchArc = nullptr;

        this->gotoStep(static_cast<unsigned int>(Step::SpecifyPoint1st));
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

void SketchDrawArcBy3Points3DGuiCmd::gotoStep(unsigned int step)
{
    _step = step;
    this->hidePopup();
    _hoverPopupState.resetValue();

    switch (static_cast<Step>(step))
    {
    case Step::SpecifyPoint1st:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawArcBy3Points3DGuiCmd",
            "Specify the first end point on arc; you can directly input the coordinate values. "
            "Press Space to switch the drawing plane."));
    }
    break;

    case Step::SpecifyPoint2nd:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawArcBy3Points3DGuiCmd",
            "Specify the second end point on arc; you can directly input the coordinate values. "
            "Press Space to switch the drawing plane."));
    }
    break;

    case Step::SpecifyPoint3rd:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawArcBy3Points3DGuiCmd",
            "Specify the third point on arc; you can directly input the coordinate values. "
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

void SketchDrawArcBy3Points3DGuiCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

void SketchDrawArcBy3Points3DGuiCmd::onMouseMove(const MouseEvent& event)
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

    if (_step == static_cast<unsigned int>(Step::SpecifyPoint1st))
    {
        return;
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyPoint2nd))
    {
        if (_pMakeSketchArc)
        {
            _pMakeSketchArc->update(_pnt1st, ret.first, wy::Vector3(),
                this->getWorkingPlane().getNormal(), false);
        }
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyPoint3rd))
    {
        if (_pMakeSketchArc)
        {
            _pMakeSketchArc->update(_pnt1st, _pnt2nd, ret.first,
                this->getWorkingPlane().getNormal(), true);
        }
    }
    else
    {
        assert(false);
    }

    return;
}

void SketchDrawArcBy3Points3DGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    this->hidePopup();
    _hoverPopupState.lastMouseX = event.x;
    _hoverPopupState.lastMouseY = event.y;
    _hoverPopupState.lastMouseMoveTime = event.time;

    if (_step == static_cast<unsigned int>(Step::SpecifyPoint1st))
    {
        _pnt1st = this->computePoint3d(event.x, event.y).first;
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyPoint2nd))
    {
        _pnt2nd = this->computePoint3d(event.x, event.y).first;
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyPoint3rd))
    {
        _pnt3rd = this->computePoint3d(event.x, event.y).first;
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

void SketchDrawArcBy3Points3DGuiCmd::initializePopups()
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
}

void SketchDrawArcBy3Points3DGuiCmd::showPopup()
{
    if (!_pXYZPopup)
    {
        this->initializePopups();
    }
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

void SketchDrawArcBy3Points3DGuiCmd::hidePopup()
{
    if (_pXYZPopup && _pXYZPopup->isVisible())
    {
        _pXYZPopup->hide();
    }
}

void SketchDrawArcBy3Points3DGuiCmd::tryShowPopupOnHover(double time)
{
    if (_step != static_cast<unsigned int>(Step::SpecifyPoint1st) &&
        _step != static_cast<unsigned int>(Step::SpecifyPoint2nd) &&
        _step != static_cast<unsigned int>(Step::SpecifyPoint3rd))
    {
        return;
    }
    if (_hoverPopupState.lastMouseMoveTime < 0.0)
    {
        return;
    }
    if (_pXYZPopup && _pXYZPopup->isVisible())
    {
        return;
    }
    if (time - _hoverPopupState.lastMouseMoveTime >= 0.45)
    {
        this->showPopup();
    }
}

void SketchDrawArcBy3Points3DGuiCmd::onPopupEnterKey()
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

    if (_step == static_cast<unsigned int>(Step::SpecifyPoint1st))
    {
        _pnt1st.set(x, y, z);
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyPoint2nd))
    {
        _pnt2nd.set(x, y, z);
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyPoint3rd))
    {
        _pnt3rd.set(x, y, z);
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

void SketchDrawArcBy3Points3DGuiCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void SketchDrawArcBy3Points3DGuiCmd::onPopupSpaceKey()
{
    this->onSpaceKey();
    this->hidePopup();
    this->simulateMouseMoveFromPopup();
}

void SketchDrawArcBy3Points3DGuiCmd::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX)
        return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}

std::pair<wy::Vector3, bool> SketchDrawArcBy3Points3DGuiCmd::computePoint3d(double x, double y)
{
    auto ret = this->computePosition3d(x, y, this->getWorkingPlane(), this->getSnapExcludeIds(), true);
    if (ret.second)
    {
        return std::make_pair(ret.second->getPosition(), true);
    }
    return std::make_pair(ret.first, false);
}

std::set<wydb::ElementId> SketchDrawArcBy3Points3DGuiCmd::getSnapExcludeIds() const
{
    std::set<wydb::ElementId> snapExcludeIds;
    // 只排除正在预览的图元,已提交的3D草图图元可作捕捉源(与2D草绘行为一致)
    if (_pMakeSketchArc)
    {
        _pMakeSketchArc->collectElements(snapExcludeIds);
    }
    return snapExcludeIds;
}

void MakeSketchArcBy3Points3D::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pSketchArc3D) idSet.insert(_pSketchArc3D->getId());
}

bool MakeSketchArcBy3Points3D::init(const wy::Vector3& pnt1, const wy3d::SketchPlane& plane, wydb::ElementId sketch3dId)
{
    if (!_pDb || !_pTopTrans || _pSketchArc3D || _isFinished)
    {
        return false;
    }

    const wy::Vector3 yDir = plane.getYDir();
    const wy::Vector3 centerPnt = pnt1 - yDir * wy3d::kMinValue;

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

    if (wy::ErrorStatus::Ok != wy3d::SketchArc3D::create(pTrans, centerPnt, plane.getNormal(), plane.getXDir(),
        wy3d::kMinValue, 0.0, wy3d::PI_2, pSketchArc3D) || !pSketchArc3D)
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

bool MakeSketchArcBy3Points3D::update(const wy::Vector3& pnt1, const wy::Vector3& pnt2, const wy::Vector3& pnt3,
    const wy::Vector3& fallbackNormal, bool useThirdPoint)
{
    if (!_pDb || !_pTopTrans || !_pSketchArc3D || _isFinished)
    {
        return false;
    }

    // 弧平面由落点反算:origin=第一点, xDir=第一点到第二点, 法向由第三点定(两点预览时取工作平面法向)
    wy::Vector3 xDir = pnt2 - pnt1;
    if (xDir.length() < wy3d::kMinValue)
    {
        return false;
    }
    xDir.normalize();

    wy::Vector3 normal = useThirdPoint ? xDir.cross(pnt3 - pnt1) : xDir.cross(fallbackNormal);
    if (normal.length() < wy3d::EPS)
    {
        return false;
    }
    normal.normalize();

    const wy3d::SketchPlane plane(pnt1, normal, xDir);
    const wy::Vector2 uv1 = plane.uv(pnt1);
    const wy::Vector2 uv2 = plane.uv(pnt2);
    const wy::Vector2 uv3 = plane.uv(pnt3);

    wy::Vector2 center;
    double radius(0.0);
    double startAngle(0.0), endAngle(0.0);
    if (useThirdPoint)
    {
        if (!MathUtils::computeArcBy3Points(uv1, uv2, uv3, center, radius, startAngle, endAngle))
        {
            return false;
        }
    }
    else
    {
        wy::Vector2 vec = uv1 - uv2;
        double len = vec.length();
        if (len <= wy3d::EPS)
        {
            return false;
        }
        wy::Vector2 middlePnt = (uv1 + uv2) / 2;
        vec.normalize();
        wy::Vector2 dir(vec.y(), -vec.x());
        center = middlePnt + dir * len / 2;

        // 需要保证vec2逆时针90度旋转到vec1
        if (wy::Vector2::rotationAngle(uv2 - center, uv1 - center) > wy3d::PI)
        {
            center = middlePnt - dir * len / 2;
        }
        radius = (uv1 - center).length();
        startAngle = wy::Vector2::rotationAngle(wy::Vector2::kXAxis, uv2 - center);
        endAngle = wy::Vector2::rotationAngle(wy::Vector2::kXAxis, uv1 - center);
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
        if (wy::ErrorStatus::Ok != _pSketchArc3D->setPlane(normal, xDir) ||
            wy::ErrorStatus::Ok != _pSketchArc3D->setCenter(plane.value(center)) ||
            wy::ErrorStatus::Ok != _pSketchArc3D->setRadius(radius) ||
            wy::ErrorStatus::Ok != _pSketchArc3D->setStartAngle(startAngle) ||
            wy::ErrorStatus::Ok != _pSketchArc3D->setEndAngle(endAngle))
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
