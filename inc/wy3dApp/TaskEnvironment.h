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

namespace wy3dApp {

class ContextMenu;
class EnvironmentUIImpl;

// Base class for task environments.
//
// The environment owns an EnvironmentUI. The default constructor creates the
// host-provided UI, so plugin environments only need to derive from this class
// and build their menus/tool bars/dock widgets through getUI(); the host main
// window is available through getApplication()->getMainWindow(). Built-in
// environments that need a specialized UI pass it to the second constructor.
//
// Derived classes should call the base implementation of onExit() so the UI is
// destroyed, and call the base implementations of onEnter()/onSuspend()/
// onResume() when overriding them.
class WY3DAPP_EXPORT TaskEnvironment : public wyap::TaskEnvironment
{
    WYRX_DECLARE_NO_CREATE_MEMBERS(TaskEnvironment, wy3dApp::TaskEnvironment, wyap::TaskEnvironment)

public:
    // Get the environment UI owned by this environment.
    EnvironmentUI* getUI() const;

    // Refresh the states of the command actions owned by this environment.
    // The default implementation does nothing.
    virtual void refreshCommandActionStates(CommandActionRefreshCause cause);

    // Override to add environment-specific items to a command context menu.
    // Returns true if any actions were added.
    virtual bool addContextMenuActions(ContextMenu& menu)
    {
        (void)menu;
        return false;
    }

protected:
    // Creates the host-provided default UI.
    TaskEnvironment();
    // Takes ownership of a specialized UI created by a built-in environment.
    explicit TaskEnvironment(std::unique_ptr<EnvironmentUIImpl> pUI);
    ~TaskEnvironment();

    virtual void onExit(ExitCode exitCode) override;

    virtual void onCommandStartFailed(
        wyap::Command* pCmd,
        wyap::CmdExecution::StartResult startResult) override;
    virtual void onCommandStarted(wyap::Command* pCmd) override;
    virtual void onCommandEnded(wyap::Command* pCmd) override;
    virtual void onCommandAborted(
        wyap::Command* pCmd,
        wyap::CmdExecution::AbortCause abortCause) override;

private:
    std::unique_ptr<EnvironmentUIImpl> _pUI;
};

} // namespace wy3dApp

#endif // WY3DAPP_TASK_ENVIRONMENT_H
