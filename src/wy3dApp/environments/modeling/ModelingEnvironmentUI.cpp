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
#include "widgets/frame/MainWindow.h"

namespace
{
struct UiTargets
{
    QMenu* pMenuFile;
    QToolBar* pToolBarBasic;
    QToolBar* pToolBarModeling;
    QToolBar* pToolBarPrimitive;
    QToolBar* pToolBarBoolean;
    QToolBar* pToolBarEdit;
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
    CommandAction* pActionMeasureDistance;
    CommandAction* pActionRunScript;
};

struct ViewActions
{
    CommandAction* pActionFitView;
    CommandAction* pActionIsometricView;
    CommandAction* pActionFrontView;
    CommandAction* pActionBackView;
    CommandAction* pActionLeftView;
    CommandAction* pActionRightView;
    CommandAction* pActionTopView;
    CommandAction* pActionBottomView;
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
        QIcon(":/images/Document_Save.svg"));

    actions.pActionSaveAsFile = pEnv->newCommandAction(
        CommandNames::SaveAsFile,
        QCoreApplication::translate("MainWindow", "Save As"),
        QIcon(":/images/Document_SaveAs.svg"));

    actions.pActionImportFile = pEnv->newCommandAction(
        CommandNames::ImportFile,
        QCoreApplication::translate("MainWindow", "Import"),
        QIcon(":/images/Document_Import.svg"));

    actions.pActionExportFile = pEnv->newCommandAction(
        CommandNames::ExportFile,
        QCoreApplication::translate("MainWindow", "Export"),
        QIcon(":/images/Document_Export.svg"));

    return actions;
}

UndoRedoActions createUndoRedoActions(ModelingEnvironment* pEnv)
{
    assert(pEnv);

    UndoRedoActions actions = {};
    actions.pActionUndo = pEnv->newCommandAction(
        CommandNames::Undo,
        QCoreApplication::translate("MainWindow", "Undo"),
        QIcon(":/images/Basic_Undo.svg"));
    actions.pActionUndo->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Z));
    actions.pActionUndo->setShortcutContext(Qt::ApplicationShortcut);

    actions.pActionRedo = pEnv->newCommandAction(
        CommandNames::Redo,
        QCoreApplication::translate("MainWindow", "Redo"),
        QIcon(":/images/Basic_Redo.svg"));
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
        QIcon(":/images/Basic_Select.svg"),
        pActionGroup);

    actions.pActionNewSketch = pEnv->newCommandAction(
        CommandNames::NewSketch,
        QCoreApplication::translate("MainWindow", "New Sketch"),
        QIcon(":/images/Modeling_NewSketch.svg"),
        pActionGroup);

    actions.pActionEditSketch = pEnv->newCommandAction(
        CommandNames::EditSketch,
        QCoreApplication::translate("MainWindow", "Edit Sketch"),
        QIcon(":/images/Edit_Sketch.svg"),
        pActionGroup);

    actions.pActionParallelDatumPlane = pEnv->newCommandAction(
        CommandNames::ParallelDatumPlane,
        QCoreApplication::translate("MainWindow", "Parallel Datum Plane"),
        QIcon(":/images/DatumPlane_Parallel.svg"),
        pActionGroup);

    actions.pActionCoincidentDatumPlane = pEnv->newCommandAction(
        CommandNames::CoincidentDatumPlane,
        QCoreApplication::translate("MainWindow", "Coincident Datum Plane"),
        QIcon(":/images/DatumPlane_Coincident.svg"),
        pActionGroup);

    actions.pActionAngularDatumPlane = pEnv->newCommandAction(
        CommandNames::AngularDatumPlane,
        QCoreApplication::translate("MainWindow", "Angular Datum Plane"),
        QIcon(":/images/DatumPlane_Angular.svg"),
        pActionGroup);

    actions.pActionPerpendicularDatumPlane = pEnv->newCommandAction(
        CommandNames::PerpendicularDatumPlane,
        QCoreApplication::translate("MainWindow", "Perpendicular Datum Plane"),
        QIcon(":/images/DatumPlane_Perpendicular.svg"),
        pActionGroup);

    actions.pActionThroughAxisDatumPlane = pEnv->newCommandAction(
        CommandNames::ThroughAxisDatumPlane,
        QCoreApplication::translate("MainWindow", "Through Axis Datum Plane"),
        QIcon(":/images/DatumPlane_ThroughAxis.svg"),
        pActionGroup);

    actions.pActionNormalToCurveDatumPlane = pEnv->newCommandAction(
        CommandNames::NormalToCurveDatumPlane,
        QCoreApplication::translate("MainWindow", "Normal To Curve Datum Plane"),
        QIcon(":/images/DatumPlane_NormalToEdge.svg"),
        pActionGroup);

    actions.pActionThrough3PointsDatumPlane = pEnv->newCommandAction(
        CommandNames::Through3PointsDatumPlane,
        QCoreApplication::translate("MainWindow", "Through 3 Points Datum Plane"),
        QIcon(":/images/DatumPlane_Through3Points.svg"),
        pActionGroup);

    actions.pActionTangentDatumPlane = pEnv->newCommandAction(
        CommandNames::TangentDatumPlane,
        QCoreApplication::translate("MainWindow", "Tangent Datum Plane"),
        QIcon(":/images/DatumPlane_Tangent.svg"),
        pActionGroup);

    actions.pActionHelix = pEnv->newCommandAction(
        CommandNames::Helix,
        QCoreApplication::translate("MainWindow", "Helix"),
        QIcon(":/images/Curve_Helix.svg"),
        pActionGroup);

    actions.pActionExtrude = pEnv->newCommandAction(
        CommandNames::Extrude,
        QCoreApplication::translate("MainWindow", "Extrude"),
        QIcon(":/images/Modeling_Extrusion.png"),
        pActionGroup);

    actions.pActionExtrudedSheet = pEnv->newCommandAction(
        CommandNames::ExtrudedSheet,
        QCoreApplication::translate("MainWindow", "Extruded Sheet"),
        QIcon(":/images/Modeling_Extrusion.png"),
        pActionGroup);

    actions.pActionRevolvedSheet = pEnv->newCommandAction(
        CommandNames::RevolvedSheet,
        QCoreApplication::translate("MainWindow", "Revolved Sheet"),
        QIcon(":/images/Modeling_Revolution.png"),
        pActionGroup);

    actions.pActionRevolve = pEnv->newCommandAction(
        CommandNames::Revolve,
        QCoreApplication::translate("MainWindow", "Revolve"),
        QIcon(":/images/Modeling_Revolution.png"),
        pActionGroup);

    actions.pActionSweep = pEnv->newCommandAction(
        CommandNames::Sweep,
        QCoreApplication::translate("MainWindow", "Sweep"),
        QIcon(":/images/Modeling_Sweep.png"),
        pActionGroup);

    actions.pActionLoft = pEnv->newCommandAction(
        CommandNames::Loft,
        QCoreApplication::translate("MainWindow", "Loft"),
        QIcon(":/images/Modeling_Loft.png"),
        pActionGroup);

    actions.pActionExtrudeCut = pEnv->newCommandAction(
        CommandNames::ExtrudeCut,
        QCoreApplication::translate("MainWindow", "Extrude Cut"),
        QIcon(":/images/Modeling_ExtrusionCut.png"),
        pActionGroup);

    actions.pActionRevolveCut = pEnv->newCommandAction(
        CommandNames::RevolveCut,
        QCoreApplication::translate("MainWindow", "Revolve Cut"),
        QIcon(":/images/Modeling_RevolutionCut.png"),
        pActionGroup);

    actions.pActionSweepCut = pEnv->newCommandAction(
        CommandNames::SweepCut,
        QCoreApplication::translate("MainWindow", "Sweep Cut"),
        QIcon(":/images/Modeling_SweepCut.png"),
        pActionGroup);

    actions.pActionLoftCut = pEnv->newCommandAction(
        CommandNames::LoftCut,
        QCoreApplication::translate("MainWindow", "Loft Cut"),
        QIcon(":/images/Modeling_LoftCut.png"),
        pActionGroup);

    actions.pActionMerge = pEnv->newCommandAction(
        CommandNames::Merge,
        QCoreApplication::translate("MainWindow", "Merge"),
        QIcon(":/images/Modeling_Merge.svg"),
        pActionGroup);

    actions.pActionChamfer = pEnv->newCommandAction(
        CommandNames::Chamfer,
        QCoreApplication::translate("MainWindow", "Chamfer"),
        QIcon(":/images/Modeling_Chamfer.png"),
        pActionGroup);

    actions.pActionFillet = pEnv->newCommandAction(
        CommandNames::Fillet,
        QCoreApplication::translate("MainWindow", "Fillet"),
        QIcon(":/images/Modeling_Fillet.png"),
        pActionGroup);

    actions.pActionShell = pEnv->newCommandAction(
        CommandNames::Shell,
        QCoreApplication::translate("MainWindow", "Shell"),
        QIcon(":/images/Modeling_Shell.png"),
        pActionGroup);

    actions.pActionDraft = pEnv->newCommandAction(
        CommandNames::Draft,
        QCoreApplication::translate("MainWindow", "Draft"),
        QIcon(":/images/Modeling_Draft.png"),
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
        QIcon(":/images/Primitive_Box.png"),
        pActionGroup);
    actions.pActionMakeBox->setShortcut(QKeySequence::fromString("B,O,X"));

    actions.pActionMakeCylinder = pEnv->newCommandAction(
        CommandNames::MakeCylinder,
        QCoreApplication::translate("MainWindow", "Cylinder"),
        QIcon(":/images/Primitive_Cylinder.png"),
        pActionGroup);
    actions.pActionMakeCylinder->setShortcut(QKeySequence::fromString("C,Y,L"));

    actions.pActionMakeSphere = pEnv->newCommandAction(
        CommandNames::MakeSphere,
        QCoreApplication::translate("MainWindow", "Sphere"),
        QIcon(":/images/Primitive_Sphere.png"),
        pActionGroup);
    actions.pActionMakeSphere->setShortcut(QKeySequence::fromString("S,P,H"));

    actions.pActionMakeCone = pEnv->newCommandAction(
        CommandNames::MakeCone,
        QCoreApplication::translate("MainWindow", "Cone"),
        QIcon(":/images/Primitive_Cone.png"),
        pActionGroup);
    actions.pActionMakeCone->setShortcut(QKeySequence::fromString("C,O,N"));

    actions.pActionMakeTorus = pEnv->newCommandAction(
        CommandNames::MakeTorus,
        QCoreApplication::translate("MainWindow", "Torus"),
        QIcon(":/images/Primitive_Torus.png"),
        pActionGroup);
    actions.pActionMakeTorus->setShortcut(QKeySequence::fromString("T,O,R"));

    actions.pActionMakeTube = pEnv->newCommandAction(
        CommandNames::MakeTube,
        QCoreApplication::translate("MainWindow", "Tube"),
        QIcon(":/images/Primitive_Tube.png"),
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
        QIcon(":/images/Modeling_Fuse.svg"),
        pActionGroup);

    actions.pActionSubtract = pEnv->newCommandAction(
        CommandNames::Subtract,
        QCoreApplication::translate("MainWindow", "Subtract"),
        QIcon(":/images/Modeling_Cut.svg"),
        pActionGroup);

    actions.pActionIntersect = pEnv->newCommandAction(
        CommandNames::Intersect,
        QCoreApplication::translate("MainWindow", "Intersect"),
        QIcon(":/images/Modeling_Common.svg"),
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
        QIcon(":/images/Edit_Move.svg"),
        pActionGroup);
    actions.pActionMove->setShortcut(QKeySequence::fromString("M,O"));

    actions.pActionRotate = pEnv->newCommandAction(
        CommandNames::Rotate,
        QCoreApplication::translate("MainWindow", "Rotate"),
        QIcon(":/images/Edit_Rotate.svg"),
        pActionGroup);
    actions.pActionRotate->setShortcut(QKeySequence::fromString("R,O"));

    actions.pActionMirror = pEnv->newCommandAction(
        CommandNames::Mirror,
        QCoreApplication::translate("MainWindow", "Mirror"),
        QIcon(":/images/Sketch_Mirror.svg"),
        pActionGroup);
    actions.pActionMirror->setShortcut(QKeySequence::fromString("M,I"));

    actions.pActionLinearPattern = pEnv->newCommandAction(
        CommandNames::LinearPattern,
        QCoreApplication::translate("MainWindow", "Linear Pattern"),
        QIcon(":/images/Sketch_RectArray.svg"),
        pActionGroup);
    actions.pActionLinearPattern->setShortcut(QKeySequence::fromString("L,P"));

    actions.pActionCircularPattern = pEnv->newCommandAction(
        CommandNames::CircularPattern,
        QCoreApplication::translate("MainWindow", "Circular Pattern"),
        QIcon(":/images/Sketch_PolarArray.svg"),
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
        QIcon(":/images/Utility_SetColor.svg"),
        pActionGroup);

    actions.pActionMeasureDistance = pEnv->newCommandAction(
        CommandNames::MeasureDistance,
        QCoreApplication::translate("MainWindow", "Measure Distance"),
        QIcon(":/images/Utility_MeasureDistance.svg"),
        pActionGroup);

    actions.pActionRunScript = pEnv->newCommandAction(
        CommandNames::RunScript,
        QCoreApplication::translate("MainWindow", "Run Script"),
        QIcon(":/images/Utility_RunScript.svg"),
        pActionGroup);

    return actions;
}

ViewActions createViewActions(ModelingEnvironment* pEnv)
{
    assert(pEnv);

    ViewActions actions = {};
    actions.pActionFitView = pEnv->newCommandAction(
        CommandNames::FitView,
        QCoreApplication::translate("MainWindow", "Fit View"),
        QIcon(":/images/View_FullScreen.svg"));

    actions.pActionIsometricView = pEnv->newCommandAction(
        CommandNames::IsometricView,
        QCoreApplication::translate("MainWindow", "IsometricView View"),
        QIcon(":/images/View_ISO.svg"));

    actions.pActionFrontView = pEnv->newCommandAction(
        CommandNames::FrontView,
        QCoreApplication::translate("MainWindow", "Front View"),
        QIcon(":/images/View_Front.svg"));

    actions.pActionBackView = pEnv->newCommandAction(
        CommandNames::BackView,
        QCoreApplication::translate("MainWindow", "Back View"),
        QIcon(":/images/View_Back.svg"));

    actions.pActionLeftView = pEnv->newCommandAction(
        CommandNames::LeftView,
        QCoreApplication::translate("MainWindow", "Left View"),
        QIcon(":/images/View_Left.svg"));

    actions.pActionRightView = pEnv->newCommandAction(
        CommandNames::RightView,
        QCoreApplication::translate("MainWindow", "Right View"),
        QIcon(":/images/View_Right.svg"));

    actions.pActionTopView = pEnv->newCommandAction(
        CommandNames::TopView,
        QCoreApplication::translate("MainWindow", "Top View"),
        QIcon(":/images/View_Top.svg"));

    actions.pActionBottomView = pEnv->newCommandAction(
        CommandNames::BottomView,
        QCoreApplication::translate("MainWindow", "Bottom View"),
        QIcon(":/images/View_Bottom.svg"));

    actions.pActionShadedDisplay = pEnv->newCommandAction(
        CommandNames::ShadedDisplay,
        QCoreApplication::translate("MainWindow", "Shaded"),
        QIcon(":/images/View_Shaded.svg"));

    actions.pActionWireframeDisplay = pEnv->newCommandAction(
        CommandNames::WireframeDisplay,
        QCoreApplication::translate("MainWindow", "Wireframe"),
        QIcon(":/images/View_Wireframe.svg"));

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
        QIcon(":/images/Test_TopoName.svg"),
        pActionGroup);

    actions.pActionCheckTopoName = pEnv->newCommandAction(
        CommandNames::CheckTopoName,
        QCoreApplication::translate("MainWindow", "CheckTopoName"),
        QIcon(":/images/Test_CheckTopoName.svg"),
        pActionGroup);

    return actions;
}
#endif // _DEBUG

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
}

void buildBasicToolBarUi(
    const FileActions& fileActions,
    const UndoRedoActions& undoRedoActions,
    QToolBar* pToolBarBasic)
{
    assert(pToolBarBasic);

    pToolBarBasic->addAction(fileActions.pActionSaveFile);
    pToolBarBasic->addAction(undoRedoActions.pActionUndo);
    pToolBarBasic->addAction(undoRedoActions.pActionRedo);
}

void buildModelingToolBarUi(
    ModelingEnvironment* pEnv,
    QActionGroup* pActionGroup,
    const ModelingActions& actions,
    QToolBar* pToolBarModeling)
{
    assert(pEnv);
    assert(pActionGroup);
    assert(pToolBarModeling);

    pToolBarModeling->addAction(actions.pActionSelect);
    pToolBarModeling->addAction(actions.pActionNewSketch);

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

    // 曲面下拉菜单
    std::list<QAction*> sheetActions;
    sheetActions.emplace_back(actions.pActionExtrudedSheet);
    sheetActions.emplace_back(actions.pActionRevolvedSheet);
    QToolButton* pSheetToolBtn = pEnv->newMenuPopupToolButton(
        pToolBarModeling,
        QCoreApplication::translate("MainWindow", "Sheet"),
        pActionGroup,
        sheetActions);
    pToolBarModeling->addWidget(pSheetToolBtn);
    pToolBarModeling->addAction(actions.pActionSweep);
    pToolBarModeling->addAction(actions.pActionLoft);
    pToolBarModeling->addAction(actions.pActionExtrudeCut);
    pToolBarModeling->addAction(actions.pActionRevolveCut);
    pToolBarModeling->addAction(actions.pActionSweepCut);
    pToolBarModeling->addAction(actions.pActionLoftCut);
    pToolBarModeling->addAction(actions.pActionMerge);
    pToolBarModeling->addAction(actions.pActionChamfer);
    pToolBarModeling->addAction(actions.pActionFillet);
    pToolBarModeling->addAction(actions.pActionShell);
    pToolBarModeling->addAction(actions.pActionDraft);
}

void buildPrimitiveToolBarUi(const PrimitiveActions& actions, QToolBar* pToolBarPrimitive)
{
    assert(pToolBarPrimitive);

    pToolBarPrimitive->addAction(actions.pActionMakeBox);
    pToolBarPrimitive->addAction(actions.pActionMakeCylinder);
    pToolBarPrimitive->addAction(actions.pActionMakeSphere);
    pToolBarPrimitive->addAction(actions.pActionMakeCone);
    pToolBarPrimitive->addAction(actions.pActionMakeTorus);
    pToolBarPrimitive->addAction(actions.pActionMakeTube);
}

void buildBooleanToolBarUi(const BooleanActions& actions, QToolBar* pToolBarBoolean)
{
    assert(pToolBarBoolean);

    pToolBarBoolean->addAction(actions.pActionUnion);
    pToolBarBoolean->addAction(actions.pActionSubtract);
    pToolBarBoolean->addAction(actions.pActionIntersect);
}

void buildEditToolBarUi(const EditActions& actions, QToolBar* pToolBarEdit)
{
    assert(pToolBarEdit);

    pToolBarEdit->addAction(actions.pActionMove);
    pToolBarEdit->addAction(actions.pActionRotate);
    pToolBarEdit->addAction(actions.pActionMirror);
    pToolBarEdit->addAction(actions.pActionLinearPattern);
    pToolBarEdit->addAction(actions.pActionCircularPattern);
}

void buildUtilityToolBarUi(const UtilityActions& actions, QToolBar* pToolBarUtility)
{
    assert(pToolBarUtility);

    pToolBarUtility->addAction(actions.pActionSetColor);
    pToolBarUtility->addAction(actions.pActionMeasureDistance);
    pToolBarUtility->addAction(actions.pActionRunScript);
}

void buildViewToolBarUi(
    ModelingEnvironment* pEnv,
    const ViewActions& actions,
    QToolBar* pToolBarView)
{
    assert(pEnv);
    assert(pToolBarView);

    pToolBarView->addAction(actions.pActionFitView);
    pToolBarView->addAction(actions.pActionIsometricView);
    pToolBarView->addAction(actions.pActionFrontView);
    pToolBarView->addAction(actions.pActionBackView);
    pToolBarView->addAction(actions.pActionLeftView);
    pToolBarView->addAction(actions.pActionRightView);
    pToolBarView->addAction(actions.pActionTopView);
    pToolBarView->addAction(actions.pActionBottomView);

    pToolBarView->addSeparator();

    std::list<QAction*> displayModeActions;
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
    targets.pToolBarModeling = pEnv->addToolBar(
        QCoreApplication::translate("MainWindow", "Modeling"),
        wy3dApp::ToolBarNames::Modeling);

    targets.pToolBarPrimitive = pEnv->addToolBar(
        QCoreApplication::translate("MainWindow", "Primitive"),
        wy3dApp::ToolBarNames::Primitive);

    targets.pToolBarBoolean = pEnv->addToolBar(
        QCoreApplication::translate("MainWindow", "Boolean"),
        wy3dApp::ToolBarNames::Boolean);

    targets.pToolBarEdit = pEnv->addToolBar(
        QCoreApplication::translate("MainWindow", "Edit"),
        wy3dApp::ToolBarNames::Edit);

    targets.pToolBarUtility = pEnv->addToolBar(
        QCoreApplication::translate("MainWindow", "Utility"),
        wy3dApp::ToolBarNames::Utility);

    targets.pToolBarView = pEnv->addToolBar(
        QCoreApplication::translate("MainWindow", "View"),
        wy3dApp::ToolBarNames::View);

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

    targets.pToolBarBasic = pMainWindow->findChild<QToolBar*>(wy3dApp::ToolBarNames::Basic);
    assert(targets.pToolBarBasic);

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
    buildBasicToolBarUi(fileActions, undoRedoActions, uiTargets.pToolBarBasic);
    buildModelingToolBarUi(pEnv, pActionGroup, modelingActions, uiTargets.pToolBarModeling);
    buildPrimitiveToolBarUi(primitiveActions, uiTargets.pToolBarPrimitive);
    buildBooleanToolBarUi(booleanActions, uiTargets.pToolBarBoolean);
    buildEditToolBarUi(editActions, uiTargets.pToolBarEdit);
    buildUtilityToolBarUi(utilityActions, uiTargets.pToolBarUtility);
    buildViewToolBarUi(pEnv, viewActions, uiTargets.pToolBarView);
#ifdef _DEBUG
    buildTestToolBarUi(testActions, uiTargets.pToolBarTest);
#endif // _DEBUG
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
