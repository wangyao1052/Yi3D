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
    return _pConfirmAction.data();
}

CommandAction* TaskEnvironmentUI::getCancelAction()
{
    return _pCancelAction.data();
}

void TaskEnvironmentUI::createConfirmationActions()
{
    if (!_pConfirmAction)
    {
        _pConfirmAction = this->findCommandAction(CommandNames::ConfirmTaskEnvironment);
    }
    if (!_pConfirmAction)
    {
        _pConfirmAction = this->newCommandAction(
            CommandNames::ConfirmTaskEnvironment,
            QCoreApplication::translate("TaskEnvironment", "Finish"),
            IconUtil::get(":/images/Sketch_OK.svg"));
    }

    if (!_pCancelAction)
    {
        _pCancelAction = this->findCommandAction(CommandNames::CancelTaskEnvironment);
    }
    if (!_pCancelAction)
    {
        _pCancelAction = this->newCommandAction(
            CommandNames::CancelTaskEnvironment,
            QCoreApplication::translate("TaskEnvironment", "Cancel"),
            IconUtil::get(":/images/Sketch_Cancel.svg"));
    }
}

void TaskEnvironmentUI::setConfirmationBarVisible(bool visible)
{
    _confirmationVisible = visible;
    if (!_pConfirmationBar)
    {
        // Not created yet: the single creation point in restoreUiState() will
        // honor _confirmationVisible once the environment layout has settled.
        return;
    }

    if (visible)
    {
        _pConfirmationBar->show();
    }
    else
    {
        _pConfirmationBar->hide();
    }
}

void TaskEnvironmentUI::createConfirmationBar()
{
    if (_pConfirmationBar || !_confirmationVisible)
    {
        return;
    }

    if (!_pConfirmAction || !_pCancelAction)
    {
        return;
    }

    QWidget* pViewWidget = ApplicationImpl::instance().getActiveViewWidget();
    if (!pViewWidget)
    {
        return;
    }

    // Created hidden; restoreUiState() shows it after the layout has settled.
    _pConfirmationBar = new ConfirmationBar(
        pViewWidget, _pConfirmAction.data(), _pCancelAction.data());
}

void TaskEnvironmentUI::destroyConfirmationBar()
{
    if (_pConfirmationBar)
    {
        _pConfirmationBar->hide();
        // Synchronous: the command actions the buttons reference may be
        // destroyed before a queued deleteLater() would run.
        delete _pConfirmationBar;
    }
}

void TaskEnvironmentUI::destroy()
{
    // The bar's buttons reference command actions; drop the bar before any of
    // those actions are destroyed. Clear the pointers too so a later caller
    // cannot resurrect an action that is already scheduled for deletion.
    this->destroyConfirmationBar();
    _pConfirmAction = nullptr;
    _pCancelAction = nullptr;

    EnvironmentUIImpl::destroy();
}

void TaskEnvironmentUI::restoreUiState()
{
    EnvironmentUIImpl::restoreUiState();

    // Single creation point for the confirmation bar: build() only records the
    // visibility request, so this is the first moment the floating bar can be
    // created with a settled layout. Called from both onEnvironmentEntered()
    // and onEnvironmentResumed(), and always reached even when there is no
    // saved ui-state file.
    if (!_confirmationVisible)
        return;

    if (!_pConfirmationBar)
        this->createConfirmationBar();
    if (!_pConfirmationBar)
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
