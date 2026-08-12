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

#include "SketchDrawCircleGuiCmd.h"

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
#include "widgets/frame/MainWindow.h"

static bool parseDoubleText(const QString& text, double& value)
{
    bool ok(false);
    value = text.trimmed().toDouble(&ok);
    return ok;
}


SketchDrawCircleGuiCmd::SketchDrawCircleGuiCmd()
    : OsgGuiCommand()
    , _step(Step::Undefined)
    , _centerPnt()
    , _radius(0.0)
    , _pSnapContext(nullptr)
    , _pCenterPointTransient(nullptr)
    , _pMakeSketchCircle(nullptr)
    , _pXYPopup(nullptr)
    , _pRadiusPopup(nullptr)
    , _hoverPopupState()
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

SketchDrawCircleGuiCmd::~SketchDrawCircleGuiCmd()
{
}

wyap::CmdExecution::StartResult SketchDrawCircleGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = GuiCommand::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);
 
    _sketchInfo = GuiCommandUtil::initSketchInfo();
    if (_sketchInfo.pSketchSnapSys) _sketchInfo.pSketchSnapSys->clearSnapResult();

    // 初始化
    this->gotoStep(Step::SpecifyCenterPnt);

    // 鼠标样式
    Application::instance().setCursor(CursorType::Locate);

    return wyap::CmdExecution::StartResult::Succeeded;
}

void SketchDrawCircleGuiCmd::cleanup()
{
    this->hidePopup();

    _step = Step::Undefined;
    _centerPnt.set(0.0, 0.0);
    _radius = 0.0;
    _pSnapContext = nullptr;
    _pCenterPointTransient = nullptr;
    _hoverPopupState.resetValue();
    _pMakeSketchCircle = nullptr;
}

void SketchDrawCircleGuiCmd::reset()
{
    this->cleanup();
    this->gotoStep(Step::SpecifyCenterPnt);
}

void SketchDrawCircleGuiCmd::onEscapeKey()
{
    this->hidePopup();

    if (_step == Step::SpecifyCenterPnt || _step == Step::Undefined)
    {
        this->requestAbort(AbortCause::UserCancel);
    }
    else if (_step == Step::SpecifyRadius)
    {
        this->reset();
        this->simulateMouseMoveFromPopup();
    }
    else
    {
        assert(false);
    }
}

bool SketchDrawCircleGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SpecifyCenterPnt:
    {
        wydb::Database* pDb = Application::instance().getActiveDatabase();
        if (pDb)
        {
            _pMakeSketchCircle = std::make_shared<MakeSketchCircle>(this);
            if (!_pMakeSketchCircle->init(_centerPnt, _sketchInfo.sketchId))
            {
                _pMakeSketchCircle = nullptr;
                return false;
            }
        }
        else
        {
            this->reset();
            return false;
        }

        // 圆心标记
        _pCenterPointTransient = std::make_shared<CenterPointTransient>();
        _pCenterPointTransient->update(_sketchInfo.sketchPlane, _centerPnt);
        _pCenterPointTransient->show();

        // next step
        this->gotoStep(Step::SpecifyRadius);
        return true;
    }
    break;

    case Step::SpecifyRadius:
    {
        if (_pMakeSketchCircle)
        {
            if (!_pMakeSketchCircle->update(_radius))
            {
                return false;
            }
            _pMakeSketchCircle->commit();
            _pMakeSketchCircle = nullptr;
        }

        // 中心点标记
        _pCenterPointTransient = nullptr;

        // next step
        this->gotoStep(Step::SpecifyCenterPnt);
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

void SketchDrawCircleGuiCmd::gotoStep(Step step)
{
    _step = step;
    this->hidePopup();
    _hoverPopupState.resetValue();

    switch (step)
    {
    case Step::SpecifyCenterPnt:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawCircle",
            "Specify the center point; you can directly input the coordinate values."));

        if (_sketchInfo.pSketchSnapSys)
        {
            _sketchInfo.pSketchSnapSys->partiallyUpdate(Application::instance().getActiveDatabase());
        }
        _pSnapContext = std::make_shared<SketchLocateContext>(wydb::ElementId::kNull);
    }
    break;

    case Step::SpecifyRadius:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawCircle",
            "Specify the radius; you can directly input the value."));

        _pSnapContext = std::make_shared<SketchDrawCircleContext>(
            _pMakeSketchCircle ? _pMakeSketchCircle->getId() : wydb::ElementId::kNull, _centerPnt);
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

void SketchDrawCircleGuiCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

void SketchDrawCircleGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (event.x != _hoverPopupState.lastMouseX ||
        event.y != _hoverPopupState.lastMouseY)
    {
        this->hidePopup();
        _hoverPopupState.lastMouseX = event.x;
        _hoverPopupState.lastMouseY = event.y;
        _hoverPopupState.lastMouseMoveTime = event.time;
    }

    if (_step == Step::SpecifyCenterPnt)
    {
        wy::Vector2 centerPnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, this->getSnapExcludeIds(), _pSnapContext, _sketchInfo.pSketchSnapSys);
        _hoverPopupState.point = centerPnt;
    }
    else if (_step == Step::SpecifyRadius)
    {
        wy::Vector2 pnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, this->getSnapExcludeIds(), _pSnapContext, _sketchInfo.pSketchSnapSys);
        double radius = (pnt - _centerPnt).length();
        _hoverPopupState.radius = radius;

        {
            if (_pMakeSketchCircle)
            {
                _pMakeSketchCircle->update(radius);
            }
        }
    }

    return;
}

void SketchDrawCircleGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    this->hidePopup();
    _hoverPopupState.lastMouseX = event.x;
    _hoverPopupState.lastMouseY = event.y;
    _hoverPopupState.lastMouseMoveTime = event.time;


    if (_step == Step::SpecifyCenterPnt)
    {
        _centerPnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, this->getSnapExcludeIds(), _pSnapContext, _sketchInfo.pSketchSnapSys);
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
    }
    else if (_step == Step::SpecifyRadius)
    {
        wy::Vector2 pnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, this->getSnapExcludeIds(), _pSnapContext, _sketchInfo.pSketchSnapSys);
        _radius = (pnt - _centerPnt).length();
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

void SketchDrawCircleGuiCmd::initializePopups()
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
            QCoreApplication::translate("SketchDrawCircle", "Radius"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pRadiusPopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pRadiusPopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pRadiusPopup->hide();
    }
}

void SketchDrawCircleGuiCmd::showPopup()
{
    if (!_pXYPopup || !_pRadiusPopup)
    {
        this->initializePopups();
    }

    if (_step == Step::SpecifyCenterPnt)
    {
        if (!_pXYPopup)
        {
            return;
        }
        _pXYPopup->setValues(
            _hoverPopupState.point.x(),
            _hoverPopupState.point.y());
        _pXYPopup->showAtGlobal(QCursor::pos());
    }
    else if (_step == Step::SpecifyRadius)
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

void SketchDrawCircleGuiCmd::hidePopup()
{
    if (_pXYPopup && _pXYPopup->isVisible())
    {
        _pXYPopup->hide();
    }
    if (_pRadiusPopup && _pRadiusPopup->isVisible())
    {
        _pRadiusPopup->hide();
    }
}

void SketchDrawCircleGuiCmd::tryShowPopupOnHover(double time)
{
    if (_step != Step::SpecifyCenterPnt && _step != Step::SpecifyRadius)
    {
        return;
    }
    if (_hoverPopupState.lastMouseMoveTime < 0.0)
    {
        return;
    }
    if ((_pXYPopup && _pXYPopup->isVisible()) ||
        (_pRadiusPopup && _pRadiusPopup->isVisible()))
    {
        return;
    }
    if (time - _hoverPopupState.lastMouseMoveTime >= 0.45)
    {
        this->showPopup();
    }
}

void SketchDrawCircleGuiCmd::onPopupEnterKey()
{
    if (_step == Step::SpecifyCenterPnt)
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
        _centerPnt.set(x, y);
    }
    else if (_step == Step::SpecifyRadius)
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

void SketchDrawCircleGuiCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void SketchDrawCircleGuiCmd::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX)
        return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}

std::set<wydb::ElementId> SketchDrawCircleGuiCmd::getSnapExcludeIds() const
{
    std::set<wydb::ElementId> snapExcludeIds;
    snapExcludeIds.insert(_sketchInfo.sketchId);
    if (_pMakeSketchCircle) _pMakeSketchCircle->collectElements(snapExcludeIds);
    return snapExcludeIds;
}

void MakeSketchCircle::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pSketchCircle) idSet.insert(_pSketchCircle->getId());
}

bool MakeSketchCircle::init(const wy::Vector2& centerPnt, wydb::ElementId sketchId)
{
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
    if (wy::ErrorStatus::Ok != wy3d::SketchCircle::create(pTrans, centerPnt, wy3d::kMinValue, pSketchCircle) || !pSketchCircle)
    {
        goto ABORT_TRANS;
    }
    _pSketchCircle = pSketchCircle;
    if (wy::ErrorStatus::Ok != pSketch->addEntity(pSketchCircle))
    {
        goto ABORT_TRANS;
    }
    _pDb->getTransactionManager()->endTransaction();
    _pSketchCircle = pSketchCircle;
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    _pSketchCircle = nullptr;
    return false;
}

bool MakeSketchCircle::update(double radius)
{
    if (!_pDb || !_pTopTrans || !_pSketchCircle || _isFinished)
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
        _pSketchCircle->upgradeForWrite();
        _pSketchCircle->setRadius(radius);
    }
    if (wy::ErrorStatus::Ok == _pDb->getTransactionManager()->endTransaction())
    {
        wydb::TransactionManager* pTransMgr = _pDb->getTransactionManager();
        pTransMgr->mergeTransaction();
    }
    return true;
}
