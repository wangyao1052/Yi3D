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
#include <QCoreApplication>
#include <QFile>
#include <QMenuBar>
#include <QToolButton>
#include <QTimer>

#include <wy3dApp/EnvironmentUI.h>
#include <wy3dApp/Application.h>
#include "application/ApplicationImpl.h"
#include "commands/CommandNames.h"
#include <wy3dApp/Scene.h>
#include "widgets/frame/MainWindow.h"
#include <wy3dApp/wy3dAppDefs.h>

#define TOOLBAR_ICON_SIZE 32

namespace wy3dApp {

EnvironmentUI::EnvironmentUI() : _isCmdTerminatedOnEventLoopPosted(false)
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

EnvironmentUI::~EnvironmentUI()
{
}

QMenu* EnvironmentUI::addMenu(const QString& title, const std::string& name)
{
    MainWindow* pMainWindow = ApplicationImpl::instance().getMainWindow();
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

void EnvironmentUI::insertMenuAfter(QMenu* pAnchorMenu, QMenu* pMenu)
{
    if (!pAnchorMenu || !pMenu)
    {
        assert(false);
        return;
    }

    MainWindow* pMainWindow = ApplicationImpl::instance().getMainWindow();
    if (!pMainWindow)
    {
        assert(false);
        return;
    }

    QMenuBar* pMenuBar = pMainWindow->menuBar();
    if (!pMenuBar)
    {
        assert(false);
        return;
    }

    // insertMenu inserts before its anchor, so look up the action after the anchor menu
    const QList<QAction*> menuActions = pMenuBar->actions();
    for (int i = 0; i + 1 < menuActions.size(); ++i)
    {
        if (menuActions[i] == pAnchorMenu->menuAction())
        {
            pMenuBar->insertMenu(menuActions[i + 1], pMenu);
            return;
        }
    }
}

QToolBar* EnvironmentUI::addToolBar(const QString& title, const std::string& name)
{
    MainWindow* pMainWindow = ApplicationImpl::instance().getMainWindow();
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

CommandAction* EnvironmentUI::newCommandAction(
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

QToolButton* EnvironmentUI::newMenuPopupToolButton(
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
    // Hand the series action to the button: with the default parent (this environment) it would
    // outlive every UI rebuild and pile up one action per rebuild for the life of the document.
    pActionSeries->setParent(pToolBtn);
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

void EnvironmentUI::destroyUI()
{
    this->saveUiState();
    this->destroyToolBars();
    this->destroyMenus();
    this->destroyActionGroups();
    this->destroyCommandActions();
}

void EnvironmentUI::destroyMenus()
{
    for (QMenu* pMenu : _menus)
    {
        if (!pMenu)
        {
            assert(false);
            continue;
        }
        if (QMenuBar* pMenuBar = qobject_cast<QMenuBar*>(pMenu->parentWidget()))
            pMenuBar->removeAction(pMenu->menuAction());
        pMenu->deleteLater();
    }
    _menus.clear();
}

void EnvironmentUI::setEnvironment(const wyap::Environment* pEnv)
{
    _envName = pEnv ? pEnv->getName() : std::string();
}

void EnvironmentUI::saveUiState()
{
    if (_envName.empty()) return;
    MainWindow* pMainWindow = ApplicationImpl::instance().getMainWindow();
    if (!pMainWindow) return;

    const QString path = QCoreApplication::applicationDirPath()
        + "/ui-state-" + _envName.c_str() + ".dat";
    QFile file(path);
    if (file.open(QIODevice::WriteOnly))
    {
        const QByteArray state = pMainWindow->saveState();
        const qint64 written = file.write(state);
        if (written != state.size())
            file.remove();
    }
}

void EnvironmentUI::destroyToolBars()
{
    MainWindow* pMainWindow = ApplicationImpl::instance().getMainWindow();
    for (QToolBar* pToolBar : _toolBars)
    {
        if (!pToolBar)
        {
            assert(false);
            continue;
        }
        if (pMainWindow) pMainWindow->removeToolBar(pToolBar);
        // Deletion is deferred, so the tool bar is still around when the next environment
        // builds its UI. Drop the name now: a lookup would otherwise hand out this doomed
        // tool bar instead of the live one the next environment just created.
        pToolBar->setObjectName(QString());
        pToolBar->deleteLater();
    }
    _toolBars.clear();
}

void EnvironmentUI::restoreUiState()
{
    if (_envName.empty()) return;
    MainWindow* pMainWindow = ApplicationImpl::instance().getMainWindow();
    if (!pMainWindow) return;

    const QString path = QCoreApplication::applicationDirPath()
        + "/ui-state-" + _envName.c_str() + ".dat";
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return;
    const QByteArray state = file.readAll();
    if (!state.isEmpty())
        pMainWindow->restoreState(state);
}

void EnvironmentUI::destroyActionGroups()
{
    for (QActionGroup* pActionGroup : _actionGroups)
        if (pActionGroup) pActionGroup->deleteLater();
    _actionGroups.clear();
}

void EnvironmentUI::destroyCommandActions()
{
    for (auto& kvp : _cmdName2Action)
        if (kvp.second) kvp.second->deleteLater();
    _cmdName2Action.clear();
}

void EnvironmentUI::postCommandTerminatedOnEventLoop()
{
    if (_isCmdTerminatedOnEventLoopPosted)
    {
        return;
    }

    _isCmdTerminatedOnEventLoopPosted = true;
    _cmdTerminatedOnEventLoopTimer.start(0);
}

void EnvironmentUI::cancelCommandTerminatedOnEventLoop()
{
    if (!_isCmdTerminatedOnEventLoopPosted)
    {
        return;
    }

    _cmdTerminatedOnEventLoopTimer.stop();
    _isCmdTerminatedOnEventLoopPosted = false;
}

void EnvironmentUI::processCommandTerminatedOnEventLoop()
{
    wyap::Command* pCurModalCmd = ApplicationImpl::instance().getCmdManager()->getCurrentModalCommand();
    if (!pCurModalCmd)
    {
        ApplicationImpl::instance().getCmdManager()->executeCommand(CommandNames::Select);
    }
}

void EnvironmentUI::onCommandStartFailed(
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

void EnvironmentUI::onCommandStarted(wyap::Command* pCmd)
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

void EnvironmentUI::onCommandEnded(wyap::Command* pCmd)
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

void EnvironmentUI::onCommandAborted(
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

void EnvironmentUI::onEnter()
{
}

void EnvironmentUI::onExit(wyap::Environment::ExitCode exitCode)
{
    this->cancelCommandTerminatedOnEventLoop();
}

void EnvironmentUI::onSuspend()
{
    this->cancelCommandTerminatedOnEventLoop();
}

void EnvironmentUI::onResume()
{
}

void EnvironmentUI::syncDisplayModeAction()
{
    Scene* pScene = ApplicationImpl::instance().getActiveScene();
    if (!pScene) return;

    CommandAction* pAction = nullptr;
    switch (pScene->getDisplayMode())
    {
    case Scene::DisplayMode::ShadedWithEdges:
        pAction = this->findCommandAction(CommandNames::ShadedWithEdgesDisplay);
        break;
    case Scene::DisplayMode::Shaded:
        pAction = this->findCommandAction(CommandNames::ShadedDisplay);
        break;
    case Scene::DisplayMode::Wireframe:
        pAction = this->findCommandAction(CommandNames::WireframeDisplay);
        break;
    }
    if (pAction) pAction->trigger();
}

} // namespace wy3dApp
