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

#include "ModelingEnvironment.h"

#include <cassert>
#include <cstddef>

#include <wyapCmdStack.h>
#include <wyrxClassInfo.h>

#include "application/Application.h"
#include "commands/CommandNames.h"
#include "commands/edit/ModelingSelectGuiCmd.h"
#include "commands/modeling/solid/primitives/PrimitiveCommands.h"
#include "commands/modeling/solid/boolean/BooleanGuiCmds.h"
#include "commands/modeling/solid/generation/ExtrudeGuiCmd.h"
#include "commands/modeling/solid/generation/RevolveGuiCmd.h"
#include "commands/modeling/sheet/generation/ExtrudedSheetGuiCmd.h"
#include "commands/modeling/sheet/generation/RevolvedSheetGuiCmd.h"
#include "commands/modeling/sheet/generation/ThickenGuiCmd.h"
#include "commands/modeling/sheet/generation/OffsetSheetGuiCmd.h"
#include "commands/modeling/solid/generation/SweepGuiCmd.h"
#include "commands/modeling/solid/generation/LoftGuiCmd.h"
#include "commands/modeling/solid/MergeGuiCmd.h"
#include "commands/modeling/solid/modification/ChamferGuiCmd.h"
#include "commands/modeling/solid/modification/FilletGuiCmd.h"
#include "commands/modeling/solid/modification/ShellGuiCmd.h"
#include "commands/modeling/solid/modification/DraftGuiCmd.h"
#ifdef _DEBUG
#include "commands/test/TopoNameGuiCmd.h"
#include "commands/test/CheckTopoNameGuiCmd.h"
#endif // _DEBUG
#include "commands/datumPlane/ParallelDatumPlnCmd.h"
#include "commands/datumPlane/CoincidentDatumPlnCmd.h"
#include "commands/datumPlane/AngularDatumPlnCmd.h"
#include "commands/datumPlane/NormalToEdgeDatumPlnCmd.h"
#include "commands/datumPlane/Through3PointsDatumPlnCmd.h"
#include "commands/datumPlane/TangentDatumPlnCmd.h"
#include "commands/modeling/curves/HelixGuiCmd.h"
#include "commands/sketch/NewSketchGuiCmd.h"
#include "commands/sketch/SketchCommands.h"
#include "commands/edit/ModelingMoveGuiCmd.h"
#include "commands/edit/EditCommands.h"
#include "commands/edit/ModelingRotateGuiCmd.h"
#include "commands/edit/MirrorGuiCmd.h"
#include "commands/edit/LinearPatternGuiCmd.h"
#include "commands/edit/CircularPatternGuiCmd.h"
#include "commands/utilities/MeasureDistanceGuiCmd.h"
#include "commands/utilities/SetColorGuiCmd.h"
#include "commands/utilities/FindElementByIdCommand.h"
#include "commands/FileCommands.h"
#include "commands/UndoRedoCommands.h"
#include "commands/ViewCommands.h"
#include "commands/CameraCommands.h"

#define WY3DAPP_MODELING_ENV_COMMAND_LIST(X) \
    X(CommandNames::Select, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, ModelingSelectGuiCmd::classInfo()) \
    X(CommandNames::NewSketch, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST, NewSketchGuiCmd::classInfo()) \
    X(CommandNames::ParallelDatumPlane, WYAP_CMD_MODAL, ParallelDatumPlnCmd::classInfo()) \
    X(CommandNames::CoincidentDatumPlane, WYAP_CMD_MODAL, CoincidentDatumPlnCmd::classInfo()) \
    X(CommandNames::AngularDatumPlane, WYAP_CMD_MODAL, AngularDatumPlnCmd::classInfo()) \
    X(CommandNames::PerpendicularDatumPlane, WYAP_CMD_MODAL, PerpendicularDatumPlnCmd::classInfo()) \
    X(CommandNames::ThroughAxisDatumPlane, WYAP_CMD_MODAL, ThroughAxisDatumPlnCmd::classInfo()) \
    X(CommandNames::NormalToCurveDatumPlane, WYAP_CMD_MODAL, NormalToEdgeDatumPlnCmd::classInfo()) \
    X(CommandNames::Through3PointsDatumPlane, WYAP_CMD_MODAL, Through3PointsDatumPlnCmd::classInfo()) \
    X(CommandNames::TangentDatumPlane, WYAP_CMD_MODAL, TangentDatumPlnCmd::classInfo()) \
    X(CommandNames::Helix, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST, HelixGuiCmd::classInfo()) \
    X(CommandNames::Extrude, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST, ExtrudeGuiCmd::classInfo()) \
    X(CommandNames::Revolve, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST, RevolveGuiCmd::classInfo()) \
    X(CommandNames::ExtrudedSheet, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST, ExtrudedSheetGuiCmd::classInfo()) \
    X(CommandNames::RevolvedSheet, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST, RevolvedSheetGuiCmd::classInfo()) \
    X(CommandNames::Thicken, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST, ThickenGuiCmd::classInfo()) \
    X(CommandNames::OffsetSheet, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST, OffsetSheetGuiCmd::classInfo()) \
    X(CommandNames::Sweep, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST, SweepGuiCmd::classInfo()) \
    X(CommandNames::Loft, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST, LoftGuiCmd::classInfo()) \
    X(CommandNames::ExtrudeCut, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST, ExtrudeCutGuiCmd::classInfo()) \
    X(CommandNames::RevolveCut, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST, RevolveCutGuiCmd::classInfo()) \
    X(CommandNames::SweepCut, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST, SweepCutGuiCmd::classInfo()) \
    X(CommandNames::LoftCut, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST, LoftCutGuiCmd::classInfo()) \
    X(CommandNames::Merge, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST, MergeGuiCmd::classInfo()) \
    X(CommandNames::Chamfer, WYAP_CMD_MODAL, ChamferGuiCmd::classInfo()) \
    X(CommandNames::Fillet, WYAP_CMD_MODAL, FilletGuiCmd::classInfo()) \
    X(CommandNames::Shell, WYAP_CMD_MODAL, ShellGuiCmd::classInfo()) \
    X(CommandNames::Draft, WYAP_CMD_MODAL, DraftGuiCmd::classInfo()) \
    X(CommandNames::MakeBox, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST, MakeBoxGuiCmd::classInfo()) \
    X(CommandNames::MakeCylinder, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST, MakeCylinderGuiCmd::classInfo()) \
    X(CommandNames::MakeSphere, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST, MakeSphereGuiCmd::classInfo()) \
    X(CommandNames::MakeCone, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST, MakeConeGuiCmd::classInfo()) \
    X(CommandNames::MakeTorus, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST, MakeTorusGuiCmd::classInfo()) \
    X(CommandNames::MakeTube, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST, MakeTubeGuiCmd::classInfo()) \
    X(CommandNames::Union, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST, UnionGuiCmd::classInfo()) \
    X(CommandNames::Subtract, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST, SubtractGuiCmd::classInfo()) \
    X(CommandNames::Intersect, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST, IntersectGuiCmd::classInfo()) \
    X(CommandNames::Move, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST, ModelingMoveGuiCmd::classInfo()) \
    X(CommandNames::Rotate, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST, ModelingRotateGuiCmd::classInfo()) \
    X(CommandNames::Mirror, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST, MirrorGuiCmd::classInfo()) \
    X(CommandNames::LinearPattern, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST, LinearPatternGuiCmd::classInfo()) \
    X(CommandNames::CircularPattern, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST, CircularPatternGuiCmd::classInfo()) \
    X(CommandNames::SetColor, WYAP_CMD_MODAL, SetColorGuiCmd::classInfo()) \
    X(CommandNames::MeasureDistance, WYAP_CMD_MODAL, MeasureDistanceGuiCmd::classInfo()) \
    X(CommandNames::RunScript, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, RunScriptCommand::classInfo()) \
    X(CommandNames::FindElementById, WYAP_CMD_MODAL, FindElementByIdCommand::classInfo()) \
    X(CommandNames::Undo, WYAP_CMD_TRANSPARENT | WYAP_CMD_NOHISTORY, UndoCommand::classInfo()) \
    X(CommandNames::Redo, WYAP_CMD_TRANSPARENT | WYAP_CMD_NOHISTORY, RedoCommand::classInfo()) \
    X(CommandNames::SaveFile, WYAP_CMD_MODAL | WYAP_CMD_NOHISTORY, SaveFileCommand::classInfo()) \
    X(CommandNames::SaveAsFile, WYAP_CMD_MODAL | WYAP_CMD_NOHISTORY, SaveAsFileCommand::classInfo()) \
    X(CommandNames::ExportFile, WYAP_CMD_MODAL | WYAP_CMD_NOHISTORY, ExportFileCommand::classInfo()) \
    X(CommandNames::ExportSelected, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, ExportSelectedCommand::classInfo()) \
    X(CommandNames::ImportFile, WYAP_CMD_MODAL | WYAP_CMD_NOHISTORY, ImportFileCommand::classInfo()) \
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
    X(CommandNames::OrthoCamera, WYAP_CMD_TRANSPARENT | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, OrthoCameraCommand::classInfo()) \
    X(CommandNames::PerspectiveCamera, WYAP_CMD_TRANSPARENT | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, PerspectiveCameraCommand::classInfo()) \
    X(CommandNames::ShadedDisplay, WYAP_CMD_TRANSPARENT | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, ShadedDisplayCommand::classInfo()) \
    X(CommandNames::WireframeDisplay, WYAP_CMD_TRANSPARENT | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, WireframeDisplayCommand::classInfo()) \
    X(CommandNames::EditSketch, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, EditSketchCommand::classInfo()) \
    X(CommandNames::CopyClip, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, CopyClipCommand::classInfo()) \
    X(CommandNames::PasteClip, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, PasteClipCommand::classInfo()) \
    X(CommandNames::Show, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, ShowCommand::classInfo()) \
    X(CommandNames::Hide, WYAP_CMD_MODAL | WYAP_CMD_USEPICKFIRST | WYAP_CMD_NOHISTORY, HideCommand::classInfo())

namespace
{
struct CommandEntry
{
    std::string commandName;
    unsigned int commandFlags;
    wyrx::ClassInfo* classDesc;
};

#define WY3DAPP_MODELING_ENV_COMMAND_ENTRY(commandName, commandFlags, classDesc) \
    { commandName, commandFlags, classDesc },
static const CommandEntry kCommandEntries[] =
{
    WY3DAPP_MODELING_ENV_COMMAND_LIST(WY3DAPP_MODELING_ENV_COMMAND_ENTRY)
};
#undef WY3DAPP_MODELING_ENV_COMMAND_ENTRY

static constexpr size_t kCommandEntryCount =
    sizeof(kCommandEntries) / sizeof(kCommandEntries[0]);
} // namespace

void ModelingEnvironment::registerCommands()
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

#ifdef _DEBUG
    error = pCmdStack->addCommand(CommandNames::TopoName, WYAP_CMD_MODAL, TopoNameGuiCmd::classInfo());
    assert(wy::ErrorStatus::Ok == error);
    error = pCmdStack->addCommand(CommandNames::CheckTopoName, WYAP_CMD_MODAL, CheckTopoNameGuiCmd::classInfo());
    assert(wy::ErrorStatus::Ok == error);
#endif // _DEBUG
}

void ModelingEnvironment::removeCommands()
{
    wyap::CmdStack* pCmdStack = Application::instance().getCmdStack();
    if (!pCmdStack)
    {
        assert(false);
        return;
    }

    wy::ErrorStatus error = wy::ErrorStatus::Ok;
#ifdef _DEBUG
    error = pCmdStack->removeCommand(CommandNames::CheckTopoName);
    assert(wy::ErrorStatus::Ok == error);
    error = pCmdStack->removeCommand(CommandNames::TopoName);
    assert(wy::ErrorStatus::Ok == error);
#endif // _DEBUG

    for (size_t i = kCommandEntryCount; i > 0; --i)
    {
        const CommandEntry& entry = kCommandEntries[i - 1];
        error = pCmdStack->removeCommand(entry.commandName);
        assert(wy::ErrorStatus::Ok == error);
    }
}

