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

#ifndef WY3DAPP_ENVIRONMENT_UI_IMPL_H
#define WY3DAPP_ENVIRONMENT_UI_IMPL_H

#include <QTimer>

#include <list>
#include <map>
#include <string>
#include <vector>

#include <wyapCmdStack.h>
#include <wyapEnvironment.h>

#include <wy3dApp/EnvironmentUI.h>

class QAction;
class QActionGroup;
class QIcon;
class QMenu;
class QToolBar;
class QToolButton;

namespace wy3dApp {

class CommandAction;
class ContextMenu;

class EnvironmentUIImpl : public EnvironmentUI
{
public:
    explicit EnvironmentUIImpl(const wyap::Environment* pEnvironment);
    ~EnvironmentUIImpl() override;

    // EnvironmentUI
    QMenu* addMenu(const QString& title, const std::string& name) override;
    QToolBar* addToolBar(const QString& title, const std::string& name) override;
    QActionGroup* newActionGroup() override;
    CommandAction* newCommandAction(
        const std::string& commandName,
        const QString& text,
        const QIcon& icon,
        QActionGroup* pActionGroup = nullptr) override;
    bool addContextMenuActions(ContextMenu& menu) override;
    const std::vector<QMenu*>& getMenus() const override;
    const std::vector<QToolBar*>& getToolBars() const override;

    // Insert menu immediately after an existing menu in the menu bar.
    void insertMenuAfter(QMenu* pAnchorMenu, QMenu* pMenu);

    // Find command action by command name.
    CommandAction* findCommandAction(const std::string& commandName) const;

    // New menu popup tool button.
    QToolButton* newMenuPopupToolButton(
        QToolBar* pToolBar,
        const QString& text,
        QActionGroup* pActionGroup,
        const std::list<QAction*>& actions);

    // Destroy all ui elements created and managed by this environment UI instance.
    void destroyUI();

    // Save toolbar/dock positions to persisted state.
    void saveUiState();
    // Restore toolbar/dock positions from persisted state.
    void restoreUiState();

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

    // Sync the display mode action state.
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
    // The owning environment, used for UI state persistence.
    const wyap::Environment* _pEnvironment;
};

} // namespace wy3dApp

#endif // WY3DAPP_ENVIRONMENT_UI_IMPL_H
