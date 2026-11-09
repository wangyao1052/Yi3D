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

#include <QPointer>
#include <QTimer>

#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <wyapCmdStack.h>
#include <wyapEnvironment.h>

#include <wy3dApp/CommandAction.h>
#include <wy3dApp/EnvironmentUI.h>
#include "widgets/frame/ConfirmationBar.h"

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
    QMenu* newMenu(const QString& title, const std::string& name) override;
    QToolBar* newToolBar(const QString& title, const std::string& name) override;
    QActionGroup* newActionGroup() override;
    CommandAction* newCommandAction(
        const std::string& commandName,
        const QString& text,
        const QIcon& icon,
        QActionGroup* pActionGroup = nullptr) override;
    virtual bool addContextMenuActions(ContextMenu& menu);
    const std::vector<QMenu*>& getMenus() const override;
    const std::vector<QToolBar*>& getToolBars() const override;

    // Insert menu immediately after an existing menu in the menu bar.
    void insertMenuAfter(QMenu* pAnchorMenu, QMenu* pMenu);

    // Find command action by command name.
    CommandAction* findCommandAction(const std::string& commandName) const override;

    virtual std::string getDefaultCommandName() const override
    {
        return _defaultCmdName;
    }
    virtual void setDefaultCommandName(const std::string& commandName) override
    {
        _defaultCmdName = commandName;
    }

    // Host-owned confirmation actions shared by ConfirmationBar, menus and
    // toolbars. They are created lazily with fixed icons and default texts.
    CommandAction* getConfirmAction();
    CommandAction* getCancelAction();

    // Show or hide the ConfirmationBar over the active viewport.
    void setConfirmationBarVisible(bool visible);

    // Remove one object created through this UI. Removal is structural: the
    // object must be present in this UI's bookkeeping.
    void deleteMenu(QMenu* pMenu) override;
    void deleteToolBar(QToolBar* pToolBar) override;
    void deleteActionGroup(QActionGroup* pActionGroup) override;
    void deleteCommandAction(CommandAction* pCommandAction) override;

    // New menu popup tool button. pInitialAction, when given, becomes the
    // button's initial icon/text/default action; otherwise the first action is used.
    QToolButton* newMenuPopupToolButton(
        QToolBar* pToolBar,
        const QString& text,
        QActionGroup* pActionGroup,
        const std::list<QAction*>& actions,
        QAction* pInitialAction = nullptr);

    // Destroy all ui elements created and managed by this environment UI instance.
    virtual void destroy();

    // Save toolbar/dock positions to persisted state.
    void saveUiState();
    // Restore toolbar/dock positions from persisted state.
    void restoreUiState();

    // Command notifications forwarded by the owning environment.
    virtual void onCommandStartFailed(
        wyap::Command* pCmd,
        wyap::CmdExecution::StartResult startResult);
    virtual void onCommandStarted(wyap::Command* pCmd);
    virtual void onCommandEnded(wyap::Command* pCmd);
    virtual void onCommandAborted(
        wyap::Command* pCmd,
        wyap::CmdExecution::AbortCause abortCause);

protected:
    // Set the icon/text/tool tip/default action of a menu popup tool button.
    static void setMenuPopupToolButtonAction(QToolButton* pButton, QAction* pAction);

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

    // Create the two host-owned confirmation command actions on first use.
    void ensureConfirmationActions();
    // Recreate/remove the confirmation bar according to the current state.
    void rebuildConfirmationBar();
    // Destroy the confirmation bar synchronously, before its actions.
    void destroyConfirmationBar();

private:
    // menus
    std::vector<QMenu*> _menus;
    // toolbars
    std::vector<QToolBar*> _toolBars;
    // action groups
    std::vector<QActionGroup*> _actionGroups;
    // command name <> command action
    std::map<std::string, CommandAction*> _cmdName2Action;
    // every command action created by this UI (including duplicate-name ones)
    std::set<CommandAction*> _commandActions;
    std::string _defaultCmdName;
    // One-shot timer used to defer command-terminated handling to the event loop.
    QTimer _cmdTerminatedOnEventLoopTimer;
    // Guard to prevent duplicate posting of command-terminated tasks to the event loop.
    bool _isCmdTerminatedOnEventLoopPosted;
    // The owning environment, used for UI state persistence.
    const wyap::Environment* _pEnvironment;
    // Confirmation UI: host-owned actions and the floating bar using them.
    QPointer<CommandAction> _pConfirmAction;
    QPointer<CommandAction> _pCancelAction;
    QPointer<ConfirmationBar> _pConfirmationBar;
    bool _confirmationVisible = false;
};

} // namespace wy3dApp

#endif // WY3DAPP_ENVIRONMENT_UI_IMPL_H
