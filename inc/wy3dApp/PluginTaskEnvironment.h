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

#ifndef WY3DAPP_PLUGIN_TASK_ENVIRONMENT_H
#define WY3DAPP_PLUGIN_TASK_ENVIRONMENT_H

#include <memory>

#include <wyapCmdManager.h>

#include <wy3dApp/TaskEnvironment.h>
#include <wy3dApp/wy3dAppDefs.h>

class QMainWindow;

namespace wy3dApp {

class EnvironmentUI;
class EnvironmentUIImpl;

// Base class for plugin-provided task environments.
//
// The host creates and owns the EnvironmentUI in the constructor, matching the
// concrete host environments. A plugin only has to build its menus/tool bars
// and (optionally) dock widgets. Derived classes should call the base
// implementation of onEnter() first, then build their UI via
// getUI()/mainWindow(); on exit they should remove their own dock
// widgets and commands after calling the base onExit().
// Derived classes may also override addContextMenuActions() to contribute
// environment-specific items to the command context menu.
class WY3DAPP_EXPORT PluginTaskEnvironment
    : public wy3dApp::TaskEnvironment
{
public:
    ~PluginTaskEnvironment() override;

    EnvironmentUI* getUI() const override;

    // Host main window, used to add/remove dock widgets.
    QMainWindow* mainWindow() const;

    // Plugin environments have no host-owned command actions to refresh.
    void refreshCommandActionStates(CommandActionRefreshCause cause) override;

protected:
    PluginTaskEnvironment();

    void onEnter() override;
    void onExit(wyap::Environment::ExitCode exitCode) override;
    void onSuspend() override;
    void onResume() override;

    void onCommandStartFailed(
        wyap::Command* pCmd,
        wyap::CmdExecution::StartResult startResult) override;
    void onCommandStarted(wyap::Command* pCmd) override;
    void onCommandEnded(wyap::Command* pCmd) override;
    void onCommandAborted(
        wyap::Command* pCmd,
        wyap::CmdExecution::AbortCause abortCause) override;

private:
    std::unique_ptr<EnvironmentUIImpl> _pUI;
};

} // namespace wy3dApp

#endif // WY3DAPP_PLUGIN_TASK_ENVIRONMENT_H
