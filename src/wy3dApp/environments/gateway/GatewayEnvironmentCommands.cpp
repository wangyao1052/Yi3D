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

#include "GatewayEnvironment.h"

#include <cassert>
#include <cstddef>

#include <wyapCmdStack.h>
#include <wyrxClassInfo.h>

#include "application/Application.h"
#include "commands/CommandNames.h"
#include "commands/FileCommands.h"
#include "commands/help/HelpCommands.h"

#define WY3DAPP_GATEWAY_ENV_COMMAND_LIST(X) \
    X(CommandNames::NewFile, WYAP_CMD_MODAL | WYAP_CMD_NOHISTORY, NewFileCommand::classInfo()) \
    X(CommandNames::OpenFile, WYAP_CMD_MODAL | WYAP_CMD_NOHISTORY, OpenFileCommand::classInfo()) \
    X(CommandNames::HelpDocumentation, WYAP_CMD_TRANSPARENT | WYAP_CMD_NOHISTORY, HelpDocumentationCommand::classInfo()) \
    X(CommandNames::ShortcutKeys, WYAP_CMD_TRANSPARENT | WYAP_CMD_NOHISTORY, ShortcutKeysCommand::classInfo()) \
    X(CommandNames::About, WYAP_CMD_TRANSPARENT | WYAP_CMD_NOHISTORY, AboutCommand::classInfo())

namespace
{
struct CommandEntry
{
    std::string commandName;
    unsigned int commandFlags;
    wyrx::ClassInfo* classDesc;
};

#define WY3DAPP_GATEWAY_ENV_COMMAND_ENTRY(commandName, commandFlags, classDesc) \
    { commandName, commandFlags, classDesc },
static const CommandEntry kCommandEntries[] =
{
    WY3DAPP_GATEWAY_ENV_COMMAND_LIST(WY3DAPP_GATEWAY_ENV_COMMAND_ENTRY)
};
#undef WY3DAPP_GATEWAY_ENV_COMMAND_ENTRY

static constexpr size_t kCommandEntryCount =
    sizeof(kCommandEntries) / sizeof(kCommandEntries[0]);
} // namespace

void GatewayEnvironment::registerCommands()
{
    wyap::CmdStack* pCmdStack = Application::instance().getCmdStack();
    if (!pCmdStack)
    {
        assert(false);
        return;
    }

    wy::ErrorStatus error = wy::ErrorStatus::Ok;
    for (size_t i = 0; i < kCommandEntryCount; ++i)
    {
        const CommandEntry& entry = kCommandEntries[i];
        error = pCmdStack->addCommand(entry.commandName, entry.commandFlags, entry.classDesc);
        assert(wy::ErrorStatus::Ok == error);
    }
}

void GatewayEnvironment::removeCommands()
{
    wyap::CmdStack* pCmdStack = Application::instance().getCmdStack();
    if (!pCmdStack)
    {
        assert(false);
        return;
    }

    wy::ErrorStatus error = wy::ErrorStatus::Ok;
    for (size_t i = kCommandEntryCount; i > 0; --i)
    {
        const CommandEntry& entry = kCommandEntries[i - 1];
        error = pCmdStack->removeCommand(entry.commandName);
        assert(wy::ErrorStatus::Ok == error);
    }
}
