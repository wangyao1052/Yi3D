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

#include "ModelingEnvironmentUI.h"

#include <cassert>
#include <list>

#include <QActionGroup>
#include <QCoreApplication>
#include <QIcon>
#include <QKeySequence>
#include <QToolButton>

#include "ModelingEnvironment.h"
#include "application/Application.h"
#include "scene/Scene.h"
#include "commands/CommandAction.h"
#include "commands/CommandNames.h"
#include "ui/MenuBarNames.h"
#include "ui/ToolBarNames.h"
#include "utils/IconUtil.h"
#include "widgets/frame/MainWindow.h"

namespace
{
struct UiTargets
{
    QMenu* pMenuFile;
    QMenu* pMenuTools;
    QMenu* pMenuEdit;
    QMenu* pMenuView;
    QMenu* pMenuSketch;
    QMenu* pMenuSolid;
    QMenu* pMenuSheet;
    QToolBar* pToolBarFile;
    QToolBar* pToolBarEdit;
    QToolBar* pToolBarModeling;
    QToolBar* pToolBarSheet;
    QToolBar* pToolBarUtility;
    QToolBar* pToolBarView;
#ifdef _DEBUG
    QToolBar* pToolBarTest;
#endif // _DEBUG
};

struct FileActions
{
    CommandAction* pActionSaveFile;
    CommandAction* pActionSaveAsFile;
    CommandAction* pActionImportFile;
    CommandAction* pActionExportFile;
    CommandAction* pActionImportSketch;
    CommandAction* pActionExportSketch;
};

struct UndoRedoActions
{
    CommandAction* pActionUndo;
    CommandAction* pActionRedo;
};

struct ModelingActions
{
    CommandAction* pActionSelect;
    CommandAction* pActionNewSketch;
    CommandAction* pActionNewSketch3D;
    CommandAction* pActionEditSketch;
    CommandAction* pActionParallelDatumPlane;
    CommandAction* pActionCoincidentDatumPlane;
    CommandAction* pActionAngularDatumPlane;
    CommandAction* pActionPerpendicularDatumPlane;
    CommandAction* pActionThroughAxisDatumPlane;
    CommandAction* pActionNormalToCurveDatumPlane;
    CommandAction* pActionThrough3PointsDatumPlane;
    CommandAction* pActionTangentDatumPlane;
    CommandAction* pActionHelix;
    CommandAction* pActionExtrude;
    CommandAction* pActionExtrudedSheet;
    CommandAction* pActionRevolvedSheet;
    CommandAction* pActionSweptSheet;
    CommandAction* pActionLoftedSheet;
    CommandAction* pActionPlanarSheet;
    CommandAction* pActionFilledSheet;
    CommandAction* pActionSewnSheet;
    CommandAction* pActionThicken;
    CommandAction* pActionSolidify;
    CommandAction* pActionOffsetSheet;
    CommandAction* pActionRevolve;
    CommandAction* pActionSweep;
    CommandAction* pActionLoft;
    CommandAction* pActionExtrudeCut;
    CommandAction* pActionRevolveCut;
    CommandAction* pActionSweepCut;
    CommandAction* pActionLoftCut;
    CommandAction* pActionMerge;
    CommandAction* pActionChamfer;
    CommandAction* pActionFillet;
    CommandAction* pActionShell;
    CommandAction* pActionDraft;
    CommandAction* pActionSplitFace;
    CommandAction* pActionDeleteFace;
};

struct PrimitiveActions
{
    CommandAction* pActionMakeBox;
    CommandAction* pActionMakeCylinder;
    CommandAction* pActionMakeSphere;
    CommandAction* pActionMakeCone;
    CommandAction* pActionMakeTorus;
    CommandAction* pActionMakeTube;
};

struct BooleanActions
{
    CommandAction* pActionUnion;
    CommandAction* pActionSubtract;
    CommandAction* pActionIntersect;
};

struct EditActions
{
    CommandAction* pActionMove;
    CommandAction* pActionRotate;
    CommandAction* pActionMirror;
    CommandAction* pActionLinearPattern;
    CommandAction* pActionCircularPattern;
};

struct UtilityActions
{
    CommandAction* pActionSetColor;
    CommandAction* pActionMeasure;
    CommandAction* pActionRunScript;
    CommandAction* pActionFindElementById;
};

struct ViewActions
{
    CommandAction* pActionFitView;
    CommandAction* pActionFitSelection;
    CommandAction* pActionIsometricView;
    CommandAction* pActionFrontView;
    CommandAction* pActionBackView;
    CommandAction* pActionLeftView;
    CommandAction* pActionRightView;
    CommandAction* pActionTopView;
    CommandAction* pActionBottomView;
    CommandAction* pActionShadedWithEdgesDisplay;
    CommandAction* pActionShadedDisplay;
    CommandAction* pActionWireframeDisplay;
};

#ifdef _DEBUG
struct TestActions
{
    CommandAction* pActionTopoName;
    CommandAction* pActionCheckTopoName;
};
#endif // _DEBUG

FileActions createFileActions(ModelingEnvironment* pEnv)
{
    assert(pEnv);

    FileActions actions = {};
    actions.pActionSaveFile = pEnv->newCommandAction(
        CommandNames::SaveFile,
        QCoreApplication::translate("MainWindow", "Save"),
        IconUtil::get(":/images/Document_Save.svg"));
    actions.pActionSaveFile->setShortcut(QKeySequence::Save);
    actions.pActionSaveFile->setShortcutContext(Qt::ApplicationShortcut);

    actions.pActionSaveAsFile = pEnv->newCommandAction(
        CommandNames::SaveAsFile,
        QCoreApplication::translate("MainWindow", "Save As"),
        IconUtil::get(":/images/Document_SaveAs.svg"));

    actions.pActionImportFile = pEnv->newCommandAction(
        CommandNames::ImportFile,
        QCoreApplication::translate("MainWindow", "Import"),
        IconUtil::get(":/images/Document_Import.svg"));

    actions.pActionExportFile = pEnv->newCommandAction(
        CommandNames::ExportFile,
        QCoreApplication::translate("MainWindow", "Export"),
        IconUtil::get(":/images/Document_Export.svg"));

    actions.pActionImportSketch = pEnv->newCommandAction(
        CommandNames::ImportSketch,
        QCoreApplication::translate("MainWindow", "Import Sketch"),
        IconUtil::get(":/images/Document_Import.svg"));

    actions.pActionExportSketch = pEnv->newCommandAction(
        CommandNames::ExportSketch,
        QCoreApplication::translate("MainWindow", "Export Sketch"),
        IconUtil::get(":/images/Document_Export.svg"));

    return actions;
}

UndoRedoActions createUndoRedoActions(ModelingEnvironment* pEnv)
{
    assert(pEnv);

    UndoRedoActions actions = {};
    actions.pActionUndo = pEnv->newCommandAction(
        CommandNames::Undo,
        QCoreApplication::translate("MainWindow", "Undo"),
        IconUtil::get(":/images/Basic_Undo.svg"));
    actions.pActionUndo->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Z));
    actions.pActionUndo->setShortcutContext(Qt::ApplicationShortcut);

    actions.pActionRedo = pEnv->newCommandAction(
        CommandNames::Redo,
        QCoreApplication::translate("MainWindow", "Redo"),
        IconUtil::get(":/images/Basic_Redo.svg"));
    actions.pActionRedo->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Y));
    actions.pActionRedo->setShortcutContext(Qt::ApplicationShortcut);

    return actions;
}

ModelingActions createModelingActions(ModelingEnvironment* pEnv, QActionGroup* pActionGroup)
{
    assert(pEnv);
    assert(pActionGroup);

    ModelingActions actions = {};
    actions.pActionSelect = pEnv->newCommandAction(
        CommandNames::Select,
        QCoreApplication::translate("MainWindow", "Select"),
        IconUtil::get(":/images/Basic_Select.svg"),
        pActionGroup);

    actions.pActionNewSketch = pEnv->newCommandAction(
        CommandNames::NewSketch,
        QCoreApplication::translate("MainWindow", "New Sketch"),
        IconUtil::get(":/images/Modeling_NewSketch.svg"),
        pActionGroup);

    actions.pActionNewSketch3D = pEnv->newCommandAction(
        CommandNames::NewSketch3D,
        QCoreApplication::translate("MainWindow", "New 3D Sketch"),
        IconUtil::get(":/images/Modeling_NewSketch3D.svg"),
        pActionGroup);

    actions.pActionEditSketch = pEnv->newCommandAction(
        CommandNames::EditSketch,
        QCoreApplication::translate("MainWindow", "Edit Sketch"),
        IconUtil::get(":/images/Edit_Sketch.svg"),
        pActionGroup);

    actions.pActionParallelDatumPlane = pEnv->newCommandAction(
        CommandNames::ParallelDatumPlane,
        QCoreApplication::translate("MainWindow", "Parallel Datum Plane"),
        IconUtil::get(":/images/DatumPlane_Parallel.svg"),
        pActionGroup);

    actions.pActionCoincidentDatumPlane = pEnv->newCommandAction(
        CommandNames::CoincidentDatumPlane,
        QCoreApplication::translate("MainWindow", "Coincident Datum Plane"),
        IconUtil::get(":/images/DatumPlane_Coincident.svg"),
        pActionGroup);

    actions.pActionAngularDatumPlane = pEnv->newCommandAction(
        CommandNames::AngularDatumPlane,
        QCoreApplication::translate("MainWindow", "Angular Datum Plane"),
        IconUtil::get(":/images/DatumPlane_Angular.svg"),
        pActionGroup);

    actions.pActionPerpendicularDatumPlane = pEnv->newCommandAction(
        CommandNames::PerpendicularDatumPlane,
        QCoreApplication::translate("MainWindow", "Perpendicular Datum Plane"),
        IconUtil::get(":/images/DatumPlane_Perpendicular.svg"),
        pActionGroup);

    actions.pActionThroughAxisDatumPlane = pEnv->newCommandAction(
        CommandNames::ThroughAxisDatumPlane,
        QCoreApplication::translate("MainWindow", "Through Axis Datum Plane"),
        IconUtil::get(":/images/DatumPlane_ThroughAxis.svg"),
        pActionGroup);

    actions.pActionNormalToCurveDatumPlane = pEnv->newCommandAction(
        CommandNames::NormalToCurveDatumPlane,
        QCoreApplication::translate("MainWindow", "Normal To Curve Datum Plane"),
        IconUtil::get(":/images/DatumPlane_NormalToEdge.svg"),
        pActionGroup);

    actions.pActionThrough3PointsDatumPlane = pEnv->newCommandAction(
        CommandNames::Through3PointsDatumPlane,
        QCoreApplication::translate("MainWindow", "Through 3 Points Datum Plane"),
        IconUtil::get(":/images/DatumPlane_Through3Points.svg"),
        pActionGroup);

    actions.pActionTangentDatumPlane = pEnv->newCommandAction(
        CommandNames::TangentDatumPlane,
        QCoreApplication::translate("MainWindow", "Tangent Datum Plane"),
        IconUtil::get(":/images/DatumPlane_Tangent.svg"),
        pActionGroup);

    actions.pActionHelix = pEnv->newCommandAction(
        CommandNames::Helix,
        QCoreApplication::translate("MainWindow", "Helix"),
        IconUtil::get(":/images/Curve_Helix.svg"),
        pActionGroup);

    actions.pActionExtrude = pEnv->newCommandAction(
        CommandNames::Extrude,
        QCoreApplication::translate("MainWindow", "Extrude"),
        IconUtil::get(":/images/Modeling_Extrusion.png"),
        pActionGroup);

    actions.pActionExtrudedSheet = pEnv->newCommandAction(
        CommandNames::ExtrudedSheet,
        QCoreApplication::translate("MainWindow", "Extruded Sheet"),
        IconUtil::get(":/images/Modeling_ExtrudedSheet.svg"),
        pActionGroup);

    actions.pActionRevolvedSheet = pEnv->newCommandAction(
        CommandNames::RevolvedSheet,
        QCoreApplication::translate("MainWindow", "Revolved Sheet"),
        IconUtil::get(":/images/Modeling_RevolvedSheet.svg"),
        pActionGroup);

    actions.pActionSweptSheet = pEnv->newCommandAction(
        CommandNames::SweptSheet,
        QCoreApplication::translate("MainWindow", "Swept Sheet"),
        IconUtil::get(":/images/Modeling_SweptSheet.svg"),
        pActionGroup);

    actions.pActionLoftedSheet = pEnv->newCommandAction(
        CommandNames::LoftedSheet,
        QCoreApplication::translate("MainWindow", "Lofted Sheet"),
        IconUtil::get(":/images/Modeling_LoftedSheet.svg"),
        pActionGroup);

    actions.pActionPlanarSheet = pEnv->newCommandAction(
        CommandNames::PlanarSheet,
        QCoreApplication::translate("MainWindow", "Planar Sheet"),
        IconUtil::get(":/images/Modeling_PlanarSheet.svg"),
        pActionGroup);

    actions.pActionFilledSheet = pEnv->newCommandAction(
        CommandNames::FilledSheet,
        QCoreApplication::translate("MainWindow", "Filled Surface"),
        IconUtil::get(":/images/Modeling_FilledSheet.svg"),
        pActionGroup);

    actions.pActionSewnSheet = pEnv->newCommandAction(
        CommandNames::SewnSheet,
        QCoreApplication::translate("MainWindow", "Sewn Sheet"),
        IconUtil::get(":/images/Modeling_SewnSheet.svg"),
        pActionGroup);

    actions.pActionThicken = pEnv->newCommandAction(
        CommandNames::Thicken,
        QCoreApplication::translate("MainWindow", "Thicken"),
        IconUtil::get(":/images/Modeling_Thicken.svg"),
        pActionGroup);

    actions.pActionSolidify = pEnv->newCommandAction(
        CommandNames::Solidify,
        QCoreApplication::translate("MainWindow", "Solidify"),
        IconUtil::get(":/images/Modeling_Solidify.svg"),
        pActionGroup);

    actions.pActionOffsetSheet = pEnv->newCommandAction(
        CommandNames::OffsetSheet,
        QCoreApplication::translate("MainWindow", "Offset Sheet"),
        IconUtil::get(":/images/Modeling_OffsetSheet.png"),
        pActionGroup);

    actions.pActionRevolve = pEnv->newCommandAction(
        CommandNames::Revolve,
        QCoreApplication::translate("MainWindow", "Revolve"),
        IconUtil::get(":/images/Modeling_Revolution.png"),
        pActionGroup);

    actions.pActionSweep = pEnv->newCommandAction(
        CommandNames::Sweep,
        QCoreApplication::translate("MainWindow", "Sweep"),
        IconUtil::get(":/images/Modeling_Sweep.png"),
        pActionGroup);

    actions.pActionLoft = pEnv->newCommandAction(
        CommandNames::Loft,
        QCoreApplication::translate("MainWindow", "Loft"),
        IconUtil::get(":/images/Modeling_Loft.png"),
        pActionGroup);

    actions.pActionExtrudeCut = pEnv->newCommandAction(
        CommandNames::ExtrudeCut,
        QCoreApplication::translate("MainWindow", "Extrude Cut"),
        IconUtil::get(":/images/Modeling_ExtrusionCut.png"),
        pActionGroup);

    actions.pActionRevolveCut = pEnv->newCommandAction(
        CommandNames::RevolveCut,
        QCoreApplication::translate("MainWindow", "Revolve Cut"),
        IconUtil::get(":/images/Modeling_RevolutionCut.png"),
        pActionGroup);

    actions.pActionSweepCut = pEnv->newCommandAction(
        CommandNames::SweepCut,
        QCoreApplication::translate("MainWindow", "Sweep Cut"),
        IconUtil::get(":/images/Modeling_SweepCut.png"),
        pActionGroup);

    actions.pActionLoftCut = pEnv->newCommandAction(
        CommandNames::LoftCut,
        QCoreApplication::translate("MainWindow", "Loft Cut"),
        IconUtil::get(":/images/Modeling_LoftCut.png"),
        pActionGroup);

    actions.pActionMerge = pEnv->newCommandAction(
        CommandNames::Merge,
        QCoreApplication::translate("MainWindow", "Merge"),
        IconUtil::get(":/images/Modeling_Merge.svg"),
        pActionGroup);

    actions.pActionChamfer = pEnv->newCommandAction(
        CommandNames::Chamfer,
        QCoreApplication::translate("MainWindow", "Chamfer"),
        IconUtil::get(":/images/Modeling_Chamfer.png"),
        pActionGroup);

    actions.pActionFillet = pEnv->newCommandAction(
        CommandNames::Fillet,
        QCoreApplication::translate("MainWindow", "Fillet"),
        IconUtil::get(":/images/Modeling_Fillet.png"),
        pActionGroup);

    actions.pActionShell = pEnv->newCommandAction(
        CommandNames::Shell,
        QCoreApplication::translate("MainWindow", "Shell"),
        IconUtil::get(":/images/Modeling_Shell.png"),
        pActionGroup);

    actions.pActionDraft = pEnv->newCommandAction(
        CommandNames::Draft,
        QCoreApplication::translate("MainWindow", "Draft"),
        IconUtil::get(":/images/Modeling_Draft.png"),
        pActionGroup);

    actions.pActionSplitFace = pEnv->newCommandAction(
        CommandNames::SplitFace,
        QCoreApplication::translate("MainWindow", "Split Face"),
        IconUtil::get(":/images/Modeling_SplitFace.svg"),
        pActionGroup);

    actions.pActionDeleteFace = pEnv->newCommandAction(
        CommandNames::DeleteFace,
        QCoreApplication::translate("MainWindow", "Delete Face"),
        IconUtil::get(":/images/Modeling_DeleteFace.svg"),
        pActionGroup);

    return actions;
}

PrimitiveActions createPrimitiveActions(ModelingEnvironment* pEnv, QActionGroup* pActionGroup)
{
    assert(pEnv);
    assert(pActionGroup);

    PrimitiveActions actions = {};
    actions.pActionMakeBox = pEnv->newCommandAction(
        CommandNames::MakeBox,
        QCoreApplication::translate("MainWindow", "Box"),
        IconUtil::get(":/images/Primitive_Box.png"),
        pActionGroup);
    actions.pActionMakeBox->setShortcut(QKeySequence::fromString("B,O,X"));

    actions.pActionMakeCylinder = pEnv->newCommandAction(
        CommandNames::MakeCylinder,
        QCoreApplication::translate("MainWindow", "Cylinder"),
        IconUtil::get(":/images/Primitive_Cylinder.png"),
        pActionGroup);
    actions.pActionMakeCylinder->setShortcut(QKeySequence::fromString("C,Y,L"));

    actions.pActionMakeSphere = pEnv->newCommandAction(
        CommandNames::MakeSphere,
        QCoreApplication::translate("MainWindow", "Sphere"),
        IconUtil::get(":/images/Primitive_Sphere.png"),
        pActionGroup);
    actions.pActionMakeSphere->setShortcut(QKeySequence::fromString("S,P,H"));

    actions.pActionMakeCone = pEnv->newCommandAction(
        CommandNames::MakeCone,
        QCoreApplication::translate("MainWindow", "Cone"),
        IconUtil::get(":/images/Primitive_Cone.png"),
        pActionGroup);
    actions.pActionMakeCone->setShortcut(QKeySequence::fromString("C,O,N"));

    actions.pActionMakeTorus = pEnv->newCommandAction(
        CommandNames::MakeTorus,
        QCoreApplication::translate("MainWindow", "Torus"),
        IconUtil::get(":/images/Primitive_Torus.png"),
        pActionGroup);
    actions.pActionMakeTorus->setShortcut(QKeySequence::fromString("T,O,R"));

    actions.pActionMakeTube = pEnv->newCommandAction(
        CommandNames::MakeTube,
        QCoreApplication::translate("MainWindow", "Tube"),
        IconUtil::get(":/images/Primitive_Tube.png"),
        pActionGroup);
    actions.pActionMakeTube->setShortcut(QKeySequence::fromString("T,U,B"));

    return actions;
}

BooleanActions createBooleanActions(ModelingEnvironment* pEnv, QActionGroup* pActionGroup)
{
    assert(pEnv);
    assert(pActionGroup);

    BooleanActions actions = {};
    actions.pActionUnion = pEnv->newCommandAction(
        CommandNames::Union,
        QCoreApplication::translate("MainWindow", "Union"),
        IconUtil::get(":/images/Modeling_Fuse.svg"),
        pActionGroup);

    actions.pActionSubtract = pEnv->newCommandAction(
        CommandNames::Subtract,
        QCoreApplication::translate("MainWindow", "Subtract"),
        IconUtil::get(":/images/Modeling_Cut.svg"),
        pActionGroup);

    actions.pActionIntersect = pEnv->newCommandAction(
        CommandNames::Intersect,
        QCoreApplication::translate("MainWindow", "Intersect"),
        IconUtil::get(":/images/Modeling_Common.svg"),
        pActionGroup);

    return actions;
}

EditActions createEditActions(ModelingEnvironment* pEnv, QActionGroup* pActionGroup)
{
    assert(pEnv);
    assert(pActionGroup);

    EditActions actions = {};
    actions.pActionMove = pEnv->newCommandAction(
        CommandNames::Move,
        QCoreApplication::translate("MainWindow", "Move"),
        IconUtil::get(":/images/Edit_Move.svg"),
        pActionGroup);
    actions.pActionMove->setShortcut(QKeySequence::fromString("M,O"));

    actions.pActionRotate = pEnv->newCommandAction(
        CommandNames::Rotate,
        QCoreApplication::translate("MainWindow", "Rotate"),
        IconUtil::get(":/images/Edit_Rotate.svg"),
        pActionGroup);
    actions.pActionRotate->setShortcut(QKeySequence::fromString("R,O"));

    actions.pActionMirror = pEnv->newCommandAction(
        CommandNames::Mirror,
        QCoreApplication::translate("MainWindow", "Mirror"),
        IconUtil::get(":/images/Sketch_Mirror.svg"),
        pActionGroup);
    actions.pActionMirror->setShortcut(QKeySequence::fromString("M,I"));

    actions.pActionLinearPattern = pEnv->newCommandAction(
        CommandNames::LinearPattern,
        QCoreApplication::translate("MainWindow", "Linear Pattern"),
        IconUtil::get(":/images/Sketch_RectArray.svg"),
        pActionGroup);
    actions.pActionLinearPattern->setShortcut(QKeySequence::fromString("L,P"));

    actions.pActionCircularPattern = pEnv->newCommandAction(
        CommandNames::CircularPattern,
        QCoreApplication::translate("MainWindow", "Circular Pattern"),
        IconUtil::get(":/images/Sketch_PolarArray.svg"),
        pActionGroup);
    actions.pActionCircularPattern->setShortcut(QKeySequence::fromString("C,P"));

    return actions;
}

UtilityActions createUtilityActions(ModelingEnvironment* pEnv, QActionGroup* pActionGroup)
{
    assert(pEnv);
    assert(pActionGroup);

    UtilityActions actions = {};
    actions.pActionSetColor = pEnv->newCommandAction(
        CommandNames::SetColor,
        QCoreApplication::translate("MainWindow", "Set Color"),
        IconUtil::get(":/images/Utility_SetColor.svg"),
        pActionGroup);

    actions.pActionMeasure = pEnv->newCommandAction(
        CommandNames::Measure,
        QCoreApplication::translate("MainWindow", "Measure"),
        IconUtil::get(":/images/Utility_MeasureDistance.svg"),
        pActionGroup);

    actions.pActionRunScript = pEnv->newCommandAction(
        CommandNames::RunScript,
        QCoreApplication::translate("MainWindow", "Run Script"),
        IconUtil::get(":/images/Utility_RunScript.svg"),
        pActionGroup);

    actions.pActionFindElementById = pEnv->newCommandAction(
        CommandNames::FindElementById,
        QCoreApplication::translate("MainWindow", "Find"),
        IconUtil::get(":/images/Utility_FindElementById.svg"),
        pActionGroup);
    actions.pActionFindElementById->setShortcut(QKeySequence::Find);
    actions.pActionFindElementById->setShortcutContext(Qt::ApplicationShortcut);

    return actions;
}

ViewActions createViewActions(ModelingEnvironment* pEnv)
{
    assert(pEnv);

    ViewActions actions = {};
    actions.pActionFitView = pEnv->newCommandAction(
        CommandNames::FitView,
        QCoreApplication::translate("MainWindow", "Fit View"),
        IconUtil::get(":/images/View_FullScreen.svg"));

    actions.pActionFitSelection = pEnv->newCommandAction(
        CommandNames::FitSelection,
        QCoreApplication::translate("MainWindow", "Fit Selection"),
        IconUtil::get(":/images/View_FitSelection.svg"));

    actions.pActionIsometricView = pEnv->newCommandAction(
        CommandNames::IsometricView,
        QCoreApplication::translate("MainWindow", "Isometric View"),
        IconUtil::get(":/images/View_ISO.svg"));

    actions.pActionFrontView = pEnv->newCommandAction(
        CommandNames::FrontView,
        QCoreApplication::translate("MainWindow", "Front View"),
        IconUtil::get(":/images/View_Front.svg"));

    actions.pActionBackView = pEnv->newCommandAction(
        CommandNames::BackView,
        QCoreApplication::translate("MainWindow", "Back View"),
        IconUtil::get(":/images/View_Back.svg"));

    actions.pActionLeftView = pEnv->newCommandAction(
        CommandNames::LeftView,
        QCoreApplication::translate("MainWindow", "Left View"),
        IconUtil::get(":/images/View_Left.svg"));

    actions.pActionRightView = pEnv->newCommandAction(
        CommandNames::RightView,
        QCoreApplication::translate("MainWindow", "Right View"),
        IconUtil::get(":/images/View_Right.svg"));

    actions.pActionTopView = pEnv->newCommandAction(
        CommandNames::TopView,
        QCoreApplication::translate("MainWindow", "Top View"),
        IconUtil::get(":/images/View_Top.svg"));

    actions.pActionBottomView = pEnv->newCommandAction(
        CommandNames::BottomView,
        QCoreApplication::translate("MainWindow", "Bottom View"),
        IconUtil::get(":/images/View_Bottom.svg"));

    actions.pActionShadedWithEdgesDisplay = pEnv->newCommandAction(
        CommandNames::ShadedWithEdgesDisplay,
        QCoreApplication::translate("MainWindow", "Shaded with Edges"),
        IconUtil::get(":/images/View_ShadedWithEdges.svg"));

    actions.pActionShadedDisplay = pEnv->newCommandAction(
        CommandNames::ShadedDisplay,
        QCoreApplication::translate("MainWindow", "Shaded"),
        IconUtil::get(":/images/View_Shaded.svg"));

    actions.pActionWireframeDisplay = pEnv->newCommandAction(
        CommandNames::WireframeDisplay,
        QCoreApplication::translate("MainWindow", "Wireframe"),
        IconUtil::get(":/images/View_Wireframe.svg"));

    return actions;
}

#ifdef _DEBUG
TestActions createTestActions(ModelingEnvironment* pEnv, QActionGroup* pActionGroup)
{
    assert(pEnv);
    assert(pActionGroup);

    TestActions actions = {};
    actions.pActionTopoName = pEnv->newCommandAction(
        CommandNames::TopoName,
        QCoreApplication::translate("MainWindow", "TopoName"),
        IconUtil::get(":/images/Test_TopoName.svg"),
        pActionGroup);

    actions.pActionCheckTopoName = pEnv->newCommandAction(
        CommandNames::CheckTopoName,
        QCoreApplication::translate("MainWindow", "CheckTopoName"),
        IconUtil::get(":/images/Test_CheckTopoName.svg"),
        pActionGroup);

    return actions;
}
#endif // _DEBUG

void buildEditMenuUi(
    const UndoRedoActions& undoRedoActions,
    QMenu* pMenuEdit)
{
    assert(pMenuEdit);

    pMenuEdit->addAction(undoRedoActions.pActionUndo);
    pMenuEdit->addAction(undoRedoActions.pActionRedo);
}

void buildFileMenuUi(
    const FileActions& actions,
    QMenu* pMenuFile)
{
    assert(pMenuFile);

    pMenuFile->addAction(actions.pActionSaveFile);
    pMenuFile->addAction(actions.pActionSaveAsFile);
    pMenuFile->addSeparator();
    pMenuFile->addAction(actions.pActionImportFile);
    pMenuFile->addAction(actions.pActionExportFile);
    pMenuFile->addSeparator();
    pMenuFile->addAction(actions.pActionImportSketch);
    pMenuFile->addAction(actions.pActionExportSketch);
}

void buildSketchMenuUi(
    const ModelingActions& actions,
    QMenu* pMenuSketch)
{
    assert(pMenuSketch);

    pMenuSketch->addAction(actions.pActionNewSketch);
    pMenuSketch->addAction(actions.pActionNewSketch3D);

    pMenuSketch->addSeparator();

    pMenuSketch->addAction(actions.pActionHelix);

    pMenuSketch->addSeparator();

    QMenu* pMenuDatumPlane = pMenuSketch->addMenu(
        QCoreApplication::translate("MainWindow", "Datum Plane"));
    pMenuDatumPlane->addAction(actions.pActionParallelDatumPlane);
    pMenuDatumPlane->addAction(actions.pActionCoincidentDatumPlane);
    pMenuDatumPlane->addAction(actions.pActionAngularDatumPlane);
    pMenuDatumPlane->addAction(actions.pActionPerpendicularDatumPlane);
    pMenuDatumPlane->addAction(actions.pActionThroughAxisDatumPlane);
    pMenuDatumPlane->addAction(actions.pActionNormalToCurveDatumPlane);
    pMenuDatumPlane->addAction(actions.pActionThrough3PointsDatumPlane);
    pMenuDatumPlane->addAction(actions.pActionTangentDatumPlane);
}

void buildSheetMenuUi(
    const ModelingActions& actions,
    QMenu* pMenuSheet)
{
    assert(pMenuSheet);

    pMenuSheet->addAction(actions.pActionExtrudedSheet);
    pMenuSheet->addAction(actions.pActionRevolvedSheet);
    pMenuSheet->addAction(actions.pActionSweptSheet);
    pMenuSheet->addAction(actions.pActionLoftedSheet);
    pMenuSheet->addSeparator();
    pMenuSheet->addAction(actions.pActionPlanarSheet);
    pMenuSheet->addAction(actions.pActionFilledSheet);
    pMenuSheet->addAction(actions.pActionOffsetSheet);
    pMenuSheet->addSeparator();
    pMenuSheet->addAction(actions.pActionSplitFace);
    pMenuSheet->addAction(actions.pActionDeleteFace);
    pMenuSheet->addAction(actions.pActionChamfer);
    pMenuSheet->addAction(actions.pActionFillet);
    pMenuSheet->addSeparator();
    pMenuSheet->addAction(actions.pActionSewnSheet);
    pMenuSheet->addAction(actions.pActionSolidify);
    pMenuSheet->addAction(actions.pActionThicken);
}

void buildSheetToolBarUi(
    ModelingEnvironment* pEnv,
    QActionGroup* pActionGroup,
    const ModelingActions& actions,
    QToolBar* pToolBarSheet)
{
    assert(pEnv);
    assert(pActionGroup);
    assert(pToolBarSheet);

    std::list<QAction*> sheetGenerationActions;
    sheetGenerationActions.emplace_back(actions.pActionExtrudedSheet);
    sheetGenerationActions.emplace_back(actions.pActionRevolvedSheet);
    sheetGenerationActions.emplace_back(actions.pActionSweptSheet);
    sheetGenerationActions.emplace_back(actions.pActionLoftedSheet);
    QToolButton* pToolBtnSheetGeneration = pEnv->newMenuPopupToolButton(
        pToolBarSheet,
        QCoreApplication::translate("MainWindow", "Sheet Generation Series"),
        pActionGroup,
        sheetGenerationActions);
    pToolBarSheet->addWidget(pToolBtnSheetGeneration);

    std::list<QAction*> sheetBuildActions;
    sheetBuildActions.emplace_back(actions.pActionPlanarSheet);
    sheetBuildActions.emplace_back(actions.pActionFilledSheet);
    sheetBuildActions.emplace_back(actions.pActionOffsetSheet);
    QToolButton* pToolBtnSheetBuild = pEnv->newMenuPopupToolButton(
        pToolBarSheet,
        QCoreApplication::translate("MainWindow", "Sheet Build Series"),
        pActionGroup,
        sheetBuildActions);
    pToolBarSheet->addWidget(pToolBtnSheetBuild);

    std::list<QAction*> faceModifyActions;
    faceModifyActions.emplace_back(actions.pActionSplitFace);
    faceModifyActions.emplace_back(actions.pActionDeleteFace);
    faceModifyActions.emplace_back(actions.pActionChamfer);
    faceModifyActions.emplace_back(actions.pActionFillet);
    QToolButton* pToolBtnFaceModify = pEnv->newMenuPopupToolButton(
        pToolBarSheet,
        QCoreApplication::translate("MainWindow", "Face Modify Series"),
        pActionGroup,
        faceModifyActions);
    pToolBarSheet->addWidget(pToolBtnFaceModify);

    std::list<QAction*> solidifyActions;
    solidifyActions.emplace_back(actions.pActionSewnSheet);
    solidifyActions.emplace_back(actions.pActionSolidify);
    solidifyActions.emplace_back(actions.pActionThicken);
    QToolButton* pToolBtnSolidify = pEnv->newMenuPopupToolButton(
        pToolBarSheet,
        QCoreApplication::translate("MainWindow", "Solidify Series"),
        pActionGroup,
        solidifyActions);
    pToolBarSheet->addWidget(pToolBtnSolidify);
}

void buildSolidMenuUi(
    const ModelingActions& actions,
    const BooleanActions& booleanActions,
    const PrimitiveActions& primitiveActions,
    const EditActions& editActions,
    QMenu* pMenuSolid)
{
    assert(pMenuSolid);

    QMenu* pMenuBoss = pMenuSolid->addMenu(
        QCoreApplication::translate("MainWindow", "Boss"));
    pMenuBoss->addAction(actions.pActionExtrude);
    pMenuBoss->addAction(actions.pActionRevolve);
    pMenuBoss->addAction(actions.pActionSweep);
    pMenuBoss->addAction(actions.pActionLoft);

    QMenu* pMenuCut = pMenuSolid->addMenu(
        QCoreApplication::translate("MainWindow", "Cut"));
    pMenuCut->addAction(actions.pActionExtrudeCut);
    pMenuCut->addAction(actions.pActionRevolveCut);
    pMenuCut->addAction(actions.pActionSweepCut);
    pMenuCut->addAction(actions.pActionLoftCut);

    pMenuSolid->addAction(actions.pActionMerge);

    QMenu* pMenuModification = pMenuSolid->addMenu(
        QCoreApplication::translate("MainWindow", "Modification"));
    pMenuModification->addAction(actions.pActionChamfer);
    pMenuModification->addAction(actions.pActionFillet);
    pMenuModification->addAction(actions.pActionShell);
    pMenuModification->addAction(actions.pActionDraft);
    pMenuModification->addAction(actions.pActionSplitFace);

    QMenu* pMenuBoolean = pMenuSolid->addMenu(
        QCoreApplication::translate("MainWindow", "Boolean"));
    pMenuBoolean->addAction(booleanActions.pActionUnion);
    pMenuBoolean->addAction(booleanActions.pActionSubtract);
    pMenuBoolean->addAction(booleanActions.pActionIntersect);

    QMenu* pMenuPrimitive = pMenuSolid->addMenu(
        QCoreApplication::translate("MainWindow", "Primitive"));
    pMenuPrimitive->addAction(primitiveActions.pActionMakeBox);
    pMenuPrimitive->addAction(primitiveActions.pActionMakeCylinder);
    pMenuPrimitive->addAction(primitiveActions.pActionMakeSphere);
    pMenuPrimitive->addAction(primitiveActions.pActionMakeCone);
    pMenuPrimitive->addAction(primitiveActions.pActionMakeTorus);
    pMenuPrimitive->addAction(primitiveActions.pActionMakeTube);

    QMenu* pMenuTransform = pMenuSolid->addMenu(
        QCoreApplication::translate("MainWindow", "Transform"));
    pMenuTransform->addAction(editActions.pActionMove);
    pMenuTransform->addAction(editActions.pActionRotate);
    pMenuTransform->addAction(editActions.pActionMirror);
    pMenuTransform->addAction(editActions.pActionLinearPattern);
    pMenuTransform->addAction(editActions.pActionCircularPattern);
}

void buildViewMenuUi(const ViewActions& actions, QMenu* pMenuView)
{
    assert(pMenuView);

    pMenuView->addAction(actions.pActionFitView);
    pMenuView->addAction(actions.pActionFitSelection);
    pMenuView->addSeparator();

    QMenu* pMenuStandardView = pMenuView->addMenu(
        QCoreApplication::translate("MainWindow", "Standard View"));
    pMenuStandardView->addAction(actions.pActionIsometricView);
    pMenuStandardView->addAction(actions.pActionFrontView);
    pMenuStandardView->addAction(actions.pActionBackView);
    pMenuStandardView->addAction(actions.pActionLeftView);
    pMenuStandardView->addAction(actions.pActionRightView);
    pMenuStandardView->addAction(actions.pActionTopView);
    pMenuStandardView->addAction(actions.pActionBottomView);

    QMenu* pMenuDisplayMode = pMenuView->addMenu(
        QCoreApplication::translate("MainWindow", "Display Mode"));
    pMenuDisplayMode->addAction(actions.pActionShadedWithEdgesDisplay);
    pMenuDisplayMode->addAction(actions.pActionShadedDisplay);
    pMenuDisplayMode->addAction(actions.pActionWireframeDisplay);
}

void buildToolsMenuUi(
    CommandAction* pActionSelect,
    const UtilityActions& actions,
    QMenu* pMenuTools)
{
    assert(pActionSelect);
    assert(pMenuTools);

    const QList<QAction*> existingActions = pMenuTools->actions();
    QAction* pAnchor = existingActions.isEmpty() ? nullptr : existingActions.first();

    // Inserted before the anchor, so the first insertion ends up on top.
    if (pAnchor)
    {
        pMenuTools->insertAction(pAnchor, pActionSelect);

        QAction* pSeparatorSelect = new QAction(pActionSelect);
        pSeparatorSelect->setSeparator(true);
        pMenuTools->insertAction(pAnchor, pSeparatorSelect);
    }
    else
    {
        pMenuTools->addAction(pActionSelect);
    }

    std::list<QAction*> utilityActions;
    utilityActions.emplace_back(actions.pActionSetColor);
    utilityActions.emplace_back(actions.pActionMeasure);
    utilityActions.emplace_back(actions.pActionRunScript);
    utilityActions.emplace_back(actions.pActionFindElementById);
    for (QAction* pAction : utilityActions)
    {
        if (pAnchor)
            pMenuTools->insertAction(pAnchor, pAction);
        else
            pMenuTools->addAction(pAction);
    }

    if (pAnchor)
    {
        QAction* pSeparator = new QAction(actions.pActionSetColor);
        pSeparator->setSeparator(true);
        pMenuTools->insertAction(pAnchor, pSeparator);
    }
}

void buildFileToolBarUi(
    const FileActions& fileActions,
    QToolBar* pToolBarFile)
{
    assert(pToolBarFile);

    pToolBarFile->addAction(fileActions.pActionSaveFile);
}

void buildEditToolBarUi(
    const UndoRedoActions& undoRedoActions,
    QToolBar* pToolBarEdit)
{
    assert(pToolBarEdit);

    pToolBarEdit->addAction(undoRedoActions.pActionUndo);
    pToolBarEdit->addAction(undoRedoActions.pActionRedo);
}

void buildModelingToolBarUi(
    ModelingEnvironment* pEnv,
    QActionGroup* pActionGroup,
    const ModelingActions& actions,
    const BooleanActions& booleanActions,
    const PrimitiveActions& primitiveActions,
    const EditActions& editActions,
    QToolBar* pToolBarModeling)
{
    assert(pEnv);
    assert(pActionGroup);
    assert(pToolBarModeling);

    pToolBarModeling->addAction(actions.pActionSelect);

    std::list<QAction*> sketchActions;
    sketchActions.emplace_back(actions.pActionNewSketch);
    sketchActions.emplace_back(actions.pActionNewSketch3D);
    QToolButton* pToolBtnSketch = pEnv->newMenuPopupToolButton(
        pToolBarModeling,
        QCoreApplication::translate("MainWindow", "Sketch Series"),
        pActionGroup,
        sketchActions);
    pToolBarModeling->addWidget(pToolBtnSketch);

    std::list<QAction*> datumPlaneActions;
    datumPlaneActions.emplace_back(actions.pActionParallelDatumPlane);
    datumPlaneActions.emplace_back(actions.pActionCoincidentDatumPlane);
    datumPlaneActions.emplace_back(actions.pActionAngularDatumPlane);
    datumPlaneActions.emplace_back(actions.pActionPerpendicularDatumPlane);
    datumPlaneActions.emplace_back(actions.pActionThroughAxisDatumPlane);
    datumPlaneActions.emplace_back(actions.pActionNormalToCurveDatumPlane);
    datumPlaneActions.emplace_back(actions.pActionThrough3PointsDatumPlane);
    datumPlaneActions.emplace_back(actions.pActionTangentDatumPlane);

    QToolButton* pToolBtn = pEnv->newMenuPopupToolButton(
        pToolBarModeling,
        QCoreApplication::translate("MainWindow", "Datum Plane Series"),
        pActionGroup,
        datumPlaneActions);
    pToolBarModeling->addWidget(pToolBtn);

    pToolBarModeling->addAction(actions.pActionHelix);
    pToolBarModeling->addAction(actions.pActionExtrude);
    pToolBarModeling->addAction(actions.pActionRevolve);

    pToolBarModeling->addAction(actions.pActionSweep);
    pToolBarModeling->addAction(actions.pActionLoft);

    std::list<QAction*> cutActions;
    cutActions.emplace_back(actions.pActionExtrudeCut);
    cutActions.emplace_back(actions.pActionRevolveCut);
    cutActions.emplace_back(actions.pActionSweepCut);
    cutActions.emplace_back(actions.pActionLoftCut);
    QToolButton* pToolBtnCut = pEnv->newMenuPopupToolButton(
        pToolBarModeling,
        QCoreApplication::translate("MainWindow", "Cut Series"),
        pActionGroup,
        cutActions);
    pToolBarModeling->addWidget(pToolBtnCut);

    pToolBarModeling->addAction(actions.pActionMerge);

    std::list<QAction*> modificationActions;
    modificationActions.emplace_back(actions.pActionChamfer);
    modificationActions.emplace_back(actions.pActionFillet);
    modificationActions.emplace_back(actions.pActionShell);
    modificationActions.emplace_back(actions.pActionDraft);
    modificationActions.emplace_back(actions.pActionSplitFace);
    QToolButton* pToolBtnModification = pEnv->newMenuPopupToolButton(
        pToolBarModeling,
        QCoreApplication::translate("MainWindow", "Modification Series"),
        pActionGroup,
        modificationActions);
    pToolBarModeling->addWidget(pToolBtnModification);

    std::list<QAction*> booleanActionsList;
    booleanActionsList.emplace_back(booleanActions.pActionUnion);
    booleanActionsList.emplace_back(booleanActions.pActionSubtract);
    booleanActionsList.emplace_back(booleanActions.pActionIntersect);
    QToolButton* pToolBtnBoolean = pEnv->newMenuPopupToolButton(
        pToolBarModeling,
        QCoreApplication::translate("MainWindow", "Boolean Series"),
        pActionGroup,
        booleanActionsList);
    pToolBarModeling->addWidget(pToolBtnBoolean);

    std::list<QAction*> primitiveActionsList;
    primitiveActionsList.emplace_back(primitiveActions.pActionMakeBox);
    primitiveActionsList.emplace_back(primitiveActions.pActionMakeCylinder);
    primitiveActionsList.emplace_back(primitiveActions.pActionMakeSphere);
    primitiveActionsList.emplace_back(primitiveActions.pActionMakeCone);
    primitiveActionsList.emplace_back(primitiveActions.pActionMakeTorus);
    primitiveActionsList.emplace_back(primitiveActions.pActionMakeTube);
    QToolButton* pToolBtnPrimitive = pEnv->newMenuPopupToolButton(
        pToolBarModeling,
        QCoreApplication::translate("MainWindow", "Primitive Series"),
        pActionGroup,
        primitiveActionsList);
    pToolBarModeling->addWidget(pToolBtnPrimitive);

    std::list<QAction*> editActionsList;
    editActionsList.emplace_back(editActions.pActionMove);
    editActionsList.emplace_back(editActions.pActionRotate);
    editActionsList.emplace_back(editActions.pActionMirror);
    editActionsList.emplace_back(editActions.pActionLinearPattern);
    editActionsList.emplace_back(editActions.pActionCircularPattern);
    QToolButton* pToolBtnEdit = pEnv->newMenuPopupToolButton(
        pToolBarModeling,
        QCoreApplication::translate("MainWindow", "Transform Series"),
        pActionGroup,
        editActionsList);
    pToolBarModeling->addWidget(pToolBtnEdit);
}

void buildUtilityToolBarUi(const UtilityActions& actions, QToolBar* pToolBarUtility)
{
    assert(pToolBarUtility);

    pToolBarUtility->addAction(actions.pActionSetColor);
    pToolBarUtility->addAction(actions.pActionMeasure);
    pToolBarUtility->addAction(actions.pActionRunScript);
    pToolBarUtility->addAction(actions.pActionFindElementById);
}

void buildViewToolBarUi(
    ModelingEnvironment* pEnv,
    const ViewActions& actions,
    QToolBar* pToolBarView)
{
    assert(pEnv);
    assert(pToolBarView);

    pToolBarView->addAction(actions.pActionFitView);
    pToolBarView->addAction(actions.pActionFitSelection);

    std::list<QAction*> standardViewActions;
    standardViewActions.emplace_back(actions.pActionIsometricView);
    standardViewActions.emplace_back(actions.pActionFrontView);
    standardViewActions.emplace_back(actions.pActionBackView);
    standardViewActions.emplace_back(actions.pActionLeftView);
    standardViewActions.emplace_back(actions.pActionRightView);
    standardViewActions.emplace_back(actions.pActionTopView);
    standardViewActions.emplace_back(actions.pActionBottomView);
    QToolButton* pToolBtnStandardView = pEnv->newMenuPopupToolButton(
        pToolBarView,
        QCoreApplication::translate("MainWindow", "Standard View Series"),
        nullptr,
        standardViewActions);
    pToolBarView->addWidget(pToolBtnStandardView);

    pToolBarView->addSeparator();

    std::list<QAction*> displayModeActions;
    displayModeActions.emplace_back(actions.pActionShadedWithEdgesDisplay);
    displayModeActions.emplace_back(actions.pActionShadedDisplay);
    displayModeActions.emplace_back(actions.pActionWireframeDisplay);

    QActionGroup* pDisplayModeGroup = pEnv->newActionGroup();
    QToolButton* pToolBtn = pEnv->newMenuPopupToolButton(
        pToolBarView,
        QCoreApplication::translate("MainWindow", "Display Mode"),
        pDisplayModeGroup,
        displayModeActions);
    pToolBarView->addWidget(pToolBtn);
}

#ifdef _DEBUG
void buildTestToolBarUi(const TestActions& actions, QToolBar* pToolBarTest)
{
    assert(pToolBarTest);

    pToolBarTest->addAction(actions.pActionTopoName);
    pToolBarTest->addAction(actions.pActionCheckTopoName);
}
#endif // _DEBUG

UiTargets createUiTargets(ModelingEnvironment* pEnv)
{
    assert(pEnv);

    UiTargets targets = {};
    // Created before the environment's own tool bars so it lands right after the file
    // tool bar, which the gateway environment owns.
    targets.pToolBarEdit = pEnv->addToolBar(
        QCoreApplication::translate("MainWindow", "Edit"),
        wy3dApp::ToolBarNames::Edit);
    assert(targets.pToolBarEdit);

    targets.pToolBarModeling = pEnv->addToolBar(
        QCoreApplication::translate("MainWindow", "Modeling"),
        wy3dApp::ToolBarNames::Modeling);

    targets.pToolBarSheet = pEnv->addToolBar(
        QCoreApplication::translate("MainWindow", "Sheet"),
        wy3dApp::ToolBarNames::Sheet);

    targets.pToolBarUtility = pEnv->addToolBar(
        QCoreApplication::translate("MainWindow", "Utility"),
        wy3dApp::ToolBarNames::Utility);

    targets.pToolBarView = pEnv->addToolBar(
        QCoreApplication::translate("MainWindow", "View"),
        wy3dApp::ToolBarNames::ModelingView);

#ifdef _DEBUG
    targets.pToolBarTest = pEnv->addToolBar(
        QCoreApplication::translate("MainWindow", "Test"),
        wy3dApp::ToolBarNames::Test);
#endif // _DEBUG

    MainWindow* pMainWindow = Application::instance().getMainWindow();
    assert(pMainWindow);
    if (!pMainWindow)
    {
        return targets;
    }

    targets.pMenuFile = pMainWindow->findChild<QMenu*>(wy3dApp::MenuBarNames::File);
    assert(targets.pMenuFile);

    targets.pMenuSheet = pEnv->addMenu(QCoreApplication::translate("MainWindow", "Sheet"),
                                       wy3dApp::MenuBarNames::Sheet);
    assert(targets.pMenuSheet);
    // 以工具菜单为锚,曲面插到工具之前;工具菜单缺失时回退到帮助菜单
    QMenu* pAnchorMenu = pMainWindow->findChild<QMenu*>(wy3dApp::MenuBarNames::Tools);
    if (!pAnchorMenu)
        pAnchorMenu = pMainWindow->findChild<QMenu*>(wy3dApp::MenuBarNames::Help);
    if (pAnchorMenu)
        pMainWindow->menuBar()->insertMenu(pAnchorMenu->menuAction(), targets.pMenuSheet);

    targets.pMenuTools = pMainWindow->findChild<QMenu*>(wy3dApp::MenuBarNames::Tools);
    assert(targets.pMenuTools);

    targets.pMenuSolid = pEnv->addMenu(QCoreApplication::translate("MainWindow", "Solid"),
                                       wy3dApp::MenuBarNames::Solid);
    assert(targets.pMenuSolid);
    // Anchor on the sheet menu so the solid menu lands immediately to its left.
    if (targets.pMenuSolid && targets.pMenuSheet)
        pMainWindow->menuBar()->insertMenu(targets.pMenuSheet->menuAction(), targets.pMenuSolid);

    targets.pMenuView = pEnv->addMenu(QCoreApplication::translate("MainWindow", "View"),
                                      wy3dApp::MenuBarNames::View);
    assert(targets.pMenuView);
    pEnv->insertMenuAfter(targets.pMenuFile, targets.pMenuView);

    targets.pMenuSketch = pEnv->addMenu(QCoreApplication::translate("MainWindow", "Sketch"),
                                        wy3dApp::MenuBarNames::Sketch);
    assert(targets.pMenuSketch);
    // Anchor on the view menu so the sketch menu lands immediately to its right,
    // i.e. before the solid menu.
    pEnv->insertMenuAfter(targets.pMenuView, targets.pMenuSketch);

    targets.pMenuEdit = pEnv->addMenu(QCoreApplication::translate("MainWindow", "Edit"),
                                      wy3dApp::MenuBarNames::Edit);
    assert(targets.pMenuEdit);
    // Anchor on the file menu so the edit menu lands immediately to its right.
    pEnv->insertMenuAfter(targets.pMenuFile, targets.pMenuEdit);

    targets.pToolBarFile = pMainWindow->findChild<QToolBar*>(wy3dApp::ToolBarNames::File);
    assert(targets.pToolBarFile);

    return targets;
}
} // namespace

ModelingEnvironmentUI::ModelingEnvironmentUI() {}

ModelingEnvironmentUI::~ModelingEnvironmentUI()
{
}

void ModelingEnvironmentUI::initialize(ModelingEnvironment* pEnv)
{
    if (!pEnv)
    {
        assert(false);
        return;
    }

    const UiTargets uiTargets = createUiTargets(pEnv);
    QActionGroup* pActionGroup = pEnv->newActionGroup();
    const FileActions fileActions = createFileActions(pEnv);
    const UndoRedoActions undoRedoActions = createUndoRedoActions(pEnv);
    const ModelingActions modelingActions = createModelingActions(pEnv, pActionGroup);
    const PrimitiveActions primitiveActions = createPrimitiveActions(pEnv, pActionGroup);
    const BooleanActions booleanActions = createBooleanActions(pEnv, pActionGroup);
    const EditActions editActions = createEditActions(pEnv, pActionGroup);
    const UtilityActions utilityActions = createUtilityActions(pEnv, pActionGroup);
    const ViewActions viewActions = createViewActions(pEnv);
#ifdef _DEBUG
    const TestActions testActions = createTestActions(pEnv, pActionGroup);
#endif // _DEBUG

    buildFileMenuUi(fileActions, uiTargets.pMenuFile);
    buildViewMenuUi(viewActions, uiTargets.pMenuView);
    buildEditMenuUi(undoRedoActions, uiTargets.pMenuEdit);
    buildSketchMenuUi(modelingActions, uiTargets.pMenuSketch);
    buildSolidMenuUi(modelingActions, booleanActions, primitiveActions, editActions,
                     uiTargets.pMenuSolid);
    buildSheetMenuUi(modelingActions, uiTargets.pMenuSheet);
    buildSheetToolBarUi(pEnv, pActionGroup, modelingActions, uiTargets.pToolBarSheet);
    buildFileToolBarUi(fileActions, uiTargets.pToolBarFile);
    buildEditToolBarUi(undoRedoActions, uiTargets.pToolBarEdit);
    buildModelingToolBarUi(
        pEnv,
        pActionGroup,
        modelingActions,
        booleanActions,
        primitiveActions,
        editActions,
        uiTargets.pToolBarModeling);
    buildUtilityToolBarUi(utilityActions, uiTargets.pToolBarUtility);
    buildToolsMenuUi(modelingActions.pActionSelect, utilityActions, uiTargets.pMenuTools);
    buildViewToolBarUi(pEnv, viewActions, uiTargets.pToolBarView);
#ifdef _DEBUG
    buildTestToolBarUi(testActions, uiTargets.pToolBarTest);
#endif // _DEBUG

    pEnv->restoreUiState();
}

void ModelingEnvironmentUI::teardown(ModelingEnvironment* pEnv)
{
    if (!pEnv)
    {
        assert(false);
        return;
    }

    pEnv->destroyUI();
}
