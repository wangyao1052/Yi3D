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

#include <QApplication>
#include <QMenuBar>
#include <QToolButton>
#include <QTimer>

#include "EnvironmentBase.h"
#include "application/Application.h"
#include "commands/CommandNames.h"
#include "scene/Scene.h"
#include "widgets/frame/MainWindow.h"

#define TOOLBAR_ICON_SIZE 32

EnvironmentBase::EnvironmentBase() : _isCmdTerminatedOnEventLoopPosted(false)
{
    _toolBars.reserve(20);
    _actionGroups.reserve(10);

    _cmdTerminatedOnEventLoopTimer.setSingleShot(true);
    QObject::connect(&_cmdTerminatedOnEventLoopTimer, &QTimer::timeout, this, [this]()
    {
        _isCmdTerminatedOnEventLoopPosted = false;
        this->processCommandTerminatedOnEventLoop();
    });
}

EnvironmentBase::~EnvironmentBase()
{
}

QMenu* EnvironmentBase::addMenu(const QString& title, const std::string& name)
{
    MainWindow* pMainWindow = Application::instance().getMainWindow();
    if (!pMainWindow)
    {
        assert(false);
        return nullptr;
    }

    QMenuBar* pMenuBar = pMainWindow->menuBar();
    if (!pMenuBar)
    {
        assert(false);
        return nullptr;
    }

    QMenu* pMenu = new QMenu(pMenuBar);
    pMenu->setObjectName(name.c_str());
    pMenu->setTitle(title);
    pMenuBar->addAction(pMenu->menuAction());
    _menus.emplace_back(pMenu);
    return pMenu;
}

QToolBar* EnvironmentBase::addToolBar(const QString& title, const std::string& name)
{
    MainWindow* pMainWindow = Application::instance().getMainWindow();
    if (!pMainWindow)
    {
        assert(false);
        return nullptr;
    }
    QToolBar* pToolbar = pMainWindow->addToolBar(title);
    pToolbar->setObjectName(name.c_str());
    pToolbar->setIconSize(QSize(TOOLBAR_ICON_SIZE, TOOLBAR_ICON_SIZE));
    _toolBars.emplace_back(pToolbar);
    return pToolbar;
}

CommandAction* EnvironmentBase::newCommandAction(
    const std::string& commandName,
    const QString& text,
    const QIcon& icon,
    QActionGroup* pActionGroup)
{
    CommandAction* pAction = new CommandAction(commandName, this);
    pAction->setText(text);
    pAction->setIcon(icon);
    if (pActionGroup)
    {
        pAction->setCheckable(true);
        pAction->setChecked(false);
        pActionGroup->addAction(pAction);
    }

    _cmdName2Action.emplace(pAction->getCommandName(), pAction);

    return pAction;
};

QToolButton* EnvironmentBase::newMenuPopupToolButton(
    QToolBar* pToolBar,
    const QString& text,
    QActionGroup* pActionGroup,
    const std::list<QAction*>& actions)
{
    if (!pToolBar)
    {
        assert(false);
        return nullptr;
    }

    QAction* pActionSeries = new QAction(this);
    pActionSeries->setText(text);
    pActionSeries->setCheckable(true);
    pActionSeries->setChecked(false);
    if (pActionGroup) pActionGroup->addAction(pActionSeries);

    QToolButton* pToolBtn = new QToolButton(pToolBar);
    pToolBtn->setDefaultAction(pActionSeries);
    pToolBtn->setPopupMode(QToolButton::MenuButtonPopup);

    QMenu* pMenu = new QMenu(pToolBtn);
    int index(-1);
    for (QAction* pAction : actions)
    {
        if (!pAction)
        {
            assert(false);
            continue;
        }
        pMenu->addAction(pAction);
        ++index;
        if (0 == index)
        {
            pToolBtn->setIcon(pAction->icon());
            pToolBtn->setText(pAction->text());
            pToolBtn->setToolTip(pAction->toolTip());
            pToolBtn->setDefaultAction(pAction);
        }
        QObject::connect(pAction, &QAction::triggered, pToolBtn, [pToolBtn, pAction]()
        {
            pToolBtn->setIcon(pAction->icon());
            pToolBtn->setText(pAction->text());
            pToolBtn->setToolTip(pAction->toolTip());
            pToolBtn->setDefaultAction(pAction);
        });
    }
    pToolBtn->setMenu(pMenu);

    return pToolBtn;
}

void EnvironmentBase::destroyUI()
{
    this->destroyActionGroups();
    this->destroyMenus();
    this->destroyToolBars();
    this->destroyCommandActions();
}

void EnvironmentBase::destroyMenus()
{
    for (QMenu* pMenu : _menus)
    {
        if (!pMenu)
        {
            assert(false);
            continue;
        }
        delete pMenu;
    }
    _menus.clear();
}

void EnvironmentBase::destroyToolBars()
{
    for (QToolBar* pToolBar : _toolBars)
    {
        if (!pToolBar)
        {
            assert(false);
            continue;
        }
        delete pToolBar;
    }
    _toolBars.clear();
}

void EnvironmentBase::destroyActionGroups()
{
    for (QActionGroup* pActionGroup : _actionGroups)
    {
        if (!pActionGroup)
        {
            assert(false);
            continue;
        }
        delete pActionGroup;
    }
    _actionGroups.clear();
}

void EnvironmentBase::destroyCommandActions()
{
    for (auto& kvp : _cmdName2Action)
    {
        if (kvp.second)
        {
            delete kvp.second;
        }
        else
        {
            assert(false);
        }
    }
    _cmdName2Action.clear();
}

void EnvironmentBase::postCommandTerminatedOnEventLoop()
{
    if (_isCmdTerminatedOnEventLoopPosted)
    {
        return;
    }

    _isCmdTerminatedOnEventLoopPosted = true;
    _cmdTerminatedOnEventLoopTimer.start(0);
}

void EnvironmentBase::cancelCommandTerminatedOnEventLoop()
{
    if (!_isCmdTerminatedOnEventLoopPosted)
    {
        return;
    }

    _cmdTerminatedOnEventLoopTimer.stop();
    _isCmdTerminatedOnEventLoopPosted = false;
}

void EnvironmentBase::processCommandTerminatedOnEventLoop()
{
    wyap::Command* pCurModalCmd = Application::instance().getCmdManager()->getCurrentModalCommand();
    if (!pCurModalCmd)
    {
        Application::instance().getCmdManager()->executeCommand(CommandNames::Select);
    }
}

void EnvironmentBase::onCommandStartFailed(
    wyap::Command* pCmd,
    wyap::CmdExecution::StartResult startResult)
{
    if (!pCmd)
    {
        assert(false);
        return;
    }
    CommandAction* pCmdAction = this->findCommandAction(pCmd->getName());
    if (!pCmdAction)
    {
        return;
    }

    if (pCmdAction->isCheckable() && pCmdAction->isChecked())
    {
        QSignalBlocker blocker(pCmdAction);
        pCmdAction->setChecked(false);
    }

    if (pCmd->isModal())
    {
        this->postCommandTerminatedOnEventLoop();
    }
}

void EnvironmentBase::onCommandStarted(wyap::Command* pCmd)
{
    if (!pCmd)
    {
        assert(false);
        return;
    }
    CommandAction* pCmdAction = this->findCommandAction(pCmd->getName());
    if (!pCmdAction)
    {
        return;
    }

    if (pCmdAction->isCheckable() && !pCmdAction->isChecked())
    {
        QSignalBlocker blocker(pCmdAction);
        pCmdAction->setChecked(true);
    }
}

void EnvironmentBase::onCommandEnded(wyap::Command* pCmd)
{
    if (!pCmd)
    {
        assert(false);
        return;
    }
    CommandAction* pCmdAction = this->findCommandAction(pCmd->getName());
    if (pCmdAction)
    {
        if (pCmdAction->isCheckable() && pCmdAction->isChecked())
        {
            QSignalBlocker blocker(pCmdAction);
            pCmdAction->setChecked(false);
        }
    }

    if (pCmd->isModal())
    {
        this->postCommandTerminatedOnEventLoop();
    }
}

void EnvironmentBase::onCommandAborted(
    wyap::Command* pCmd,
    wyap::CmdExecution::AbortCause abortCause)
{
    if (!pCmd)
    {
        assert(false);
        return;
    }
    CommandAction* pCmdAction = this->findCommandAction(pCmd->getName());
    if (pCmdAction)
    {
        if (pCmdAction->isCheckable() && pCmdAction->isChecked())
        {
            QSignalBlocker blocker(pCmdAction);
            pCmdAction->setChecked(false);
        }
    }

    if (pCmd->isModal())
    {
        this->postCommandTerminatedOnEventLoop();
    }
}

void EnvironmentBase::onEnter()
{
}

void EnvironmentBase::onExit(wyap::Environment::ExitCode exitCode)
{
    this->cancelCommandTerminatedOnEventLoop();
}

void EnvironmentBase::onSuspend()
{
    this->cancelCommandTerminatedOnEventLoop();
}

void EnvironmentBase::onResume()
{
}

void EnvironmentBase::syncDisplayModeAction()
{
    Scene* pScene = Application::instance().getActiveScene();
    if (!pScene) return;

    CommandAction* pAction = nullptr;
    if (pScene->getDisplayMode() == Scene::DisplayMode::Wireframe)
    {
        pAction = this->findCommandAction(CommandNames::WireframeDisplay);
    }
    else
    {
        pAction = this->findCommandAction(CommandNames::ShadedDisplay);
    }
    if (pAction) pAction->trigger();
}
