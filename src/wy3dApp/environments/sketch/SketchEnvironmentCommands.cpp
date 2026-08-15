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

#include "SketchEnvironment.h"

#include <cassert>
#include <cstddef>

#include <wyapCmdStack.h>
#include <wyrxClassInfo.h>

#include "application/Application.h"
#include "commands/CommandNames.h"
#include "commands/edit/SketchSelectGuiCmd.h"
#include "commands/ViewCommands.h"
#include "commands/sketch/SketchDrawPointGuiCmd.h"
#include "commands/sketch/SketchDrawLineGuiCmd.h"
#include "commands/sketch/SketchDrawLineTangentGuiCmd.h"
#include "commands/sketch/SketchDrawRectangleGuiCmd.h"
#include "commands/sketch/SketchDrawCenterRectangleGuiCmd.h"
#include "commands/sketch/SketchDrawPolygonGuiCmd.h"
#include "commands/sketch/SketchDrawCircleGuiCmd.h"
#include "commands/sketch/SketchDrawCircleBy3PointsGuiCmd.h"
#include "commands/sketch/SketchDrawArcGuiCmd.h"
#include "commands/sketch/SketchDrawArcBy3PointsGuiCmd.h"
#include "commands/sketch/SketchDrawEllipseGuiCmd.h"
#include "commands/sketch/SketchDrawEllipseArcGuiCmd.h"
#include "commands/sketch/SketchDrawSplineGuiCmd.h"
#include "commands/sketch/SketchEquationDrivenSplineCommand.h"
#include "commands/sketch/SketchDrawCenterLineGuiCmd.h"
#include "commands/sketch/SketchTextCommand.h"
#include "commands/edit/SketchMoveGuiCmd.h"
#include "commands/edit/CopyGuiCmd.h"
#include "commands/edit/EditCommands.h"
#include "commands/edit/SketchRotateGuiCmd.h"
#include "commands/edit/MirrorGuiCmd.h"
#include "commands/edit/SketchMirrorGuiCmd.h"
#include "commands/edit/SketchScaleGuiCmd.h"
#include "commands/sketch/SketchTrimGuiCmd.h"
#include "commands/sketch/SketchExtendGuiCmd.h"
#include "commands/sketch/SketchFilletGuiCmd.h"
#include "commands/sketch/SketchChamferGuiCmd.h"
#include "commands/sketch/SketchOffsetGuiCmd.h"
#include "commands/sketch/SketchRectArrayGuiCmd.h"
#include "commands/sketch/SketchPolarArrayGuiCmd.h"
#include "commands/edit/LinearPatternGuiCmd.h"
#include "commands/edit/CircularPatternGuiCmd.h"
#include "commands/FileCommands.h"
#include "commands/UndoRedoCommands.h"
#include "commands/sketch/SketchCommands.h"
#include "commands/sketch/SketchRelocateCsysGuiCmd.h"
#include "commands/sketch/SketchProjectGuiCmd.h"
#include "commands/utilities/FindElementByIdCommand.h"

#define WY3DAPP_SKETCH_ENV_COMMAND_LIST(X) \
    X(CommandNames::Select, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, SketchSelectGuiCmd::classInfo()) \
    X(CommandNames::Point, WYAP_CMD_MODAL, SketchDrawPointGuiCmd::classInfo()) \
    X(CommandNames::Line, WYAP_CMD_MODAL, SketchDrawLineGuiCmd::classInfo()) \
    X(CommandNames::LineTangent, WYAP_CMD_MODAL, SketchDrawLineTangentGuiCmd::classInfo()) \
    X(CommandNames::CenterLine, WYAP_CMD_MODAL, SketchDrawCenterLineGuiCmd::classInfo()) \
    X(CommandNames::Rectangle, WYAP_CMD_MODAL, SketchDrawRectangleGuiCmd::classInfo()) \
    X(CommandNames::CenterRectangle, WYAP_CMD_MODAL, SketchDrawCenterRectangleGuiCmd::classInfo()) \
    X(CommandNames::Polygon, WYAP_CMD_MODAL, SketchDrawPolygonGuiCmd::classInfo()) \
    X(CommandNames::Circle, WYAP_CMD_MODAL, SketchDrawCircleGuiCmd::classInfo()) \
    X(CommandNames::CircleBy3Points, WYAP_CMD_MODAL, SketchDrawCircleBy3PointsGuiCmd::classInfo()) \
    X(CommandNames::Arc, WYAP_CMD_MODAL, SketchDrawArcGuiCmd::classInfo()) \
    X(CommandNames::ArcBy3Points, WYAP_CMD_MODAL, SketchDrawArcBy3PointsGuiCmd::classInfo()) \
    X(CommandNames::Ellipse, WYAP_CMD_MODAL, SketchDrawEllipseGuiCmd::classInfo()) \
    X(CommandNames::EllipseArc, WYAP_CMD_MODAL, SketchDrawEllipseArcGuiCmd::classInfo()) \
    X(CommandNames::Spline, WYAP_CMD_MODAL, SketchDrawSplineGuiCmd_FitPoints::classInfo()) \
    X(CommandNames::StyleSpline, WYAP_CMD_MODAL, SketchDrawSplineGuiCmd_ControlPoints::classInfo()) \
    X(CommandNames::EquationDrivenSpline, WYAP_CMD_MODAL, SketchEquationDrivenSplineCommand::classInfo()) \
    X(CommandNames::SketchText, WYAP_CMD_MODAL, SketchTextCommand::classInfo()) \
    X(CommandNames::Move, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST, SketchMoveGuiCmd::classInfo()) \
    X(CommandNames::Copy, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST, CopyGuiCmd::classInfo()) \
    X(CommandNames::CopyClip, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, CopyClipCommand::classInfo()) \
    X(CommandNames::PasteClip, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, PasteClipCommand::classInfo()) \
    X(CommandNames::Rotate, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST, SketchRotateGuiCmd::classInfo()) \
    X(CommandNames::Mirror, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST, MirrorGuiCmd::classInfo()) \
    X(CommandNames::SketchMirror, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST, SketchMirrorGuiCmd::classInfo()) \
    X(CommandNames::SketchScale, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST, SketchScaleGuiCmd::classInfo()) \
    X(CommandNames::Trim, WYAP_CMD_MODAL, SketchTrimGuiCmd::classInfo()) \
    X(CommandNames::Extend, WYAP_CMD_MODAL, SketchExtendGuiCmd::classInfo()) \
    X(CommandNames::SketchFillet, WYAP_CMD_MODAL, SketchFilletGuiCmd::classInfo()) \
    X(CommandNames::SketchChamfer, WYAP_CMD_MODAL, SketchChamferGuiCmd::classInfo()) \
    X(CommandNames::SketchOffset, WYAP_CMD_MODAL, SketchOffsetGuiCmd::classInfo()) \
    X(CommandNames::SketchProject, WYAP_CMD_MODAL, SketchProjectGuiCmd::classInfo()) \
    X(CommandNames::SketchRectArray, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST, SketchRectArrayGuiCmd::classInfo()) \
    X(CommandNames::SketchPolarArray, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST, SketchPolarArrayGuiCmd::classInfo()) \
    X(CommandNames::LinearPattern, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST, LinearPatternGuiCmd::classInfo()) \
    X(CommandNames::CircularPattern, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST, CircularPatternGuiCmd::classInfo()) \
    X(CommandNames::Show, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, ShowCommand::classInfo()) \
    X(CommandNames::Hide, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, HideCommand::classInfo()) \
    X(CommandNames::FindElementById, WYAP_CMD_MODAL, FindElementByIdCommand::classInfo()) \
    X(CommandNames::Undo, WYAP_CMD_TRANSPARENT | WYAP_CMD_NOHISTORY, UndoCommand::classInfo()) \
    X(CommandNames::Redo, WYAP_CMD_TRANSPARENT | WYAP_CMD_NOHISTORY, RedoCommand::classInfo()) \
    X(CommandNames::EndSketch, WYAP_CMD_MODAL | WYAP_CMD_NOHISTORY, EndSketchCommand::classInfo()) \
    X(CommandNames::CancelSketch, WYAP_CMD_MODAL | WYAP_CMD_NOHISTORY, CancelSketchCommand::classInfo()) \
    X(CommandNames::RelocateSketchCSYS, WYAP_CMD_MODAL, SketchRelocateCsysGuiCmd::classInfo()) \
    X(CommandNames::FitView, WYAP_CMD_TRANSPARENT | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, FitViewCommand::classInfo()) \
    X(CommandNames::FitSelection, WYAP_CMD_TRANSPARENT | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, FitSelectionCommand::classInfo()) \
    X(CommandNames::IsometricView, WYAP_CMD_TRANSPARENT | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, IsometricViewCommand::classInfo()) \
    X(CommandNames::FrontView, WYAP_CMD_TRANSPARENT | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, FrontViewCommand::classInfo()) \
    X(CommandNames::BackView, WYAP_CMD_TRANSPARENT | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, BackViewCommand::classInfo()) \
    X(CommandNames::LeftView, WYAP_CMD_TRANSPARENT | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, LeftViewCommand::classInfo()) \
    X(CommandNames::RightView, WYAP_CMD_TRANSPARENT | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, RightViewCommand::classInfo()) \
    X(CommandNames::TopView, WYAP_CMD_TRANSPARENT | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, TopViewCommand::classInfo()) \
    X(CommandNames::BottomView, WYAP_CMD_TRANSPARENT | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, BottomViewCommand::classInfo()) \
    X(CommandNames::ViewNormalTo, WYAP_CMD_TRANSPARENT | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, ViewNormalToCommand::classInfo()) \
    X(CommandNames::OrientToSketch, WYAP_CMD_TRANSPARENT | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, OrientToSketchCommand::classInfo()) \
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

#define WY3DAPP_SKETCH_ENV_COMMAND_ENTRY(commandName, commandFlags, classDesc) \
    { commandName, commandFlags, classDesc },
static const CommandEntry kCommandEntries[] =
{
    WY3DAPP_SKETCH_ENV_COMMAND_LIST(WY3DAPP_SKETCH_ENV_COMMAND_ENTRY)
};
#undef WY3DAPP_SKETCH_ENV_COMMAND_ENTRY

static constexpr size_t kCommandEntryCount =
    sizeof(kCommandEntries) / sizeof(kCommandEntries[0]);
} // namespace

void SketchEnvironment::registerCommands()
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

void SketchEnvironment::removeCommands()
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
