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

#include "TaskEnvironmentUI.h"

#include <QCoreApplication>
#include <QLayout>
#include <QTimer>
#include <QWidget>

#include "application/ApplicationImpl.h"
#include "commands/CommandNames.h"
#include "utils/IconUtil.h"
#include "widgets/frame/MainWindow.h"

namespace wy3dApp {

TaskEnvironmentUI::TaskEnvironmentUI(const wyap::Environment* pEnvironment)
    : EnvironmentUIImpl(pEnvironment)
{
}

TaskEnvironmentUI::~TaskEnvironmentUI()
{
    this->destroyConfirmationBar();
}

CommandAction* TaskEnvironmentUI::getConfirmAction()
{
    return this->findCommandAction(CommandNames::ConfirmTaskEnvironment);
}

CommandAction* TaskEnvironmentUI::getCancelAction()
{
    return this->findCommandAction(CommandNames::CancelTaskEnvironment);
}

void TaskEnvironmentUI::createConfirmationActions()
{
    this->newCommandAction(
        CommandNames::ConfirmTaskEnvironment,
        QCoreApplication::translate("TaskEnvironment", "Finish"),
        IconUtil::get(":/images/Sketch_OK.svg"));

    this->newCommandAction(
        CommandNames::CancelTaskEnvironment,
        QCoreApplication::translate("TaskEnvironment", "Cancel"),
        IconUtil::get(":/images/Sketch_Cancel.svg"));
}

void TaskEnvironmentUI::setConfirmationBarVisible(bool visible)
{
    _confirmationVisible = visible;
    if (_pConfirmationBar)
    {
        if (visible)
        {
            _pConfirmationBar->show();
        }
        else
        {
            _pConfirmationBar->hide();
        }
    }
}

void TaskEnvironmentUI::createConfirmationBar()
{
    if (_pConfirmationBar)
    {
        return;
    }

    CommandAction* pConfirmAction =
        this->findCommandAction(CommandNames::ConfirmTaskEnvironment);
    CommandAction* pCancelAction =
        this->findCommandAction(CommandNames::CancelTaskEnvironment);
    if (!pConfirmAction || !pCancelAction)
    {
        return;
    }

    QWidget* pViewWidget = ApplicationImpl::instance().getActiveViewWidget();
    if (!pViewWidget)
    {
        return;
    }
    _pConfirmationBar = new ConfirmationBar(pViewWidget, pConfirmAction, pCancelAction);
    _pConfirmationBar->hide();
}

void TaskEnvironmentUI::destroyConfirmationBar()
{
    if (_pConfirmationBar)
    {
        _pConfirmationBar->hide();
        // Deferred: this is normally called from a click on one of the bar's
        // own buttons, and QToolButton::mouseReleaseEvent() still touches its
        // private data after the click signal returns. Deleting the bar here
        // would free the button under the running event handler.
        // destroy() calls this before EnvironmentUIImpl::destroy(), so the
        // queued bar deletion runs before the queued command-action deletion.
        _pConfirmationBar->deleteLater();
        _pConfirmationBar = nullptr;
    }
}

void TaskEnvironmentUI::destroy()
{
    // The bar's buttons reference command actions; drop the bar before the
    // environment UI destroys those actions. The base destroy also clears
    // _cmdName2Action, so later findCommandAction() calls return nullptr.
    this->destroyConfirmationBar();

    EnvironmentUIImpl::destroy();
}

void TaskEnvironmentUI::restoreUiState()
{
    EnvironmentUIImpl::restoreUiState();

    // Single creation point for the confirmation bar: build() only records the
    // visibility request, so this is the first moment the floating bar can be
    // created with a settled layout. Called from both onEnvironmentEntered()
    // and onEnvironmentResumed(), and always reached even when there is no
    // saved ui-state file. The bar is created for every session; visibility is
    // a separate show()/hide() concern.
    if (!_pConfirmationBar)
        this->createConfirmationBar();
    if (!_pConfirmationBar)
        return;
    if (!_confirmationVisible)
        return;

    // restoreState() may only have posted the relayout request. Settle the
    // layout in the next event-loop turn before the first show(), so
    // showEvent -> anchorToTopLeft() uses the final viewport position.
    QTimer::singleShot(0, this, [this]()
    {
        if (!_confirmationVisible || !_pConfirmationBar)
            return;

        if (MainWindow* pMainWindow = ApplicationImpl::instance().getMainWindowImpl())
        {
            if (QLayout* pLayout = pMainWindow->layout())
                pLayout->activate();
        }

        _pConfirmationBar->show();
    });
}

} // namespace wy3dApp
