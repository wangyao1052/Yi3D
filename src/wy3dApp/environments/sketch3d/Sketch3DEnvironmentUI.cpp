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
#include "ui/ToolBarNames.h"
#include "widgets/frame/MainWindow.h"

namespace
{
struct UiTargets
{
    QToolBar* pToolBarBasic;
    QToolBar* pToolBarSketch3D;
    QToolBar* pToolBarSketch3DEnvironment;
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

UiTargets createUiTargets(Sketch3DEnvironment* pEnv)
{
    assert(pEnv);

    UiTargets targets = {};
    targets.pToolBarSketch3D = pEnv->addToolBar(
        QCoreApplication::translate("MainWindow", "3D Sketch"),
        wy3dApp::ToolBarNames::Sketch3D);

    targets.pToolBarSketch3DEnvironment = pEnv->addToolBar(
        QCoreApplication::translate("MainWindow", "3D Sketch Environment"),
        wy3dApp::ToolBarNames::Sketch3DEnvironment);

    targets.pToolBarView = pEnv->addToolBar(
        QCoreApplication::translate("MainWindow", "View"),
        wy3dApp::ToolBarNames::Sketch3DView);

    MainWindow* pMainWindow = Application::instance().getMainWindow();
    assert(pMainWindow);
    if (!pMainWindow)
    {
        return targets;
    }

    targets.pToolBarBasic = pMainWindow->findChild<QToolBar*>(wy3dApp::ToolBarNames::Basic);
    assert(targets.pToolBarBasic);

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
        QCoreApplication::translate("MainWindow", "Line 3D"),
        QIcon(":/images/Sketch_DrawLine.svg"),
        pActionGroup);
    if (actions.pActionDrawLine3D)
    {
        actions.pActionDrawLine3D->setShortcut(QKeySequence(Qt::Key_L));
    }

    actions.pActionDrawCircle3D = pEnv->newCommandAction(
        CommandNames::Circle3D,
        QCoreApplication::translate("MainWindow", "Circle 3D"),
        QIcon(":/images/Sketch_DrawCircle.svg"),
        pActionGroup);
    if (actions.pActionDrawCircle3D)
    {
        actions.pActionDrawCircle3D->setShortcut(QKeySequence(Qt::Key_C));
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

void buildBasicToolBarUi(
    const UndoRedoActions& undoRedoActions,
    QToolBar* pToolBarBasic)
{
    assert(pToolBarBasic);

    pToolBarBasic->addAction(undoRedoActions.pActionUndo);
    pToolBarBasic->addAction(undoRedoActions.pActionRedo);
}

void buildSketch3DToolBarUi(
    const Sketch3DActions& actions,
    QToolBar* pToolBarSketch3D)
{
    assert(pToolBarSketch3D);

    pToolBarSketch3D->addAction(actions.pActionSelect);
    pToolBarSketch3D->addAction(actions.pActionDrawLine3D);
    pToolBarSketch3D->addAction(actions.pActionDrawCircle3D);
}

void buildSketch3DEnvironmentToolBarUi(
    const Sketch3DEnvironmentActions& actions,
    QToolBar* pToolBarSketch3DEnvironment)
{
    assert(pToolBarSketch3DEnvironment);

    pToolBarSketch3DEnvironment->addAction(actions.pActionEndSketch3D);
    pToolBarSketch3DEnvironment->addAction(actions.pActionCancelSketch3D);
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
    pToolBarView->addAction(actions.pActionIsometricView);
    pToolBarView->addAction(actions.pActionFrontView);
    pToolBarView->addAction(actions.pActionBackView);
    pToolBarView->addAction(actions.pActionLeftView);
    pToolBarView->addAction(actions.pActionRightView);
    pToolBarView->addAction(actions.pActionTopView);
    pToolBarView->addAction(actions.pActionBottomView);

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

    buildBasicToolBarUi(undoRedoActions, uiTargets.pToolBarBasic);
    buildSketch3DToolBarUi(
        sketch3DActions,
        uiTargets.pToolBarSketch3D);
    buildSketch3DEnvironmentToolBarUi(
        sketch3DEnvironmentActions,
        uiTargets.pToolBarSketch3DEnvironment);
    buildViewToolBarUi(pEnv, viewActions, uiTargets.pToolBarView);

    pEnv->restoreUiState();
}

void Sketch3DEnvironmentUI::teardown(Sketch3DEnvironment* pEnv)
{
    if (!pEnv)
    {
        assert(false);
        return;
    }

    pEnv->destroyUI();
}
