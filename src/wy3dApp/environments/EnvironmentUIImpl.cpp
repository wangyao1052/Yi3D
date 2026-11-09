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

#include <algorithm>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCoreApplication>
#include <QFile>
#include <QIcon>
#include <QMenu>
#include <QMenuBar>
#include <QToolBar>
#include <QToolButton>
#include <QTimer>

#include "EnvironmentUIImpl.h"
#include <wy3dApp/Application.h>
#include <wy3dApp/CommandAction.h>
#include "application/ApplicationImpl.h"
#include "commands/CommandNames.h"
#include "commands/TaskEnvironmentCommands.h"
#include <wyapCommand.h>
#include <wy3dApp/Scene.h>
#include "widgets/frame/MainWindow.h"
#include "utils/IconUtil.h"
#include <wy3dApp/wy3dAppDefs.h>

#define TOOLBAR_ICON_SIZE 32

namespace wy3dApp {

EnvironmentUIImpl::EnvironmentUIImpl(const wyap::Environment* pEnvironment)
    : _defaultCmdName(CommandNames::Select)
    , _isCmdTerminatedOnEventLoopPosted(false)
    , _pEnvironment(pEnvironment)
{
    assert(pEnvironment);

    _toolBars.reserve(20);
    _actionGroups.reserve(10);

    _cmdTerminatedOnEventLoopTimer.setSingleShot(true);
    QObject::connect(&_cmdTerminatedOnEventLoopTimer, &QTimer::timeout, this, [this]()
    {
        _isCmdTerminatedOnEventLoopPosted = false;
        this->processCommandTerminatedOnEventLoop();
    });
}

EnvironmentUIImpl::~EnvironmentUIImpl()
{
    this->destroyConfirmationBar();
}

QMenu* EnvironmentUIImpl::newMenu(const QString& title, const std::string& name)
{
    MainWindow* pMainWindow = ApplicationImpl::instance().getMainWindowImpl();
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

void EnvironmentUIImpl::insertMenuAfter(QMenu* pAnchorMenu, QMenu* pMenu)
{
    if (!pAnchorMenu || !pMenu)
    {
        assert(false);
        return;
    }

    MainWindow* pMainWindow = ApplicationImpl::instance().getMainWindowImpl();
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

QToolBar* EnvironmentUIImpl::newToolBar(const QString& title, const std::string& name)
{
    MainWindow* pMainWindow = ApplicationImpl::instance().getMainWindowImpl();
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

const std::vector<QMenu*>& EnvironmentUIImpl::getMenus() const
{
    return _menus;
}

const std::vector<QToolBar*>& EnvironmentUIImpl::getToolBars() const
{
    return _toolBars;
}

QActionGroup* EnvironmentUIImpl::newActionGroup()
{
    QActionGroup* pActionGroup = new QActionGroup(this);
    _actionGroups.emplace_back(pActionGroup);
    return pActionGroup;
}

CommandAction* EnvironmentUIImpl::findCommandAction(const std::string& commandName) const
{
    auto iter = _cmdName2Action.find(commandName);
    return (_cmdName2Action.cend() != iter) ? iter->second : nullptr;
}

CommandAction* EnvironmentUIImpl::getConfirmAction()
{
    this->ensureConfirmationActions();
    return _pConfirmAction.data();
}

CommandAction* EnvironmentUIImpl::getCancelAction()
{
    this->ensureConfirmationActions();
    return _pCancelAction.data();
}

void EnvironmentUIImpl::ensureConfirmationActions()
{
    ensureTaskEnvironmentCommandsRegistered();

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

void EnvironmentUIImpl::setConfirmationBarVisible(bool visible)
{
    if (_confirmationVisible == visible)
    {
        if (!visible)
        {
            if (_pConfirmationBar)
            {
                _pConfirmationBar->hide();
            }
            return;
        }
        if (_pConfirmationBar)
        {
            _pConfirmationBar->show();
            return;
        }
    }

    _confirmationVisible = visible;
    if (!visible)
    {
        if (_pConfirmationBar)
        {
            _pConfirmationBar->hide();
        }
        return;
    }

    this->rebuildConfirmationBar();
}

void EnvironmentUIImpl::rebuildConfirmationBar()
{
    this->destroyConfirmationBar();
    if (!_confirmationVisible)
    {
        return;
    }

    this->ensureConfirmationActions();
    if (!_pConfirmAction || !_pCancelAction)
    {
        return;
    }

    QWidget* pViewWidget = ApplicationImpl::instance().getActiveViewWidget();
    if (!pViewWidget)
    {
        return;
    }

    _pConfirmationBar = new ConfirmationBar(
        pViewWidget, _pConfirmAction.data(), _pCancelAction.data());
    _pConfirmationBar->show();
}

void EnvironmentUIImpl::destroyConfirmationBar()
{
    if (_pConfirmationBar)
    {
        _pConfirmationBar->hide();
        // Synchronous: the environment UI that owns the button actions may be
        // destroyed before a queued deleteLater() would run.
        delete _pConfirmationBar;
    }
}

CommandAction* EnvironmentUIImpl::newCommandAction(
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

    _commandActions.insert(pAction);
    _cmdName2Action.emplace(pAction->getCommandName(), pAction);

    return pAction;
};

void EnvironmentUIImpl::deleteCommandAction(CommandAction* pCommandAction)
{
    if (_commandActions.erase(pCommandAction) == 0) return;

    pCommandAction->setVisible(false);
    pCommandAction->setEnabled(false);

    auto iter = _cmdName2Action.find(pCommandAction->getCommandName());
    if (iter != _cmdName2Action.end() && iter->second == pCommandAction)
        _cmdName2Action.erase(iter);

    pCommandAction->deleteLater();
}

void EnvironmentUIImpl::deleteMenu(QMenu* pMenu)
{
    const auto iter = std::find(_menus.begin(), _menus.end(), pMenu);
    if (iter == _menus.end()) return;

    if (QMenuBar* pMenuBar = qobject_cast<QMenuBar*>(pMenu->parentWidget()))
        pMenuBar->removeAction(pMenu->menuAction());
    pMenu->deleteLater();
    _menus.erase(iter);
}

void EnvironmentUIImpl::deleteToolBar(QToolBar* pToolBar)
{
    const auto iter = std::find(_toolBars.begin(), _toolBars.end(), pToolBar);
    if (iter == _toolBars.end()) return;

    MainWindow* pMainWindow = ApplicationImpl::instance().getMainWindowImpl();
    if (pMainWindow) pMainWindow->removeToolBar(pToolBar);
    pToolBar->setObjectName(QString());
    pToolBar->deleteLater();
    _toolBars.erase(iter);
}

void EnvironmentUIImpl::deleteActionGroup(QActionGroup* pActionGroup)
{
    const auto iter = std::find(_actionGroups.begin(), _actionGroups.end(), pActionGroup);
    if (iter == _actionGroups.end()) return;

    pActionGroup->deleteLater();
    _actionGroups.erase(iter);
}

QToolButton* EnvironmentUIImpl::newMenuPopupToolButton(
    QToolBar* pToolBar,
    const QString& text,
    QActionGroup* pActionGroup,
    const std::list<QAction*>& actions,
    QAction* pInitialAction)
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

    QAction* pCurrentAction = pInitialAction;
    QMenu* pMenu = new QMenu(pToolBtn);
    for (QAction* pAction : actions)
    {
        if (!pAction)
        {
            assert(false);
            continue;
        }
        pMenu->addAction(pAction);
        if (!pCurrentAction) pCurrentAction = pAction;
        QObject::connect(pAction, &QAction::triggered, pToolBtn, [pToolBtn, pAction]()
        {
            EnvironmentUIImpl::setMenuPopupToolButtonAction(pToolBtn, pAction);
        });
    }
    pToolBtn->setMenu(pMenu);

    setMenuPopupToolButtonAction(pToolBtn, pCurrentAction);

    return pToolBtn;
}

bool EnvironmentUIImpl::addContextMenuActions(ContextMenu&)
{
    return false;
}

void EnvironmentUIImpl::destroy()
{
    // The bar's buttons reference command actions; drop the bar before any of
    // those actions are destroyed. Clear the pointers too so a later caller
    // cannot resurrect an action that is already scheduled for deletion.
    this->destroyConfirmationBar();
    _confirmationVisible = false;
    _pConfirmAction = nullptr;
    _pCancelAction = nullptr;

    this->cancelCommandTerminatedOnEventLoop();

    // Nothing left to destroy: either destroy() was already called, or the UI
    // was never built. The containers themselves are the destroyed-state.
    if (_menus.empty() && _toolBars.empty()
        && _actionGroups.empty() && _cmdName2Action.empty()
        && _commandActions.empty())
    {
        return;
    }

    this->saveUiState();
    this->destroyToolBars();
    this->destroyMenus();
    this->destroyActionGroups();
    this->destroyCommandActions();
}

void EnvironmentUIImpl::destroyMenus()
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

// Plugin-created environments participate too. Their authors must make sure any
// dock/toolbar that can outlive the plugin module uses a heap-owned name and is
// removed from the main window before the DLL is unloaded; otherwise a later
// QMainWindow::saveState() can read strings from unmapped plugin memory.
void EnvironmentUIImpl::saveUiState()
{
    if (!_pEnvironment) return;
    MainWindow* pMainWindow = ApplicationImpl::instance().getMainWindowImpl();
    if (!pMainWindow) return;

    const QString path = QCoreApplication::applicationDirPath()
        + "/ui-state-" + _pEnvironment->getName().c_str() + ".dat";
    QFile file(path);
    if (file.open(QIODevice::WriteOnly))
    {
        const QByteArray state = pMainWindow->saveState();
        const qint64 written = file.write(state);
        if (written != state.size())
            file.remove();
    }
}

void EnvironmentUIImpl::destroyToolBars()
{
    MainWindow* pMainWindow = ApplicationImpl::instance().getMainWindowImpl();
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

void EnvironmentUIImpl::restoreUiState()
{
    if (!_pEnvironment) return;
    MainWindow* pMainWindow = ApplicationImpl::instance().getMainWindowImpl();
    if (!pMainWindow) return;

    const QString path = QCoreApplication::applicationDirPath()
        + "/ui-state-" + _pEnvironment->getName().c_str() + ".dat";
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return;
    const QByteArray state = file.readAll();
    if (!state.isEmpty())
        pMainWindow->restoreState(state);
}

void EnvironmentUIImpl::destroyActionGroups()
{
    for (QActionGroup* pActionGroup : _actionGroups)
        if (pActionGroup) pActionGroup->deleteLater();
    _actionGroups.clear();
}

void EnvironmentUIImpl::destroyCommandActions()
{
    for (CommandAction* pAction : _commandActions)
        if (pAction) pAction->deleteLater();
    _commandActions.clear();
    _cmdName2Action.clear();
}

void EnvironmentUIImpl::postCommandTerminatedOnEventLoop()
{
    if (_isCmdTerminatedOnEventLoopPosted)
    {
        return;
    }

    _isCmdTerminatedOnEventLoopPosted = true;
    _cmdTerminatedOnEventLoopTimer.start(0);
}

void EnvironmentUIImpl::cancelCommandTerminatedOnEventLoop()
{
    if (!_isCmdTerminatedOnEventLoopPosted)
    {
        return;
    }

    _cmdTerminatedOnEventLoopTimer.stop();
    _isCmdTerminatedOnEventLoopPosted = false;
}

void EnvironmentUIImpl::processCommandTerminatedOnEventLoop()
{
    // An empty default command means automatic restart is disabled.
    if (_defaultCmdName.empty()) return;

    wyap::Environment* pActiveEnv = ApplicationImpl::instance().getEnvManager()->getActiveEnvironment();
    if (pActiveEnv != _pEnvironment)
    {
        return;
    }

    wyap::Command* pCurModalCmd = ApplicationImpl::instance().getCmdManager()->getCurrentModalCommand();
    if (!pCurModalCmd)
    {
        ApplicationImpl::instance().getCmdManager()->executeCommand(_defaultCmdName);
    }
}

void EnvironmentUIImpl::onCommandStartFailed(
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

void EnvironmentUIImpl::onCommandStarted(wyap::Command* pCmd)
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

void EnvironmentUIImpl::onCommandEnded(wyap::Command* pCmd)
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

void EnvironmentUIImpl::onCommandAborted(
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

void EnvironmentUIImpl::setMenuPopupToolButtonAction(QToolButton* pButton, QAction* pAction)
{
    if (!pButton || !pAction) return;

    pButton->setIcon(pAction->icon());
    pButton->setText(pAction->text());
    pButton->setToolTip(pAction->toolTip());
    pButton->setDefaultAction(pAction);
}

} // namespace wy3dApp
