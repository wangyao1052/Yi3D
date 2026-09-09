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

#include "SketchDrawCircle3DGuiCmd.h"

#include <cassert>
#include <utility>

#include <QCoreApplication>
#include <QCursor>
#include <QString>

#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dSketch3D.h>
#include <wy3dSketchCircle3D.h>
#include <wy3dImpl.h>

#include "application/Application.h"
#include "commands/sketch/dialogs/GuiCmdHoverInputPopup.h"
#include "widgets/frame/MainWindow.h"
#include "utils/GuiCommandUtil.h"

static bool parseDoubleText(const QString& text, double& value)
{
    bool ok(false);
    value = text.trimmed().toDouble(&ok);
    return ok;
}

SketchDrawCircle3DGuiCmd::SketchDrawCircle3DGuiCmd() : Sketch3DDrawGuiCmd(),
    _step(0), _centerPnt(), _radius(0.0), _snapPlaneState(), _pMakeSketchCircle3D(nullptr),
    _pXYZPopup(nullptr), _pRadiusPopup(nullptr), _hoverPopupState()
{
}

SketchDrawCircle3DGuiCmd::~SketchDrawCircle3DGuiCmd()
{
}

wyap::CmdExecution::StartResult SketchDrawCircle3DGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = Sketch3DDrawGuiCmd::onStart();
    if (wyap::CmdExecution::StartResult::Succeeded != ret)
    {
        return ret;
    }

    this->gotoStep(static_cast<unsigned int>(Step::SpecifyCenterPnt));
    return ret;
}

void SketchDrawCircle3DGuiCmd::cleanup()
{
    _step = 0;
    _centerPnt.set(0.0, 0.0, 0.0);
    _radius = 0.0;
    _pMakeSketchCircle3D = nullptr;

    this->hidePopup();
    _hoverPopupState.resetValue();
}

void SketchDrawCircle3DGuiCmd::onEscapeKey()
{
    this->hidePopup();

    if (_step == static_cast<unsigned int>(Step::SpecifyCenterPnt))
    {
        this->requestAbort(AbortCause::UserCancel);
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyRadius))
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

bool SketchDrawCircle3DGuiCmd::finishStep(unsigned int step)
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
        _pMakeSketchCircle3D = std::make_shared<MakeSketchCircle3D>(this);
        if (!_pMakeSketchCircle3D->init(_centerPnt, this->getWorkingPlane().getNormal(),
            this->getWorkingPlane().getXDir(), _sketch3DInfo.sketch3dId))
        {
            _pMakeSketchCircle3D = nullptr;
            return false;
        }

        this->moveWorkPlaneOriginTo(_centerPnt);
        this->gotoStep(static_cast<unsigned int>(Step::SpecifyRadius));
        return true;
    }
    break;

    case Step::SpecifyRadius:
    {
        if (!_pMakeSketchCircle3D)
        {
            assert(false);
            return false;
        }
        bool ok = _snapPlaneState.snapped
            ? _pMakeSketchCircle3D->updateRadiusAndPlane(_radius, _snapPlaneState.normal, _snapPlaneState.xDir)
            : _pMakeSketchCircle3D->update(_radius);
        if (!ok)
        {
            return false;
        }
        _pMakeSketchCircle3D->commit();
        _pMakeSketchCircle3D = nullptr;

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

void SketchDrawCircle3DGuiCmd::gotoStep(unsigned int step)
{
    _step = step;
    this->hidePopup();
    _hoverPopupState.resetValue();
    _snapPlaneState.resetValue();

    switch (static_cast<Step>(step))
    {
    case Step::SpecifyCenterPnt:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawCircle3DGuiCmd",
            "Specify the center point; you can directly input the coordinate values. "
            "Press Space to switch the drawing plane."));
    }
    break;

    case Step::SpecifyRadius:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawCircle3DGuiCmd",
            "Specify the radius; you can directly input the value. "
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

void SketchDrawCircle3DGuiCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

void SketchDrawCircle3DGuiCmd::onSpaceKey()
{
    Sketch3DDrawGuiCmd::onSpaceKey();
    if (_step == static_cast<unsigned int>(Step::SpecifyRadius) && _pMakeSketchCircle3D)
    {
        const wy3d::SketchPlane& plane = this->getWorkingPlane();
        _pMakeSketchCircle3D->updatePlane(plane.getNormal(), plane.getXDir());
    }
}

void SketchDrawCircle3DGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (event.x != _hoverPopupState.lastMouseX ||
        event.y != _hoverPopupState.lastMouseY)
    {
        this->hidePopup();
        _hoverPopupState.lastMouseX = event.x;
        _hoverPopupState.lastMouseY = event.y;
        _hoverPopupState.lastMouseMoveTime = event.time;
    }

    if (_step == static_cast<unsigned int>(Step::SpecifyCenterPnt))
    {
        _hoverPopupState.point = this->computePoint3d(event.x, event.y).first;
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyRadius))
    {
        std::pair<wy::Vector3, bool> ret = this->computePoint3d(event.x, event.y);
        wy::Vector3 pnt = ret.first;
        double radius = (pnt - _centerPnt).length();
        _hoverPopupState.radius = radius;
        if (_pMakeSketchCircle3D)
        {
            const wy3d::SketchPlane& plane = this->getWorkingPlane();
            wy::Vector3 normal = plane.getNormal();
            wy::Vector3 xDir = plane.getXDir();
            if (ret.second)
            {
                this->computeCirclePlane(pnt, normal, xDir);
            }
            _pMakeSketchCircle3D->updateRadiusAndPlane(radius, normal, xDir);
        }
    }

    return;
}

void SketchDrawCircle3DGuiCmd::onLeftMouseDown(const MouseEvent& event)
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
    else if (_step == static_cast<unsigned int>(Step::SpecifyRadius))
    {
        std::pair<wy::Vector3, bool> ret = this->computePoint3d(event.x, event.y);
        _radius = (ret.first - _centerPnt).length();
        _snapPlaneState.snapped = ret.second && this->computeCirclePlane(ret.first, _snapPlaneState.normal, _snapPlaneState.xDir);
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

void SketchDrawCircle3DGuiCmd::initializePopups()
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

    if (!_pRadiusPopup)
    {
        _pRadiusPopup = std::make_unique<GuiCmdHoverInputPopup1>(
            QCoreApplication::translate("SketchDrawCircle3DGuiCmd", "Radius"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pRadiusPopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pRadiusPopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pRadiusPopup->setSpaceKeyHandler([this]() { this->onPopupSpaceKey(); });
        _pRadiusPopup->hide();
    }
}

void SketchDrawCircle3DGuiCmd::showPopup()
{
    if (!_pXYZPopup || !_pRadiusPopup)
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
    else if (_step == static_cast<unsigned int>(Step::SpecifyRadius))
    {
        if (!_pRadiusPopup)
        {
            return;
        }
        _pRadiusPopup->setValue(
            _hoverPopupState.radius);
        _pRadiusPopup->showAtGlobal(QCursor::pos());
    }
}

void SketchDrawCircle3DGuiCmd::hidePopup()
{
    if (_pXYZPopup && _pXYZPopup->isVisible())
    {
        _pXYZPopup->hide();
    }
    if (_pRadiusPopup && _pRadiusPopup->isVisible())
    {
        _pRadiusPopup->hide();
    }
}

void SketchDrawCircle3DGuiCmd::tryShowPopupOnHover(double time)
{
    if (_step != static_cast<unsigned int>(Step::SpecifyCenterPnt) && _step != static_cast<unsigned int>(Step::SpecifyRadius))
    {
        return;
    }
    if (_hoverPopupState.lastMouseMoveTime < 0.0)
    {
        return;
    }
    if ((_pXYZPopup && _pXYZPopup->isVisible()) ||
        (_pRadiusPopup && _pRadiusPopup->isVisible()))
    {
        return;
    }
    if (time - _hoverPopupState.lastMouseMoveTime >= 0.45)
    {
        this->showPopup();
    }
}

void SketchDrawCircle3DGuiCmd::onPopupEnterKey()
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
    else if (_step == static_cast<unsigned int>(Step::SpecifyRadius))
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
        _radius = radius;
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

void SketchDrawCircle3DGuiCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void SketchDrawCircle3DGuiCmd::onPopupSpaceKey()
{
    this->onSpaceKey();
    this->hidePopup();
    this->simulateMouseMoveFromPopup();
}

void SketchDrawCircle3DGuiCmd::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX)
        return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}

std::pair<wy::Vector3, bool> SketchDrawCircle3DGuiCmd::computePoint3d(double x, double y)
{
    auto ret = this->computePosition3d(x, y, this->getWorkingPlane(), this->getSnapExcludeIds(), true);
    if (ret.second)
    {
        return std::make_pair(ret.second->getPosition(), true);
    }
    return std::make_pair(ret.first, false);
}

bool SketchDrawCircle3DGuiCmd::computeCirclePlane(const wy::Vector3& pnt, wy::Vector3& outNormal, wy::Vector3& outXDir) const
{
    wy::Vector3 xDirNew = pnt - _centerPnt;
    if (xDirNew.length() < 1e-5) return false;
    xDirNew.normalize();

    const wy3d::SketchPlane& plane = this->getWorkingPlane();
    wy::Vector3 xDirOld = plane.getXDir();
    xDirOld.normalize();
    wy::Vector3 normalOld = plane.getNormal();
    normalOld.normalize();

    wy::Vector3 normalNew;
    wy::Vector3 axis = xDirOld.cross(xDirNew);
    double sinTheta = axis.length();
    if (sinTheta < 1e-9)
    {
        normalNew = normalOld;
    }
    else
    {
        axis *= (1.0 / sinTheta);
        double cosTheta = xDirOld.dot(xDirNew);
        normalNew = normalOld * cosTheta
            + axis.cross(normalOld) * sinTheta
            + axis * (axis.dot(normalOld) * (1.0 - cosTheta));
    }

    outNormal = normalNew;
    outXDir = xDirNew;
    return true;
}

std::set<wydb::ElementId> SketchDrawCircle3DGuiCmd::getSnapExcludeIds() const
{
    std::set<wydb::ElementId> snapExcludeIds;
    // 只排除正在预览的图元,已提交的3D草图图元可作捕捉源(与2D草绘行为一致)
    if (_pMakeSketchCircle3D)
    {
        _pMakeSketchCircle3D->collectElements(snapExcludeIds);
    }
    return snapExcludeIds;
}

void MakeSketchCircle3D::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pSketchCircle3D) idSet.insert(_pSketchCircle3D->getId());
}

bool MakeSketchCircle3D::init(const wy::Vector3& centerPnt, const wy::Vector3& normal, const wy::Vector3& xDir, wydb::ElementId sketch3dId)
{
    if (!_pDb || !_pTopTrans || _pSketchCircle3D || _isFinished)
    {
        return false;
    }

    // 创建SketchCircle3D
    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    wy3d::Sketch3D* pSketch3D = nullptr;
    wy3d::SketchCircle3D* pSketchCircle3D = nullptr;
    wydb::Element* pSketchElem = pTrans->getElementForWrite(sketch3dId);
    if (!pSketchElem) goto ABORT_TRANS;
    pSketch3D = wy3d::Sketch3D::cast(pSketchElem);
    if (!pSketch3D) goto ABORT_TRANS;

    if (wy::ErrorStatus::Ok != wy3d::SketchCircle3D::create(pTrans, centerPnt, normal, xDir, wy3d::kMinValue, pSketchCircle3D) || !pSketchCircle3D)
    {
        goto ABORT_TRANS;
    }
    _pSketchCircle3D = pSketchCircle3D;
    if (wy::ErrorStatus::Ok != pSketch3D->addEntity(pSketchCircle3D))
    {
        goto ABORT_TRANS;
    }
    _pDb->getTransactionManager()->endTransaction();
    _pSketchCircle3D = pSketchCircle3D;
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    _pSketchCircle3D = nullptr;
    return false;
}

bool MakeSketchCircle3D::update(double radius)
{
    if (!_pDb || !_pTopTrans || !_pSketchCircle3D || _isFinished)
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
        _pSketchCircle3D->upgradeForWrite();
        if (wy::ErrorStatus::Ok != _pSketchCircle3D->setRadius(radius))
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

bool MakeSketchCircle3D::updateRadiusAndPlane(double radius, const wy::Vector3& normal, const wy::Vector3& xDir)
{
    if (!_pDb || !_pTopTrans || !_pSketchCircle3D || _isFinished)
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
        _pSketchCircle3D->upgradeForWrite();
        if (wy::ErrorStatus::Ok != _pSketchCircle3D->setPlane(normal, xDir) ||
            wy::ErrorStatus::Ok != _pSketchCircle3D->setRadius(radius))
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

bool MakeSketchCircle3D::updatePlane(const wy::Vector3& normal, const wy::Vector3& xDir)
{
    if (!_pDb || !_pTopTrans || !_pSketchCircle3D || _isFinished)
    {
        return false;
    }

    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    {
        _pSketchCircle3D->upgradeForWrite();
        if (wy::ErrorStatus::Ok != _pSketchCircle3D->setPlane(normal, xDir))
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
