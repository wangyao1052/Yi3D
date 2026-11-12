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

#include <wy3dApp/TaskEnvironment.h>

#include <cassert>
#include <string>
#include <utility>

#include <wyapCmdStack.h>
#include <wyapEnvManager.h>
#include <wyapEnums.h>

#include "TaskEnvironmentUI.h"
#include "application/ApplicationImpl.h"
#include "commands/CommandNames.h"
#include "commands/TaskEnvironmentCommands.h"

namespace wy3dApp {

namespace {

// The Finish/Cancel commands are intrinsic to every task environment session:
// onEnter/onResume register them, onSuspend/onExit remove them. DuplicateKey
// and KeyNotFound are tolerated so repeated transitions stay harmless.
void registerTaskEnvironmentCommands()
{
    wyap::CmdStack* pCmdStack = ApplicationImpl::instance().getCmdStack();
    if (!pCmdStack)
    {
        return;
    }

    const unsigned int flags = WYAP_CMD_MODAL | WYAP_CMD_NOHISTORY;
    const auto addCommand = [pCmdStack, flags](
        const std::string& name, wyrx::ClassInfo* pClassInfo)
    {
        const wy::ErrorStatus status =
            pCmdStack->addCommand(name, flags, pClassInfo);
        assert(status == wy::ErrorStatus::Ok
            || status == wy::ErrorStatus::DuplicateKey);
        (void)status;
    };

    addCommand(CommandNames::ConfirmTaskEnvironment,
               ConfirmTaskEnvironmentCommand::classInfo());
    addCommand(CommandNames::CancelTaskEnvironment,
               CancelTaskEnvironmentCommand::classInfo());
}

void removeTaskEnvironmentCommands()
{
    wyap::CmdStack* pCmdStack = ApplicationImpl::instance().getCmdStack();
    if (!pCmdStack)
    {
        return;
    }

    const auto removeCommand = [pCmdStack](const std::string& name)
    {
        const wy::ErrorStatus status = pCmdStack->removeCommand(name);
        assert(status == wy::ErrorStatus::Ok
            || status == wy::ErrorStatus::KeyNotFound);
        (void)status;
    };

    removeCommand(CommandNames::ConfirmTaskEnvironment);
    removeCommand(CommandNames::CancelTaskEnvironment);
}

} // namespace

TaskEnvironment::TaskEnvironment()
    : _pUI(std::make_unique<TaskEnvironmentUI>(this))
{
}

TaskEnvironment::TaskEnvironment(std::unique_ptr<TaskEnvironmentUI> pUI)
    : _pUI(std::move(pUI))
{
    assert(_pUI);
}

TaskEnvironment::~TaskEnvironment() = default;

EnvironmentUI* TaskEnvironment::getUI() const
{
    return _pUI.get();
}

CommandAction* TaskEnvironment::getConfirmAction() const
{
    return _pUI ? _pUI->getConfirmAction() : nullptr;
}

CommandAction* TaskEnvironment::getCancelAction() const
{
    return _pUI ? _pUI->getCancelAction() : nullptr;
}

void TaskEnvironment::setConfirmationBarVisible(bool visible)
{
    if (_pUI)
    {
        _pUI->setConfirmationBarVisible(visible);
    }
}

void TaskEnvironment::onConfirm()
{
    wyap::EnvManager* pEnvMgr = ApplicationImpl::instance().getEnvManager();
    if (!pEnvMgr || pEnvMgr->getActiveEnvironment() != this)
    {
        return;
    }

    const wy::ErrorStatus status = pEnvMgr->exitActiveEnvironment(
        ExitCode::Ok, wyap::ExecutionMode::Async);
    assert(status == wy::ErrorStatus::Ok);
    (void)status;
}

void TaskEnvironment::onCancel()
{
    wyap::EnvManager* pEnvMgr = ApplicationImpl::instance().getEnvManager();
    if (!pEnvMgr || pEnvMgr->getActiveEnvironment() != this)
    {
        return;
    }

    const wy::ErrorStatus status = pEnvMgr->exitActiveEnvironment(
        ExitCode::Cancel, wyap::ExecutionMode::Async);
    assert(status == wy::ErrorStatus::Ok);
    (void)status;
}

void TaskEnvironment::onEnter()
{
    wyap::TaskEnvironment::onEnter();

    registerTaskEnvironmentCommands();
    if (_pUI)
    {
        _pUI->createConfirmationActions();
    }
}

void TaskEnvironment::onResume()
{
    wyap::TaskEnvironment::onResume();

    registerTaskEnvironmentCommands();
    if (_pUI)
    {
        _pUI->createConfirmationActions();
    }
}

void TaskEnvironment::onExit(ExitCode exitCode)
{
    if (_pUI)
    {
        _pUI->destroy();
    }
    removeTaskEnvironmentCommands();

    wyap::TaskEnvironment::onExit(exitCode);
}

void TaskEnvironment::onSuspend()
{
    if (_pUI)
    {
        _pUI->destroy();
    }
    removeTaskEnvironmentCommands();

    wyap::TaskEnvironment::onSuspend();
}

void TaskEnvironment::onCommandStartFailed(
    wyap::Command* pCmd,
    wyap::CmdExecution::StartResult startResult)
{
    if (_pUI)
    {
        _pUI->onCommandStartFailed(pCmd, startResult);
    }
}

void TaskEnvironment::onCommandStarted(wyap::Command* pCmd)
{
    if (_pUI)
    {
        _pUI->onCommandStarted(pCmd);
    }
}

void TaskEnvironment::onCommandEnded(wyap::Command* pCmd)
{
    if (_pUI)
    {
        _pUI->onCommandEnded(pCmd);
    }
}

void TaskEnvironment::onCommandAborted(
    wyap::Command* pCmd,
    wyap::CmdExecution::AbortCause abortCause)
{
    if (_pUI)
    {
        _pUI->onCommandAborted(pCmd, abortCause);
    }
}

} // namespace wy3dApp
