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

#ifndef WY3DAPP_ENVIRONMENT_BASE_H
#define WY3DAPP_ENVIRONMENT_BASE_H

#include <QObject>
#include <QAction>
#include <QMenu>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>

#include <wyapEnvironment.h>
#include <wyapCmdStack.h>

#include "commands/CommandAction.h"

class EnvironmentBase : public QObject
{
    Q_OBJECT
public:
    EnvironmentBase();
    virtual ~EnvironmentBase();

    // Add menu.
    QMenu* addMenu(const QString& title, const std::string& name);

    // Add tool bar.
    QToolBar* addToolBar(const QString& title, const std::string& name);

    // Get all menus.
    inline const std::vector<QMenu*>& getMenus() const
    {
        return _menus;
    }

    // Get all tool bars.
    inline const std::vector<QToolBar*>& getToolBars() const
    {
        return _toolBars;
    }

    // New action group.
    inline QActionGroup* newActionGroup()
    {
        QActionGroup* pActionGroup = new QActionGroup(this);
        _actionGroups.emplace_back(pActionGroup);
        return pActionGroup;
    }

    // New command action.
    CommandAction* newCommandAction(
        const std::string& commandName,
        const QString& text,
        const QIcon& icon,
        QActionGroup* pActionGroup = nullptr);

    // Find command action by command name.
    inline CommandAction* findCommandAction(const std::string& commandName) const
    {
        auto iter = _cmdName2Action.find(commandName);
        return (_cmdName2Action.cend() != iter) ? iter->second : nullptr;
    }

    // New menu popup tool button.
    QToolButton* newMenuPopupToolButton(
        QToolBar* pToolBar,
        const QString& text,
        QActionGroup* pActionGroup,
        const std::list<QAction*>& actions);

    // Destroy all ui elements created and managed by this environment base instance.
    void destroyUI();

protected:
    // command manager reactor
    void onCommandStartFailed(
        wyap::Command* pCmd,
        wyap::CmdExecution::StartResult startResult);
    void onCommandStarted(wyap::Command* pCmd);
    void onCommandEnded(wyap::Command* pCmd);
    void onCommandAborted(
        wyap::Command* pCmd,
        wyap::CmdExecution::AbortCause abortCause);

    void onEnter();
    void onExit(wyap::Environment::ExitCode exitCode);
    void onSuspend();
    void onResume();

    // 同步显示模式按钮状态
    void syncDisplayModeAction();

private:
    // Schedules a one-shot call to handle command-terminated tasks
    // when control next returns to the event loop. Idempotent.
    void postCommandTerminatedOnEventLoop();
    // Cancels the pending one-shot call that handles command-terminated tasks.
    void cancelCommandTerminatedOnEventLoop();

    // Executes the scheduled command-terminated tasks.
    // Called from the event loop callback.
    virtual void processCommandTerminatedOnEventLoop();

private:
    // Destroy all menus.
    void destroyMenus();
    // Destroy all tool bars.
    void destroyToolBars();
    // Destroy all action groups.
    void destroyActionGroups();
    // Destroy all command actions.
    void destroyCommandActions();

private:
    // menus
    std::vector<QMenu*> _menus;
    // toolbars
    std::vector<QToolBar*> _toolBars;
    // action groups
    std::vector<QActionGroup*> _actionGroups;
    // command name <> command action
    std::map<std::string, CommandAction*> _cmdName2Action;
    // One-shot timer used to defer command-terminated handling to the event loop.
    QTimer _cmdTerminatedOnEventLoopTimer;
    // Guard to prevent duplicate posting of command-terminated tasks to the event loop.
    bool _isCmdTerminatedOnEventLoopPosted;
};

#endif // WY3DAPP_ENVIRONMENT_BASE_H
