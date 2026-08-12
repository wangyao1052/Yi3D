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

#include "SketchDrawRectangleGuiCmd.h"

#include <QCoreApplication>
#include <QCursor>
#include <QString>
#include <cmath>

#include <wyVector2.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapSelManager.h>
#include <wy3dImpl.h>
#include <wy3dSketch.h>
#include <wy3dSketchLine.h>

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


SketchDrawRectangleGuiCmd::SketchDrawRectangleGuiCmd() : OsgGuiCommand(),
    _step(Step::Undefined), _startPnt(), _endPnt(),
    _pXYPopup(nullptr), _pLengthWidthPopup(nullptr), _hoverPopupState()
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

SketchDrawRectangleGuiCmd::~SketchDrawRectangleGuiCmd()
{
}

wyap::CmdExecution::StartResult SketchDrawRectangleGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = GuiCommand::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);
 
    _sketchInfo = GuiCommandUtil::initSketchInfo();
    if (_sketchInfo.pSketchSnapSys) _sketchInfo.pSketchSnapSys->clearSnapResult();

    // 清空选择集
    Application::instance().getSelManager()->beginChange();
    Application::instance().getSelManager()->clearSelections();
    Application::instance().getSelManager()->endChange();

    // 初始化
    this->gotoStep(Step::SpecifyStartPnt);

    // 鼠标样式
    Application::instance().setCursor(CursorType::Locate);

    return wyap::CmdExecution::StartResult::Succeeded;
}

void SketchDrawRectangleGuiCmd::cleanup()
{
    this->hidePopup();

    _step = Step::Undefined;
    _startPnt.set(0.0, 0.0);
    _endPnt.set(0.0, 0.0);
    _pSnapContext = nullptr;
    _hoverPopupState.resetValue();
    _pMakeSketchRectangle = nullptr;
}

void SketchDrawRectangleGuiCmd::reset()
{
    this->cleanup();
    this->gotoStep(Step::SpecifyStartPnt);
}

void SketchDrawRectangleGuiCmd::onEscapeKey()
{
    this->hidePopup();

    if (_step == Step::SpecifyStartPnt || _step == Step::Undefined)
    {
        this->requestAbort(AbortCause::UserCancel);
    }
    else
    {
        this->reset();
        this->simulateMouseMoveFromPopup();
    }
}

bool SketchDrawRectangleGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SpecifyStartPnt:
    {
        wydb::Database* pDb = Application::instance().getActiveDatabase();
        if (pDb)
        {
            _pMakeSketchRectangle = std::make_shared<MakeSketchRectangle>(this, MakeSketchRectangle::Mode::CornerRect, _sketchInfo.sketchId);
            if (!_pMakeSketchRectangle->init(_startPnt))
            {
                _pMakeSketchRectangle = nullptr;
                return false;
            }
        }
        else
        {
            this->reset();
            return false;
        }

        // next step
        this->gotoStep(Step::SpecifyEndPnt);
        return true;
    }
    break;

    case Step::SpecifyEndPnt:
    {
        if (_pMakeSketchRectangle)
        {
            if (!_pMakeSketchRectangle->update(_endPnt))
            {
                return false;
            }
            _pMakeSketchRectangle->commit();
            _pMakeSketchRectangle = nullptr;
        }

        // next step
        this->gotoStep(Step::SpecifyStartPnt);
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

void SketchDrawRectangleGuiCmd::gotoStep(Step step)
{
    _step = step;
    this->hidePopup();
    _hoverPopupState.resetValue();

    switch (step)
    {
    case Step::SpecifyStartPnt:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawRectangle",
            "Specify the first corner point; you can directly input the coordinate values."));

        if (_sketchInfo.pSketchSnapSys)
        {
            _sketchInfo.pSketchSnapSys->partiallyUpdate(Application::instance().getActiveDatabase());
        }
        _pSnapContext = std::make_shared<SketchLocateContext>(wydb::ElementId::kNull);
    }
    break;

    case Step::SpecifyEndPnt:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawRectangle",
            "Specify the other corner point or input length & width."));

        _pSnapContext = std::make_shared<SketchLocateContext>(wydb::ElementId::kNull);
        _pSnapContext->setExcludedIds(std::move(this->getSnapExcludeIds()));
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

void SketchDrawRectangleGuiCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

void SketchDrawRectangleGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (event.x != _hoverPopupState.lastMouseX ||
        event.y != _hoverPopupState.lastMouseY) // moved
    {
        this->hidePopup();
        _hoverPopupState.lastMouseX = event.x;
        _hoverPopupState.lastMouseY = event.y;
        _hoverPopupState.lastMouseMoveTime = event.time;
    }

    if (_step == Step::SpecifyStartPnt)
    {
        wy::Vector2 startPnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, this->getSnapExcludeIds(), _pSnapContext, _sketchInfo.pSketchSnapSys);
        _hoverPopupState.point = startPnt;
    }
    else if (_step == Step::SpecifyEndPnt)
    {
        wy::Vector2 endPnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, this->getSnapExcludeIds(), _pSnapContext, _sketchInfo.pSketchSnapSys);
        double length = std::fabs(endPnt.x() - _startPnt.x());
        double width = std::fabs(endPnt.y() - _startPnt.y());
        _hoverPopupState.point = endPnt;
        _hoverPopupState.length = length;
        _hoverPopupState.width = width;
        {
            if (_pMakeSketchRectangle)
            {
                _pMakeSketchRectangle->update(endPnt);
            }
        }
    }

    return;
}

void SketchDrawRectangleGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    this->hidePopup();
    _hoverPopupState.lastMouseX = event.x;
    _hoverPopupState.lastMouseY = event.y;
    _hoverPopupState.lastMouseMoveTime = event.time;


    if (_step == Step::SpecifyStartPnt)
    {
        _startPnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, this->getSnapExcludeIds(), _pSnapContext, _sketchInfo.pSketchSnapSys);
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
    }
    else if (_step == Step::SpecifyEndPnt)
    {
        _endPnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, this->getSnapExcludeIds(), _pSnapContext, _sketchInfo.pSketchSnapSys);
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

void SketchDrawRectangleGuiCmd::initializePopups()
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
    if (!_pLengthWidthPopup)
    {
        _pLengthWidthPopup = std::make_unique<GuiCmdHoverInputPopup2>(
            QCoreApplication::translate("SketchDrawRectangle", "Length"),
            QCoreApplication::translate("SketchDrawRectangle", "Width"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pLengthWidthPopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pLengthWidthPopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pLengthWidthPopup->hide();
    }
}

void SketchDrawRectangleGuiCmd::showPopup()
{
    if (!_pXYPopup || !_pLengthWidthPopup)
    {
        this->initializePopups();
    }

    GuiCmdHoverInputPopup2* pActivePopup = this->getActivePopup();
    if (!pActivePopup)
    {
        return;
    }

    if (_step == Step::SpecifyStartPnt)
    {
        pActivePopup->setValues(
            _hoverPopupState.point.x(),
            _hoverPopupState.point.y());
    }
    else if (_step == Step::SpecifyEndPnt)
    {
        pActivePopup->setValues(
            _hoverPopupState.length,
            _hoverPopupState.width);
    }
    else
    {
        return;
    }
    pActivePopup->showAtGlobal(QCursor::pos());
}

void SketchDrawRectangleGuiCmd::hidePopup()
{
    if (_pXYPopup && _pXYPopup->isVisible())
    {
        _pXYPopup->hide();
    }
    if (_pLengthWidthPopup && _pLengthWidthPopup->isVisible())
    {
        _pLengthWidthPopup->hide();
    }
}

GuiCmdHoverInputPopup2* SketchDrawRectangleGuiCmd::getActivePopup() const
{
    if (_step == Step::SpecifyStartPnt)
    {
        return _pXYPopup.get();
    }
    if (_step == Step::SpecifyEndPnt)
    {
        return _pLengthWidthPopup.get();
    }
    else
    {
        return nullptr;
    }
}

void SketchDrawRectangleGuiCmd::tryShowPopupOnHover(double time)
{
    if (_step != Step::SpecifyStartPnt && _step != Step::SpecifyEndPnt)
    {
        return;
    }
    if (_hoverPopupState.lastMouseMoveTime < 0.0)
    {
        return;
    }
    if ((_pXYPopup && _pXYPopup->isVisible()) ||
        (_pLengthWidthPopup && _pLengthWidthPopup->isVisible()))
    {
        return;
    }
    if (time - _hoverPopupState.lastMouseMoveTime >= 0.45)
    {
        this->showPopup();
    }
}

void SketchDrawRectangleGuiCmd::onPopupEnterKey()
{
    GuiCmdHoverInputPopup2* pActivePopup = this->getActivePopup();
    if (!pActivePopup)
    {
        return;
    }

    if (_step == Step::SpecifyStartPnt)
    {
        double x(0.0);
        double y(0.0);
        if (!parseDoubleText(pActivePopup->getRow1Text(), x) ||
            !parseDoubleText(pActivePopup->getRow2Text(), y))
        {
            return;
        }
        _startPnt.set(x, y);
    }
    else if (_step == Step::SpecifyEndPnt)
    {
        double length(0.0);
        double width(0.0);
        if (!parseDoubleText(pActivePopup->getRow1Text(), length))
        {
            return;
        }

        QString widthText = pActivePopup->getRow2Text().trimmed();
        if (widthText.isEmpty())
        {
            width = length;
        }
        else if (!parseDoubleText(widthText, width))
        {
            return;
        }

        if (_hoverPopupState.point.x() < _startPnt.x())
        {
            length = -length;
        }
        if (_hoverPopupState.point.y() < _startPnt.y())
        {
            width = -width;
        }
        _endPnt.set(_startPnt.x() + length, _startPnt.y() + width);
    }

    if (this->finishStep(_step))
    {
        this->simulateMouseMoveFromPopup();
    }
}

void SketchDrawRectangleGuiCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void SketchDrawRectangleGuiCmd::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX)
        return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}

std::set<wydb::ElementId> SketchDrawRectangleGuiCmd::getSnapExcludeIds() const
{
    std::set<wydb::ElementId> snapExcludeIds;
    snapExcludeIds.insert(_sketchInfo.sketchId);
    if (_pMakeSketchRectangle) _pMakeSketchRectangle->collectElements(snapExcludeIds);
    return snapExcludeIds;
}

void MakeSketchRectangle::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pSketchLine1st) idSet.insert(_pSketchLine1st->getId());
    if (_pSketchLine2nd) idSet.insert(_pSketchLine2nd->getId());
    if (_pSketchLine3rd) idSet.insert(_pSketchLine3rd->getId());
    if (_pSketchLine4th) idSet.insert(_pSketchLine4th->getId());
}

bool MakeSketchRectangle::init(const wy::Vector2& startPnt)
{
    if (!_pDb || !_pTopTrans || _pSketchLine1st || _pSketchLine2nd || _pSketchLine3rd || _pSketchLine4th || _isFinished)
    {
        return false;
    }

    // 开启事务
    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    assert(_pGuiCmd);
    wy3d::Sketch* pSketch = nullptr;
    wy::Vector2 pnt1;
    wy::Vector2 pnt2;
    wy::Vector2 pnt3;
    wy::Vector2 pnt4;
    wy3d::SketchLine* pSketchLine = nullptr;
    wy::ErrorStatus error;
    pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(_sketchId));
    if (!pSketch) goto ABORT_TRANS;

    // 数据
    this->computeRectEndPoints(_mode, startPnt, startPnt + wy::Vector2(wy3d::kMinValue, wy3d::kMinValue),
        pnt1, pnt2, pnt3, pnt4);
    _startPnt = startPnt;
    // 第一条线
    if (wy::ErrorStatus::Ok != wy3d::SketchLine::create(pTrans, pnt1, pnt2, pSketchLine) || !pSketchLine)
    {
        goto ABORT_TRANS;
    }
    _pSketchLine1st = pSketchLine;
    if (wy::ErrorStatus::Ok != pSketch->addEntity(pSketchLine)) goto ABORT_TRANS;

    // 第二条线
    if (wy::ErrorStatus::Ok != wy3d::SketchLine::create(pTrans, pnt2, pnt3, pSketchLine) || !pSketchLine)
    {
        goto ABORT_TRANS;
    }
    _pSketchLine2nd = pSketchLine;
    if (wy::ErrorStatus::Ok != pSketch->addEntity(pSketchLine)) goto ABORT_TRANS;

    // 第三条线
    if (wy::ErrorStatus::Ok != wy3d::SketchLine::create(pTrans, pnt3, pnt4, pSketchLine) || !pSketchLine)
    {
        goto ABORT_TRANS;
    }
    _pSketchLine3rd = pSketchLine;
    if (wy::ErrorStatus::Ok != pSketch->addEntity(pSketchLine)) goto ABORT_TRANS;

    // 第四条线
    if (wy::ErrorStatus::Ok != wy3d::SketchLine::create(pTrans, pnt4, pnt1, pSketchLine) || !pSketchLine)
    {
        goto ABORT_TRANS;
    }
    _pSketchLine4th = pSketchLine;
    if (wy::ErrorStatus::Ok != pSketch->addEntity(pSketchLine)) goto ABORT_TRANS;

    // 提交事务
    error = _pDb->getTransactionManager()->endTransaction();
    assert(wy::ErrorStatus::Ok == error);
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

bool MakeSketchRectangle::update(const wy::Vector2& endPnt)
{
    if (!_pDb || !_pTopTrans || !_pSketchLine1st || !_pSketchLine2nd || !_pSketchLine3rd || !_pSketchLine4th || _isFinished)
    {
        return false;
    }

    // 校验
    if (!this->checkValid(_mode, _startPnt, endPnt))
    {
        return false;
    }

    // 点坐标
    wy::Vector2 pnt1;
    wy::Vector2 pnt2;
    wy::Vector2 pnt3;
    wy::Vector2 pnt4;
    this->computeRectEndPoints(_mode, _startPnt, endPnt,
        pnt1, pnt2, pnt3, pnt4);
    
    // 开启事务修改
    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    {
        _pSketchLine1st->upgradeForWrite();
        _pSketchLine1st->setStartPoint(pnt1);
        _pSketchLine1st->setEndPoint(pnt2);

        _pSketchLine2nd->upgradeForWrite();
        _pSketchLine2nd->setStartPoint(pnt2);
        _pSketchLine2nd->setEndPoint(pnt3);

        _pSketchLine3rd->upgradeForWrite();
        _pSketchLine3rd->setStartPoint(pnt3);
        _pSketchLine3rd->setEndPoint(pnt4);

        _pSketchLine4th->upgradeForWrite();
        _pSketchLine4th->setStartPoint(pnt4);
        _pSketchLine4th->setEndPoint(pnt1);
    }

    // 提交事务
    if (wy::ErrorStatus::Ok == _pDb->getTransactionManager()->endTransaction())
    {
        wydb::TransactionManager* pTransMgr = _pDb->getTransactionManager();
        pTransMgr->mergeTransaction();
    }
    return true;
}

bool MakeSketchRectangle::checkValid(
    Mode mode,
    const wy::Vector2& startPnt,
    const wy::Vector2& endPnt) const
{
    switch (_mode)
    {
    case Mode::CenterRect:
    {
        double length = std::fabs(endPnt.x() - startPnt.x());
        double width = std::fabs(endPnt.y() - startPnt.y());
        if (length * 2 < wy3d::kMinValue || width * 2 < wy3d::kMinValue)
        {
            return false;
        }
        else
        {
            return true;
        }
    }
    break;

    case Mode::CornerRect:
    default:
    {
        double length = std::fabs(endPnt.x() - startPnt.x());
        double width = std::fabs(endPnt.y() - startPnt.y());
        if (length < wy3d::kMinValue || width < wy3d::kMinValue)
        {
            return false;
        }
        else
        {
            return true;
        }
    }
    break;
    }

    return false;
}

void MakeSketchRectangle::computeRectEndPoints(
    Mode mode,
    const wy::Vector2& startPnt,
    const wy::Vector2& endPnt,
    wy::Vector2& pnt1,
    wy::Vector2& pnt2,
    wy::Vector2& pnt3,
    wy::Vector2& pnt4) const
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
