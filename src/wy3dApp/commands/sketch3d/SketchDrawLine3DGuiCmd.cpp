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

#include "SketchDrawLine3DGuiCmd.h"

#include <cassert>
#include <cmath>
#include <utility>

#include <QCoreApplication>
#include <QCursor>
#include <QString>

#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dSketch3D.h>
#include <wy3dSketchLine3D.h>
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

SketchDrawLine3DGuiCmd::SketchDrawLine3DGuiCmd() : Sketch3DDrawGuiCmd(),
    _step(0), _startPnt(), _endPnt(), _pMakeSketchLine3D(nullptr),
    _pXYZPopup(nullptr), _pLengthAnglePopup(nullptr), _hoverPopupState()
{
}

SketchDrawLine3DGuiCmd::~SketchDrawLine3DGuiCmd()
{
}

wyap::CmdExecution::StartResult SketchDrawLine3DGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = Sketch3DDrawGuiCmd::onStart();
    if (wyap::CmdExecution::StartResult::Succeeded != ret)
    {
        return ret;
    }

    this->gotoStep(static_cast<unsigned int>(Step::SpecifyStartPnt));
    return wyap::CmdExecution::StartResult::Succeeded;
}

void SketchDrawLine3DGuiCmd::cleanup()
{
    _step = 0;
    _startPnt.set(0.0, 0.0, 0.0);
    _endPnt.set(0.0, 0.0, 0.0);
    _pMakeSketchLine3D = nullptr;

    this->hidePopup();
    _hoverPopupState.resetValue();
}

void SketchDrawLine3DGuiCmd::onEscapeKey()
{
    this->hidePopup();

    if (_step == static_cast<unsigned int>(Step::SpecifyStartPnt))
    {
        this->requestAbort(AbortCause::UserCancel);
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyEndPnt))
    {
        this->cleanup();
        this->gotoStep(static_cast<unsigned int>(Step::SpecifyStartPnt));
        this->simulateMouseMoveFromPopup();
    }
    else
    {
        assert(false);
        this->requestAbort(AbortCause::ForceTerminate);
    }
}

bool SketchDrawLine3DGuiCmd::finishStep(unsigned int step)
{
    switch (static_cast<Step>(step))
    {
    case Step::SpecifyStartPnt:
    {
        wydb::Database* pDb = Application::instance().getActiveDatabase();
        if (!pDb)
        {
            assert(false);
            return false;
        }
        _pMakeSketchLine3D = std::make_shared<MakeSketchLine3D>(this);
        if (!_pMakeSketchLine3D->init(_startPnt, _sketch3DInfo.sketch3dId))
        {
            _pMakeSketchLine3D = nullptr;
            return false;
        }

        this->moveWorkPlaneOriginTo(_startPnt);
        this->gotoStep(static_cast<unsigned int>(Step::SpecifyEndPnt));
        return true;
    }
    break;

    case Step::SpecifyEndPnt:
    {
        if (!_pMakeSketchLine3D)
        {
            assert(false);
            return false;
        }
        if (!_pMakeSketchLine3D->update(_endPnt))
        {
            return false;
        }
        _pMakeSketchLine3D->commit();
        _pMakeSketchLine3D = nullptr;

        this->gotoStep(static_cast<unsigned int>(Step::SpecifyStartPnt));
        _startPnt = _endPnt;
        bool result = this->finishStep(static_cast<unsigned int>(Step::SpecifyStartPnt));
        assert(result);
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

void SketchDrawLine3DGuiCmd::gotoStep(unsigned int step)
{
    _step = step;

    this->hidePopup();
    _hoverPopupState.resetValue();

    switch (static_cast<Step>(step))
    {
    case Step::SpecifyStartPnt:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawLine3DGuiCmd",
            "Specify the start point; you can directly input the coordinate values. "
            "Press Space to switch the drawing plane."));
    }
    break;

    case Step::SpecifyEndPnt:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawLine3DGuiCmd",
            "Specify the end point; you can directly input the length and angle. "
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

void SketchDrawLine3DGuiCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

void SketchDrawLine3DGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (event.x != _hoverPopupState.lastMouseX ||
        event.y != _hoverPopupState.lastMouseY)
    {
        this->hidePopup();
        _hoverPopupState.lastMouseX = event.x;
        _hoverPopupState.lastMouseY = event.y;
        _hoverPopupState.lastMouseMoveTime = event.time;
    }

    if (_step == static_cast<unsigned int>(Step::SpecifyStartPnt))
    {
        _hoverPopupState.point = this->computePoint3d(event.x, event.y).first;
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyEndPnt))
    {
        std::pair<wy::Vector3, bool> ret = this->computePoint3d(event.x, event.y);
        wy::Vector3 endPnt = ret.first;
        _hoverPopupState.point = endPnt;
        _hoverPopupState.snapped = ret.second;

        if (!_hoverPopupState.snapped)
        {
            wy::Vector3 delta = endPnt - _startPnt;
            const wy3d::SketchPlane& plane = this->getWorkingPlane();
            _hoverPopupState.length = delta.length();
            _hoverPopupState.angleDeg = wy3d::radiansToDegrees(
                std::atan2(delta.dot(plane.getYDir()), delta.dot(plane.getXDir())));
        }
        if (_pMakeSketchLine3D)
        {
            _pMakeSketchLine3D->update(endPnt);
        }
    }

    return;
}

void SketchDrawLine3DGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    this->hidePopup();
    _hoverPopupState.lastMouseX = event.x;
    _hoverPopupState.lastMouseY = event.y;
    _hoverPopupState.lastMouseMoveTime = event.time;

    if (_step == static_cast<unsigned int>(Step::SpecifyStartPnt))
    {
        _startPnt = this->computePoint3d(event.x, event.y).first;
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyEndPnt))
    {
        _endPnt = this->computePoint3d(event.x, event.y).first;
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

void SketchDrawLine3DGuiCmd::initializePopups()
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
        _pXYZPopup->hide();
    }

    if (!_pLengthAnglePopup)
    {
        _pLengthAnglePopup = std::make_unique<GuiCmdHoverInputPopup2>(
            QCoreApplication::translate("SketchDrawLine3DGuiCmd", "Length"),
            QCoreApplication::translate("SketchDrawLine3DGuiCmd", "Angle"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pLengthAnglePopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pLengthAnglePopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pLengthAnglePopup->hide();
    }
}

void SketchDrawLine3DGuiCmd::showPopup()
{
    if (!_pXYZPopup || !_pLengthAnglePopup)
    {
        this->initializePopups();
    }

    if (_step == static_cast<unsigned int>(Step::SpecifyStartPnt))
    {
        if (!_pXYZPopup) return;
        if (_pLengthAnglePopup) _pLengthAnglePopup->hide();

        _pXYZPopup->setValues(
            _hoverPopupState.point.x(),
            _hoverPopupState.point.y(),
            _hoverPopupState.point.z());
        _pXYZPopup->showAtGlobal(QCursor::pos());
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyEndPnt))
    {
        if (_hoverPopupState.snapped)
        {
            if (!_pXYZPopup) return;
            if (_pLengthAnglePopup) _pLengthAnglePopup->hide();

            _pXYZPopup->setValues(
                _hoverPopupState.point.x(),
                _hoverPopupState.point.y(),
                _hoverPopupState.point.z());
            _pXYZPopup->showAtGlobal(QCursor::pos());
        }
        else
        {
            if (!_pLengthAnglePopup) return;
            if (_pXYZPopup) _pXYZPopup->hide();

            _pLengthAnglePopup->setValues(
                _hoverPopupState.length,
                _hoverPopupState.angleDeg);
            _pLengthAnglePopup->showAtGlobal(QCursor::pos());
        }
    }
}

void SketchDrawLine3DGuiCmd::hidePopup()
{
    if (_pXYZPopup && _pXYZPopup->isVisible())
    {
        _pXYZPopup->hide();
    }
    if (_pLengthAnglePopup && _pLengthAnglePopup->isVisible())
    {
        _pLengthAnglePopup->hide();
    }
}

void SketchDrawLine3DGuiCmd::tryShowPopupOnHover(double time)
{
    if (_step != static_cast<unsigned int>(Step::SpecifyStartPnt) &&
        _step != static_cast<unsigned int>(Step::SpecifyEndPnt))
    {
        return;
    }
    if (_hoverPopupState.lastMouseMoveTime < 0.0)
    {
        return;
    }
    if ((_pXYZPopup && _pXYZPopup->isVisible()) ||
        (_pLengthAnglePopup && _pLengthAnglePopup->isVisible()))
    {
        return;
    }
    if (time - _hoverPopupState.lastMouseMoveTime >= 0.45)
    {
        this->showPopup();
    }
}

void SketchDrawLine3DGuiCmd::onPopupEnterKey()
{
    if (_step == static_cast<unsigned int>(Step::SpecifyStartPnt))
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
        _startPnt.set(x, y, z);
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyEndPnt))
    {
        if (_hoverPopupState.snapped)
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
            _endPnt.set(x, y, z);
        }
        else
        {
            if (!_pLengthAnglePopup)
            {
                return;
            }

            double length(0.0);
            if (!parseDoubleText(_pLengthAnglePopup->getRow1Text(), length))
            {
                return;
            }

            QString angleText = _pLengthAnglePopup->getRow2Text().trimmed();
            double angleDeg(_hoverPopupState.angleDeg);
            if (!angleText.isEmpty() && !parseDoubleText(angleText, angleDeg))
            {
                return;
            }

            const wy3d::SketchPlane& plane = this->getWorkingPlane();
            const double angle = wy3d::degreesToRadians(angleDeg);
            _endPnt = _startPnt
                + plane.getXDir() * (length * std::cos(angle))
                + plane.getYDir() * (length * std::sin(angle));
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

void SketchDrawLine3DGuiCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void SketchDrawLine3DGuiCmd::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX) return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}

std::pair<wy::Vector3, bool> SketchDrawLine3DGuiCmd::computePoint3d(double x, double y)
{
    auto ret = this->computePosition3d(x, y, this->getWorkingPlane(), this->getSnapExcludeIds(), true);
    if (ret.second)
    {
        return std::make_pair(ret.second->getPosition(), true);
    }
    return std::make_pair(ret.first, false);
}

std::set<wydb::ElementId> SketchDrawLine3DGuiCmd::getSnapExcludeIds() const
{
    std::set<wydb::ElementId> snapExcludeIds;
    if (_pMakeSketchLine3D)
    {
        _pMakeSketchLine3D->collectElements(snapExcludeIds);
    }
    return snapExcludeIds;
}

void MakeSketchLine3D::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pSketchLine3D) idSet.insert(_pSketchLine3D->getId());
}

bool MakeSketchLine3D::init(const wy::Vector3& startPnt, wydb::ElementId sketch3dId)
{
    if (!_pDb || !_pTopTrans || _pSketchLine3D || _isFinished)
    {
        return false;
    }

    // 创建SketchLine3D
    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    wy3d::Sketch3D* pSketch3D = nullptr;
    wy3d::SketchLine3D* pSketchLine3D = nullptr;
    wy::Vector3 endPnt;
    wydb::Element* pSketchElem = pTrans->getElementForWrite(sketch3dId);
    if (!pSketchElem) goto ABORT_TRANS;
    pSketch3D = wy3d::Sketch3D::cast(pSketchElem);
    if (!pSketch3D) goto ABORT_TRANS;

    endPnt = startPnt + wy::Vector3(wy3d::kMinValue, 0.0, 0.0);
    if (wy::ErrorStatus::Ok != wy3d::SketchLine3D::create(pTrans, startPnt, endPnt, pSketchLine3D) || !pSketchLine3D)
    {
        goto ABORT_TRANS;
    }
    _pSketchLine3D = pSketchLine3D;
    if (wy::ErrorStatus::Ok != pSketch3D->addEntity(pSketchLine3D))
    {
        goto ABORT_TRANS;
    }
    _pDb->getTransactionManager()->endTransaction();
    _pSketchLine3D = pSketchLine3D;
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    _pSketchLine3D = nullptr;
    return false;
}

bool MakeSketchLine3D::update(const wy::Vector3& endPnt)
{
    if (!_pDb || !_pTopTrans || !_pSketchLine3D || _isFinished)
    {
        return false;
    }
    if ((endPnt - _pSketchLine3D->getStartPoint()).length() < wy3d::kMinValue)
    {
        return false;
    }

    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    {
        _pSketchLine3D->upgradeForWrite();
        _pSketchLine3D->setEndPoint(endPnt);
    }
    if (wy::ErrorStatus::Ok == _pDb->getTransactionManager()->endTransaction())
    {
        wydb::TransactionManager* pTransMgr = _pDb->getTransactionManager();
        pTransMgr->mergeTransaction();
    }
    return true;
}
