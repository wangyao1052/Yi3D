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

#include "commands/sketch/SketchTextCommand.h"

#include <QCoreApplication>
#include <QCursor>
#include <QString>
#include <muParser.h>
#include <wyVector2.h>
#include <wyVector3.h>
#include <wyapEnvironment.h>
#include <wy3dSketchSpline.h>
#include <wy3dSketchSpline.h>
#include <wy3dMakeSketchText.h>
#include <wy3dApp/Application.h>
#include "commands/sketch/dialogs/GuiCmdHoverInputPopup.h"
#include <wy3dApp/SketchEnvironment.h>
#include "snap/SketchSnapSystem.h"
#include "commands/dialogs/SketchTextDialog.h"
#include <wy3dApp/Scene.h>
#include <wy3dApp/ElementNode.h>
#include "utils/MathUtils.h"
#include "utils/MessageBoxUtil.h"
#include "widgets/frame/MainWindow.h"

static bool parseDoubleText(const QString& text, double& value)
{
    bool ok(false);
    value = text.trimmed().toDouble(&ok);
    return ok;
}

std::shared_ptr<SketchTextHistory> SketchTextCommand::_pHistory = nullptr;

SketchTextCommand::SketchTextCommand() : OsgGuiCommand(),
    _step(Step::Undefined),
    _position(),
    _pSnapContext(nullptr),
    _pXYPopup(nullptr),
    _hoverPopupState(),
    _pMakeText(nullptr),
    _isRevising(false)
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

SketchTextCommand::~SketchTextCommand()
{
}

wyap::CmdExecution::StartResult SketchTextCommand::onStart()
{
    wyap::CmdExecution::StartResult ret = GuiCommand::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);
 
    _sketchInfo = GuiCommandUtil::initSketchInfo();
    if (_sketchInfo.pSketchSnapSys) _sketchInfo.pSketchSnapSys->clearSnapResult();
 
    _sketchInfo = GuiCommandUtil::initSketchInfo();

    // 第一步
    this->gotoStep(Step::MakeText);

    return wyap::CmdExecution::StartResult::Succeeded;
}

bool SketchTextCommand::finishStep(Step step)
{
    switch (step)
    {
    case Step::MakeText:
    {
        // 条件校验
        if (!_pMakeText)
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }

        // 下一步
        this->gotoStep(Step::SpecifyPosition);
        return true;
    }
    break;

    case Step::SpecifyPosition:
    {
        // 条件校验
        if (!_pMakeText)
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }

        // 执行移动并提交
        _pMakeText->performTranslate(_position);
        bool commitRet = _pMakeText->commit();
        assert(commitRet);
        _pMakeText = nullptr;

        // 退出命令
        this->requestEnd();
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

void SketchTextCommand::gotoStep(Step step)
{
    _step = step;
    this->hidePopup();
    _hoverPopupState.resetValue();

    switch (step)
    {
    case Step::MakeText:
    {
        // 禁用输入
        // 清空提示信息
        Application::instance().getStatusBar()->setTips("");

        // 鼠标样式
        Application::instance().setCursor(CursorType::Select);

        // 如果在非修订状态说明文本内容不需要使用历史记录的
        if (_pHistory && !_isRevising)
        {
            _pHistory->data.text.clear();
        }

        // 草图文本对话框
        SketchTextDialog dlg(nullptr, _pHistory.get());
        if (QDialog::Accepted != dlg.exec())
        {
            this->requestAbort(AbortCause::UserCancel);
            return;
        }

        // 草图文本数据
        _data.text = dlg.getText();
        _data.fontFilePath = dlg.getFontFilePath();
        _data.fontFaceIndex = dlg.getFontFaceIndex();
        _data.height = dlg.getFontHeight();
        _data.horizontalSpacing = dlg.getHorzSpacing();
        if (_data.fontFilePath.empty())
        {
            assert(false);
            MessageBoxUtil::showError(QCoreApplication::translate("SketchTextCommand", 
                "Font file not found!"));
            this->requestAbort(AbortCause::ErrorTerminate);
            return;
        }

        // 创建草图文本
        _pMakeText = std::make_shared<MakeText>(this);
        assert(_pMakeText);
        if (!_pMakeText->create(_sketchInfo.sketchId, _data))
        {
            assert(false);
            _pMakeText = nullptr;
            MessageBoxUtil::showError(QCoreApplication::translate("SketchTextCommand",
                "Create text profile failed!"));
            this->requestAbort(AbortCause::ErrorTerminate);
            return;
        }

        // 历史
        if (!_pHistory)
        {
            _pHistory = std::make_shared<SketchTextHistory>();
        }
        _pHistory->data = _data;
        _pHistory->font = dlg.getFont();

        // 下一步
        this->gotoStep(Step::SpecifyPosition);
    }
    break;

    case Step::SpecifyPosition:
    {
        // 草图捕捉排除元素
        _snapExcludeIds.clear();
        if (_pMakeText)
        {
            _pMakeText->collectElements(_snapExcludeIds);
        }
        _snapExcludeIds.insert(_sketchInfo.sketchId);

        // 允许输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchTextCommand",
            "Specify the start text position; you can directly input the coordinate values."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::Locate);

        // 草图捕捉上下文
        _pSnapContext = std::make_shared<SketchLocateContext>(wydb::ElementId::kNull);
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

void SketchTextCommand::onEscapeKey()
{
    if (Step::SpecifyPosition == _step)
    {
        this->hidePopup();
        _pMakeText = nullptr;
        _isRevising = true;
        this->gotoStep(Step::MakeText);
    }
    else
    {
        return __baseClass::onEscapeKey();
    }
}

void SketchTextCommand::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

void SketchTextCommand::onMouseMove(const MouseEvent& event)
{
    if (event.x != _hoverPopupState.lastMouseX ||
        event.y != _hoverPopupState.lastMouseY)
    {
        this->hidePopup();
        _hoverPopupState.lastMouseX = event.x;
        _hoverPopupState.lastMouseY = event.y;
        _hoverPopupState.lastMouseMoveTime = event.time;
    }

    if (Step::SpecifyPosition == _step)
    {
        wy::Vector2 position = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, _snapExcludeIds, _pSnapContext, _sketchInfo.pSketchSnapSys);
        _hoverPopupState.point = position;
        if (_pMakeText)
        {
            _pMakeText->update(position);
        }
        return;
    }

    return;
}

void SketchTextCommand::onLeftMouseDown(const MouseEvent& event)
{
    this->hidePopup();
    _hoverPopupState.lastMouseX = event.x;
    _hoverPopupState.lastMouseY = event.y;
    _hoverPopupState.lastMouseMoveTime = event.time;


    if (Step::SpecifyPosition == _step)
    {
        _position = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, _snapExcludeIds, _pSnapContext, _sketchInfo.pSketchSnapSys);
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
        return;
    }

    return;
}


void SketchTextCommand::initializePopups()
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

void SketchTextCommand::showPopup()
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

void SketchTextCommand::hidePopup()
{
    if (_pXYPopup && _pXYPopup->isVisible())
    {
        _pXYPopup->hide();
    }
}

void SketchTextCommand::tryShowPopupOnHover(double time)
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

void SketchTextCommand::onPopupEnterKey()
{
    if (!_pXYPopup || _step != Step::SpecifyPosition)
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

void SketchTextCommand::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void SketchTextCommand::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX)
        return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}
