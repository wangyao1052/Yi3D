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

#include "PluginManagerCommands.h"

#include <cassert>

#include <QPointer>

#include <wy3dApp/EnvironmentUI.h>
#include <wy3dApp/wy3dAppDefs.h>
#include <wyapCmdManager.h>

#include "application/ApplicationImpl.h"
#include "widgets/frame/MainWindow.h"
#include "widgets/frame/PluginManagerDialog.h"

namespace wy3dApp {

void showPluginManagerDialog()
{
    QPointer<EnvironmentUI> pActiveEnvUI =
        ApplicationImpl::instance().getActiveEnvironmentUI();
    if (!pActiveEnvUI)
    {
        assert(false);
        return;
    }

    wyap::CmdManager* pCmdMgr = ApplicationImpl::instance().getCmdManager();
    const std::string defaultCmdName = pActiveEnvUI->getDefaultCommandName();
    pActiveEnvUI->setDefaultCommandName("");

    if (pCmdMgr->getCurrentModalCommand())
    {
        const wy::ErrorStatus error = pCmdMgr->abortCurrentModalCommand(
            wyap::CmdExecution::AbortCause::ForceTerminate);
        if (wy::ErrorStatus::Ok != error)
        {
            assert(false);
            pActiveEnvUI->setDefaultCommandName(defaultCmdName);
            return;
        }
    }
    
    // abortCurrentModalCommand() calls tryProcessPendingRequest(), so a pending
    // async environment transition or command request may run here.
    if (pCmdMgr->getCurrentModalCommand()
        || !pActiveEnvUI
        || pActiveEnvUI != ApplicationImpl::instance().getActiveEnvironmentUI())
    {
        if (pActiveEnvUI) pActiveEnvUI->setDefaultCommandName(defaultCmdName);
        return;
    }

    {
        PluginManagerDialog dialog(ApplicationImpl::instance().getMainWindowImpl());
        dialog.exec();
    }

    if (!pActiveEnvUI) return;
    pActiveEnvUI->setDefaultCommandName(defaultCmdName);
    if (pActiveEnvUI == ApplicationImpl::instance().getActiveEnvironmentUI()
        && !defaultCmdName.empty())
    {
        pCmdMgr->executeCommand(defaultCmdName);
    }
}

} // namespace wy3dApp
