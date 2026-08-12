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

#include "commands/sketch/SketchDrawCircleBy3PointsGuiCmd.h"

#include <QCoreApplication>
#include <QCursor>
#include <QString>

#include <wyVector2.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapSelManager.h>
#include <wy3dImpl.h>
#include <wy3dSketch.h>
#include <wy3dSketchCircle.h>

#include "application/Application.h"
#include "commands/sketch/dialogs/GuiCmdHoverInputPopup.h"
#include "snap/SketchSnapSystem.h"
#include "utils/MathUtils.h"
#include "scene/Colors.h"
#include "snap/SnapConsts.h"
#include "gizmo/OsgGizmoNode.h"
#include "widgets/frame/MainWindow.h"

static bool parseDoubleText(const QString& text, double& value)
{
    bool ok(false);
    value = text.trimmed().toDouble(&ok);
    return ok;
}


SketchDrawCircleBy3PointsGuiCmd::SketchDrawCircleBy3PointsGuiCmd() : OsgGuiCommand(),
    _step(Step::Undefined), _pnt1st(), _pnt2nd(), _pnt3rd(), _sketchCircleId(wydb::ElementId::kNull),
    _pXYPopup(nullptr), _hoverPopupState()
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

SketchDrawCircleBy3PointsGuiCmd::~SketchDrawCircleBy3PointsGuiCmd()
{
}

wyap::CmdExecution::StartResult SketchDrawCircleBy3PointsGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = GuiCommand::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);
 
    _sketchInfo = GuiCommandUtil::initSketchInfo();
    if (_sketchInfo.pSketchSnapSys) _sketchInfo.pSketchSnapSys->clearSnapResult();

    // 初始化
    this->gotoStep(Step::SpecifyPoint1st);

    // 鼠标样式
    Application::instance().setCursor(CursorType::Locate);

    return wyap::CmdExecution::StartResult::Succeeded;
}

void SketchDrawCircleBy3PointsGuiCmd::cleanup()
{
    this->hidePopup();

    _step = Step::Undefined;
    _pnt1st.set(0.0, 0.0);
    _pnt2nd.set(0.0, 0.0);
    _pnt3rd.set(0.0, 0.0);
    _sketchCircleId = wydb::ElementId::kNull;

    _snapExcludeIds.clear();
    _pSnapContext = nullptr;

    _pFirstPoint = nullptr;
    _pSecondPoint = nullptr;

    _hoverPopupState.resetValue();
    _pMakeSketchCircle = nullptr;
}

void SketchDrawCircleBy3PointsGuiCmd::reset()
{
    this->cleanup();
    this->gotoStep(Step::SpecifyPoint1st);
}

void SketchDrawCircleBy3PointsGuiCmd::onEscapeKey()
{
    this->hidePopup();

    if (_step == Step::SpecifyPoint1st || _step == Step::Undefined)
    {
        this->requestAbort(AbortCause::UserCancel);
    }
    else
    {
        this->reset();
        this->simulateMouseMoveFromPopup();
    }
}

bool SketchDrawCircleBy3PointsGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SpecifyPoint1st:
    {
        // 创建圆
        _pMakeSketchCircle = std::make_shared<MakeSketchCircleBy3Points>(this);
        const wy3d::SketchCircle* pSketchCircle(nullptr);
        if (!_pMakeSketchCircle->init(_pnt1st, _sketchInfo.sketchId, pSketchCircle) || !pSketchCircle)
        {
            assert(false);
            _pMakeSketchCircle = nullptr;
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }
        _sketchCircleId = pSketchCircle->getId();

        // 更新捕捉排除对象
        _snapExcludeIds.clear();
        _snapExcludeIds.insert(_sketchInfo.sketchId);
        _pMakeSketchCircle->collectElements(_snapExcludeIds);

        // 第一个点
        _pFirstPoint = std::make_shared<PointTransient>(_sketchInfo.sketchPlane.value(_pnt1st),
            OsgGizmoNode::SKETCH_ENTITY_COLOR, SnapConsts::PickSize);
        _pFirstPoint->show();

        // 下一步
        this->gotoStep(Step::SpecifyPoint2nd);
        return true;
    }
    break;

    case Step::SpecifyPoint2nd:
    {
        // 校验
        if (!_pMakeSketchCircle)
        {
            assert(false);
            _pMakeSketchCircle = nullptr;
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }

        // 更新圆
        if (!_pMakeSketchCircle->update(_pnt1st, _pnt2nd, wy::Vector2(), false)) // 不使用第三点
        {
            return false;
        }

        // 第二个点
        _pSecondPoint = std::make_shared<PointTransient>(_sketchInfo.sketchPlane.value(_pnt2nd),
            OsgGizmoNode::SKETCH_ENTITY_COLOR, SnapConsts::PickSize);
        _pSecondPoint->show();

        // 下一步
        this->gotoStep(Step::SpecifyPoint3rd);
        return true;
    }
    break;

    case Step::SpecifyPoint3rd:
    {
        // 校验
        if (!_pMakeSketchCircle)
        {
            assert(false);
            _pMakeSketchCircle = nullptr;
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }

        // 更新圆
        if (!_pMakeSketchCircle->update(_pnt1st, _pnt2nd, _pnt3rd))
        {
            return false;
        }

        //
        _pFirstPoint = nullptr;
        _pSecondPoint = nullptr;

        // 提交事务
        _pMakeSketchCircle->commit();
        _pMakeSketchCircle = nullptr;

        // 下一步(循环)
        this->gotoStep(Step::SpecifyPoint1st);
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

void SketchDrawCircleBy3PointsGuiCmd::gotoStep(Step step)
{
    _step = step;
    this->hidePopup();
    _hoverPopupState.resetValue();

    // 清空捕捉结果
    Application::instance().getSnapSystem()->clearSnapResult();
    // 清空草图捕捉结果
    if (_sketchInfo.pSketchSnapSys) _sketchInfo.pSketchSnapSys->clearSnapResult();

    switch (step)
    {
    case Step::SpecifyPoint1st:
    {
        // 允许文本输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawCircle",
            "Specify the first point on circle; you can directly input the coordinate values."));
        // 局部更新草图捕捉系统
        if (_sketchInfo.pSketchSnapSys)
        {
            _sketchInfo.pSketchSnapSys->partiallyUpdate(Application::instance().getActiveDatabase());
        }
        // 草图捕捉上下文
        _pSnapContext = std::make_shared<SketchLocateContext>(wydb::ElementId::kNull);
    }
    break;

    case Step::SpecifyPoint2nd:
    {
        // 允许文本输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawCircle",
            "Specify the second point on circle; you can directly input the coordinate values."));
        // 草图捕捉上下文
        _pSnapContext = std::make_shared<SketchDrawLineContext>(_sketchCircleId, _pnt1st);
    }
    break;

    case Step::SpecifyPoint3rd:
    {
        // 允许文本输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawCircle",
            "Specify the third point on circle; you can directly input the coordinate values."));
        // 草图捕捉上下文
        _pSnapContext = std::make_shared<SketchLocateContext>(_sketchCircleId);
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

void SketchDrawCircleBy3PointsGuiCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

void SketchDrawCircleBy3PointsGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (event.x != _hoverPopupState.lastMouseX ||
        event.y != _hoverPopupState.lastMouseY) // moved
    {
        this->hidePopup();
        _hoverPopupState.lastMouseX = event.x;
        _hoverPopupState.lastMouseY = event.y;
        _hoverPopupState.lastMouseMoveTime = event.time;
    }

    switch (_step)
    {
    case Step::SpecifyPoint1st:
    {
        wy::Vector2 pnt1st = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, _snapExcludeIds, _pSnapContext, _sketchInfo.pSketchSnapSys);
        _hoverPopupState.point = pnt1st;
        return;
    }
    break;

    case Step::SpecifyPoint2nd:
    {
        wy::Vector2 pnt2nd = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, _snapExcludeIds, _pSnapContext, _sketchInfo.pSketchSnapSys);
        _hoverPopupState.point = pnt2nd;
        {
            if (_pMakeSketchCircle)
            {
                _pMakeSketchCircle->update(_pnt1st, pnt2nd, wy::Vector2(), false); // 不使用第三点
            }
        }
        return;
    }
    break;

    case Step::SpecifyPoint3rd:
    {
        wy::Vector2 pnt3rd = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, _snapExcludeIds, _pSnapContext, _sketchInfo.pSketchSnapSys);
        _hoverPopupState.point = pnt3rd;
        {
            if (_pMakeSketchCircle)
            {
                _pMakeSketchCircle->update(_pnt1st, _pnt2nd, pnt3rd);
            }
        }
        return;
    }
    break;

    default:
    {
        //assert(false);
    }
    break;
    }

    return;
}

void SketchDrawCircleBy3PointsGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    this->hidePopup();
    _hoverPopupState.lastMouseX = event.x;
    _hoverPopupState.lastMouseY = event.y;
    _hoverPopupState.lastMouseMoveTime = event.time;

    switch (_step)
    {
    case Step::SpecifyPoint1st:
    {
        _pnt1st = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, _snapExcludeIds, _pSnapContext, _sketchInfo.pSketchSnapSys);
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
        return;
    }
    break;

    case Step::SpecifyPoint2nd:
    {
        _pnt2nd = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, _snapExcludeIds, _pSnapContext, _sketchInfo.pSketchSnapSys);
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
        return;
    }
    break;

    case Step::SpecifyPoint3rd:
    {
        _pnt3rd = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, _snapExcludeIds, _pSnapContext, _sketchInfo.pSketchSnapSys);
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
    }
    break;
    }

    return;
}

void SketchDrawCircleBy3PointsGuiCmd::initializePopups()
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

void SketchDrawCircleBy3PointsGuiCmd::showPopup()
{
    if (!_pXYPopup)
    {
        this->initializePopups();
    }
    if (!_pXYPopup)
    {
        return;
    }
    if (_step != Step::SpecifyPoint1st &&
        _step != Step::SpecifyPoint2nd &&
        _step != Step::SpecifyPoint3rd)
    {
        return;
    }

    _pXYPopup->setValues(
        _hoverPopupState.point.x(),
        _hoverPopupState.point.y());
    _pXYPopup->showAtGlobal(QCursor::pos());
}

void SketchDrawCircleBy3PointsGuiCmd::hidePopup()
{
    if (_pXYPopup && _pXYPopup->isVisible())
    {
        _pXYPopup->hide();
    }
}

void SketchDrawCircleBy3PointsGuiCmd::tryShowPopupOnHover(double time)
{
    if (_step != Step::SpecifyPoint1st &&
        _step != Step::SpecifyPoint2nd &&
        _step != Step::SpecifyPoint3rd)
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

void SketchDrawCircleBy3PointsGuiCmd::onPopupEnterKey()
{
    if (!_pXYPopup)
    {
        return;
    }

    double x(0.0);
    double y(0.0);
    if (!parseDoubleText(_pXYPopup->getRow1Text(), x) ||
        !parseDoubleText(_pXYPopup->getRow2Text(), y))
    {
        return;
    }

    if (_step == Step::SpecifyPoint1st)
    {
        _pnt1st.set(x, y);
    }
    else if (_step == Step::SpecifyPoint2nd)
    {
        _pnt2nd.set(x, y);
    }
    else if (_step == Step::SpecifyPoint3rd)
    {
        _pnt3rd.set(x, y);
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

void SketchDrawCircleBy3PointsGuiCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void SketchDrawCircleBy3PointsGuiCmd::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX)
        return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}

void MakeSketchCircleBy3Points::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pSketchCircle) idSet.insert(_pSketchCircle->getId());
}

bool MakeSketchCircleBy3Points::init(
    const wy::Vector2& pnt1,
    wydb::ElementId sketchId,
    const wy3d::SketchCircle*& pOutSketchCircle)
{
    pOutSketchCircle = nullptr;
    if (!_pDb || !_pTopTrans || _pSketchCircle || _isFinished)
    {
        return false;
    }

    // 创建SketchCircle
    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    wy3d::Sketch* pSketch = nullptr;
    wy3d::SketchCircle* pSketchCircle = nullptr;
    wydb::Element* pSketchElem = pTrans->getElementForWrite(sketchId);
    if (!pSketchElem) goto ABORT_TRANS;
    pSketch = wy3d::Sketch::cast(pSketchElem);
    if (!pSketch) goto ABORT_TRANS;
    if (wy::ErrorStatus::Ok != wy3d::SketchCircle::create(pTrans, pnt1 + wy::Vector2(wy3d::kMinValue, 0.0),
        wy3d::kMinValue, pSketchCircle) || !pSketchCircle)
    {
        goto ABORT_TRANS;
    }
    _pSketchCircle = pSketchCircle;
    if (wy::ErrorStatus::Ok != pSketch->addEntity(pSketchCircle))
    {
        goto ABORT_TRANS;
    }
    _pDb->getTransactionManager()->endTransaction();
    pOutSketchCircle = _pSketchCircle;
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    _pSketchCircle = nullptr;
    return false;
}

bool MakeSketchCircleBy3Points::update(
    const wy::Vector2& pnt1,
    const wy::Vector2& pnt2,
    const wy::Vector2& pnt3,
    bool useThirdPoint)
{
    if (!_pDb || !_pTopTrans || !_pSketchCircle || _isFinished)
    {
        return false;
    }

    wy::Vector2 center;
    double radius(0.0);
    if (useThirdPoint)
    {
        if (!MathUtils::computeCircleBy3Points(pnt1, pnt2, pnt3, center, radius))
        {
            return false;
        }
    }
    else
    {
        center = (pnt1 + pnt2) / 2;
        radius = (pnt1 - pnt2).length() / 2;
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
        if (wy::ErrorStatus::Ok != _pSketchCircle->upgradeForWrite()) goto ABORT_TRANS;
        if (wy::ErrorStatus::Ok != _pSketchCircle->setCenter(center)) goto ABORT_TRANS;
        if (wy::ErrorStatus::Ok != _pSketchCircle->setRadius(radius)) goto ABORT_TRANS;
    }
    if (wy::ErrorStatus::Ok == _pDb->getTransactionManager()->endTransaction())
    {
        wydb::TransactionManager* pTransMgr = _pDb->getTransactionManager();
        pTransMgr->mergeTransaction();
    }
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    return false;
}
