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

#ifndef WY3DAPP_TASK_ENVIRONMENT_H
#define WY3DAPP_TASK_ENVIRONMENT_H

#include <memory>

#include <wyapEnvironment.h>
#include <wy3dApp/EnvironmentUI.h>
#include <wy3dApp/wy3dAppDefs.h>

class QActionGroup;

namespace wy3dApp {

class CommandAction;
class ContextMenu;
class TaskEnvironmentUI;

// Base class for task environments.
class WY3DAPP_EXPORT TaskEnvironment : public wyap::TaskEnvironment
{
    WYRX_DECLARE_NO_CREATE_MEMBERS(TaskEnvironment, wy3dApp::TaskEnvironment, wyap::TaskEnvironment)

public:
    // Get the environment UI owned by this environment.
    EnvironmentUI* getUI() const;

    // Host-owned confirmation actions. They have fixed icons and default texts
    // (Finish/Cancel); environments may set their text/tooltip and add them to
    // a menu, toolbar or QActionGroup.
    CommandAction* getConfirmAction() const;
    CommandAction* getCancelAction() const;

    // Show or hide the ConfirmationBar over the active viewport. Hiding the
    // bar does not disable the confirmation actions shared with menus/toolbars.
    void setConfirmationBarVisible(bool visible);

    // Called by the host Confirm/Cancel commands. The default implementations
    // exit this environment with ExitCode::Ok/Cancel using Async execution.
    // Environments may override to validate or customize the action.
    virtual void onConfirm();
    virtual void onCancel();

    // Refresh the states of the command actions owned by this environment.
    // The default implementation does nothing.
    virtual void refreshCommandActionStates(CommandActionRefreshCause cause) {}

    // Override to add environment-specific items to a command context menu.
    // Returns true if any actions were added.
    virtual bool addContextMenuActions(ContextMenu& menu) { return false; }

protected:
    TaskEnvironment();
    explicit TaskEnvironment(std::unique_ptr<TaskEnvironmentUI> pUI);
    ~TaskEnvironment();

    virtual void onEnter() override;
    virtual void onResume() override;
    virtual void onExit(ExitCode exitCode) override;
    virtual void onSuspend() override;

    virtual void onCommandStartFailed(
        wyap::Command* pCmd,
        wyap::CmdExecution::StartResult startResult) override;
    virtual void onCommandStarted(wyap::Command* pCmd) override;
    virtual void onCommandEnded(wyap::Command* pCmd) override;
    virtual void onCommandAborted(
        wyap::Command* pCmd,
        wyap::CmdExecution::AbortCause abortCause) override;

private:
    std::unique_ptr<TaskEnvironmentUI> _pUI;
};

} // namespace wy3dApp

#endif // WY3DAPP_TASK_ENVIRONMENT_H
