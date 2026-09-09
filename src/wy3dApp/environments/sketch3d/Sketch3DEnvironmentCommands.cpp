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

#include "Sketch3DEnvironment.h"

#include <cassert>
#include <cstddef>

#include <wyapCmdStack.h>
#include <wyrxClassInfo.h>

#include "application/Application.h"
#include "commands/CommandNames.h"
#include "commands/edit/Sketch3DSelectGuiCmd.h"
#include "commands/ViewCommands.h"
#include "commands/sketch3d/SketchDrawLine3DGuiCmd.h"
#include "commands/sketch3d/SketchDrawCircle3DGuiCmd.h"
#include "commands/UndoRedoCommands.h"
#include "commands/sketch3d/Sketch3DCommands.h"

#define WY3DAPP_SKETCH3D_ENV_COMMAND_LIST(X) \
    X(CommandNames::Select, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, Sketch3DSelectGuiCmd::classInfo()) \
    X(CommandNames::Line3D, WYAP_CMD_MODAL, SketchDrawLine3DGuiCmd::classInfo()) \
    X(CommandNames::Circle3D, WYAP_CMD_MODAL, SketchDrawCircle3DGuiCmd::classInfo()) \
    X(CommandNames::Undo, WYAP_CMD_TRANSPARENT | WYAP_CMD_NOHISTORY, UndoCommand::classInfo()) \
    X(CommandNames::Redo, WYAP_CMD_TRANSPARENT | WYAP_CMD_NOHISTORY, RedoCommand::classInfo()) \
    X(CommandNames::EndSketch3D, WYAP_CMD_MODAL | WYAP_CMD_NOHISTORY, EndSketch3DCommand::classInfo()) \
    X(CommandNames::CancelSketch3D, WYAP_CMD_MODAL | WYAP_CMD_NOHISTORY, CancelSketch3DCommand::classInfo()) \
    X(CommandNames::FitView, WYAP_CMD_TRANSPARENT | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, FitViewCommand::classInfo()) \
    X(CommandNames::FitSelection, WYAP_CMD_TRANSPARENT | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, FitSelectionCommand::classInfo()) \
    X(CommandNames::IsometricView, WYAP_CMD_TRANSPARENT | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, IsometricViewCommand::classInfo()) \
    X(CommandNames::FrontView, WYAP_CMD_TRANSPARENT | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, FrontViewCommand::classInfo()) \
    X(CommandNames::BackView, WYAP_CMD_TRANSPARENT | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, BackViewCommand::classInfo()) \
    X(CommandNames::LeftView, WYAP_CMD_TRANSPARENT | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, LeftViewCommand::classInfo()) \
    X(CommandNames::RightView, WYAP_CMD_TRANSPARENT | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, RightViewCommand::classInfo()) \
    X(CommandNames::TopView, WYAP_CMD_TRANSPARENT | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, TopViewCommand::classInfo()) \
    X(CommandNames::BottomView, WYAP_CMD_TRANSPARENT | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, BottomViewCommand::classInfo()) \
    X(CommandNames::ShadedWithEdgesDisplay, WYAP_CMD_TRANSPARENT | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, ShadedWithEdgesDisplayCommand::classInfo()) \
    X(CommandNames::ShadedDisplay, WYAP_CMD_TRANSPARENT | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, ShadedDisplayCommand::classInfo()) \
    X(CommandNames::WireframeDisplay, WYAP_CMD_TRANSPARENT | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, WireframeDisplayCommand::classInfo())

namespace
{
struct CommandEntry
{
    std::string commandName;
    unsigned int commandFlags;
    wyrx::ClassInfo* classDesc;
};

#define WY3DAPP_SKETCH3D_ENV_COMMAND_ENTRY(commandName, commandFlags, classDesc) \
    { commandName, commandFlags, classDesc },
static const CommandEntry kCommandEntries[] =
{
    WY3DAPP_SKETCH3D_ENV_COMMAND_LIST(WY3DAPP_SKETCH3D_ENV_COMMAND_ENTRY)
};
#undef WY3DAPP_SKETCH3D_ENV_COMMAND_ENTRY

static constexpr size_t kCommandEntryCount =
    sizeof(kCommandEntries) / sizeof(kCommandEntries[0]);
} // namespace

void Sketch3DEnvironment::registerCommands()
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

void Sketch3DEnvironment::removeCommands()
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
