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

#include "SketchDrawArcBy3PointsGuiCmd.h"

#include <QCoreApplication>
#include <QCursor>
#include <QString>

#include <wyVector2.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapSelManager.h>
#include <wy3dImpl.h>
#include <wy3dSketch.h>
#include <wy3dSketchArc.h>

#include "application/Application.h"
#include "commands/sketch/dialogs/GuiCmdHoverInputPopup.h"
#include "commands/transient/SketchBasicTransient.h"
#include "snap/SketchSnapSystem.h"
#include "utils/MathUtils.h"
#include "widgets/frame/MainWindow.h"


static bool parseDoubleText(const QString& text, double& value)
{
    bool ok(false);
    value = text.trimmed().toDouble(&ok);
    return ok;
}

SketchDrawArcBy3PointsGuiCmd::SketchDrawArcBy3PointsGuiCmd() : OsgGuiCommand(),
    _step(Step::Undefined), _pnt1st(), _pnt2nd(), _pnt3rd(), _sketchArcId(wydb::ElementId::kNull),
    _pXYPopup(nullptr), _hoverPopupState()
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

SketchDrawArcBy3PointsGuiCmd::~SketchDrawArcBy3PointsGuiCmd()
{
}

wyap::CmdExecution::StartResult SketchDrawArcBy3PointsGuiCmd::onStart()
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

void SketchDrawArcBy3PointsGuiCmd::cleanup()
{
    this->hidePopup();

    _step = Step::Undefined;
    _pnt1st.set(0.0, 0.0);
    _pnt2nd.set(0.0, 0.0);
    _pnt3rd.set(0.0, 0.0);
    _sketchArcId = wydb::ElementId::kNull;

    _snapExcludeIds.clear();
    _pSnapContext = nullptr;
    _hoverPopupState.resetValue();

    _pCenterPointTransient = nullptr;

    _pMakeSketchArc = nullptr;
}

void SketchDrawArcBy3PointsGuiCmd::reset()
{
    this->cleanup();
    this->gotoStep(Step::SpecifyPoint1st);
}

void SketchDrawArcBy3PointsGuiCmd::onEscapeKey()
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

bool SketchDrawArcBy3PointsGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SpecifyPoint1st:
    {
        // 创建圆弧
        _pMakeSketchArc = std::make_shared<MakeSketchArcBy3Points>(this);
        const wy3d::SketchArc* pSketchArc(nullptr);
        if (!_pMakeSketchArc->init(_pnt1st, _sketchInfo.sketchId, pSketchArc) || !pSketchArc)
        {
            assert(false);
            _pMakeSketchArc = nullptr;
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }
        _sketchArcId = pSketchArc->getId();

        // 更新捕捉排除对象
        _snapExcludeIds.clear();
        _pMakeSketchArc->collectElements(_snapExcludeIds);
        _snapExcludeIds.insert(_sketchInfo.sketchId);

        // 圆心标记
        _pCenterPointTransient = std::make_shared<CenterPointTransient>();
        _pCenterPointTransient->update(_sketchInfo.sketchPlane, pSketchArc->getCenter());
        _pCenterPointTransient->show();

        // next step
        this->gotoStep(Step::SpecifyPoint2nd);
        return true;
    }
    break;

    case Step::SpecifyPoint2nd:
    {
        // 校验
        if (!_pMakeSketchArc)
        {
            assert(false);
            _pMakeSketchArc = nullptr;
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }

        // 更新圆弧
        if (!_pMakeSketchArc->update(_pnt1st, _pnt2nd, wy::Vector2(), false)) // 不使用第三点
        {
            return false;
        }

        // 更新圆心标记
        this->updateCenterPointTrasient();

        // 下一步
        this->gotoStep(Step::SpecifyPoint3rd);
        return true;
    }
    break;

    case Step::SpecifyPoint3rd:
    {
        // 校验
        if (!_pMakeSketchArc)
        {
            assert(false);
            _pMakeSketchArc = nullptr;
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }

        // 更新圆
        if (!_pMakeSketchArc->update(_pnt1st, _pnt2nd, _pnt3rd))
        {
            return false;
        }

        // 圆心标记
        _pCenterPointTransient = nullptr;

        // 提交事务
        _pMakeSketchArc->commit();
        _pMakeSketchArc = nullptr;

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

void SketchDrawArcBy3PointsGuiCmd::gotoStep(Step step)
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
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawArc",
            "Specify the first end point on arc; you can directly input the coordinate values."));
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
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawArc",
            "Specify the second end point on arc; you can directly input the coordinate values."));
        // 草图捕捉上下文
        _pSnapContext = std::make_shared<SketchDrawLineContext>(_sketchArcId, _pnt1st);
    }
    break;

    case Step::SpecifyPoint3rd:
    {
        // 允许文本输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawArc",
            "Specify the third point on arc; you can directly input the coordinate values."));
        // 草图捕捉上下文
        _pSnapContext = std::make_shared<SketchLocateContext>(_sketchArcId);
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

void SketchDrawArcBy3PointsGuiCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

void SketchDrawArcBy3PointsGuiCmd::onMouseMove(const MouseEvent& event)
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
            if (_pMakeSketchArc)
            {
                _pMakeSketchArc->update(_pnt1st, pnt2nd, wy::Vector2(), false); // 不使用第三点
            }
        }
        // 更新圆心标记
        this->updateCenterPointTrasient();
        return;
    }
    break;

    case Step::SpecifyPoint3rd:
    {
        wy::Vector2 pnt3rd = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, _snapExcludeIds, _pSnapContext, _sketchInfo.pSketchSnapSys);
        _hoverPopupState.point = pnt3rd;
        {
            if (_pMakeSketchArc)
            {
                _pMakeSketchArc->update(_pnt1st, _pnt2nd, pnt3rd);
            }
        }
        // 更新圆心标记
        this->updateCenterPointTrasient();
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

void SketchDrawArcBy3PointsGuiCmd::onLeftMouseDown(const MouseEvent& event)
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

void SketchDrawArcBy3PointsGuiCmd::initializePopups()
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

void SketchDrawArcBy3PointsGuiCmd::showPopup()
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

void SketchDrawArcBy3PointsGuiCmd::hidePopup()
{
    if (_pXYPopup && _pXYPopup->isVisible())
    {
        _pXYPopup->hide();
    }
}

void SketchDrawArcBy3PointsGuiCmd::tryShowPopupOnHover(double time)
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

void SketchDrawArcBy3PointsGuiCmd::onPopupEnterKey()
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
    case Step::SpecifyPoint1st:
        _pnt1st.set(x, y);
        break;
    case Step::SpecifyPoint2nd:
        _pnt2nd.set(x, y);
        break;
    case Step::SpecifyPoint3rd:
        _pnt3rd.set(x, y);
        break;
    default:
        return;
    }

    if (this->finishStep(_step))
    {
        this->simulateMouseMoveFromPopup();
    }
}

void SketchDrawArcBy3PointsGuiCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void SketchDrawArcBy3PointsGuiCmd::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX)
        return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}

void SketchDrawArcBy3PointsGuiCmd::updateCenterPointTrasient()
{
    if (!_pCenterPointTransient)
    {
        return;
    }

    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return;
    const wy3d::SketchArc* pArc = wy3d::SketchArc::cast(pDb->getElement(_sketchArcId));
    if (!pArc) return;
    wy::Vector2 center = pArc->getCenter();
    _pCenterPointTransient->update(_sketchInfo.sketchPlane, center);
}

void MakeSketchArcBy3Points::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pSketchArc) idSet.insert(_pSketchArc->getId());
}

bool MakeSketchArcBy3Points::init(
    const wy::Vector2& pnt1,
    wydb::ElementId sketchId,
    const wy3d::SketchArc*& pOutSketchArc)
{
    pOutSketchArc = nullptr;
    if (!_pDb || !_pTopTrans || _pSketchArc || _isFinished)
    {
        return false;
    }

    // 创建SketchArc
    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    wy3d::Sketch* pSketch = nullptr;
    wy3d::SketchArc* pSketchArc = nullptr;
    wy::Vector2 centerPnt = pnt1 - wy::Vector2(0.0, wy3d::kMinValue);
    wydb::Element* pSketchElem = pTrans->getElementForWrite(sketchId);
    if (!pSketchElem) goto ABORT_TRANS;
    pSketch = wy3d::Sketch::cast(pSketchElem);
    if (!pSketch) goto ABORT_TRANS;
    if (wy::ErrorStatus::Ok != wy3d::SketchArc::create(pTrans, centerPnt, wy3d::kMinValue, 0.0, wy3d::PI_2, pSketchArc)
        || !pSketchArc)
    {
        goto ABORT_TRANS;
    }
    _pSketchArc = pSketchArc;
    if (wy::ErrorStatus::Ok != pSketch->addEntity(pSketchArc))
    {
        goto ABORT_TRANS;
    }
    _pDb->getTransactionManager()->endTransaction();
    pOutSketchArc = _pSketchArc;
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    _pSketchArc = nullptr;
    return false;
}

bool MakeSketchArcBy3Points::update(
    const wy::Vector2& pnt1,
    const wy::Vector2& pnt2,
    const wy::Vector2& pnt3,
    bool useThirdPoint)
{
    if (!_pDb || !_pTopTrans || !_pSketchArc || _isFinished)
    {
        return false;
    }

    wy::Vector2 center;
    double radius(0.0);
    double startAngle(0.0), endAngle(0.0);
    if (useThirdPoint)
    {
        if (!MathUtils::computeArcBy3Points(pnt1, pnt2, pnt3, center, radius, startAngle, endAngle))
        {
            return false;
        }
    }
    else
    {
        wy::Vector2 vec = pnt1 - pnt2;
        double len = vec.length();
        if (len <= wy3d::EPS)
        {
            return false;
        }
        wy::Vector2 middlePnt = (pnt1 + pnt2) / 2;
        vec.normalize();
        wy::Vector2 dir(vec.y(), -vec.x());
        center = middlePnt + dir * len / 2;

        // 需要保证vec2逆时针90度旋转到vec1
        if (wy::Vector2::rotationAngle(pnt2 - center, pnt1 - center) > wy3d::PI)
        {
            center = middlePnt - dir * len / 2;
        }
        radius = (pnt1 - center).length();
        startAngle = wy::Vector2::rotationAngle(wy::Vector2::kXAxis, pnt2 - center);
        endAngle = wy::Vector2::rotationAngle(wy::Vector2::kXAxis, pnt1 - center);
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
        if (wy::ErrorStatus::Ok != _pSketchArc->upgradeForWrite()) goto ABORT_TRANS;
        if (wy::ErrorStatus::Ok != _pSketchArc->setCenter(center)) goto ABORT_TRANS;
        if (wy::ErrorStatus::Ok != _pSketchArc->setRadius(radius)) goto ABORT_TRANS;
        if (wy::ErrorStatus::Ok != _pSketchArc->setStartAngle(startAngle)) goto ABORT_TRANS;
        if (wy::ErrorStatus::Ok != _pSketchArc->setEndAngle(endAngle)) goto ABORT_TRANS;
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
