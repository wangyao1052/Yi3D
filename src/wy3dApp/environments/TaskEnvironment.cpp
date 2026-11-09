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
#include <utility>

#include <wyapEnvManager.h>
#include <wyapEnums.h>

#include "EnvironmentUIImpl.h"
#include "application/ApplicationImpl.h"

namespace wy3dApp {

TaskEnvironment::TaskEnvironment()
    : _pUI(std::make_unique<EnvironmentUIImpl>(this))
{
}

TaskEnvironment::TaskEnvironment(std::unique_ptr<EnvironmentUIImpl> pUI)
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
}

void TaskEnvironment::onResume()
{
    wyap::TaskEnvironment::onResume();
}

void TaskEnvironment::onExit(ExitCode exitCode)
{
    if (_pUI)
    {
        _pUI->destroy();
    }

    wyap::TaskEnvironment::onExit(exitCode);
}

void TaskEnvironment::onSuspend()
{
    if (_pUI)
    {
        _pUI->destroy();
    }

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
