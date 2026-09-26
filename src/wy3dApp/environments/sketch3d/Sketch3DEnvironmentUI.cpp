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

#include "Sketch3DEnvironmentUI.h"

#include <cassert>
#include <list>

#include <QActionGroup>
#include <QCoreApplication>
#include <QIcon>
#include <QKeySequence>
#include <QToolButton>

#include "Sketch3DEnvironment.h"
#include "application/Application.h"
#include "commands/CommandAction.h"
#include "commands/CommandNames.h"
#include "ui/MenuBarNames.h"
#include "ui/ToolBarNames.h"
#include "widgets/frame/MainWindow.h"
#include "widgets/frame/ViewWidget.h"
#include "widgets/frame/ViewWidgetContainer.h"
#include "widgets/frame/ViewportOverlayBar.h"

namespace
{
struct UiTargets
{
    QMenu* pMenuView;
    QMenu* pMenuEdit;
    QMenu* pMenuTools;
    QToolBar* pToolBarEdit;
    QToolBar* pToolBarSketch3D;
    QToolBar* pToolBarSketch3DEnvironment;
    QToolBar* pToolBarUtility;
    QToolBar* pToolBarView;
};

struct UndoRedoActions
{
    CommandAction* pActionUndo;
    CommandAction* pActionRedo;
};

struct Sketch3DActions
{
    CommandAction* pActionSelect;
    CommandAction* pActionDrawLine3D;
    CommandAction* pActionDrawCircle3D;
    CommandAction* pActionDrawArc3D;
    CommandAction* pActionDrawArcBy3Points3D;
    CommandAction* pActionDrawRectangle3D;
    CommandAction* pActionDrawCenterRectangle3D;
    CommandAction* pActionDrawEllipse3D;
    CommandAction* pActionDrawEllipseArc3D;
    CommandAction* pActionDrawSpline3D;
    CommandAction* pActionDrawStyleSpline3D;
    CommandAction* pActionIncludeCurve3D;
    CommandAction* pActionIntersectionCurve3D;
    CommandAction* pActionProjectCurve3D;
    CommandAction* pActionTrim3D;
    CommandAction* pActionExtend3D;
    CommandAction* pActionFillet3D;
    CommandAction* pActionChamfer3D;
};

struct Sketch3DEnvironmentActions
{
    CommandAction* pActionEndSketch3D;
    CommandAction* pActionCancelSketch3D;
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

struct UtilityActions
{
    CommandAction* pActionFindElementById;
};

UiTargets createUiTargets(Sketch3DEnvironment* pEnv)
{
    assert(pEnv);

    UiTargets targets = {};
    targets.pToolBarEdit = pEnv->addToolBar(
        QCoreApplication::translate("MainWindow", "Edit"),
        wy3dApp::ToolBarNames::Edit);
    assert(targets.pToolBarEdit);

    targets.pToolBarSketch3D = pEnv->addToolBar(
        QCoreApplication::translate("MainWindow", "3D Sketch"),
        wy3dApp::ToolBarNames::Sketch3D);

    targets.pToolBarSketch3DEnvironment = pEnv->addToolBar(
        QCoreApplication::translate("MainWindow", "3D Sketch Environment"),
        wy3dApp::ToolBarNames::Sketch3DEnvironment);

    targets.pToolBarUtility = pEnv->addToolBar(
        QCoreApplication::translate("MainWindow", "Utility"),
        wy3dApp::ToolBarNames::Utility);

    targets.pToolBarView = pEnv->addToolBar(
        QCoreApplication::translate("MainWindow", "View"),
        wy3dApp::ToolBarNames::Sketch3DView);

    MainWindow* pMainWindow = Application::instance().getMainWindow();
    assert(pMainWindow);
    if (!pMainWindow)
    {
        return targets;
    }

    targets.pMenuView = pEnv->addMenu(QCoreApplication::translate("MainWindow", "View"),
                                      wy3dApp::MenuBarNames::View);
    assert(targets.pMenuView);

    QMenu* pMenuFile = pMainWindow->findChild<QMenu*>(wy3dApp::MenuBarNames::File);
    assert(pMenuFile);
    if (pMenuFile)
        pEnv->insertMenuAfter(pMenuFile, targets.pMenuView);

    targets.pMenuEdit = pEnv->addMenu(QCoreApplication::translate("MainWindow", "Edit"),
                                      wy3dApp::MenuBarNames::Edit);
    assert(targets.pMenuEdit);
    if (pMenuFile)
        pEnv->insertMenuAfter(pMenuFile, targets.pMenuEdit);

    targets.pMenuTools = pMainWindow->findChild<QMenu*>(wy3dApp::MenuBarNames::Tools);
    assert(targets.pMenuTools);

    return targets;
}

UndoRedoActions createUndoRedoActions(Sketch3DEnvironment* pEnv)
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

Sketch3DActions createSketch3DActions(Sketch3DEnvironment* pEnv, QActionGroup* pActionGroup)
{
    assert(pEnv);
    assert(pActionGroup);

    Sketch3DActions actions = {};
    actions.pActionSelect = pEnv->newCommandAction(
        CommandNames::Select,
        QCoreApplication::translate("MainWindow", "Select"),
        QIcon(":/images/Basic_Select.svg"),
        pActionGroup);

    actions.pActionDrawLine3D = pEnv->newCommandAction(
        CommandNames::Line3D,
        QCoreApplication::translate("MainWindow", "Line"),
        QIcon(":/images/Sketch_DrawLine.svg"),
        pActionGroup);
    if (actions.pActionDrawLine3D)
    {
        actions.pActionDrawLine3D->setShortcut(QKeySequence(Qt::Key_L));
    }

    actions.pActionDrawCircle3D = pEnv->newCommandAction(
        CommandNames::Circle3D,
        QCoreApplication::translate("MainWindow", "Circle"),
        QIcon(":/images/Sketch_DrawCircle.svg"),
        pActionGroup);
    if (actions.pActionDrawCircle3D)
    {
        actions.pActionDrawCircle3D->setShortcut(QKeySequence(Qt::Key_C));
    }

    actions.pActionDrawArc3D = pEnv->newCommandAction(
        CommandNames::Arc3D,
        QCoreApplication::translate("MainWindow", "Arc"),
        QIcon(":/images/Sketch_DrawArc.svg"),
        pActionGroup);
    if (actions.pActionDrawArc3D)
    {
        actions.pActionDrawArc3D->setShortcut(QKeySequence(Qt::Key_A));
    }

    actions.pActionDrawArcBy3Points3D = pEnv->newCommandAction(
        CommandNames::ArcBy3Points3D,
        QCoreApplication::translate("MainWindow", "Arc by 3 Points"),
        QIcon(":/images/Sketch_DrawArcBy3Points.svg"),
        pActionGroup);
    if (actions.pActionDrawArcBy3Points3D)
    {
        actions.pActionDrawArcBy3Points3D->setShortcut(QKeySequence(Qt::Key_D));
    }

    actions.pActionDrawRectangle3D = pEnv->newCommandAction(
        CommandNames::Rectangle3D,
        QCoreApplication::translate("MainWindow", "Rectangle"),
        QIcon(":/images/Sketch_DrawRectangle.svg"),
        pActionGroup);
    if (actions.pActionDrawRectangle3D)
    {
        actions.pActionDrawRectangle3D->setShortcut(QKeySequence(Qt::Key_R));
    }

    actions.pActionDrawCenterRectangle3D = pEnv->newCommandAction(
        CommandNames::CenterRectangle3D,
        QCoreApplication::translate("MainWindow", "CenterRectangle"),
        QIcon(":/images/Sketch_DrawCenterRectangle.svg"),
        pActionGroup);

    actions.pActionDrawEllipse3D = pEnv->newCommandAction(
        CommandNames::Ellipse3D,
        QCoreApplication::translate("MainWindow", "Ellipse"),
        QIcon(":/images/Sketch_DrawEllipse.svg"),
        pActionGroup);
    if (actions.pActionDrawEllipse3D)
    {
        actions.pActionDrawEllipse3D->setShortcut(QKeySequence(Qt::Key_E));
    }

    actions.pActionDrawEllipseArc3D = pEnv->newCommandAction(
        CommandNames::EllipseArc3D,
        QCoreApplication::translate("MainWindow", "Ellipse Arc"),
        QIcon(":/images/Sketch_DrawEllipseArc.svg"),
        pActionGroup);

    actions.pActionDrawSpline3D = pEnv->newCommandAction(
        CommandNames::Spline3D,
        QCoreApplication::translate("MainWindow", "Spline"),
        QIcon(":/images/Sketch_DrawSpline.svg"),
        pActionGroup);
    if (actions.pActionDrawSpline3D)
    {
        actions.pActionDrawSpline3D->setShortcut(QKeySequence(Qt::Key_S));
    }

    actions.pActionDrawStyleSpline3D = pEnv->newCommandAction(
        CommandNames::StyleSpline3D,
        QCoreApplication::translate("MainWindow", "Style Spline"),
        QIcon(":/images/Sketch_DrawStyleSpline.svg"),
        pActionGroup);

    actions.pActionIncludeCurve3D = pEnv->newCommandAction(
        CommandNames::IncludeCurve3D,
        QCoreApplication::translate("MainWindow", "Include Curve"),
        QIcon(":/images/Sketch_IncludeCurve.svg"),
        pActionGroup);
    if (actions.pActionIncludeCurve3D)
    {
        actions.pActionIncludeCurve3D->setShortcut(QKeySequence(Qt::Key_I));
    }

    actions.pActionIntersectionCurve3D = pEnv->newCommandAction(
        CommandNames::IntersectionCurve3D,
        QCoreApplication::translate("MainWindow", "Intersection Curve"),
        QIcon(":/images/Sketch_IntersectionCurve.png"),
        pActionGroup);
    if (actions.pActionIntersectionCurve3D)
    {
        actions.pActionIntersectionCurve3D->setShortcut(QKeySequence(Qt::Key_X));
    }

    actions.pActionProjectCurve3D = pEnv->newCommandAction(
        CommandNames::ProjectCurve3D,
        QCoreApplication::translate("MainWindow", "Project Curve"),
        QIcon(":/images/Sketch_ProjectCurve.png"),
        pActionGroup);
    if (actions.pActionProjectCurve3D)
    {
        actions.pActionProjectCurve3D->setShortcut(QKeySequence(Qt::Key_P));
    }

    actions.pActionTrim3D = pEnv->newCommandAction(
        CommandNames::Trim3D,
        QCoreApplication::translate("MainWindow", "Trim"),
        QIcon(":/images/Sketch_Trim.svg"),
        pActionGroup);
    if (actions.pActionTrim3D)
    {
        actions.pActionTrim3D->setShortcut(QKeySequence::fromString("T,R"));
    }

    actions.pActionExtend3D = pEnv->newCommandAction(
        CommandNames::Extend3D,
        QCoreApplication::translate("MainWindow", "Extend"),
        QIcon(":/images/Sketch_Extend.svg"),
        pActionGroup);
    if (actions.pActionExtend3D)
    {
        actions.pActionExtend3D->setShortcut(QKeySequence::fromString("T,E"));
    }

    actions.pActionFillet3D = pEnv->newCommandAction(
        CommandNames::Fillet3D,
        QCoreApplication::translate("MainWindow", "Sketch Fillet"),
        QIcon(":/images/Sketch_Fillet.svg"),
        pActionGroup);
    if (actions.pActionFillet3D)
    {
        actions.pActionFillet3D->setShortcut(QKeySequence::fromString("F,I"));
    }

    actions.pActionChamfer3D = pEnv->newCommandAction(
        CommandNames::Chamfer3D,
        QCoreApplication::translate("MainWindow", "Sketch Chamfer"),
        QIcon(":/images/Sketch_Chamfer.svg"),
        pActionGroup);
    if (actions.pActionChamfer3D)
    {
        actions.pActionChamfer3D->setShortcut(QKeySequence::fromString("Shift+C,H"));
    }

    return actions;
}

Sketch3DEnvironmentActions createSketch3DEnvironmentActions(
    Sketch3DEnvironment* pEnv,
    QActionGroup* pActionGroup)
{
    assert(pEnv);
    assert(pActionGroup);

    Sketch3DEnvironmentActions actions = {};
    actions.pActionEndSketch3D = pEnv->newCommandAction(
        CommandNames::EndSketch3D,
        QCoreApplication::translate("MainWindow", "End 3D Sketch"),
        QIcon(":/images/Sketch_OK.svg"),
        pActionGroup);

    actions.pActionCancelSketch3D = pEnv->newCommandAction(
        CommandNames::CancelSketch3D,
        QCoreApplication::translate("MainWindow", "Cancel 3D Sketch"),
        QIcon(":/images/Sketch_Cancel.svg"),
        pActionGroup);

    return actions;
}

ViewActions createViewActions(Sketch3DEnvironment* pEnv, QActionGroup* pActionGroup)
{
    assert(pEnv);
    assert(pActionGroup);

    ViewActions actions = {};
    actions.pActionFitView = pEnv->newCommandAction(
        CommandNames::FitView,
        QCoreApplication::translate("MainWindow", "Fit View"),
        QIcon(":/images/View_FullScreen.svg"),
        pActionGroup);

    actions.pActionFitSelection = pEnv->newCommandAction(
        CommandNames::FitSelection,
        QCoreApplication::translate("MainWindow", "Fit Selection"),
        QIcon(":/images/View_FitSelection.svg"),
        pActionGroup);

    actions.pActionIsometricView = pEnv->newCommandAction(
        CommandNames::IsometricView,
        QCoreApplication::translate("MainWindow", "Isometric View"),
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

    actions.pActionShadedWithEdgesDisplay = pEnv->newCommandAction(
        CommandNames::ShadedWithEdgesDisplay,
        QCoreApplication::translate("MainWindow", "Shaded with Edges"),
        QIcon(":/images/View_ShadedWithEdges.svg"));

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

UtilityActions createUtilityActions(Sketch3DEnvironment* pEnv, QActionGroup* pActionGroup)
{
    assert(pEnv);
    assert(pActionGroup);

    UtilityActions actions = {};
    actions.pActionFindElementById = pEnv->newCommandAction(
        CommandNames::FindElementById,
        QCoreApplication::translate("MainWindow", "Find"),
        QIcon(":/images/Utility_FindElementById.svg"),
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

void buildSketch3DToolBarUi(
    Sketch3DEnvironment* pEnv,
    QActionGroup* pActionGroup,
    const Sketch3DActions& actions,
    QToolBar* pToolBarSketch3D)
{
    assert(pEnv);
    assert(pActionGroup);
    assert(pToolBarSketch3D);

    pToolBarSketch3D->addAction(actions.pActionSelect);
    pToolBarSketch3D->addAction(actions.pActionDrawLine3D);
    pToolBarSketch3D->addAction(actions.pActionDrawCircle3D);

    std::list<QAction*> arcSeriesActions;
    arcSeriesActions.emplace_back(actions.pActionDrawArc3D);
    arcSeriesActions.emplace_back(actions.pActionDrawArcBy3Points3D);
    QToolButton* pToolBtnArcSeries = pEnv->newMenuPopupToolButton(
        pToolBarSketch3D,
        QCoreApplication::translate("MainWindow", "Arc Series"),
        pActionGroup,
        arcSeriesActions);
    pToolBarSketch3D->addWidget(pToolBtnArcSeries);

    std::list<QAction*> rectangleSeriesActions;
    rectangleSeriesActions.emplace_back(actions.pActionDrawRectangle3D);
    rectangleSeriesActions.emplace_back(actions.pActionDrawCenterRectangle3D);
    QToolButton* pToolBtnRectangleSeries = pEnv->newMenuPopupToolButton(
        pToolBarSketch3D,
        QCoreApplication::translate("MainWindow", "Rectangle Series"),
        pActionGroup,
        rectangleSeriesActions);
    pToolBarSketch3D->addWidget(pToolBtnRectangleSeries);

    std::list<QAction*> ellipseSeriesActions;
    ellipseSeriesActions.emplace_back(actions.pActionDrawEllipse3D);
    ellipseSeriesActions.emplace_back(actions.pActionDrawEllipseArc3D);
    QToolButton* pToolBtnEllipseSeries = pEnv->newMenuPopupToolButton(
        pToolBarSketch3D,
        QCoreApplication::translate("MainWindow", "Ellipse Series"),
        pActionGroup,
        ellipseSeriesActions);
    pToolBarSketch3D->addWidget(pToolBtnEllipseSeries);

    std::list<QAction*> splineSeriesActions;
    splineSeriesActions.emplace_back(actions.pActionDrawSpline3D);
    splineSeriesActions.emplace_back(actions.pActionDrawStyleSpline3D);
    QToolButton* pToolBtnSplineSeries = pEnv->newMenuPopupToolButton(
        pToolBarSketch3D,
        QCoreApplication::translate("MainWindow", "Spline Series"),
        pActionGroup,
        splineSeriesActions);
    pToolBarSketch3D->addWidget(pToolBtnSplineSeries);

    pToolBarSketch3D->addAction(actions.pActionIncludeCurve3D);
    pToolBarSketch3D->addAction(actions.pActionIntersectionCurve3D);
    pToolBarSketch3D->addAction(actions.pActionProjectCurve3D);
    std::list<QAction*> trimSeriesActions;
    trimSeriesActions.emplace_back(actions.pActionTrim3D);
    trimSeriesActions.emplace_back(actions.pActionExtend3D);
    QToolButton* pToolBtnTrimSeries = pEnv->newMenuPopupToolButton(
        pToolBarSketch3D,
        QCoreApplication::translate("MainWindow", "Trim Series"),
        pActionGroup,
        trimSeriesActions);
    pToolBarSketch3D->addWidget(pToolBtnTrimSeries);

    std::list<QAction*> filletSeriesActions;
    filletSeriesActions.emplace_back(actions.pActionFillet3D);
    filletSeriesActions.emplace_back(actions.pActionChamfer3D);
    QToolButton* pToolBtnFilletSeries = pEnv->newMenuPopupToolButton(
        pToolBarSketch3D,
        QCoreApplication::translate("MainWindow", "Fillet Series"),
        pActionGroup,
        filletSeriesActions);
    pToolBarSketch3D->addWidget(pToolBtnFilletSeries);
}

void buildSketch3DEnvironmentToolBarUi(
    const Sketch3DEnvironmentActions& actions,
    QToolBar* pToolBarSketch3DEnvironment)
{
    assert(pToolBarSketch3DEnvironment);

    pToolBarSketch3DEnvironment->addAction(actions.pActionEndSketch3D);
    pToolBarSketch3DEnvironment->addAction(actions.pActionCancelSketch3D);
}

// The viewport of the document being sketched. findChild<QOpenGLWidget*>() would answer with the
// first document's viewport instead whenever more than one document is open.
QWidget* findActiveViewWidget()
{
    MainWindow* pMainWindow = Application::instance().getMainWindow();
    wyap::Document* pActiveDoc = Application::instance().getActiveDocument();
    if (!pMainWindow || !pActiveDoc) return nullptr;

    ViewWidgetContainer* pViewWidgetContainer = pMainWindow->getViewWidgetContainer();
    if (!pViewWidgetContainer) return nullptr;

    return pViewWidgetContainer->getViewWidget(pActiveDoc);
}

ViewportOverlayBar* buildOverlayBarUi(
    QWidget* pViewWidget,
    const Sketch3DEnvironmentActions& actions)
{
    assert(pViewWidget);
    assert(actions.pActionEndSketch3D);
    assert(actions.pActionCancelSketch3D);

    ViewportOverlayBar* pBar = new ViewportOverlayBar(pViewWidget);
    pBar->addAction(actions.pActionCancelSketch3D);
    pBar->addAction(actions.pActionEndSketch3D);
    pBar->show();
    return pBar;
}

void buildViewToolBarUi(
    Sketch3DEnvironment* pEnv,
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

Sketch3DEnvironmentUI::Sketch3DEnvironmentUI()
{
}

Sketch3DEnvironmentUI::~Sketch3DEnvironmentUI()
{
}

void Sketch3DEnvironmentUI::initialize(Sketch3DEnvironment* pEnv)
{
    if (!pEnv)
    {
        assert(false);
        return;
    }

    const UiTargets uiTargets = createUiTargets(pEnv);
    QActionGroup* pActionGroup = pEnv->newActionGroup();
    const UndoRedoActions undoRedoActions = createUndoRedoActions(pEnv);
    const Sketch3DActions sketch3DActions = createSketch3DActions(pEnv, pActionGroup);
    const Sketch3DEnvironmentActions sketch3DEnvironmentActions =
        createSketch3DEnvironmentActions(pEnv, pActionGroup);
    const ViewActions viewActions = createViewActions(pEnv, pActionGroup);
    const UtilityActions utilityActions = createUtilityActions(pEnv, pActionGroup);

    buildEditToolBarUi(undoRedoActions, uiTargets.pToolBarEdit);
    buildSketch3DToolBarUi(
        pEnv,
        pActionGroup,
        sketch3DActions,
        uiTargets.pToolBarSketch3D);
    buildSketch3DEnvironmentToolBarUi(
        sketch3DEnvironmentActions,
        uiTargets.pToolBarSketch3DEnvironment);
    buildUtilityToolBarUi(utilityActions, uiTargets.pToolBarUtility);
    buildViewToolBarUi(pEnv, viewActions, uiTargets.pToolBarView);

    buildViewMenuUi(viewActions, uiTargets.pMenuView);
    buildEditMenuUi(undoRedoActions, uiTargets.pMenuEdit);
    buildToolsMenuUi(sketch3DActions.pActionSelect, utilityActions, uiTargets.pMenuTools);

    pEnv->restoreUiState();

    QWidget* pViewWidget = findActiveViewWidget();
    if (pViewWidget)
        _pOverlayBar = buildOverlayBarUi(pViewWidget, sketch3DEnvironmentActions);
}

void Sketch3DEnvironmentUI::teardown(Sketch3DEnvironment* pEnv)
{
    if (!pEnv)
    {
        assert(false);
        return;
    }

    // The bar's buttons are bound to command actions and destroyUI() destroys those:
    // the bar goes first or it would be left holding dangling pointers.
    if (_pOverlayBar)
    {
        _pOverlayBar->hide();
        delete _pOverlayBar;
    }

    pEnv->destroyUI();
}
