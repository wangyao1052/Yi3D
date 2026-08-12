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

#include "SketchDrawSplineGuiCmd.h"

#include <QCoreApplication>
#include <QCursor>
#include <QString>

#include <wyVector2.h>
#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapSelManager.h>
#include <wy3dImpl.h>
#include <wy3dSketch.h>
#include <wy3dSketchSpline.h>

#include "application/Application.h"
#include "commands/sketch/dialogs/GuiCmdHoverInputPopup.h"
#include "snap/SketchSnapSystem.h"
#include "scene/Colors.h"
#include "snap/SnapConsts.h"
#include "snap/SnapObject.h"
#include "gizmo/OsgGizmoNode.h"
#include "widgets/frame/MainWindow.h"


static bool parseDoubleText(const QString& text, double& value)
{
    bool ok(false);
    value = text.trimmed().toDouble(&ok);
    return ok;
}

SketchDrawSplineGuiCmd::SketchDrawSplineGuiCmd() : OsgGuiCommand(),
    _mode(wy3d::SplineMode::Undefined), _step(Step::Undefined), _startPoint(), _nextPoint(),
    _pXYPopup(nullptr), _hoverPopupState()
{
    _options.pointSelect = false;
    _options.boxSelect = false;
    _fitPoints.reserve(20);
}

SketchDrawSplineGuiCmd::~SketchDrawSplineGuiCmd()
{
}

wyap::CmdExecution::StartResult SketchDrawSplineGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = GuiCommand::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);
 
    _sketchInfo = GuiCommandUtil::initSketchInfo();
    if (_sketchInfo.pSketchSnapSys) _sketchInfo.pSketchSnapSys->clearSnapResult();

    // 初始化
    this->gotoStep(Step::SpecifyStartPnt);

    return wyap::CmdExecution::StartResult::Succeeded;
}

void SketchDrawSplineGuiCmd::cleanup()
{
    this->hidePopup();

    _step = Step::Undefined;
    _startPoint.set(0.0, 0.0);
    _nextPoint.set(0.0, 0.0);
    _fitPoints.clear();
    _pSnapContext = nullptr;

    _pointTransients.clear();
    _pathTransients.clear();
    _pActivePathTransient = nullptr;

    _pMakeSketchSpline = nullptr;
    _hoverPopupState.resetValue();

    // added by wangyao 2025.08.14 {
    if (_pStartPointSnapObject)
    {
        wyap::SnapSystem* pSnapSys = Application::instance().getSnapSystem();
        pSnapSys->beginChange();
        {
            pSnapSys->removeResidentSnapObject(_pStartPointSnapObject);
        }
        pSnapSys->endChange();
    }
    _pStartPointSnapObject = nullptr;
    // }
}

void SketchDrawSplineGuiCmd::reset()
{
    this->cleanup();
    this->gotoStep(Step::SpecifyStartPnt);
}

void SketchDrawSplineGuiCmd::onEscapeKey()
{
    this->hidePopup();

    switch (_step)
    {
    case Step::SpecifyNextPnt:
    {
        if (!_pMakeSketchSpline)
        {
            assert(false);
            this->requestAbort(AbortCause::UserCancel);
            return;
        }

        if (_fitPoints.size() >= 2)
        {
            if (_pMakeSketchSpline->update(_fitPoints))
            {
                _pMakeSketchSpline->commit();
            }
        }
        _pMakeSketchSpline = nullptr;

        this->reset();
        this->simulateMouseMoveFromPopup();
        return;
    }
    break;

    case Step::Undefined:
    case Step::SpecifyStartPnt:
    default:
    {
        this->requestAbort(AbortCause::UserCancel);
        return;
    }
    break;
    }
}

bool SketchDrawSplineGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SpecifyStartPnt:
    {
        // 将起始点存到插值点集合
        assert(_fitPoints.empty());
        _fitPoints.clear();
        _fitPoints.emplace_back(_startPoint);

        // 创建样条曲线
        _pMakeSketchSpline = std::make_shared<MakeSketchSpline>(this, _mode);
        if (!_pMakeSketchSpline->init(_fitPoints.front(), _sketchInfo.sketchId))
        {
            assert(false);
            _pMakeSketchSpline = nullptr;
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }

        // 控制点或插值点
        PointTransientSPtr pPointTransient = std::make_shared<PointTransient>(_sketchInfo.sketchPlane.value(_startPoint),
            OsgGizmoNode::SKETCH_ENTITY_COLOR, SnapConsts::PickSize);
        pPointTransient->show();
        _pointTransients.emplace_back(pPointTransient);

        // 下一步
        this->gotoStep(Step::SpecifyNextPnt);
        return true;
    }
    break;

    case Step::SpecifyNextPnt:
    {
        // 校验
        if (!_pMakeSketchSpline)
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }

        // 尝试修正nextPoint
        _nextPoint = this->tryReviseNextPoint(_fitPoints, _nextPoint);
        if (this->isDisallowedDuplicatePoint(_fitPoints, _nextPoint))
        {
            return false;
        }

        // 刷新样条曲线
        assert(!_fitPoints.empty());
        if (!_pMakeSketchSpline->update(_fitPoints, _nextPoint))
        {
            return false;
        }
        _fitPoints.emplace_back(_nextPoint);

        // 控制点或插值点
        PointTransientSPtr pPointTransient = std::make_shared<PointTransient>(_sketchInfo.sketchPlane.value(_nextPoint),
            OsgGizmoNode::SKETCH_ENTITY_COLOR, SnapConsts::PickSize);
        pPointTransient->show();
        _pointTransients.emplace_back(pPointTransient);

        // 控制点模式
        if (wy3d::SplineMode::ControlPoints == _mode && _fitPoints.size() >= 2)
        {
            const wy::Vector2& pnt1 = _fitPoints[_fitPoints.size() - 2];
            const wy::Vector2& pnt2 = _fitPoints[_fitPoints.size() - 1];

            LineTransientSPtr pPathLineTransient = std::make_shared<LineTransient>(
                new osg::LineStipple(CENTER_LINE_STIPPLE_FACTOR, CENTER_LINE_STIPPLE_PATTERN));
            pPathLineTransient->update(_sketchInfo.sketchPlane, pnt1, pnt2);
            pPathLineTransient->show();
            _pathTransients.emplace_back(pPathLineTransient);

            _pActivePathTransient = std::make_shared<LineTransient>(
                new osg::LineStipple(CENTER_LINE_STIPPLE_FACTOR, CENTER_LINE_STIPPLE_PATTERN));
            _pActivePathTransient->update(_sketchInfo.sketchPlane, _fitPoints.back(), _fitPoints.back());
            _pActivePathTransient->show();
        }

        // added by wangyao 2025.08.14 {
        // 如果样条曲线是闭合的则完成本次绘制,开启下一条样条曲线的绘制
        if (_pMakeSketchSpline && this->isClosed())
        {
            _pMakeSketchSpline->commit();
            _pMakeSketchSpline = nullptr;
            this->reset(); // reset()中调用了this->gotoStep(Step::SpecifyStartPnt);
            return true;
        }
        // }

        // 循环指定下一点
        this->gotoStep(Step::SpecifyNextPnt);
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

void SketchDrawSplineGuiCmd::gotoStep(Step step)
{
    _step = step;
    this->hidePopup();
    _hoverPopupState.resetValue();

    switch (step)
    {
    case Step::SpecifyStartPnt:
    {
        // 允许文本输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawSplineGuiCmd",
            "Specify the start point; you can directly input the coordinate values."));

        // 局部刷新草图捕捉系统
        if (_sketchInfo.pSketchSnapSys)
        {
            _sketchInfo.pSketchSnapSys->partiallyUpdate(Application::instance().getActiveDatabase());
        }
        // 草图捕捉上下文
        _pSnapContext = std::make_shared<SketchLocateContext>(wydb::ElementId::kNull);

        // 草图捕捉排除对象
        _snapExcludeIds.clear();
        _snapExcludeIds.insert(_sketchInfo.sketchId);

        // 鼠标样式
        Application::instance().setCursor(CursorType::Locate);
    }
    break;

    case Step::SpecifyNextPnt:
    {
        // 允许文本输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawSplineGuiCmd",
            "Specify the next point; you can directly input the coordinate values."));

        // 草图捕捉上下文
        _pSnapContext = std::make_shared<SketchLocateContext>(
            _pMakeSketchSpline ? _pMakeSketchSpline->getId() : wydb::ElementId::kNull);

        // 草图捕捉排除对象
        _snapExcludeIds.clear();
        _snapExcludeIds.insert(_sketchInfo.sketchId);
        if (_pMakeSketchSpline) _snapExcludeIds.insert(_pMakeSketchSpline->getId());

        // 鼠标样式
        Application::instance().setCursor(CursorType::Locate);

        // added by wangyao 2025.08.14 {
        this->tryAddStartPntAsResidentSnapObject();
        // }
    }
    break;

    default:
    {
        assert(false);
        Application::instance().getStatusBar()->setTips("");
        Application::instance().setCursor(CursorType::Select);
    }
    break;
    }
}

void SketchDrawSplineGuiCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

void SketchDrawSplineGuiCmd::tryAddStartPntAsResidentSnapObject()
{
    if (_pStartPointSnapObject) // 已经添加了
    {
        return;
    }

    bool add(false);
    switch (_mode)
    {
    case wy3d::SplineMode::InterpolationPoints: // 插值
    {
        if (_fitPoints.size() >= 3)
        {
            add = true;
        }
    }
    break;

    case wy3d::SplineMode::ControlPoints: // 控制点
    {
        if (_fitPoints.size() >= 4)
        {
            add = true;
        }
    }
    break;

    default:
    {
        assert(false);
    }
    break;
    }
    if (!add) return;

    wy::Vector3 point = _sketchInfo.sketchPlane.value(_startPoint);
    wyap::SnapSystem* pSnapSys = Application::instance().getSnapSystem();
    pSnapSys->beginChange();
    {
        _pStartPointSnapObject = std::make_shared<SnapCoordinatePoint>(point);
        pSnapSys->addResidentSnapObject(_pStartPointSnapObject);
    }
    pSnapSys->endChange();
}

void SketchDrawSplineGuiCmd::onMouseMove(const MouseEvent& event)
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
    case Step::SpecifyStartPnt:
    {
        wy::Vector2 startPnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, _snapExcludeIds, _pSnapContext, _sketchInfo.pSketchSnapSys);
        _hoverPopupState.point = startPnt;
        return;
    }
    break;

    case Step::SpecifyNextPnt:
    {
        wy::Vector2 nextPoint = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, _snapExcludeIds, _pSnapContext, _sketchInfo.pSketchSnapSys);
        nextPoint = this->tryReviseNextPoint(_fitPoints, nextPoint); // 尝试修正nextPoint
        _hoverPopupState.point = nextPoint;
        if (this->isDisallowedDuplicatePoint(_fitPoints, nextPoint))
        {
            return;
        }
        {
            if (_pMakeSketchSpline)
            {
                _pMakeSketchSpline->update(_fitPoints, nextPoint);
            }

            // 控制点模式
            if (wy3d::SplineMode::ControlPoints == _mode && _fitPoints.size() >= 2 && _pActivePathTransient)
            {
                _pActivePathTransient->update(_sketchInfo.sketchPlane, _fitPoints.back(), nextPoint);
            }
        }
        return;
    }
    break;

    case Step::Undefined:
    default:
    {
        return;
    }
    break;
    }

    return;
}

void SketchDrawSplineGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    this->hidePopup();
    _hoverPopupState.lastMouseX = event.x;
    _hoverPopupState.lastMouseY = event.y;
    _hoverPopupState.lastMouseMoveTime = event.time;

    switch (_step)
    {
    case Step::SpecifyStartPnt:
    {
        _startPoint = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, _snapExcludeIds, _pSnapContext, _sketchInfo.pSketchSnapSys);
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
        return;
    }
    break;

    case Step::SpecifyNextPnt:
    {
        _nextPoint = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, _snapExcludeIds, _pSnapContext, _sketchInfo.pSketchSnapSys);
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
        return;
    }
    break;

    case Step::Undefined:
    default:
    {
        return;
    }
    break;
    }

    return;
}

void SketchDrawSplineGuiCmd::initializePopups()
{
    if (_pXYPopup)
    {
        return;
    }

    MainWindow* pMainWindow = Application::instance().getMainWindow();
    _pXYPopup = std::make_unique<GuiCmdHoverInputPopup2>(
        QStringLiteral("X"),
        QStringLiteral("Y"),
        QStringLiteral("-1234.56"),
        pMainWindow);
    _pXYPopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
    _pXYPopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
    _pXYPopup->hide();
}

void SketchDrawSplineGuiCmd::showPopup()
{
    if (!_pXYPopup)
    {
        this->initializePopups();
    }
    if (!_pXYPopup)
    {
        return;
    }
    if (_step != Step::SpecifyStartPnt && _step != Step::SpecifyNextPnt)
    {
        return;
    }

    _pXYPopup->setValues(
        _hoverPopupState.point.x(),
        _hoverPopupState.point.y());
    _pXYPopup->showAtGlobal(QCursor::pos());
}

void SketchDrawSplineGuiCmd::hidePopup()
{
    if (_pXYPopup && _pXYPopup->isVisible())
    {
        _pXYPopup->hide();
    }
}

void SketchDrawSplineGuiCmd::tryShowPopupOnHover(double time)
{
    if (_step != Step::SpecifyStartPnt && _step != Step::SpecifyNextPnt)
    {
        return;
    }
    if (_hoverPopupState.lastMouseMoveTime < 0.0)
    {
        return;
    }
    if (_pXYPopup && _pXYPopup->isVisible())
    {
        return;
    }
    if (time - _hoverPopupState.lastMouseMoveTime >= 0.45)
    {
        this->showPopup();
    }
}

void SketchDrawSplineGuiCmd::onPopupEnterKey()
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

    switch (_step)
    {
    case Step::SpecifyStartPnt:
        _startPoint.set(x, y);
        break;
    case Step::SpecifyNextPnt:
        _nextPoint.set(x, y);
        break;
    default:
        return;
    }

    if (this->finishStep(_step))
    {
        this->simulateMouseMoveFromPopup();
    }
}

void SketchDrawSplineGuiCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void SketchDrawSplineGuiCmd::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX)
        return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}

void SketchDrawSplineGuiCmd::onLeftMouseDoubleClicked(const MouseEvent& event)
{
    switch (_step)
    {
    case Step::SpecifyNextPnt:
    {
        this->onEscapeKey();
        return;
    }
    break;

    case Step::Undefined:
    case Step::SpecifyStartPnt:
    default:
    {
        return; // 不做任何操作
    }
    break;
    }

    return;
}

bool SketchDrawSplineGuiCmd::isDisallowedDuplicatePoint(
    const std::vector<wy::Vector2>& points,
    const wy::Vector2& nextPoint) const
{
    if (points.empty())
    {
        return false;
    }

    if (this->isAllowedClosurePoint(points, nextPoint))
    {
        return false;
    }

    for (const wy::Vector2& pnt : points)
    {
        if ((pnt - nextPoint).length() <= wy3d::TOL)
        {
            return true;
        }
    }
    return false;
}

bool SketchDrawSplineGuiCmd_FitPoints::isClosed() const
{
    if (_fitPoints.size() >= 4)
    {
        const wy::Vector2& firstPoint = _fitPoints.front();
        const wy::Vector2& lastPoint = _fitPoints.back();
        return firstPoint.x() == lastPoint.x() && firstPoint.y() == lastPoint.y();
    }
    else
    {
        return false;
    }
}

bool SketchDrawSplineGuiCmd_ControlPoints::isClosed() const
{
    if (_fitPoints.size() >= 5)
    {
        const wy::Vector2& firstPoint = _fitPoints.front();
        const wy::Vector2& lastPoint = _fitPoints.back();
        return firstPoint.x() == lastPoint.x() && firstPoint.y() == lastPoint.y();
    }
    else
    {
        return false;
    }
}

wy::Vector2 SketchDrawSplineGuiCmd_FitPoints::tryReviseNextPoint(
    const std::vector<wy::Vector2>& points,
    const wy::Vector2& nextPoint) const
{
    if (points.size() < 3)
    {
        return nextPoint;
    }
    if ((points.front() - nextPoint).length() <= wy3d::TOL)
    {
        return points.front();
    }
    else
    {
        return nextPoint;
    }
}

bool SketchDrawSplineGuiCmd_FitPoints::isAllowedClosurePoint(
    const std::vector<wy::Vector2>& points,
    const wy::Vector2& nextPoint) const
{
    if (points.size() < 3)
    {
        return false;
    }
    return (points.front() - nextPoint).length() <= wy3d::TOL;
}

wy::Vector2 SketchDrawSplineGuiCmd_ControlPoints::tryReviseNextPoint(
    const std::vector<wy::Vector2>& points,
    const wy::Vector2& nextPoint) const
{
    if (points.size() < 4)
    {
        return nextPoint;
    }
    if ((points.front() - nextPoint).length() <= wy3d::TOL)
    {
        return points.front();
    }
    else
    {
        return nextPoint;
    }
}

bool SketchDrawSplineGuiCmd_ControlPoints::isAllowedClosurePoint(
    const std::vector<wy::Vector2>& points,
    const wy::Vector2& nextPoint) const
{
    if (points.size() < 4)
    {
        return false;
    }
    return (points.front() - nextPoint).length() <= wy3d::TOL;
}

void MakeSketchSpline::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pSketchSpline) idSet.insert(_pSketchSpline->getId());
}

bool MakeSketchSpline::init(const wy::Vector2& startPnt, wydb::ElementId sketchId)
{
    if (!_pDb || !_pTopTrans || _pSketchSpline || _isFinished)
    {
        return false;
    }
    if (wy3d::SplineMode::InterpolationPoints != _mode &&
        wy3d::SplineMode::ControlPoints != _mode)
    {
        return false;
    }

    // 创建样条曲线
    wy3d::SketchSpline* pSketchSpline(nullptr);
    std::vector<wy::Vector2> points;
    points.reserve(2);
    points.emplace_back(startPnt);
    points.emplace_back(startPnt + wy::Vector2(wy3d::kMinValue, 0.0));
    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    wy3d::Sketch* pSketch = nullptr;
    wydb::Element* pSketchElem = pTrans->getElementForWrite(sketchId);
    if (!pSketchElem) goto ABORT_TRANS;
    pSketch = wy3d::Sketch::cast(pSketchElem);
    if (!pSketch) goto ABORT_TRANS;
    if (wy3d::SplineMode::ControlPoints == _mode) // 控制点
    {
        if (wy::ErrorStatus::Ok != wy3d::SketchSpline::create(pTrans, 2, points, pSketchSpline) || !pSketchSpline)
        {
            goto ABORT_TRANS;
        }
    }
    else // 插值点
    {
        if (wy::ErrorStatus::Ok != wy3d::SketchSpline::create(pTrans, points, pSketchSpline) || !pSketchSpline)
        {
            goto ABORT_TRANS;
        }
    }
    _pSketchSpline = pSketchSpline;
    if (wy::ErrorStatus::Ok != pSketch->addEntity(pSketchSpline))
    {
        goto ABORT_TRANS;
    }
    _pDb->getTransactionManager()->endTransaction();

    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    _pSketchSpline = nullptr;
    return false;
}

bool MakeSketchSpline::update(const std::vector<wy::Vector2>& points)
{
    if (!_pDb || !_pTopTrans || !_pSketchSpline || _isFinished)
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
    if (wy::ErrorStatus::Ok != _pSketchSpline->upgradeForWrite())
    {
        _pDb->getTransactionManager()->abortTransaction();
        return false;
    }
    if (wy3d::SplineMode::ControlPoints == _pSketchSpline->getMode())
    {
        switch (points.size())
        {
        case 2: // 2个点一次
        {
            _pSketchSpline->setDegree(1);
        }
        break;

        case 3: // 3个点两次
        {
            _pSketchSpline->setDegree(2);
        }
        break;

        default: // 其它三次
        {
            assert(points.size() > 3);
            _pSketchSpline->setDegree(3);
        }
        break;
        }
    }
    if (wy::ErrorStatus::Ok != _pSketchSpline->setPoints(points))
    {
        _pDb->getTransactionManager()->abortTransaction();
        return false;
    }

    if (wy::ErrorStatus::Ok == _pDb->getTransactionManager()->endTransaction())
    {
        wydb::TransactionManager* pTransMgr = _pDb->getTransactionManager();
        assert(pTransMgr);
        pTransMgr->mergeTransaction();
    }
    return true;
}

bool MakeSketchSpline::update(const std::vector<wy::Vector2>& points, const wy::Vector2& nextPnt)
{
    _pnts.clear();
    if (_pnts.capacity() < (points.size() + 1))
    {
        _pnts.reserve(points.size() + 10);
    }
    _pnts.insert(_pnts.cend(), points.cbegin(), points.cend());
    _pnts.emplace_back(nextPnt);
    return this->update(_pnts);
}
