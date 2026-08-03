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

#include "SketchEnvironmentUI.h"

#include <cassert>
#include <list>

#include <QActionGroup>
#include <QCoreApplication>
#include <QIcon>
#include <QKeySequence>
#include <QToolButton>

#include "SketchEnvironment.h"
#include "application/Application.h"
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
    QToolBar* pToolBarEdit;
    QToolBar* pToolBarSketch;
    QToolBar* pToolBarSketchEnvironment;
    QToolBar* pToolBarView;
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

struct EditActions
{
    CommandAction* pActionCopy;
    CommandAction* pActionMove;
    CommandAction* pActionRotate;
    CommandAction* pActionSketchMirror;
    CommandAction* pActionSketchScale;
    CommandAction* pActionTrim;
    CommandAction* pActionExtend;
    CommandAction* pActionSketchFillet;
    CommandAction* pActionSketchChamfer;
    CommandAction* pActionSketchOffset;
    CommandAction* pActionSketchRectArray;
    CommandAction* pActionSketchPolarArray;
};

struct SketchActions
{
    CommandAction* pActionSelect;
    CommandAction* pActionDrawPoint;
    CommandAction* pActionDrawLine;
    CommandAction* pActionDrawLineTangent;
    CommandAction* pActionDrawCenterLine;
    CommandAction* pActionDrawRectangle;
    CommandAction* pActionDrawCenterRectangle;
    CommandAction* pActionDrawPolygon;
    CommandAction* pActionDrawCircle;
    CommandAction* pActionDrawCircleBy3Points;
    CommandAction* pActionDrawArc;
    CommandAction* pActionDrawArcBy3Points;
    CommandAction* pActionDrawEllipse;
    CommandAction* pActionDrawEllipseArc;
    CommandAction* pActionDrawSpline;
    CommandAction* pActionDrawStyleSpline;
    CommandAction* pActionDrawEquationDrivenSpline;
    CommandAction* pActionSketchText;
    CommandAction* pActionSketchProject;
};

struct SketchEnvironmentActions
{
    CommandAction* pActionEndSketch;
    CommandAction* pActionCancelSketch;
    CommandAction* pActionRelocateSketchCsys;
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
    CommandAction* pActionOrientToSketch;
};

UiTargets createUiTargets(SketchEnvironment* pEnv)
{
    assert(pEnv);

    UiTargets targets = {};
    targets.pToolBarSketch = pEnv->addToolBar(
        QCoreApplication::translate("MainWindow", "Sketch"),
        wy3dApp::ToolBarNames::Sketch);

    targets.pToolBarEdit = pEnv->addToolBar(
        QCoreApplication::translate("MainWindow", "Edit"),
        wy3dApp::ToolBarNames::Edit);

    targets.pToolBarSketchEnvironment = pEnv->addToolBar(
        QCoreApplication::translate("MainWindow", "SketchEnvironment"),
        wy3dApp::ToolBarNames::SketchEnvironment);

    targets.pToolBarView = pEnv->addToolBar(
        QCoreApplication::translate("MainWindow", "View"),
        wy3dApp::ToolBarNames::View);

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

FileActions createFileActions(SketchEnvironment* pEnv)
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

UndoRedoActions createUndoRedoActions(SketchEnvironment* pEnv)
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

EditActions createEditActions(SketchEnvironment* pEnv, QActionGroup* pActionGroup)
{
    assert(pEnv);
    assert(pActionGroup);

    EditActions actions = {};
    actions.pActionCopy = pEnv->newCommandAction(
        CommandNames::Copy,
        QCoreApplication::translate("MainWindow", "Copy"),
        QIcon(":/images/Edit_Copy.svg"),
        pActionGroup);
    if (actions.pActionCopy)
    {
        actions.pActionCopy->setShortcut(QKeySequence::fromString("Shift+C,O"));
    }

    actions.pActionMove = pEnv->newCommandAction(
        CommandNames::Move,
        QCoreApplication::translate("MainWindow", "Move"),
        QIcon(":/images/Edit_Move.svg"),
        pActionGroup);
    if (actions.pActionMove)
    {
        actions.pActionMove->setShortcut(QKeySequence::fromString("M,O"));
    }

    actions.pActionRotate = pEnv->newCommandAction(
        CommandNames::Rotate,
        QCoreApplication::translate("MainWindow", "Rotate"),
        QIcon(":/images/Edit_Rotate.svg"),
        pActionGroup);
    if (actions.pActionRotate)
    {
        actions.pActionRotate->setShortcut(QKeySequence::fromString("R,O"));
    }

    actions.pActionSketchMirror = pEnv->newCommandAction(
        CommandNames::SketchMirror,
        QCoreApplication::translate("MainWindow", "Mirror"),
        QIcon(":/images/Sketch_Mirror.svg"),
        pActionGroup);
    if (actions.pActionSketchMirror)
    {
        actions.pActionSketchMirror->setShortcut(QKeySequence::fromString("M,I"));
    }

    actions.pActionSketchScale = pEnv->newCommandAction(
        CommandNames::SketchScale,
        QCoreApplication::translate("MainWindow", "Scale"),
        QIcon(":/images/Sketch_Scale.svg"),
        pActionGroup);
    if (actions.pActionSketchScale)
    {
        actions.pActionSketchScale->setShortcut(QKeySequence::fromString("S,C"));
    }

    actions.pActionTrim = pEnv->newCommandAction(
        CommandNames::Trim,
        QCoreApplication::translate("MainWindow", "Trim"),
        QIcon(":/images/Sketch_Trim.svg"),
        pActionGroup);
    if (actions.pActionTrim)
    {
        actions.pActionTrim->setShortcut(QKeySequence::fromString("T,R"));
    }

    actions.pActionExtend = pEnv->newCommandAction(
        CommandNames::Extend,
        QCoreApplication::translate("MainWindow", "Extend"),
        QIcon(":/images/Sketch_Extend.svg"),
        pActionGroup);
    if (actions.pActionExtend)
    {
        actions.pActionExtend->setShortcut(QKeySequence::fromString("E,X"));
    }

    actions.pActionSketchFillet = pEnv->newCommandAction(
        CommandNames::SketchFillet,
        QCoreApplication::translate("MainWindow", "Sketch Fillet"),
        QIcon(":/images/Sketch_Fillet.svg"),
        pActionGroup);
    if (actions.pActionSketchFillet)
    {
        actions.pActionSketchFillet->setShortcut(QKeySequence::fromString("F,I"));
    }

    actions.pActionSketchChamfer = pEnv->newCommandAction(
        CommandNames::SketchChamfer,
        QCoreApplication::translate("MainWindow", "Sketch Chamfer"),
        QIcon(":/images/Sketch_Chamfer.svg"),
        pActionGroup);
    if (actions.pActionSketchChamfer)
    {
        actions.pActionSketchChamfer->setShortcut(QKeySequence::fromString("Shift+C,H"));
    }

    actions.pActionSketchOffset = pEnv->newCommandAction(
        CommandNames::SketchOffset,
        QCoreApplication::translate("MainWindow", "Sketch Offset"),
        QIcon(":/images/Sketch_Offset.svg"),
        pActionGroup);
    if (actions.pActionSketchOffset)
    {
        actions.pActionSketchOffset->setShortcut(QKeySequence::fromString("O,F"));
    }

    actions.pActionSketchRectArray = pEnv->newCommandAction(
        CommandNames::SketchRectArray,
        QCoreApplication::translate("MainWindow", "Sketch Rect Array"),
        QIcon(":/images/Sketch_RectArray.svg"),
        pActionGroup);
    if (actions.pActionSketchRectArray)
    {
        actions.pActionSketchRectArray->setShortcut(QKeySequence::fromString("Shift+A,R"));
    }

    actions.pActionSketchPolarArray = pEnv->newCommandAction(
        CommandNames::SketchPolarArray,
        QCoreApplication::translate("MainWindow", "Sketch Polar Array"),
        QIcon(":/images/Sketch_PolarArray.svg"),
        pActionGroup);

    return actions;
}

SketchActions createSketchActions(SketchEnvironment* pEnv, QActionGroup* pActionGroup)
{
    assert(pEnv);
    assert(pActionGroup);

    SketchActions actions = {};
    actions.pActionSelect = pEnv->newCommandAction(
        CommandNames::Select,
        QCoreApplication::translate("MainWindow", "Select"),
        QIcon(":/images/Basic_Select.svg"),
        pActionGroup);

    actions.pActionDrawPoint = pEnv->newCommandAction(
        CommandNames::Point,
        QCoreApplication::translate("MainWindow", "Point"),
        QIcon(":/images/Sketch_DrawPoint.svg"),
        pActionGroup);
    if (actions.pActionDrawPoint)
    {
        actions.pActionDrawPoint->setShortcut(QKeySequence::fromString("P,O,I"));
    }

    actions.pActionDrawLine = pEnv->newCommandAction(
        CommandNames::Line,
        QCoreApplication::translate("MainWindow", "Line"),
        QIcon(":/images/Sketch_DrawLine.svg"),
        pActionGroup);
    if (actions.pActionDrawLine)
    {
        actions.pActionDrawLine->setShortcut(QKeySequence(Qt::Key_L));
    }

    actions.pActionDrawLineTangent = pEnv->newCommandAction(
        CommandNames::LineTangent,
        QCoreApplication::translate("MainWindow", "Line Tangent"),
        QIcon(":/images/Sketch_DrawLineTangent.svg"),
        pActionGroup);

    actions.pActionDrawCenterLine = pEnv->newCommandAction(
        CommandNames::CenterLine,
        QCoreApplication::translate("MainWindow", "Center Line"),
        QIcon(":/images/Sketch_DrawCenterLine.svg"),
        pActionGroup);

    actions.pActionDrawRectangle = pEnv->newCommandAction(
        CommandNames::Rectangle,
        QCoreApplication::translate("MainWindow", "Rectangle"),
        QIcon(":/images/Sketch_DrawRectangle.svg"),
        pActionGroup);
    if (actions.pActionDrawRectangle)
    {
        actions.pActionDrawRectangle->setShortcut(QKeySequence::fromString("R,E,C"));
    }

    actions.pActionDrawCenterRectangle = pEnv->newCommandAction(
        CommandNames::CenterRectangle,
        QCoreApplication::translate("MainWindow", "CenterRectangle"),
        QIcon(":/images/Sketch_DrawCenterRectangle.svg"),
        pActionGroup);

    actions.pActionDrawPolygon = pEnv->newCommandAction(
        CommandNames::Polygon,
        QCoreApplication::translate("MainWindow", "Polygon"),
        QIcon(":/images/Sketch_DrawPolygon.svg"),
        pActionGroup);
    if (actions.pActionDrawPolygon)
    {
        actions.pActionDrawPolygon->setShortcut(QKeySequence::fromString("P,O,L"));
    }

    actions.pActionDrawCircle = pEnv->newCommandAction(
        CommandNames::Circle,
        QCoreApplication::translate("MainWindow", "Circle"),
        QIcon(":/images/Sketch_DrawCircle.svg"),
        pActionGroup);
    if (actions.pActionDrawCircle)
    {
        actions.pActionDrawCircle->setShortcut(QKeySequence(Qt::Key_C));
    }

    actions.pActionDrawCircleBy3Points = pEnv->newCommandAction(
        CommandNames::CircleBy3Points,
        QCoreApplication::translate("MainWindow", "Circle by 3 Points"),
        QIcon(":/images/Sketch_DrawCircleBy3Points.svg"),
        pActionGroup);

    actions.pActionDrawArc = pEnv->newCommandAction(
        CommandNames::Arc,
        QCoreApplication::translate("MainWindow", "Arc"),
        QIcon(":/images/Sketch_DrawArc.svg"),
        pActionGroup);
    if (actions.pActionDrawArc)
    {
        actions.pActionDrawArc->setShortcut(QKeySequence(Qt::Key_A));
    }

    actions.pActionDrawArcBy3Points = pEnv->newCommandAction(
        CommandNames::ArcBy3Points,
        QCoreApplication::translate("MainWindow", "Arc by 3 Points"),
        QIcon(":/images/Sketch_DrawArcBy3Points.svg"),
        pActionGroup);

    actions.pActionDrawEllipse = pEnv->newCommandAction(
        CommandNames::Ellipse,
        QCoreApplication::translate("MainWindow", "Ellipse"),
        QIcon(":/images/Sketch_DrawEllipse.svg"),
        pActionGroup);
    if (actions.pActionDrawEllipse)
    {
        actions.pActionDrawEllipse->setShortcut(QKeySequence::fromString("E,L"));
    }

    actions.pActionDrawEllipseArc = pEnv->newCommandAction(
        CommandNames::EllipseArc,
        QCoreApplication::translate("MainWindow", "Ellipse Arc"),
        QIcon(":/images/Sketch_DrawEllipseArc.svg"),
        pActionGroup);
    if (actions.pActionDrawEllipseArc)
    {
        actions.pActionDrawEllipseArc->setShortcut(QKeySequence::fromString("E,A"));
    }

    actions.pActionDrawSpline = pEnv->newCommandAction(
        CommandNames::Spline,
        QCoreApplication::translate("MainWindow", "Spline"),
        QIcon(":/images/Sketch_DrawSpline.svg"),
        pActionGroup);
    if (actions.pActionDrawSpline)
    {
        actions.pActionDrawSpline->setShortcut(QKeySequence::fromString("S,P,L"));
    }

    actions.pActionDrawStyleSpline = pEnv->newCommandAction(
        CommandNames::StyleSpline,
        QCoreApplication::translate("MainWindow", "Style Spline"),
        QIcon(":/images/Sketch_DrawStyleSpline.svg"),
        pActionGroup);

    actions.pActionDrawEquationDrivenSpline = pEnv->newCommandAction(
        CommandNames::EquationDrivenSpline,
        QCoreApplication::translate("MainWindow", "Equation Driven Spline"),
        QIcon(":/images/Sketch_DrawSplineFx.svg"),
        pActionGroup);

    actions.pActionSketchText = pEnv->newCommandAction(
        CommandNames::SketchText,
        QCoreApplication::translate("MainWindow", "Sketch Text"),
        QIcon(":/images/Sketch_DrawText.svg"),
        pActionGroup);
    if (actions.pActionSketchText)
    {
        actions.pActionSketchText->setShortcut(QKeySequence::fromString("T,E"));
    }

    actions.pActionSketchProject = pEnv->newCommandAction(
        CommandNames::SketchProject,
        QCoreApplication::translate("MainWindow", "Project Edge"),
        QIcon(":/images/Sketch_Project.svg"),
        pActionGroup);

    return actions;
}

SketchEnvironmentActions createSketchEnvironmentActions(
    SketchEnvironment* pEnv,
    QActionGroup* pActionGroup)
{
    assert(pEnv);
    assert(pActionGroup);

    SketchEnvironmentActions actions = {};
    actions.pActionEndSketch = pEnv->newCommandAction(
        CommandNames::EndSketch,
        QCoreApplication::translate("MainWindow", "End Sketch"),
        QIcon(":/images/Sketch_OK.svg"),
        pActionGroup);

    actions.pActionCancelSketch = pEnv->newCommandAction(
        CommandNames::CancelSketch,
        QCoreApplication::translate("MainWindow", "Cancel Sketch"),
        QIcon(":/images/Sketch_Cancel.svg"),
        pActionGroup);

    actions.pActionRelocateSketchCsys = pEnv->newCommandAction(
        CommandNames::RelocateSketchCSYS,
        QCoreApplication::translate("MainWindow", "Relocate Sketch CSYS"),
        QIcon(":/images/Sketch_CSYS.svg"),
        pActionGroup);

    return actions;
}

ViewActions createViewActions(SketchEnvironment* pEnv, QActionGroup* pActionGroup)
{
    assert(pEnv);
    assert(pActionGroup);

    ViewActions actions = {};
    actions.pActionFitView = pEnv->newCommandAction(
        CommandNames::FitView,
        QCoreApplication::translate("MainWindow", "Fit View"),
        QIcon(":/images/View_FullScreen.svg"),
        pActionGroup);

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

    actions.pActionOrientToSketch = pEnv->newCommandAction(
        CommandNames::OrientToSketch,
        QCoreApplication::translate("MainWindow", "Orient to Sketch"),
        QIcon(":/images/View_Normal.svg"));

    return actions;
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

void buildEditToolBarUi(const EditActions& actions, QToolBar* pToolBarEdit)
{
    assert(pToolBarEdit);

    pToolBarEdit->addAction(actions.pActionCopy);
    pToolBarEdit->addAction(actions.pActionMove);
    pToolBarEdit->addAction(actions.pActionRotate);
    pToolBarEdit->addAction(actions.pActionSketchMirror);
    pToolBarEdit->addAction(actions.pActionSketchScale);
    pToolBarEdit->addAction(actions.pActionTrim);
    pToolBarEdit->addAction(actions.pActionExtend);
    pToolBarEdit->addAction(actions.pActionSketchFillet);
    pToolBarEdit->addAction(actions.pActionSketchChamfer);
    pToolBarEdit->addAction(actions.pActionSketchOffset);
    pToolBarEdit->addAction(actions.pActionSketchRectArray);
    pToolBarEdit->addAction(actions.pActionSketchPolarArray);
}

void buildSketchToolBarUi(
    SketchEnvironment* pEnv,
    QActionGroup* pActionGroup,
    const SketchActions& actions,
    QToolBar* pToolBarSketch)
{
    assert(pEnv);
    assert(pActionGroup);
    assert(pToolBarSketch);

    pToolBarSketch->addAction(actions.pActionSelect);
    pToolBarSketch->addAction(actions.pActionDrawPoint);

    std::list<QAction*> lineSeriesActions;
    lineSeriesActions.emplace_back(actions.pActionDrawLine);
    lineSeriesActions.emplace_back(actions.pActionDrawLineTangent);
    lineSeriesActions.emplace_back(actions.pActionDrawCenterLine);
    QToolButton* pToolBtnLineSeries = pEnv->newMenuPopupToolButton(
        pToolBarSketch,
        QCoreApplication::translate("MainWindow", "Line Series"),
        pActionGroup,
        lineSeriesActions);
    pToolBarSketch->addWidget(pToolBtnLineSeries);

    pToolBarSketch->addAction(actions.pActionDrawRectangle);
    pToolBarSketch->addAction(actions.pActionDrawCenterRectangle);
    pToolBarSketch->addAction(actions.pActionDrawPolygon);
    pToolBarSketch->addAction(actions.pActionDrawCircle);
    pToolBarSketch->addAction(actions.pActionDrawCircleBy3Points);
    pToolBarSketch->addAction(actions.pActionDrawArc);
    pToolBarSketch->addAction(actions.pActionDrawArcBy3Points);
    pToolBarSketch->addAction(actions.pActionDrawEllipse);
    pToolBarSketch->addAction(actions.pActionDrawEllipseArc);

    std::list<QAction*> splineSeriesActions;
    splineSeriesActions.emplace_back(actions.pActionDrawSpline);
    splineSeriesActions.emplace_back(actions.pActionDrawStyleSpline);
    splineSeriesActions.emplace_back(actions.pActionDrawEquationDrivenSpline);
    QToolButton* pToolBtnSplineSeries = pEnv->newMenuPopupToolButton(
        pToolBarSketch,
        QCoreApplication::translate("MainWindow", "Spline Series"),
        pActionGroup,
        splineSeriesActions);
    pToolBarSketch->addWidget(pToolBtnSplineSeries);

    pToolBarSketch->addAction(actions.pActionSketchText);
    pToolBarSketch->addAction(actions.pActionSketchProject);
}

void buildSketchEnvironmentToolBarUi(
    const SketchEnvironmentActions& actions,
    QToolBar* pToolBarSketchEnvironment)
{
    assert(pToolBarSketchEnvironment);

    pToolBarSketchEnvironment->addAction(actions.pActionEndSketch);
    pToolBarSketchEnvironment->addAction(actions.pActionCancelSketch);
    pToolBarSketchEnvironment->addAction(actions.pActionRelocateSketchCsys);
}

void buildViewToolBarUi(const ViewActions& actions, QToolBar* pToolBarView)
{
    assert(pToolBarView);

    pToolBarView->addAction(actions.pActionFitView);
    pToolBarView->addAction(actions.pActionIsometricView);
    pToolBarView->addAction(actions.pActionFrontView);
    pToolBarView->addAction(actions.pActionBackView);
    pToolBarView->addAction(actions.pActionLeftView);
    pToolBarView->addAction(actions.pActionRightView);
    pToolBarView->addAction(actions.pActionTopView);
    pToolBarView->addAction(actions.pActionBottomView);
    pToolBarView->addAction(actions.pActionOrientToSketch);
}
} // namespace

SketchEnvironmentUI::SketchEnvironmentUI()
{
}

SketchEnvironmentUI::~SketchEnvironmentUI()
{
}

void SketchEnvironmentUI::initialize(SketchEnvironment* pEnv)
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
    const EditActions editActions = createEditActions(pEnv, pActionGroup);
    const SketchActions sketchActions = createSketchActions(pEnv, pActionGroup);
    const SketchEnvironmentActions sketchEnvironmentActions =
        createSketchEnvironmentActions(pEnv, pActionGroup);
    const ViewActions viewActions = createViewActions(pEnv, pActionGroup);

    buildFileMenuUi(fileActions, uiTargets.pMenuFile);
    buildBasicToolBarUi(fileActions, undoRedoActions, uiTargets.pToolBarBasic);
    buildEditToolBarUi(editActions, uiTargets.pToolBarEdit);
    buildSketchToolBarUi(
        pEnv,
        pActionGroup,
        sketchActions,
        uiTargets.pToolBarSketch);
    buildSketchEnvironmentToolBarUi(
        sketchEnvironmentActions,
        uiTargets.pToolBarSketchEnvironment);
    buildViewToolBarUi(viewActions, uiTargets.pToolBarView);
}

void SketchEnvironmentUI::teardown(SketchEnvironment* pEnv)
{
    if (!pEnv)
    {
        assert(false);
        return;
    }

    pEnv->destroyUI();
}
