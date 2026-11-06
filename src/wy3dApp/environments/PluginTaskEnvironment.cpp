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

#include <wy3dApp/PluginTaskEnvironment.h>

#include <QMainWindow>

#include <wy3dApp/Application.h>
#include <wy3dApp/EnvironmentUI.h>

#include "EnvironmentUIImpl.h"

namespace wy3dApp {

PluginTaskEnvironment::PluginTaskEnvironment()
    : _pUI(std::make_unique<EnvironmentUIImpl>(this))
{
}

PluginTaskEnvironment::~PluginTaskEnvironment() = default;

EnvironmentUI* PluginTaskEnvironment::getUI() const
{
    return _pUI.get();
}

QMainWindow* PluginTaskEnvironment::mainWindow() const
{
    Application* pApp = getApplication();
    return pApp ? pApp->getMainWindow() : nullptr;
}

void PluginTaskEnvironment::refreshCommandActionStates(CommandActionRefreshCause cause)
{
    (void)cause;

    // Plugin environments have no host-owned command actions to refresh.
}

void PluginTaskEnvironment::onEnter()
{
    wy3dApp::TaskEnvironment::onEnter();
}

void PluginTaskEnvironment::onExit(wyap::Environment::ExitCode exitCode)
{
    _pUI->destroy();
    wy3dApp::TaskEnvironment::onExit(exitCode);
}

void PluginTaskEnvironment::onSuspend()
{
    wy3dApp::TaskEnvironment::onSuspend();
}

void PluginTaskEnvironment::onResume()
{
    wy3dApp::TaskEnvironment::onResume();
}

void PluginTaskEnvironment::onCommandStartFailed(
    wyap::Command* pCmd,
    wyap::CmdExecution::StartResult startResult)
{
    _pUI->onCommandStartFailed(pCmd, startResult);
}

void PluginTaskEnvironment::onCommandStarted(wyap::Command* pCmd)
{
    _pUI->onCommandStarted(pCmd);
}

void PluginTaskEnvironment::onCommandEnded(wyap::Command* pCmd)
{
    _pUI->onCommandEnded(pCmd);
}

void PluginTaskEnvironment::onCommandAborted(
    wyap::Command* pCmd,
    wyap::CmdExecution::AbortCause abortCause)
{
    _pUI->onCommandAborted(pCmd, abortCause);
}

} // namespace wy3dApp
