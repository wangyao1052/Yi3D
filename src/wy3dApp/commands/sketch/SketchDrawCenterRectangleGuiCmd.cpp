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

#include "SketchDrawCenterRectangleGuiCmd.h"

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

#include <wy3dApp/Application.h>
#include "commands/sketch/dialogs/GuiCmdHoverInputPopup.h"
#include "snap/SketchSnapSystem.h"
#include "widgets/frame/MainWindow.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

static bool parseDoubleText(const QString& text, double& value)
{
    bool ok(false);
    value = text.trimmed().toDouble(&ok);
    return ok;
}


SketchDrawCenterRectangleGuiCmd::SketchDrawCenterRectangleGuiCmd() : OsgGuiCommand(),
    _step(Step::Undefined), _startPnt(), _endPnt(),
    _pXYPopup(nullptr), _pLengthWidthPopup(nullptr), _hoverPopupState()
{
}

SketchDrawCenterRectangleGuiCmd::~SketchDrawCenterRectangleGuiCmd()
{
}

wyap::CmdExecution::StartResult SketchDrawCenterRectangleGuiCmd::onStart()
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

void SketchDrawCenterRectangleGuiCmd::cleanUp()
{
    this->hidePopup();

    _step = Step::Undefined;
    _startPnt.set(0.0, 0.0);
    _endPnt.set(0.0, 0.0);
    _pSnapContext = nullptr;
    _hoverPopupState.resetValue();

    _pDiagonal1st = nullptr;
    _pDiagonal2nd = nullptr;

    _pMakeSketchRectangle = nullptr;
}

void SketchDrawCenterRectangleGuiCmd::reset()
{
    this->cleanUp();
    this->gotoStep(Step::SpecifyStartPnt);
}

void SketchDrawCenterRectangleGuiCmd::onEscapeKey()
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

bool SketchDrawCenterRectangleGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SpecifyStartPnt:
    {
        wydb::Database* pDb = Application::instance().getActiveDatabase();
        if (pDb)
        {
            _pMakeSketchRectangle = std::make_shared<MakeSketchRectangle>(this, MakeSketchRectangle::Mode::CenterRect, _sketchInfo.sketchId);
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

        // 对角线
        _pDiagonal1st = std::make_shared<LineTransient>();
        _pDiagonal2nd = std::make_shared<LineTransient>();
        this->updateDiagonalTransients(_startPnt, _startPnt);

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

        // 对角线
        _pDiagonal1st = nullptr;
        _pDiagonal2nd = nullptr;

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

void SketchDrawCenterRectangleGuiCmd::gotoStep(Step step)
{
    _step = step;
    this->hidePopup();
    _hoverPopupState.resetValue();

    switch (step)
    {
    case Step::SpecifyStartPnt:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawCenterRectangle",
            "Specify the rectangle center point; you can directly input the coordinate values."));

        if (_sketchInfo.pSketchSnapSys)
        {
            _sketchInfo.pSketchSnapSys->clearSnapResult();
        }
        _pSnapContext = std::make_shared<Locate2dContext>();
    }
    break;

    case Step::SpecifyEndPnt:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawCenterRectangle",
            "Specify the rectangle corner point or input length & width."));

        _pSnapContext = std::make_shared<Locate2dContext>();
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

void SketchDrawCenterRectangleGuiCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

void SketchDrawCenterRectangleGuiCmd::onMouseMove(const MouseEvent& event)
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
        wy::Vector2 startPnt = this->computePoint2d(event.x, event.y, _sketchInfo.pSketchSnapSys, _pSnapContext, this->getSnapExcludeIds()).first;
        _hoverPopupState.point = startPnt;
    }
    else if (_step == Step::SpecifyEndPnt)
    {
        wy::Vector2 endPnt = this->computePoint2d(event.x, event.y, _sketchInfo.pSketchSnapSys, _pSnapContext, this->getSnapExcludeIds()).first;
        double length = std::fabs((endPnt.x() - _startPnt.x()) * 2);
        double width = std::fabs((endPnt.y() - _startPnt.y()) * 2);
        _hoverPopupState.point = endPnt;
        _hoverPopupState.length = length;
        _hoverPopupState.width = width;
        {
            if (_pMakeSketchRectangle)
            {
                _pMakeSketchRectangle->update(endPnt);
            }

            // 更新对角线
            this->updateDiagonalTransients(_startPnt, endPnt);
        }
    }
    else
    {
        //assert(false);
    }

    return;
}

void SketchDrawCenterRectangleGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    this->hidePopup();
    _hoverPopupState.lastMouseX = event.x;
    _hoverPopupState.lastMouseY = event.y;
    _hoverPopupState.lastMouseMoveTime = event.time;


    if (_step == Step::SpecifyStartPnt)
    {
        _startPnt = this->computePoint2d(event.x, event.y, _sketchInfo.pSketchSnapSys, _pSnapContext, this->getSnapExcludeIds()).first;
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
    }
    else if (_step == Step::SpecifyEndPnt)
    {
        _endPnt = this->computePoint2d(event.x, event.y, _sketchInfo.pSketchSnapSys, _pSnapContext, this->getSnapExcludeIds()).first;
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

void SketchDrawCenterRectangleGuiCmd::initializePopups()
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
            QCoreApplication::translate("SketchDrawCenterRectangle", "Length"),
            QCoreApplication::translate("SketchDrawCenterRectangle", "Width"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pLengthWidthPopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pLengthWidthPopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pLengthWidthPopup->hide();
    }
}

void SketchDrawCenterRectangleGuiCmd::showPopup()
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

void SketchDrawCenterRectangleGuiCmd::hidePopup()
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

GuiCmdHoverInputPopup2* SketchDrawCenterRectangleGuiCmd::getActivePopup() const
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

void SketchDrawCenterRectangleGuiCmd::tryShowPopupOnHover(double time)
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

void SketchDrawCenterRectangleGuiCmd::onPopupEnterKey()
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
        if (!parseDoubleText(pActivePopup->getRow1Text(), length) ||
            !parseDoubleText(pActivePopup->getRow2Text(), width))
        {
            return;
        }

        double halfLength = length / 2;
        double halfWidth = width / 2;
        if (_hoverPopupState.point.x() < _startPnt.x())
        {
            halfLength = -halfLength;
        }
        if (_hoverPopupState.point.y() < _startPnt.y())
        {
            halfWidth = -halfWidth;
        }
        _endPnt.set(_startPnt.x() + halfLength, _startPnt.y() + halfWidth);
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

void SketchDrawCenterRectangleGuiCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void SketchDrawCenterRectangleGuiCmd::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX)
        return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}

std::set<wydb::ElementId> SketchDrawCenterRectangleGuiCmd::getSnapExcludeIds() const
{
    std::set<wydb::ElementId> snapExcludeIds;
    snapExcludeIds.insert(_sketchInfo.sketchId);
    if (_pMakeSketchRectangle) _pMakeSketchRectangle->collectElements(snapExcludeIds);
    return snapExcludeIds;
}

void SketchDrawCenterRectangleGuiCmd::updateDiagonalTransients(const wy::Vector2& startPnt, const wy::Vector2& endPnt)
{
    if (!_pDiagonal1st || !_pDiagonal2nd)
    {
        return;
    }

    double deltaX = endPnt.x() - startPnt.x();
    double deltaY = endPnt.y() - startPnt.y();
    wy::Vector2 pnt1(startPnt.x() - deltaX, startPnt.y() - deltaY);
    wy::Vector2 pnt2(endPnt.x(), startPnt.y() - deltaY);
    wy::Vector2 pnt3 = endPnt;
    wy::Vector2 pnt4(startPnt.x() - deltaX, endPnt.y());

    _pDiagonal1st->update(_sketchInfo.sketchPlane, pnt1, pnt3);
    _pDiagonal2nd->update(_sketchInfo.sketchPlane, pnt2, pnt4);
}

} // namespace wy3dApp
