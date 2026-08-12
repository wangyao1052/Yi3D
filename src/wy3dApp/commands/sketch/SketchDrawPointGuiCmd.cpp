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

#include "SketchDrawPointGuiCmd.h"

#include <QCoreApplication>
#include <QCursor>
#include <QString>

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


SketchDrawPointGuiCmd::SketchDrawPointGuiCmd() : OsgGuiCommand(),
    _step(Step::Undefined), _position(), _pXYPopup(nullptr), _hoverPopupState()
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

wyap::CmdExecution::StartResult SketchDrawPointGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = GuiCommand::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);
 
    _sketchInfo = GuiCommandUtil::initSketchInfo();
    if (_sketchInfo.pSketchSnapSys) _sketchInfo.pSketchSnapSys->clearSnapResult();

    // 初始化
    this->gotoStep(Step::SpecifyPosition);

    return wyap::CmdExecution::StartResult::Succeeded;
}

void SketchDrawPointGuiCmd::reset()
{
    this->cleanup();
    this->gotoStep(Step::SpecifyPosition);
}

void SketchDrawPointGuiCmd::cleanup()
{
    this->hidePopup();

    _step = Step::Undefined;
    _position.set(0.0, 0.0);
    _pSnapContext = nullptr;
    _hoverPopupState.resetValue();
}

void SketchDrawPointGuiCmd::onEscapeKey()
{
    this->hidePopup();
    this->requestAbort(AbortCause::UserCancel);
}

bool SketchDrawPointGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SpecifyPosition:
    {
        std::shared_ptr<MakeSketchPoint> pMakeSketchPoint = std::make_shared<MakeSketchPoint>(this);
        if (pMakeSketchPoint->perform(_position, _sketchInfo.sketchId))
        {
            pMakeSketchPoint->commit();
            pMakeSketchPoint = nullptr;

            // 下一步(循环)
            this->gotoStep(Step::SpecifyPosition);
            return true;
        }
        else
        {
            assert(false);
            pMakeSketchPoint = nullptr;

            // 退出命令
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }
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

void SketchDrawPointGuiCmd::gotoStep(Step step)
{
    _step = step;
    this->hidePopup();
    _hoverPopupState.resetValue();

    switch (step)
    {
    case Step::SpecifyPosition:
    {
        // 允许文本输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawPoint",
            "Specify the position of the point; you can directly input the coordinate values."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::Locate);

        // 局部更新草图捕捉系统
        if (_sketchInfo.pSketchSnapSys)
        {
            _sketchInfo.pSketchSnapSys->partiallyUpdate(Application::instance().getActiveDatabase());
        }
        // 草图捕捉上下文
        _pSnapContext = std::make_shared<SketchLocateContext>(wydb::ElementId::kNull);
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

void SketchDrawPointGuiCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

void SketchDrawPointGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (event.x != _hoverPopupState.lastMouseX ||
        event.y != _hoverPopupState.lastMouseY) // moved
    {
        this->hidePopup();
        _hoverPopupState.lastMouseX = event.x;
        _hoverPopupState.lastMouseY = event.y;
        _hoverPopupState.lastMouseMoveTime = event.time;
    }

    if (_step == Step::SpecifyPosition)
    {
        wy::Vector2 position = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, _snapExcludeIds, _pSnapContext, _sketchInfo.pSketchSnapSys);
        _hoverPopupState.point = position;
        return;
    }

    return;
}

void SketchDrawPointGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    this->hidePopup();
    _hoverPopupState.lastMouseX = event.x;
    _hoverPopupState.lastMouseY = event.y;
    _hoverPopupState.lastMouseMoveTime = event.time;


    if (_step == Step::SpecifyPosition)
    {
        _position = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, _snapExcludeIds, _pSnapContext, _sketchInfo.pSketchSnapSys);
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
        return;
    }
    else
    {
        assert(false);
    }

    return;
}

void SketchDrawPointGuiCmd::initializePopups()
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

void SketchDrawPointGuiCmd::showPopup()
{
    if (!_pXYPopup)
    {
        this->initializePopups();
    }
    if (!_pXYPopup)
    {
        return;
    }
    if (_step != Step::SpecifyPosition)
    {
        return;
    }

    _pXYPopup->setValues(
        _hoverPopupState.point.x(),
        _hoverPopupState.point.y());
    _pXYPopup->showAtGlobal(QCursor::pos());
}

void SketchDrawPointGuiCmd::hidePopup()
{
    if (_pXYPopup && _pXYPopup->isVisible())
    {
        _pXYPopup->hide();
    }
}

void SketchDrawPointGuiCmd::tryShowPopupOnHover(double time)
{
    if (_step != Step::SpecifyPosition)
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

void SketchDrawPointGuiCmd::onPopupEnterKey()
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

    _position.set(x, y);
    if (this->finishStep(_step))
    {
        this->simulateMouseMoveFromPopup();
    }
}

void SketchDrawPointGuiCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void SketchDrawPointGuiCmd::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX)
        return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}

void MakeSketchPoint::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pSketchPoint) idSet.insert(_pSketchPoint->getId());
}

bool MakeSketchPoint::perform(const wy::Vector2& position, wydb::ElementId sketchId)
{
    if (!_pDb || !_pTopTrans || _pSketchPoint || _isFinished)
    {
        return false;
    }

    // 创建草绘点
    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    wy3d::Sketch* pSketch = nullptr;
    wy3d::SketchPoint* pSketchPoint = nullptr;
    wydb::Element* pSketchElem = pTrans->getElementForWrite(sketchId);
    if (!pSketchElem) goto ABORT_TRANS;
    pSketch = wy3d::Sketch::cast(pSketchElem);
    if (!pSketch) goto ABORT_TRANS;
    if (wy::ErrorStatus::Ok != wy3d::SketchPoint::create(pTrans, position, pSketchPoint) || !pSketchPoint)
    {
        goto ABORT_TRANS;
    }
    _pSketchPoint = pSketchPoint;
    if (wy::ErrorStatus::Ok != pSketch->addEntity(pSketchPoint))
    {
        goto ABORT_TRANS;
    }
    _pDb->getTransactionManager()->endTransaction();
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    _pSketchPoint = nullptr;
    return false;
}
