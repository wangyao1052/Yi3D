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

#include <wy3dApp/SketchEnvironment.h>
#include <wy3dApp/Application.h>
#include "application/ApplicationImpl.h"
#include <wy3dApp/Scene.h>
#include <wy3dApp/CommandAction.h>
#include "commands/CommandNames.h"
#include "ui/MenuBarNames.h"
#include "ui/ToolBarNames.h"
#include "utils/IconUtil.h"
#include "widgets/frame/MainWindow.h"
#include "widgets/frame/ViewWidget.h"
#include "widgets/frame/ViewWidgetContainer.h"
#include "widgets/frame/ViewportOverlayBar.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

namespace
{
struct UiTargets
{
    QMenu* pMenuView;
    QMenu* pMenuEdit;
    QMenu* pMenuTools;
    QToolBar* pToolBarEdit;
    QToolBar* pToolBarSketchEdit;
    QToolBar* pToolBarSketch;
    QToolBar* pToolBarSketchEnvironment;
    QToolBar* pToolBarUtility;
    QToolBar* pToolBarView;
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
    CommandAction* pActionDrawArcTangent;
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
    CommandAction* pActionFitSelection;
    CommandAction* pActionIsometricView;
    CommandAction* pActionFrontView;
    CommandAction* pActionBackView;
    CommandAction* pActionLeftView;
    CommandAction* pActionRightView;
    CommandAction* pActionTopView;
    CommandAction* pActionBottomView;
    CommandAction* pActionOrientToSketch;
    CommandAction* pActionShadedWithEdgesDisplay;
    CommandAction* pActionShadedDisplay;
    CommandAction* pActionWireframeDisplay;
};

struct UtilityActions
{
    CommandAction* pActionFindElementById;
};

UiTargets createUiTargets(EnvironmentUI* pUI)
{
    assert(pUI);

    UiTargets targets = {};
    // Created before the environment's own tool bars so it lands right after the file
    // tool bar, which the gateway environment owns.
    targets.pToolBarEdit = pUI->addToolBar(
        QCoreApplication::translate("MainWindow", "Edit"),
        wy3dApp::ToolBarNames::Edit);
    assert(targets.pToolBarEdit);

    targets.pToolBarSketch = pUI->addToolBar(
        QCoreApplication::translate("MainWindow", "Sketch"),
        wy3dApp::ToolBarNames::Sketch);

    targets.pToolBarSketchEdit = pUI->addToolBar(
        QCoreApplication::translate("MainWindow", "Sketch Edit"),
        wy3dApp::ToolBarNames::SketchEdit);

    targets.pToolBarSketchEnvironment = pUI->addToolBar(
        QCoreApplication::translate("MainWindow", "SketchEnvironment"),
        wy3dApp::ToolBarNames::SketchEnvironment);

    targets.pToolBarUtility = pUI->addToolBar(
        QCoreApplication::translate("MainWindow", "Utility"),
        wy3dApp::ToolBarNames::Utility);

    targets.pToolBarView = pUI->addToolBar(
        QCoreApplication::translate("MainWindow", "View"),
        wy3dApp::ToolBarNames::SketchView);

    MainWindow* pMainWindow = ApplicationImpl::instance().getMainWindow();
    assert(pMainWindow);
    if (!pMainWindow)
    {
        return targets;
    }

    targets.pMenuView = pUI->addMenu(QCoreApplication::translate("MainWindow", "View"),
                                      wy3dApp::MenuBarNames::View);
    assert(targets.pMenuView);

    QMenu* pMenuFile = pMainWindow->findChild<QMenu*>(wy3dApp::MenuBarNames::File);
    assert(pMenuFile);
    if (pMenuFile)
        pUI->insertMenuAfter(pMenuFile, targets.pMenuView);

    targets.pMenuEdit = pUI->addMenu(QCoreApplication::translate("MainWindow", "Edit"),
                                      wy3dApp::MenuBarNames::Edit);
    assert(targets.pMenuEdit);
    if (pMenuFile)
        pUI->insertMenuAfter(pMenuFile, targets.pMenuEdit);

    targets.pMenuTools = pMainWindow->findChild<QMenu*>(wy3dApp::MenuBarNames::Tools);
    assert(targets.pMenuTools);

    return targets;
}

UndoRedoActions createUndoRedoActions(EnvironmentUI* pUI)
{
    assert(pUI);

    UndoRedoActions actions = {};
    actions.pActionUndo = pUI->newCommandAction(
        CommandNames::Undo,
        QCoreApplication::translate("MainWindow", "Undo"),
        IconUtil::get(":/images/Basic_Undo.svg"));
    actions.pActionUndo->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Z));
    actions.pActionUndo->setShortcutContext(Qt::ApplicationShortcut);

    actions.pActionRedo = pUI->newCommandAction(
        CommandNames::Redo,
        QCoreApplication::translate("MainWindow", "Redo"),
        IconUtil::get(":/images/Basic_Redo.svg"));
    actions.pActionRedo->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Y));
    actions.pActionRedo->setShortcutContext(Qt::ApplicationShortcut);

    return actions;
}

EditActions createEditActions(EnvironmentUI* pUI, QActionGroup* pActionGroup)
{
    assert(pUI);
    assert(pActionGroup);

    EditActions actions = {};
    actions.pActionCopy = pUI->newCommandAction(
        CommandNames::Copy,
        QCoreApplication::translate("MainWindow", "Copy"),
        IconUtil::get(":/images/Edit_Copy.svg"),
        pActionGroup);
    if (actions.pActionCopy)
    {
        actions.pActionCopy->setShortcut(QKeySequence::fromString("Shift+C,O"));
    }

    actions.pActionMove = pUI->newCommandAction(
        CommandNames::Move,
        QCoreApplication::translate("MainWindow", "Move"),
        IconUtil::get(":/images/Edit_Move.svg"),
        pActionGroup);
    if (actions.pActionMove)
    {
        actions.pActionMove->setShortcut(QKeySequence::fromString("M,O"));
    }

    actions.pActionRotate = pUI->newCommandAction(
        CommandNames::Rotate,
        QCoreApplication::translate("MainWindow", "Rotate"),
        IconUtil::get(":/images/Edit_Rotate.svg"),
        pActionGroup);
    if (actions.pActionRotate)
    {
        actions.pActionRotate->setShortcut(QKeySequence::fromString("R,O"));
    }

    actions.pActionSketchMirror = pUI->newCommandAction(
        CommandNames::SketchMirror,
        QCoreApplication::translate("MainWindow", "Mirror"),
        IconUtil::get(":/images/Sketch_Mirror.svg"),
        pActionGroup);
    if (actions.pActionSketchMirror)
    {
        actions.pActionSketchMirror->setShortcut(QKeySequence::fromString("M,I"));
    }

    actions.pActionSketchScale = pUI->newCommandAction(
        CommandNames::SketchScale,
        QCoreApplication::translate("MainWindow", "Scale"),
        IconUtil::get(":/images/Sketch_Scale.svg"),
        pActionGroup);
    if (actions.pActionSketchScale)
    {
        actions.pActionSketchScale->setShortcut(QKeySequence::fromString("S,C"));
    }

    actions.pActionTrim = pUI->newCommandAction(
        CommandNames::Trim,
        QCoreApplication::translate("MainWindow", "Trim"),
        IconUtil::get(":/images/Sketch_Trim.svg"),
        pActionGroup);
    if (actions.pActionTrim)
    {
        actions.pActionTrim->setShortcut(QKeySequence::fromString("T,R"));
    }

    actions.pActionExtend = pUI->newCommandAction(
        CommandNames::Extend,
        QCoreApplication::translate("MainWindow", "Extend"),
        IconUtil::get(":/images/Sketch_Extend.svg"),
        pActionGroup);
    if (actions.pActionExtend)
    {
        actions.pActionExtend->setShortcut(QKeySequence::fromString("E,X"));
    }

    actions.pActionSketchFillet = pUI->newCommandAction(
        CommandNames::SketchFillet,
        QCoreApplication::translate("MainWindow", "Sketch Fillet"),
        IconUtil::get(":/images/Sketch_Fillet.svg"),
        pActionGroup);
    if (actions.pActionSketchFillet)
    {
        actions.pActionSketchFillet->setShortcut(QKeySequence::fromString("F,I"));
    }

    actions.pActionSketchChamfer = pUI->newCommandAction(
        CommandNames::SketchChamfer,
        QCoreApplication::translate("MainWindow", "Sketch Chamfer"),
        IconUtil::get(":/images/Sketch_Chamfer.svg"),
        pActionGroup);
    if (actions.pActionSketchChamfer)
    {
        actions.pActionSketchChamfer->setShortcut(QKeySequence::fromString("Shift+C,H"));
    }

    actions.pActionSketchOffset = pUI->newCommandAction(
        CommandNames::SketchOffset,
        QCoreApplication::translate("MainWindow", "Sketch Offset"),
        IconUtil::get(":/images/Sketch_Offset.svg"),
        pActionGroup);
    if (actions.pActionSketchOffset)
    {
        actions.pActionSketchOffset->setShortcut(QKeySequence::fromString("O,F"));
    }

    actions.pActionSketchRectArray = pUI->newCommandAction(
        CommandNames::SketchRectArray,
        QCoreApplication::translate("MainWindow", "Sketch Rect Array"),
        IconUtil::get(":/images/Sketch_RectArray.svg"),
        pActionGroup);
    if (actions.pActionSketchRectArray)
    {
        actions.pActionSketchRectArray->setShortcut(QKeySequence::fromString("Shift+A,R"));
    }

    actions.pActionSketchPolarArray = pUI->newCommandAction(
        CommandNames::SketchPolarArray,
        QCoreApplication::translate("MainWindow", "Sketch Polar Array"),
        IconUtil::get(":/images/Sketch_PolarArray.svg"),
        pActionGroup);

    return actions;
}

SketchActions createSketchActions(EnvironmentUI* pUI, QActionGroup* pActionGroup)
{
    assert(pUI);
    assert(pActionGroup);

    SketchActions actions = {};
    actions.pActionSelect = pUI->newCommandAction(
        CommandNames::Select,
        QCoreApplication::translate("MainWindow", "Select"),
        IconUtil::get(":/images/Basic_Select.svg"),
        pActionGroup);

    actions.pActionDrawPoint = pUI->newCommandAction(
        CommandNames::Point,
        QCoreApplication::translate("MainWindow", "Point"),
        IconUtil::get(":/images/Sketch_DrawPoint.svg"),
        pActionGroup);
    if (actions.pActionDrawPoint)
    {
        actions.pActionDrawPoint->setShortcut(QKeySequence::fromString("P,O,I"));
    }

    actions.pActionDrawLine = pUI->newCommandAction(
        CommandNames::Line,
        QCoreApplication::translate("MainWindow", "Line"),
        IconUtil::get(":/images/Sketch_DrawLine.svg"),
        pActionGroup);
    if (actions.pActionDrawLine)
    {
        actions.pActionDrawLine->setShortcut(QKeySequence(Qt::Key_L));
    }

    actions.pActionDrawLineTangent = pUI->newCommandAction(
        CommandNames::LineTangent,
        QCoreApplication::translate("MainWindow", "Line Tangent"),
        IconUtil::get(":/images/Sketch_DrawLineTangent.svg"),
        pActionGroup);

    actions.pActionDrawCenterLine = pUI->newCommandAction(
        CommandNames::CenterLine,
        QCoreApplication::translate("MainWindow", "Center Line"),
        IconUtil::get(":/images/Sketch_DrawCenterLine.svg"),
        pActionGroup);

    actions.pActionDrawRectangle = pUI->newCommandAction(
        CommandNames::Rectangle,
        QCoreApplication::translate("MainWindow", "Rectangle"),
        IconUtil::get(":/images/Sketch_DrawRectangle.svg"),
        pActionGroup);
    if (actions.pActionDrawRectangle)
    {
        actions.pActionDrawRectangle->setShortcut(QKeySequence::fromString("R,E,C"));
    }

    actions.pActionDrawCenterRectangle = pUI->newCommandAction(
        CommandNames::CenterRectangle,
        QCoreApplication::translate("MainWindow", "CenterRectangle"),
        IconUtil::get(":/images/Sketch_DrawCenterRectangle.svg"),
        pActionGroup);

    actions.pActionDrawPolygon = pUI->newCommandAction(
        CommandNames::Polygon,
        QCoreApplication::translate("MainWindow", "Polygon"),
        IconUtil::get(":/images/Sketch_DrawPolygon.svg"),
        pActionGroup);
    if (actions.pActionDrawPolygon)
    {
        actions.pActionDrawPolygon->setShortcut(QKeySequence::fromString("P,O,L"));
    }

    actions.pActionDrawCircle = pUI->newCommandAction(
        CommandNames::Circle,
        QCoreApplication::translate("MainWindow", "Circle"),
        IconUtil::get(":/images/Sketch_DrawCircle.svg"),
        pActionGroup);
    if (actions.pActionDrawCircle)
    {
        actions.pActionDrawCircle->setShortcut(QKeySequence(Qt::Key_C));
    }

    actions.pActionDrawCircleBy3Points = pUI->newCommandAction(
        CommandNames::CircleBy3Points,
        QCoreApplication::translate("MainWindow", "Circle by 3 Points"),
        IconUtil::get(":/images/Sketch_DrawCircleBy3Points.svg"),
        pActionGroup);

    actions.pActionDrawArc = pUI->newCommandAction(
        CommandNames::Arc,
        QCoreApplication::translate("MainWindow", "Arc"),
        IconUtil::get(":/images/Sketch_DrawArc.svg"),
        pActionGroup);
    if (actions.pActionDrawArc)
    {
        actions.pActionDrawArc->setShortcut(QKeySequence(Qt::Key_A));
    }

    actions.pActionDrawArcBy3Points = pUI->newCommandAction(
        CommandNames::ArcBy3Points,
        QCoreApplication::translate("MainWindow", "Arc by 3 Points"),
        IconUtil::get(":/images/Sketch_DrawArcBy3Points.svg"),
        pActionGroup);

    actions.pActionDrawArcTangent = pUI->newCommandAction(
        CommandNames::ArcTangent,
        QCoreApplication::translate("MainWindow", "Tangent Arc"),
        IconUtil::get(":/images/Sketch_DrawTangentArc.svg"),
        pActionGroup);

    actions.pActionDrawEllipse = pUI->newCommandAction(
        CommandNames::Ellipse,
        QCoreApplication::translate("MainWindow", "Ellipse"),
        IconUtil::get(":/images/Sketch_DrawEllipse.svg"),
        pActionGroup);
    if (actions.pActionDrawEllipse)
    {
        actions.pActionDrawEllipse->setShortcut(QKeySequence::fromString("E,L"));
    }

    actions.pActionDrawEllipseArc = pUI->newCommandAction(
        CommandNames::EllipseArc,
        QCoreApplication::translate("MainWindow", "Ellipse Arc"),
        IconUtil::get(":/images/Sketch_DrawEllipseArc.svg"),
        pActionGroup);
    if (actions.pActionDrawEllipseArc)
    {
        actions.pActionDrawEllipseArc->setShortcut(QKeySequence::fromString("E,A"));
    }

    actions.pActionDrawSpline = pUI->newCommandAction(
        CommandNames::Spline,
        QCoreApplication::translate("MainWindow", "Spline"),
        IconUtil::get(":/images/Sketch_DrawSpline.svg"),
        pActionGroup);
    if (actions.pActionDrawSpline)
    {
        actions.pActionDrawSpline->setShortcut(QKeySequence::fromString("S,P,L"));
    }

    actions.pActionDrawStyleSpline = pUI->newCommandAction(
        CommandNames::StyleSpline,
        QCoreApplication::translate("MainWindow", "Style Spline"),
        IconUtil::get(":/images/Sketch_DrawStyleSpline.svg"),
        pActionGroup);

    actions.pActionDrawEquationDrivenSpline = pUI->newCommandAction(
        CommandNames::EquationDrivenSpline,
        QCoreApplication::translate("MainWindow", "Equation Driven Spline"),
        IconUtil::get(":/images/Sketch_DrawSplineFx.svg"),
        pActionGroup);

    actions.pActionSketchText = pUI->newCommandAction(
        CommandNames::SketchText,
        QCoreApplication::translate("MainWindow", "Sketch Text"),
        IconUtil::get(":/images/Sketch_DrawText.svg"),
        pActionGroup);
    if (actions.pActionSketchText)
    {
        actions.pActionSketchText->setShortcut(QKeySequence::fromString("T,E"));
    }

    actions.pActionSketchProject = pUI->newCommandAction(
        CommandNames::SketchProject,
        QCoreApplication::translate("MainWindow", "Project Edge"),
        IconUtil::get(":/images/Sketch_Project.svg"),
        pActionGroup);

    return actions;
}

SketchEnvironmentActions createSketchEnvironmentActions(
    EnvironmentUI* pUI,
    QActionGroup* pActionGroup)
{
    assert(pUI);
    assert(pActionGroup);

    SketchEnvironmentActions actions = {};
    actions.pActionEndSketch = pUI->newCommandAction(
        CommandNames::EndSketch,
        QCoreApplication::translate("MainWindow", "End Sketch"),
        IconUtil::get(":/images/Sketch_OK.svg"),
        pActionGroup);

    actions.pActionCancelSketch = pUI->newCommandAction(
        CommandNames::CancelSketch,
        QCoreApplication::translate("MainWindow", "Cancel Sketch"),
        IconUtil::get(":/images/Sketch_Cancel.svg"),
        pActionGroup);

    actions.pActionRelocateSketchCsys = pUI->newCommandAction(
        CommandNames::RelocateSketchCSYS,
        QCoreApplication::translate("MainWindow", "Relocate Sketch CSYS"),
        IconUtil::get(":/images/Sketch_CSYS.svg"),
        pActionGroup);

    return actions;
}

ViewActions createViewActions(EnvironmentUI* pUI, QActionGroup* pActionGroup)
{
    assert(pUI);
    assert(pActionGroup);

    ViewActions actions = {};
    actions.pActionFitView = pUI->newCommandAction(
        CommandNames::FitView,
        QCoreApplication::translate("MainWindow", "Fit View"),
        IconUtil::get(":/images/View_FullScreen.svg"),
        pActionGroup);

    actions.pActionFitSelection = pUI->newCommandAction(
        CommandNames::FitSelection,
        QCoreApplication::translate("MainWindow", "Fit Selection"),
        IconUtil::get(":/images/View_FitSelection.svg"),
        pActionGroup);

    actions.pActionIsometricView = pUI->newCommandAction(
        CommandNames::IsometricView,
        QCoreApplication::translate("MainWindow", "Isometric View"),
        IconUtil::get(":/images/View_ISO.svg"));

    actions.pActionFrontView = pUI->newCommandAction(
        CommandNames::FrontView,
        QCoreApplication::translate("MainWindow", "Front View"),
        IconUtil::get(":/images/View_Front.svg"));

    actions.pActionBackView = pUI->newCommandAction(
        CommandNames::BackView,
        QCoreApplication::translate("MainWindow", "Back View"),
        IconUtil::get(":/images/View_Back.svg"));

    actions.pActionLeftView = pUI->newCommandAction(
        CommandNames::LeftView,
        QCoreApplication::translate("MainWindow", "Left View"),
        IconUtil::get(":/images/View_Left.svg"));

    actions.pActionRightView = pUI->newCommandAction(
        CommandNames::RightView,
        QCoreApplication::translate("MainWindow", "Right View"),
        IconUtil::get(":/images/View_Right.svg"));

    actions.pActionTopView = pUI->newCommandAction(
        CommandNames::TopView,
        QCoreApplication::translate("MainWindow", "Top View"),
        IconUtil::get(":/images/View_Top.svg"));

    actions.pActionBottomView = pUI->newCommandAction(
        CommandNames::BottomView,
        QCoreApplication::translate("MainWindow", "Bottom View"),
        IconUtil::get(":/images/View_Bottom.svg"));

    actions.pActionOrientToSketch = pUI->newCommandAction(
        CommandNames::OrientToSketch,
        QCoreApplication::translate("MainWindow", "Orient to Sketch"),
        IconUtil::get(":/images/View_Normal.svg"));

    actions.pActionShadedWithEdgesDisplay = pUI->newCommandAction(
        CommandNames::ShadedWithEdgesDisplay,
        QCoreApplication::translate("MainWindow", "Shaded with Edges"),
        IconUtil::get(":/images/View_ShadedWithEdges.svg"));

    actions.pActionShadedDisplay = pUI->newCommandAction(
        CommandNames::ShadedDisplay,
        QCoreApplication::translate("MainWindow", "Shaded"),
        IconUtil::get(":/images/View_Shaded.svg"));

    actions.pActionWireframeDisplay = pUI->newCommandAction(
        CommandNames::WireframeDisplay,
        QCoreApplication::translate("MainWindow", "Wireframe"),
        IconUtil::get(":/images/View_Wireframe.svg"));

    return actions;
}

UtilityActions createUtilityActions(EnvironmentUI* pUI, QActionGroup* pActionGroup)
{
    assert(pUI);
    assert(pActionGroup);

    UtilityActions actions = {};
    actions.pActionFindElementById = pUI->newCommandAction(
        CommandNames::FindElementById,
        QCoreApplication::translate("MainWindow", "Find"),
        IconUtil::get(":/images/Utility_FindElementById.svg"),
        pActionGroup);
    actions.pActionFindElementById->setShortcut(QKeySequence::Find);
    actions.pActionFindElementById->setShortcutContext(Qt::ApplicationShortcut);

    return actions;
}

void buildEditMenuUi(
    const UndoRedoActions& undoRedoActions,
    QMenu* pMenuEdit)
{
    assert(pMenuEdit);

    pMenuEdit->addAction(undoRedoActions.pActionUndo);
    pMenuEdit->addAction(undoRedoActions.pActionRedo);
}

void buildEditToolBarUi(
    const UndoRedoActions& undoRedoActions,
    QToolBar* pToolBarEdit)
{
    assert(pToolBarEdit);

    pToolBarEdit->addAction(undoRedoActions.pActionUndo);
    pToolBarEdit->addAction(undoRedoActions.pActionRedo);
}

void buildSketchEditToolBarUi(
    EnvironmentUI* pUI,
    QActionGroup* pActionGroup,
    const EditActions& actions,
    QToolBar* pToolBarSketchEdit)
{
    assert(pUI);
    assert(pActionGroup);
    assert(pToolBarSketchEdit);

    pToolBarSketchEdit->addAction(actions.pActionCopy);
    pToolBarSketchEdit->addAction(actions.pActionMove);
    pToolBarSketchEdit->addAction(actions.pActionRotate);
    pToolBarSketchEdit->addAction(actions.pActionSketchMirror);
    pToolBarSketchEdit->addAction(actions.pActionSketchScale);

    std::list<QAction*> trimSeriesActions;
    trimSeriesActions.emplace_back(actions.pActionTrim);
    trimSeriesActions.emplace_back(actions.pActionExtend);
    QToolButton* pToolBtnTrimSeries = pUI->newMenuPopupToolButton(
        pToolBarSketchEdit,
        QCoreApplication::translate("MainWindow", "Trim Series"),
        pActionGroup,
        trimSeriesActions);
    pToolBarSketchEdit->addWidget(pToolBtnTrimSeries);

    std::list<QAction*> filletSeriesActions;
    filletSeriesActions.emplace_back(actions.pActionSketchFillet);
    filletSeriesActions.emplace_back(actions.pActionSketchChamfer);
    QToolButton* pToolBtnFilletSeries = pUI->newMenuPopupToolButton(
        pToolBarSketchEdit,
        QCoreApplication::translate("MainWindow", "Fillet Series"),
        pActionGroup,
        filletSeriesActions);
    pToolBarSketchEdit->addWidget(pToolBtnFilletSeries);

    pToolBarSketchEdit->addAction(actions.pActionSketchOffset);

    std::list<QAction*> arraySeriesActions;
    arraySeriesActions.emplace_back(actions.pActionSketchRectArray);
    arraySeriesActions.emplace_back(actions.pActionSketchPolarArray);
    QToolButton* pToolBtnArraySeries = pUI->newMenuPopupToolButton(
        pToolBarSketchEdit,
        QCoreApplication::translate("MainWindow", "Array Series"),
        pActionGroup,
        arraySeriesActions);
    pToolBarSketchEdit->addWidget(pToolBtnArraySeries);
}

void buildSketchToolBarUi(
    EnvironmentUI* pUI,
    QActionGroup* pActionGroup,
    const SketchActions& actions,
    QToolBar* pToolBarSketch)
{
    assert(pUI);
    assert(pActionGroup);
    assert(pToolBarSketch);

    pToolBarSketch->addAction(actions.pActionSelect);
    pToolBarSketch->addAction(actions.pActionDrawPoint);

    std::list<QAction*> lineSeriesActions;
    lineSeriesActions.emplace_back(actions.pActionDrawLine);
    lineSeriesActions.emplace_back(actions.pActionDrawLineTangent);
    lineSeriesActions.emplace_back(actions.pActionDrawCenterLine);
    QToolButton* pToolBtnLineSeries = pUI->newMenuPopupToolButton(
        pToolBarSketch,
        QCoreApplication::translate("MainWindow", "Line Series"),
        pActionGroup,
        lineSeriesActions);
    pToolBarSketch->addWidget(pToolBtnLineSeries);

    std::list<QAction*> rectangleSeriesActions;
    rectangleSeriesActions.emplace_back(actions.pActionDrawRectangle);
    rectangleSeriesActions.emplace_back(actions.pActionDrawCenterRectangle);
    QToolButton* pToolBtnRectangleSeries = pUI->newMenuPopupToolButton(
        pToolBarSketch,
        QCoreApplication::translate("MainWindow", "Rectangle Series"),
        pActionGroup,
        rectangleSeriesActions);
    pToolBarSketch->addWidget(pToolBtnRectangleSeries);

    pToolBarSketch->addAction(actions.pActionDrawPolygon);

    std::list<QAction*> circleSeriesActions;
    circleSeriesActions.emplace_back(actions.pActionDrawCircle);
    circleSeriesActions.emplace_back(actions.pActionDrawCircleBy3Points);
    QToolButton* pToolBtnCircleSeries = pUI->newMenuPopupToolButton(
        pToolBarSketch,
        QCoreApplication::translate("MainWindow", "Circle Series"),
        pActionGroup,
        circleSeriesActions);
    pToolBarSketch->addWidget(pToolBtnCircleSeries);

    std::list<QAction*> arcSeriesActions;
    arcSeriesActions.emplace_back(actions.pActionDrawArc);
    arcSeriesActions.emplace_back(actions.pActionDrawArcBy3Points);
    arcSeriesActions.emplace_back(actions.pActionDrawArcTangent);
    QToolButton* pToolBtnArcSeries = pUI->newMenuPopupToolButton(
        pToolBarSketch,
        QCoreApplication::translate("MainWindow", "Arc Series"),
        pActionGroup,
        arcSeriesActions);
    pToolBarSketch->addWidget(pToolBtnArcSeries);

    std::list<QAction*> ellipseSeriesActions;
    ellipseSeriesActions.emplace_back(actions.pActionDrawEllipse);
    ellipseSeriesActions.emplace_back(actions.pActionDrawEllipseArc);
    QToolButton* pToolBtnEllipseSeries = pUI->newMenuPopupToolButton(
        pToolBarSketch,
        QCoreApplication::translate("MainWindow", "Ellipse Series"),
        pActionGroup,
        ellipseSeriesActions);
    pToolBarSketch->addWidget(pToolBtnEllipseSeries);

    std::list<QAction*> splineSeriesActions;
    splineSeriesActions.emplace_back(actions.pActionDrawSpline);
    splineSeriesActions.emplace_back(actions.pActionDrawStyleSpline);
    splineSeriesActions.emplace_back(actions.pActionDrawEquationDrivenSpline);
    QToolButton* pToolBtnSplineSeries = pUI->newMenuPopupToolButton(
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

// The viewport of the document being sketched. findChild<QOpenGLWidget*>() would answer with the
// first document's viewport instead whenever more than one document is open.
QWidget* findActiveViewWidget()
{
    MainWindow* pMainWindow = ApplicationImpl::instance().getMainWindow();
    wyap::Document* pActiveDoc = ApplicationImpl::instance().getActiveDocument();
    if (!pMainWindow || !pActiveDoc) return nullptr;

    ViewWidgetContainer* pViewWidgetContainer = pMainWindow->getViewWidgetContainer();
    if (!pViewWidgetContainer) return nullptr;

    return pViewWidgetContainer->getViewWidget(pActiveDoc);
}

ViewportOverlayBar* buildOverlayBarUi(
    QWidget* pViewWidget,
    const SketchEnvironmentActions& actions)
{
    assert(pViewWidget);
    assert(actions.pActionEndSketch);
    assert(actions.pActionCancelSketch);

    ViewportOverlayBar* pBar = new ViewportOverlayBar(pViewWidget);
    pBar->addAction(actions.pActionCancelSketch);
    pBar->addAction(actions.pActionEndSketch);
    pBar->show();
    return pBar;
}

void buildViewToolBarUi(
    EnvironmentUI* pUI,
    const ViewActions& actions,
    QToolBar* pToolBarView)
{
    assert(pUI);
    assert(pToolBarView);

    pToolBarView->addAction(actions.pActionFitView);
    pToolBarView->addAction(actions.pActionFitSelection);
    pToolBarView->addAction(actions.pActionOrientToSketch);

    std::list<QAction*> standardViewActions;
    standardViewActions.emplace_back(actions.pActionIsometricView);
    standardViewActions.emplace_back(actions.pActionFrontView);
    standardViewActions.emplace_back(actions.pActionBackView);
    standardViewActions.emplace_back(actions.pActionLeftView);
    standardViewActions.emplace_back(actions.pActionRightView);
    standardViewActions.emplace_back(actions.pActionTopView);
    standardViewActions.emplace_back(actions.pActionBottomView);
    QToolButton* pToolBtnStandardView = pUI->newMenuPopupToolButton(
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

    QActionGroup* pDisplayModeGroup = pUI->newActionGroup();
    QToolButton* pToolBtn = pUI->newMenuPopupToolButton(
        pToolBarView,
        QCoreApplication::translate("MainWindow", "Display Mode"),
        pDisplayModeGroup,
        displayModeActions);
    pToolBarView->addWidget(pToolBtn);
}

void buildUtilityToolBarUi(const UtilityActions& actions, QToolBar* pToolBarUtility)
{
    assert(pToolBarUtility);

    pToolBarUtility->addAction(actions.pActionFindElementById);
}

void buildViewMenuUi(const ViewActions& actions, QMenu* pMenuView)
{
    assert(pMenuView);

    pMenuView->addAction(actions.pActionFitView);
    pMenuView->addAction(actions.pActionFitSelection);
    pMenuView->addAction(actions.pActionOrientToSketch);
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

        pMenuTools->insertAction(pAnchor, actions.pActionFindElementById);

        QAction* pSeparator = new QAction(actions.pActionFindElementById);
        pSeparator->setSeparator(true);
        pMenuTools->insertAction(pAnchor, pSeparator);
    }
    else
    {
        pMenuTools->addAction(pActionSelect);
        pMenuTools->addAction(actions.pActionFindElementById);
    }
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

    this->setEnvironment(pEnv);

    const UiTargets uiTargets = createUiTargets(this);
    QActionGroup* pActionGroup = this->newActionGroup();
    const UndoRedoActions undoRedoActions = createUndoRedoActions(this);
    const EditActions editActions = createEditActions(this, pActionGroup);
    const SketchActions sketchActions = createSketchActions(this, pActionGroup);
    const SketchEnvironmentActions sketchEnvironmentActions =
        createSketchEnvironmentActions(this, pActionGroup);
    const ViewActions viewActions = createViewActions(this, pActionGroup);
    const UtilityActions utilityActions = createUtilityActions(this, pActionGroup);

    buildEditToolBarUi(undoRedoActions, uiTargets.pToolBarEdit);
    buildSketchEditToolBarUi(
        this,
        pActionGroup,
        editActions,
        uiTargets.pToolBarSketchEdit);
    buildSketchToolBarUi(
        this,
        pActionGroup,
        sketchActions,
        uiTargets.pToolBarSketch);
    buildSketchEnvironmentToolBarUi(
        sketchEnvironmentActions,
        uiTargets.pToolBarSketchEnvironment);
    buildUtilityToolBarUi(utilityActions, uiTargets.pToolBarUtility);
    buildViewToolBarUi(this, viewActions, uiTargets.pToolBarView);

    buildViewMenuUi(viewActions, uiTargets.pMenuView);
    buildEditMenuUi(undoRedoActions, uiTargets.pMenuEdit);
    buildToolsMenuUi(sketchActions.pActionSelect, utilityActions, uiTargets.pMenuTools);

    this->restoreUiState();

    QWidget* pViewWidget = findActiveViewWidget();
    if (pViewWidget)
        _pOverlayBar = buildOverlayBarUi(pViewWidget, sketchEnvironmentActions);
}

void SketchEnvironmentUI::teardown(SketchEnvironment* pEnv)
{
    if (!pEnv)
    {
        assert(false);
        return;
    }

    this->setEnvironment(pEnv);

    // The bar's buttons are bound to command actions and destroyUI() destroys those:
    // the bar goes first or it would be left holding dangling pointers.
    if (_pOverlayBar)
    {
        _pOverlayBar->hide();
        delete _pOverlayBar;
    }

    this->destroyUI();
}

bool SketchEnvironmentUI::addContextMenuActions(ContextMenu& menu)
{
    bool added(false);

    if (CommandAction* pActionOrientToSketch = this->findCommandAction(CommandNames::OrientToSketch))
    {
        menu.addAction(pActionOrientToSketch);
        added = true;
    }

    if (CommandAction* pActionEndSketch = this->findCommandAction(CommandNames::EndSketch))
    {
        menu.addAction(pActionEndSketch);
        added = true;
    }

    if (CommandAction* pActionCancelSketch = this->findCommandAction(CommandNames::CancelSketch))
    {
        menu.addAction(pActionCancelSketch);
        added = true;
    }

    return added;
}

} // namespace wy3dApp
