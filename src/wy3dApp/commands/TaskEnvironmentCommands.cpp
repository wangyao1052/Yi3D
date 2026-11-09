///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 Wang Yao <wangyao1052@163.com>
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

#include "TaskEnvironmentCommands.h"

#include <cassert>

#include <wyapCmdStack.h>
#include <wyapEnvManager.h>
#include <wyapEnums.h>

#include <wy3dApp/TaskEnvironment.h>
#include "application/ApplicationImpl.h"
#include "commands/CommandNames.h"

namespace wy3dApp {

int ConfirmTaskEnvironmentCommand::run()
{
    wyap::EnvManager* pEnvMgr = ApplicationImpl::instance().getEnvManager();
    auto* pEnvironment = pEnvMgr
        ? dynamic_cast<TaskEnvironment*>(pEnvMgr->getActiveEnvironment())
        : nullptr;
    if (!pEnvironment)
    {
        return -1;
    }

    pEnvironment->onConfirm();
    return 0;
}

int CancelTaskEnvironmentCommand::run()
{
    wyap::EnvManager* pEnvMgr = ApplicationImpl::instance().getEnvManager();
    auto* pEnvironment = pEnvMgr
        ? dynamic_cast<TaskEnvironment*>(pEnvMgr->getActiveEnvironment())
        : nullptr;
    if (!pEnvironment)
    {
        return -1;
    }

    pEnvironment->onCancel();
    return 0;
}

void ensureTaskEnvironmentCommandsRegistered()
{
    static bool s_registered = false;
    if (s_registered)
    {
        return;
    }

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
        return status == wy::ErrorStatus::Ok
            || status == wy::ErrorStatus::DuplicateKey;
    };

    if (addCommand(CommandNames::ConfirmTaskEnvironment,
                   ConfirmTaskEnvironmentCommand::classInfo())
        && addCommand(CommandNames::CancelTaskEnvironment,
                      CancelTaskEnvironmentCommand::classInfo()))
    {
        s_registered = true;
    }
}

} // namespace wy3dApp
