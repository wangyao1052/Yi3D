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

#include "SketchDrawRectangle3DGuiCmd.h"

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

SketchDrawRectangle3DGuiCmd::SketchDrawRectangle3DGuiCmd() : Sketch3DDrawGuiCmd(),
    _mode(MakeSketchRectangle3D::Mode::CornerRect),
    _step(0), _startPnt(), _endPnt(), _pMakeSketchRectangle3D(nullptr),
    _pXYZPopup(nullptr), _pLengthWidthPopup(nullptr), _hoverPopupState(),
    _pDiagonal1st(nullptr), _pDiagonal2nd(nullptr)
{
}

SketchDrawRectangle3DGuiCmd::~SketchDrawRectangle3DGuiCmd()
{
}

wyap::CmdExecution::StartResult SketchDrawRectangle3DGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = Sketch3DDrawGuiCmd::onStart();
    if (wyap::CmdExecution::StartResult::Succeeded != ret)
    {
        return ret;
    }

    this->gotoStep(static_cast<unsigned int>(Step::SpecifyStartPnt));
    return ret;
}

void SketchDrawRectangle3DGuiCmd::cleanup()
{
    _step = 0;
    _startPnt.set(0.0, 0.0, 0.0);
    _endPnt.set(0.0, 0.0, 0.0);
    _pMakeSketchRectangle3D = nullptr;
    this->destroyDiagonalTransients();

    this->hidePopup();
    _hoverPopupState.resetValue();
}

void SketchDrawRectangle3DGuiCmd::onEscapeKey()
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

bool SketchDrawRectangle3DGuiCmd::finishStep(unsigned int step)
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
        // 先让工作平面原点落在锚点,再建图元:切平面后角点仍精确落在平面上
        this->moveWorkPlaneOriginTo(_startPnt);

        _pMakeSketchRectangle3D = std::make_shared<MakeSketchRectangle3D>(this, _mode);
        if (!_pMakeSketchRectangle3D->init(_startPnt, this->getWorkingPlane(), _sketch3DInfo.sketch3dId))
        {
            _pMakeSketchRectangle3D = nullptr;
            return false;
        }

        if (MakeSketchRectangle3D::Mode::CenterRect == _mode)
        {
            _pDiagonal1st = std::make_shared<LineTransient>();
            _pDiagonal2nd = std::make_shared<LineTransient>();
            this->updateDiagonalTransients();
        }

        this->gotoStep(static_cast<unsigned int>(Step::SpecifyEndPnt));
        return true;
    }
    break;

    case Step::SpecifyEndPnt:
    {
        if (!_pMakeSketchRectangle3D)
        {
            assert(false);
            return false;
        }
        if (!_pMakeSketchRectangle3D->update(_endPnt))
        {
            return false;
        }
        _pMakeSketchRectangle3D->commit();
        _pMakeSketchRectangle3D = nullptr;
        this->destroyDiagonalTransients();

        this->gotoStep(static_cast<unsigned int>(Step::SpecifyStartPnt));
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

void SketchDrawRectangle3DGuiCmd::gotoStep(unsigned int step)
{
    _step = step;
    this->hidePopup();
    _hoverPopupState.resetValue();

    switch (static_cast<Step>(step))
    {
    case Step::SpecifyStartPnt:
    {
        Application::instance().getStatusBar()->setTips(this->getStartTip());
    }
    break;

    case Step::SpecifyEndPnt:
    {
        Application::instance().getStatusBar()->setTips(this->getEndTip());
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

void SketchDrawRectangle3DGuiCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

QString SketchDrawRectangle3DGuiCmd::getStartTip() const
{
    return QCoreApplication::translate("SketchDrawRectangle3DGuiCmd",
        "Specify the first corner point; you can directly input the coordinate values. "
        "Press Space to switch the drawing plane.");
}

QString SketchDrawRectangle3DGuiCmd::getEndTip() const
{
    return QCoreApplication::translate("SketchDrawRectangle3DGuiCmd",
        "Specify the other corner point or input length & width. "
        "Press Space to switch the drawing plane.");
}

void SketchDrawRectangle3DGuiCmd::onSpaceKey()
{
    Sketch3DDrawGuiCmd::onSpaceKey();

    if (_step != static_cast<unsigned int>(Step::SpecifyEndPnt) || !_pMakeSketchRectangle3D)
    {
        return;
    }

    // 长宽不变,矩形绕锚点转到新平面
    const wy3d::SketchPlane& plane = this->getWorkingPlane();
    if (!_pMakeSketchRectangle3D->updatePlane(plane))
    {
        return;
    }

    // 同步悬停状态:切面后旧世界点在新平面里的uv会变号,不同步会画到反侧
    wy::Vector2 uvOffset;
    if (!_pMakeSketchRectangle3D->getUVOffset(uvOffset))
    {
        return;
    }
    _hoverPopupState.point = plane.value(plane.uv(_startPnt) + uvOffset);
    this->updateHoverExtents(uvOffset);
    this->updateDiagonalTransients();

    this->hidePopup();
}

void SketchDrawRectangle3DGuiCmd::onMouseMove(const MouseEvent& event)
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
    _hoverPopupState.snapped = ret.second;

    if (_step == static_cast<unsigned int>(Step::SpecifyStartPnt))
    {
        return;
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyEndPnt))
    {
        const wy3d::SketchPlane& plane = this->getWorkingPlane();
        this->updateHoverExtents(plane.uv(ret.first) - plane.uv(_startPnt));
        if (_pMakeSketchRectangle3D)
        {
            _pMakeSketchRectangle3D->update(ret.first);
            this->updateDiagonalTransients();
        }
    }
    else
    {
        assert(false);
    }

    return;
}

void SketchDrawRectangle3DGuiCmd::onLeftMouseDown(const MouseEvent& event)
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

void SketchDrawRectangle3DGuiCmd::updateHoverExtents(const wy::Vector2& uvOffset)
{
    const double scale = (MakeSketchRectangle3D::Mode::CenterRect == _mode) ? 2.0 : 1.0;
    _hoverPopupState.length = std::fabs(uvOffset.x()) * scale;
    _hoverPopupState.width = std::fabs(uvOffset.y()) * scale;
}

void SketchDrawRectangle3DGuiCmd::updateDiagonalTransients()
{
    if (!_pDiagonal1st || !_pDiagonal2nd || !_pMakeSketchRectangle3D)
    {
        return;
    }

    wy::Vector3 pnt1;
    wy::Vector3 pnt2;
    wy::Vector3 pnt3;
    wy::Vector3 pnt4;
    if (!_pMakeSketchRectangle3D->getCorners(pnt1, pnt2, pnt3, pnt4))
    {
        return;
    }

    _pDiagonal1st->update(pnt1, pnt3);
    _pDiagonal2nd->update(pnt2, pnt4);
}

void SketchDrawRectangle3DGuiCmd::destroyDiagonalTransients()
{
    _pDiagonal1st = nullptr;
    _pDiagonal2nd = nullptr;
}

void SketchDrawRectangle3DGuiCmd::initializePopups()
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

    if (!_pLengthWidthPopup)
    {
        _pLengthWidthPopup = std::make_unique<GuiCmdHoverInputPopup2>(
            QCoreApplication::translate("SketchDrawRectangle3DGuiCmd", "Length"),
            QCoreApplication::translate("SketchDrawRectangle3DGuiCmd", "Width"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pLengthWidthPopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pLengthWidthPopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pLengthWidthPopup->setSpaceKeyHandler([this]() { this->onPopupSpaceKey(); });
        _pLengthWidthPopup->hide();
    }
}

void SketchDrawRectangle3DGuiCmd::showPopup()
{
    if (!_pXYZPopup || !_pLengthWidthPopup)
    {
        this->initializePopups();
    }

    if (_step == static_cast<unsigned int>(Step::SpecifyStartPnt))
    {
        if (!_pXYZPopup) return;
        if (_pLengthWidthPopup) _pLengthWidthPopup->hide();

        _pXYZPopup->setValues(
            _hoverPopupState.point.x(),
            _hoverPopupState.point.y(),
            _hoverPopupState.point.z());
        _pXYZPopup->showAtGlobal(QCursor::pos());
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyEndPnt))
    {
        // 捕捉命中时显示坐标,否则显示长宽
        if (_hoverPopupState.snapped)
        {
            if (!_pXYZPopup) return;
            if (_pLengthWidthPopup) _pLengthWidthPopup->hide();

            _pXYZPopup->setValues(
                _hoverPopupState.point.x(),
                _hoverPopupState.point.y(),
                _hoverPopupState.point.z());
            _pXYZPopup->showAtGlobal(QCursor::pos());
        }
        else
        {
            if (!_pLengthWidthPopup) return;
            if (_pXYZPopup) _pXYZPopup->hide();

            _pLengthWidthPopup->setValues(
                _hoverPopupState.length,
                _hoverPopupState.width);
            _pLengthWidthPopup->showAtGlobal(QCursor::pos());
        }
    }
}

void SketchDrawRectangle3DGuiCmd::hidePopup()
{
    if (_pXYZPopup && _pXYZPopup->isVisible())
    {
        _pXYZPopup->hide();
    }
    if (_pLengthWidthPopup && _pLengthWidthPopup->isVisible())
    {
        _pLengthWidthPopup->hide();
    }
}

void SketchDrawRectangle3DGuiCmd::tryShowPopupOnHover(double time)
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
        (_pLengthWidthPopup && _pLengthWidthPopup->isVisible()))
    {
        return;
    }
    if (time - _hoverPopupState.lastMouseMoveTime >= 0.45)
    {
        this->showPopup();
    }
}

void SketchDrawRectangle3DGuiCmd::onPopupEnterKey()
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
            if (!_pLengthWidthPopup)
            {
                return;
            }

            double length(0.0);
            if (!parseDoubleText(_pLengthWidthPopup->getRow1Text(), length))
            {
                return;
            }
            // 宽度留空视为正方形
            double width(0.0);
            QString widthText = _pLengthWidthPopup->getRow2Text().trimmed();
            if (widthText.isEmpty())
            {
                width = length;
            }
            else if (!parseDoubleText(widthText, width))
            {
                return;
            }

            const wy3d::SketchPlane& plane = this->getWorkingPlane();
            const wy::Vector2 uvStart = plane.uv(_startPnt);
            const wy::Vector2 uvHover = plane.uv(_hoverPopupState.point);
            double deltaU = (uvHover.x() < uvStart.x()) ? -length : length;
            double deltaV = (uvHover.y() < uvStart.y()) ? -width : width;
            if (MakeSketchRectangle3D::Mode::CenterRect == _mode)
            {
                deltaU *= 0.5;
                deltaV *= 0.5;
            }
            _endPnt = plane.value(uvStart.x() + deltaU, uvStart.y() + deltaV);
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

void SketchDrawRectangle3DGuiCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void SketchDrawRectangle3DGuiCmd::onPopupSpaceKey()
{
    this->onSpaceKey();
    this->hidePopup();
}

void SketchDrawRectangle3DGuiCmd::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX)
        return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}

std::pair<wy::Vector3, bool> SketchDrawRectangle3DGuiCmd::computePoint3d(double x, double y)
{
    auto ret = this->computePosition3d(x, y, this->getWorkingPlane(), this->getSnapExcludeIds(), true);
    if (ret.second)
    {
        return std::make_pair(ret.second->getPosition(), true);
    }
    return std::make_pair(ret.first, false);
}

std::set<wydb::ElementId> SketchDrawRectangle3DGuiCmd::getSnapExcludeIds() const
{
    std::set<wydb::ElementId> snapExcludeIds;
    // 只排除正在预览的图元,已提交的3D草图图元可作捕捉源(与2D草绘行为一致)
    if (_pMakeSketchRectangle3D)
    {
        _pMakeSketchRectangle3D->collectElements(snapExcludeIds);
    }
    return snapExcludeIds;
}

void MakeSketchRectangle3D::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pSketchLine1st) idSet.insert(_pSketchLine1st->getId());
    if (_pSketchLine2nd) idSet.insert(_pSketchLine2nd->getId());
    if (_pSketchLine3rd) idSet.insert(_pSketchLine3rd->getId());
    if (_pSketchLine4th) idSet.insert(_pSketchLine4th->getId());
}

void MakeSketchRectangle3D::computeRectEndPoints(const wy::Vector2& startPnt, const wy::Vector2& endPnt,
    wy::Vector2& pnt1, wy::Vector2& pnt2, wy::Vector2& pnt3, wy::Vector2& pnt4) const
{
    switch (_mode)
    {
    case Mode::CenterRect:
    {
        double deltaX = endPnt.x() - startPnt.x();
        double deltaY = endPnt.y() - startPnt.y();
        pnt1.set(startPnt.x() - deltaX, startPnt.y() - deltaY);
        pnt2.set(endPnt.x(), startPnt.y() - deltaY);
        pnt3 = endPnt;
        pnt4.set(startPnt.x() - deltaX, endPnt.y());
    }
    break;

    case Mode::CornerRect:
    default:
    {
        pnt1 = startPnt;
        pnt2.set(endPnt.x(), startPnt.y());
        pnt3 = endPnt;
        pnt4.set(startPnt.x(), endPnt.y());
    }
    break;
    }
}

bool MakeSketchRectangle3D::checkValid(const wy::Vector2& pnt1, const wy::Vector2& pnt2,
    const wy::Vector2& pnt3, const wy::Vector2& pnt4) const
{
    return (pnt2 - pnt1).length() >= wy3d::kMinValue &&
        (pnt3 - pnt2).length() >= wy3d::kMinValue &&
        (pnt4 - pnt3).length() >= wy3d::kMinValue &&
        (pnt1 - pnt4).length() >= wy3d::kMinValue;
}

bool MakeSketchRectangle3D::buildCorners(const wy::Vector2& startUv, const wy::Vector2& endUv)
{
    wy::Vector2 pnt1;
    wy::Vector2 pnt2;
    wy::Vector2 pnt3;
    wy::Vector2 pnt4;
    this->computeRectEndPoints(startUv, endUv, pnt1, pnt2, pnt3, pnt4);
    if (!this->checkValid(pnt1, pnt2, pnt3, pnt4))
    {
        return false;
    }

    // 四个角点全部由平面反算,避免与捕捉到的原始点混用而歪斜
    _cornerPnt[0] = _plane.value(pnt1);
    _cornerPnt[1] = _plane.value(pnt2);
    _cornerPnt[2] = _plane.value(pnt3);
    _cornerPnt[3] = _plane.value(pnt4);
    return true;
}

bool MakeSketchRectangle3D::applyCorners()
{
    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    {
        _pSketchLine1st->upgradeForWrite();
        _pSketchLine2nd->upgradeForWrite();
        _pSketchLine3rd->upgradeForWrite();
        _pSketchLine4th->upgradeForWrite();
        if (wy::ErrorStatus::Ok != _pSketchLine1st->setStartPoint(_cornerPnt[0]) ||
            wy::ErrorStatus::Ok != _pSketchLine1st->setEndPoint(_cornerPnt[1]) ||
            wy::ErrorStatus::Ok != _pSketchLine2nd->setStartPoint(_cornerPnt[1]) ||
            wy::ErrorStatus::Ok != _pSketchLine2nd->setEndPoint(_cornerPnt[2]) ||
            wy::ErrorStatus::Ok != _pSketchLine3rd->setStartPoint(_cornerPnt[2]) ||
            wy::ErrorStatus::Ok != _pSketchLine3rd->setEndPoint(_cornerPnt[3]) ||
            wy::ErrorStatus::Ok != _pSketchLine4th->setStartPoint(_cornerPnt[3]) ||
            wy::ErrorStatus::Ok != _pSketchLine4th->setEndPoint(_cornerPnt[0]))
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

bool MakeSketchRectangle3D::init(const wy::Vector3& corner1Pnt, const wy3d::SketchPlane& plane, wydb::ElementId sketch3dId)
{
    if (!_pDb || !_pTopTrans || _pSketchLine1st || _pSketchLine2nd || _pSketchLine3rd || _pSketchLine4th || _isFinished)
    {
        return false;
    }

    _corner1World = corner1Pnt;
    _plane = plane;
    // 退化种子:以锚点为起点撑出最小矩形
    if (!this->buildCorners(_plane.uv(_corner1World), _plane.uv(_corner1World) + wy::Vector2(wy3d::kMinValue, wy3d::kMinValue)))
    {
        return false;
    }

    // 创建4条SketchLine3D(同一子事务)
    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    wy3d::Sketch3D* pSketch3D = nullptr;
    wydb::Element* pSketchElem = pTrans->getElementForWrite(sketch3dId);
    if (!pSketchElem) goto ABORT_TRANS;
    pSketch3D = wy3d::Sketch3D::cast(pSketchElem);
    if (!pSketch3D) goto ABORT_TRANS;

    if (wy::ErrorStatus::Ok != wy3d::SketchLine3D::create(pTrans, _cornerPnt[0], _cornerPnt[1], _pSketchLine1st)
        || !_pSketchLine1st)
    {
        goto ABORT_TRANS;
    }
    if (wy::ErrorStatus::Ok != wy3d::SketchLine3D::create(pTrans, _cornerPnt[1], _cornerPnt[2], _pSketchLine2nd)
        || !_pSketchLine2nd)
    {
        goto ABORT_TRANS;
    }
    if (wy::ErrorStatus::Ok != wy3d::SketchLine3D::create(pTrans, _cornerPnt[2], _cornerPnt[3], _pSketchLine3rd)
        || !_pSketchLine3rd)
    {
        goto ABORT_TRANS;
    }
    if (wy::ErrorStatus::Ok != wy3d::SketchLine3D::create(pTrans, _cornerPnt[3], _cornerPnt[0], _pSketchLine4th)
        || !_pSketchLine4th)
    {
        goto ABORT_TRANS;
    }

    if (wy::ErrorStatus::Ok != pSketch3D->addEntity(_pSketchLine1st) ||
        wy::ErrorStatus::Ok != pSketch3D->addEntity(_pSketchLine2nd) ||
        wy::ErrorStatus::Ok != pSketch3D->addEntity(_pSketchLine3rd) ||
        wy::ErrorStatus::Ok != pSketch3D->addEntity(_pSketchLine4th))
    {
        goto ABORT_TRANS;
    }

    _pDb->getTransactionManager()->endTransaction();
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    _pSketchLine1st = nullptr;
    _pSketchLine2nd = nullptr;
    _pSketchLine3rd = nullptr;
    _pSketchLine4th = nullptr;
    return false;
}

bool MakeSketchRectangle3D::update(const wy::Vector3& endPnt)
{
    if (!_pDb || !_pTopTrans || !_pSketchLine1st || !_pSketchLine2nd || !_pSketchLine3rd || !_pSketchLine4th || _isFinished)
    {
        return false;
    }

    const wy::Vector2 uvStart = _plane.uv(_corner1World);
    const wy::Vector2 uvEnd = _plane.uv(endPnt);
    if (!this->buildCorners(uvStart, uvEnd))
    {
        return false;
    }
    if (!this->applyCorners())
    {
        return false;
    }

    _uvOffset = uvEnd - uvStart;
    _hasExtent = true;
    return true;
}

bool MakeSketchRectangle3D::updatePlane(const wy3d::SketchPlane& plane)
{
    if (!_hasExtent || !_pSketchLine1st || !_pSketchLine2nd || !_pSketchLine3rd || !_pSketchLine4th)
    {
        return false;
    }

    _plane = plane;
    const wy::Vector2 uvStart = _plane.uv(_corner1World);
    if (!this->buildCorners(uvStart, uvStart + _uvOffset))
    {
        return false;
    }
    return this->applyCorners();
}

bool MakeSketchRectangle3D::getUVOffset(wy::Vector2& uvOffset) const
{
    if (!_hasExtent)
    {
        return false;
    }
    uvOffset = _uvOffset;
    return true;
}

bool MakeSketchRectangle3D::getCorners(wy::Vector3& pnt1, wy::Vector3& pnt2, wy::Vector3& pnt3, wy::Vector3& pnt4) const
{
    if (!_pSketchLine1st)
    {
        return false;
    }
    pnt1 = _cornerPnt[0];
    pnt2 = _cornerPnt[1];
    pnt3 = _cornerPnt[2];
    pnt4 = _cornerPnt[3];
    return true;
}
