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

#include "SketchDrawSpline3DGuiCmd.h"

#include <cassert>
#include <cmath>
#include <utility>

#include <QCoreApplication>
#include <QCursor>
#include <QString>

#include <osg/LineStipple>

#include <wyVector3.h>
#include <wyapSelManager.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dMath.h>
#include <wy3dSketch3D.h>
#include <wy3dImpl.h>

#include "application/Application.h"
#include "commands/sketch/dialogs/GuiCmdHoverInputPopup.h"
#include "gizmo/OsgGizmoNode.h"
#include "scene/Colors.h"
#include "snap/SnapConsts.h"
#include "utils/GuiCommandUtil.h"
#include "widgets/frame/MainWindow.h"

static bool parseDoubleText(const QString& text, double& value)
{
    bool ok(false);
    value = text.trimmed().toDouble(&ok);
    return ok;
}

SketchDrawSpline3DGuiCmd::SketchDrawSpline3DGuiCmd() : Sketch3DDrawGuiCmd(),
    _splineMode(wy3d::SplineMode::InterpolationPoints), _closureMinPoints(4),
    _step(0), _startPoint(), _nextPoint(), _points(), _pMakeSketchSpline3D(nullptr),
    _pXYZPopup(nullptr), _hoverPopupState(),
    _pointTransients(), _pathTransients(), _pActivePathTransient(nullptr), _pStartPointSnapObject(nullptr)
{
}

SketchDrawSpline3DGuiCmd::~SketchDrawSpline3DGuiCmd()
{
}

wyap::CmdExecution::StartResult SketchDrawSpline3DGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = Sketch3DDrawGuiCmd::onStart();
    if (wyap::CmdExecution::StartResult::Succeeded != ret)
    {
        return ret;
    }

    this->gotoStep(static_cast<unsigned int>(Step::SpecifyStartPnt));
    return ret;
}

void SketchDrawSpline3DGuiCmd::cleanup()
{
    _step = 0;
    _startPoint.set(0.0, 0.0, 0.0);
    _nextPoint.set(0.0, 0.0, 0.0);
    _points.clear();
    _pMakeSketchSpline3D = nullptr;

    this->clearTransients();
    this->removeStartPointSnapObject();

    this->hidePopup();
    _hoverPopupState.resetValue();
}

void SketchDrawSpline3DGuiCmd::reset()
{
    this->cleanup();
    this->gotoStep(static_cast<unsigned int>(Step::SpecifyStartPnt));
}

void SketchDrawSpline3DGuiCmd::onEscapeKey()
{
    this->hidePopup();

    switch (static_cast<Step>(_step))
    {
    case Step::SpecifyNextPnt:
    {
        if (!_pMakeSketchSpline3D)
        {
            assert(false);
            this->requestAbort(AbortCause::UserCancel);
            return;
        }
        // 不足两点则丢弃,够两点就提交成实体(ESC 与双击都走这里)
        if (_points.size() >= 2)
        {
            if (_pMakeSketchSpline3D->update(_points))
            {
                _pMakeSketchSpline3D->commit();
            }
        }
        _pMakeSketchSpline3D = nullptr;
        this->reset();
        this->simulateMouseMoveFromPopup();
        return;
    }
    break;

    case Step::SpecifyStartPnt:
    case Step::Undefined:
    default:
    {
        this->requestAbort(AbortCause::UserCancel);
        return;
    }
    break;
    }
}

bool SketchDrawSpline3DGuiCmd::finishStep(unsigned int step)
{
    if (Step::SpecifyStartPnt == static_cast<Step>(step))
    {
        wydb::Database* pDb = Application::instance().getActiveDatabase();
        if (!pDb)
        {
            assert(false);
            return false;
        }

        _pMakeSketchSpline3D = std::make_shared<MakeSketchSpline3D>(this, _splineMode);
        // 种子方向取工作平面 xDir,使退化种子落在当前平面上
        if (!_pMakeSketchSpline3D->init(_startPoint, this->getWorkingPlane().getXDir(), _sketch3DInfo.sketch3dId))
        {
            _pMakeSketchSpline3D = nullptr;
            return false;
        }

        _points.push_back(_startPoint);
        this->updatePointTransient(_startPoint);

        this->gotoStep(static_cast<unsigned int>(Step::SpecifyNextPnt));
        return true;
    }

    if (Step::SpecifyNextPnt == static_cast<Step>(step))
    {
        if (!_pMakeSketchSpline3D)
        {
            assert(false);
            return false;
        }
        if (!_pMakeSketchSpline3D->update(_points, _nextPoint))
        {
            return false;
        }
        _points.push_back(_nextPoint);
        this->updatePointTransient(_nextPoint);
        this->updatePolygonTransients();

        // 点回第一个点 -> 闭合提交
        if (this->isClosed(_points))
        {
            _pMakeSketchSpline3D->commit();
            _pMakeSketchSpline3D = nullptr;
            this->reset();
            return true;
        }

        this->updateStartPointSnapObject();
        return true;
    }

    assert(false);
    return false;
}

void SketchDrawSpline3DGuiCmd::gotoStep(unsigned int step)
{
    _step = step;
    this->hidePopup();
    _hoverPopupState.resetValue();

    switch (static_cast<Step>(step))
    {
    case Step::SpecifyStartPnt:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawSpline3DGuiCmd",
            "Specify the start point; you can directly input the coordinate values. "
            "Press Space to switch the drawing plane."));
    }
    break;

    case Step::SpecifyNextPnt:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawSpline3DGuiCmd",
            "Specify the next point; you can directly input the coordinate values. "
            "Press Space to switch the drawing plane. Esc or double-click finishes the spline."));
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

void SketchDrawSpline3DGuiCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

bool SketchDrawSpline3DGuiCmd::isClosed(const std::vector<wy::Vector3>& points) const
{
    if (points.size() < _closureMinPoints) return false;
    return points.front() == points.back();
}

bool SketchDrawSpline3DGuiCmd::isAllowedClosurePoint(const std::vector<wy::Vector3>& points, const wy::Vector3& nextPoint) const
{
    // 落点后若能满足闭合点数,且与首点重合,则是"点回首点闭合"
    if (points.size() + 1 < _closureMinPoints) return false;
    if (points.empty()) return false;
    return nextPoint == points.front();
}

bool SketchDrawSpline3DGuiCmd::isDisallowedDuplicatePoint(const std::vector<wy::Vector3>& points, const wy::Vector3& nextPoint) const
{
    for (const wy::Vector3& pnt : points)
    {
        if ((nextPoint - pnt).length() <= wy3d::TOL)
        {
            return !this->isAllowedClosurePoint(points, nextPoint);
        }
    }
    return false;
}

wy::Vector3 SketchDrawSpline3DGuiCmd::tryReviseNextPoint(const std::vector<wy::Vector3>& points, const wy::Vector3& nextPoint) const
{
    // 够闭合点数时把光标吸到首点上,便于点回首点闭合
    if (points.size() + 1 < _closureMinPoints) return nextPoint;
    if (points.empty()) return nextPoint;
    if ((nextPoint - points.front()).length() <= wy3d::TOL) return points.front();
    return nextPoint;
}

void SketchDrawSpline3DGuiCmd::updateStartPointSnapObject()
{
    // 武装一次即可(首点不会变),重复 add 会被 SnapSystem 断言
    if (_pStartPointSnapObject) return;
    if (_points.size() + 1 < _closureMinPoints) return;

    wyap::SnapSystem* pSnapSys = Application::instance().getSnapSystem();
    if (!pSnapSys)
    {
        assert(false);
        return;
    }

    _pStartPointSnapObject = std::make_shared<SnapCoordinatePoint>(_points.front());
    pSnapSys->beginChange();
    pSnapSys->addResidentSnapObject(_pStartPointSnapObject);
    pSnapSys->endChange();
}

void SketchDrawSpline3DGuiCmd::removeStartPointSnapObject()
{
    if (!_pStartPointSnapObject) return;

    wyap::SnapSystem* pSnapSys = Application::instance().getSnapSystem();
    if (pSnapSys)
    {
        pSnapSys->beginChange();
        pSnapSys->removeResidentSnapObject(_pStartPointSnapObject);
        pSnapSys->endChange();
    }
    else
    {
        assert(false);
    }
    _pStartPointSnapObject = nullptr;
}

void SketchDrawSpline3DGuiCmd::clearTransients()
{
    _pointTransients.clear();
    _pathTransients.clear();
    _pActivePathTransient = nullptr;
}

void SketchDrawSpline3DGuiCmd::updatePointTransient(const wy::Vector3& pnt)
{
    PointTransientSPtr pPointTransient = std::make_shared<PointTransient>(pnt,
        OsgGizmoNode::SKETCH_ENTITY_COLOR, SnapConsts::PickSize);
    pPointTransient->show();
    _pointTransients.emplace_back(pPointTransient);
}

void SketchDrawSpline3DGuiCmd::updatePolygonTransients()
{
    // 控制点式:已落点之间用虚线连出控制多边形
    if (wy3d::SplineMode::ControlPoints != _splineMode) return;
    if (_points.size() < 2) return;

    LineTransientSPtr pPathLineTransient = std::make_shared<LineTransient>(
        new osg::LineStipple(CENTER_LINE_STIPPLE_FACTOR, CENTER_LINE_STIPPLE_PATTERN));
    pPathLineTransient->update(_points[_points.size() - 2], _points.back());
    pPathLineTransient->show();
    _pathTransients.emplace_back(pPathLineTransient);

    if (!_pActivePathTransient)
    {
        _pActivePathTransient = std::make_shared<LineTransient>(
            new osg::LineStipple(CENTER_LINE_STIPPLE_FACTOR, CENTER_LINE_STIPPLE_PATTERN));
    }
}

void SketchDrawSpline3DGuiCmd::updateActivePathTransient(const wy::Vector3& nextPnt)
{
    if (!_pActivePathTransient) return;
    if (_points.empty()) return;
    _pActivePathTransient->update(_points.back(), nextPnt);
    _pActivePathTransient->show();
}

void SketchDrawSpline3DGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (event.x != _hoverPopupState.lastMouseX ||
        event.y != _hoverPopupState.lastMouseY)
    {
        this->hidePopup();
        _hoverPopupState.lastMouseX = event.x;
        _hoverPopupState.lastMouseY = event.y;
        _hoverPopupState.lastMouseMoveTime = event.time;
    }

    wy::Vector3 nextPoint = this->computePoint3d(event.x, event.y).first;
    nextPoint = this->tryReviseNextPoint(_points, nextPoint);
    _hoverPopupState.point = nextPoint;

    if (_step == static_cast<unsigned int>(Step::SpecifyStartPnt))
    {
        return;
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyNextPnt))
    {
        // 与已落点重合(非闭合点)时保留上一帧预览
        if (this->isDisallowedDuplicatePoint(_points, nextPoint)) return;
        if (_pMakeSketchSpline3D)
        {
            _pMakeSketchSpline3D->update(_points, nextPoint);
        }
        this->updateActivePathTransient(nextPoint);
    }
    else
    {
        assert(false);
    }

    return;
}

void SketchDrawSpline3DGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    this->hidePopup();
    _hoverPopupState.lastMouseX = event.x;
    _hoverPopupState.lastMouseY = event.y;
    _hoverPopupState.lastMouseMoveTime = event.time;

    if (_step == static_cast<unsigned int>(Step::SpecifyStartPnt))
    {
        _startPoint = this->computePoint3d(event.x, event.y).first;
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyNextPnt))
    {
        _nextPoint = this->computePoint3d(event.x, event.y).first;
        _nextPoint = this->tryReviseNextPoint(_points, _nextPoint);
        if (this->isDisallowedDuplicatePoint(_points, _nextPoint))
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

void SketchDrawSpline3DGuiCmd::onLeftMouseDoubleClicked(const MouseEvent& event)
{
    // 双击 = 结束当前样条(与 2D 样条一致)
    if (_step == static_cast<unsigned int>(Step::SpecifyNextPnt))
    {
        this->onEscapeKey();
    }
}

void SketchDrawSpline3DGuiCmd::initializePopups()
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

void SketchDrawSpline3DGuiCmd::showPopup()
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

void SketchDrawSpline3DGuiCmd::hidePopup()
{
    if (_pXYZPopup && _pXYZPopup->isVisible())
    {
        _pXYZPopup->hide();
    }
}

void SketchDrawSpline3DGuiCmd::tryShowPopupOnHover(double time)
{
    if (_step != static_cast<unsigned int>(Step::SpecifyStartPnt) &&
        _step != static_cast<unsigned int>(Step::SpecifyNextPnt))
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

void SketchDrawSpline3DGuiCmd::onPopupEnterKey()
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

    // 替换"当前正在指定的那个点",再走同一状态机
    if (_step == static_cast<unsigned int>(Step::SpecifyStartPnt))
    {
        _startPoint.set(x, y, z);
    }
    else if (_step == static_cast<unsigned int>(Step::SpecifyNextPnt))
    {
        _nextPoint.set(x, y, z);
        if (this->isDisallowedDuplicatePoint(_points, _nextPoint))
        {
            return;
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

void SketchDrawSpline3DGuiCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void SketchDrawSpline3DGuiCmd::onPopupSpaceKey()
{
    this->onSpaceKey();
    this->hidePopup();
    this->simulateMouseMoveFromPopup();
}

void SketchDrawSpline3DGuiCmd::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX)
        return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}

std::pair<wy::Vector3, bool> SketchDrawSpline3DGuiCmd::computePoint3d(double x, double y)
{
    auto ret = this->computePosition3d(x, y, this->getWorkingPlane(), this->getSnapExcludeIds(), true);
    if (ret.second)
    {
        return std::make_pair(ret.second->getPosition(), true);
    }
    return std::make_pair(ret.first, false);
}

std::set<wydb::ElementId> SketchDrawSpline3DGuiCmd::getSnapExcludeIds() const
{
    std::set<wydb::ElementId> snapExcludeIds;
    // 只排除正在预览的图元,已提交的3D草图图元可作捕捉源(与2D草绘行为一致)
    if (_pMakeSketchSpline3D)
    {
        _pMakeSketchSpline3D->collectElements(snapExcludeIds);
    }
    return snapExcludeIds;
}

void MakeSketchSpline3D::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pSketchSpline3D) idSet.insert(_pSketchSpline3D->getId());
}

bool MakeSketchSpline3D::init(const wy::Vector3& startPnt, const wy::Vector3& seedDir, wydb::ElementId sketch3dId)
{
    if (!_pDb || !_pTopTrans || _pSketchSpline3D || _isFinished)
    {
        return false;
    }

    std::vector<wy::Vector3> points;
    points.emplace_back(startPnt);
    points.emplace_back(startPnt + seedDir * wy3d::kMinValue);

    // 创建SketchSpline3D
    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    wy3d::Sketch3D* pSketch3D = nullptr;
    wy3d::SketchSpline3D* pSketchSpline3D = nullptr;
    wydb::Element* pSketchElem = pTrans->getElementForWrite(sketch3dId);
    if (!pSketchElem) goto ABORT_TRANS;
    pSketch3D = wy3d::Sketch3D::cast(pSketchElem);
    if (!pSketch3D) goto ABORT_TRANS;

    // 控制点式的种子用 1 次(2 点撑不起更高次数,会被降次)
    wy::ErrorStatus error = wy::ErrorStatus::Ok;
    if (wy3d::SplineMode::ControlPoints == _mode)
    {
        error = wy3d::SketchSpline3D::create(pTrans, 1u, points, pSketchSpline3D);
    }
    else
    {
        error = wy3d::SketchSpline3D::create(pTrans, points, pSketchSpline3D);
    }
    if (wy::ErrorStatus::Ok != error || !pSketchSpline3D)
    {
        goto ABORT_TRANS;
    }

    _pSketchSpline3D = pSketchSpline3D;
    if (wy::ErrorStatus::Ok != pSketch3D->addEntity(pSketchSpline3D))
    {
        goto ABORT_TRANS;
    }
    _pDb->getTransactionManager()->endTransaction();
    _pSketchSpline3D = pSketchSpline3D;
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    _pSketchSpline3D = nullptr;
    return false;
}

bool MakeSketchSpline3D::update(const std::vector<wy::Vector3>& points)
{
    if (!_pDb || !_pTopTrans || !_pSketchSpline3D || _isFinished)
    {
        return false;
    }
    if (points.size() < 2)
    {
        return false;
    }

    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    {
        _pSketchSpline3D->upgradeForWrite();
        if (wy3d::SplineMode::ControlPoints == _pSketchSpline3D->getMode())
        {
            // 次数按点数定:2 点 1 次、3 点 2 次、更多 3 次(镜像 2D)
            std::uint32_t degree = 3;
            if (2 == points.size()) degree = 1;
            else if (3 == points.size()) degree = 2;
            if (wy::ErrorStatus::Ok != _pSketchSpline3D->setDegree(degree))
            {
                _pDb->getTransactionManager()->abortTransaction();
                return false;
            }
        }
        if (wy::ErrorStatus::Ok != _pSketchSpline3D->setPoints(points))
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

bool MakeSketchSpline3D::update(const std::vector<wy::Vector3>& points, const wy::Vector3& nextPnt)
{
    _pnts = points;
    _pnts.emplace_back(nextPnt);
    return this->update(_pnts);
}
