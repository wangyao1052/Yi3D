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

#include <wy3dApp/CommandAction.h>
#include <wy3dApp/Application.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

CommandAction::CommandAction(
    const std::string& commandName,
    QObject* parent)
    : QAction(parent), _commandName(commandName)
{
    connect(this, &QAction::triggered, this, &CommandAction::onTriggered);
}

void CommandAction::onTriggered()
{
    wyap::CmdStack* pCmdStack = Application::instance().getCmdStack();
    wyap::CmdManager* pCmdMgr = Application::instance().getCmdManager();

    wyap::Command* pCmd = pCmdStack->getCommand(_commandName);
    if (!pCmd)
    {
        assert(false);
        return;
    }

    // modal command
    if (pCmd->isModal())
    {
        wyap::Command* pCurModalCmd = pCmdMgr->getCurrentModalCommand();
        if (pCurModalCmd)
        {
            pCmdMgr->abortCurrentModalCommand(
                wyap::CmdExecution::AbortCause::ForceTerminate);
        }
        pCmdMgr->executeCommand(_commandName);
    }
    // transparent command
    else
    {
        assert(pCmd->isTransparent());
        pCmdMgr->executeCommand(_commandName);
    }
}

} // namespace wy3dApp
