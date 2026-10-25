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

#include <wy3dApp/GuiCommand.h>
#include <cassert>

#include <QCoreApplication>
#include <QCursor>

#include <wy3dApp/Application.h>
#include "application/ApplicationImpl.h"
#include <wyVector2.h>
#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapSelManager.h>
#include <wyapGizmoManager.h>
#include <wy3dApp/GizmoFactory.h>
#include "widgets/panels/DockPanelIds.h"
#include "widgets/panels/DockPanelManager.h"
#include "widgets/panels/featureTree/FeatureTreeWidget.h"
#include "widgets/panels/property/PropertyEditorWidget.h"
#include "widgets/frame/MainWindow.h"

#include <wy3dApp/Scene.h>
#include "scene/RenderConst.h"
#include "snap/SnapContext.h"
#include <wy3dApp/EnvironmentUI.h>
#include "environments/IEnvironmentUIHost.h"
#include "snap/SnapTipWidget.h"
#include "commands/GuiEventDispatcher.h"
#include "select/SelectHandler.h"
#include "view/BaseView.h"
#include "select/SelectFilterFunctors.h"
#include <wy3dApp/GuiCommandUtil.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

namespace
{
void onPropertyWidgetToggled(bool checked)
{
    if (checked)
    {
        ApplicationImpl::instance().getDockPanelManager()->showPanel(DockPanelIds::Property);
    }
    else
    {
        ApplicationImpl::instance().getDockPanelManager()->hidePanel(DockPanelIds::Property);
    }
}

void onWCSToggled(bool checked)
{
    Scene* pScene = ApplicationImpl::instance().getActiveScene();
    if (!pScene)
    {
        assert(false);
        return;
    }

    if (checked)
    {
        pScene->showWCS();
    }
    else
    {
        pScene->hideWCS();
    }
}
} // namespace

GuiCommand::GuiCommand() : wyap::InteractiveCmdExecution()
{
}

GuiCommand::~GuiCommand()
{
}

void GuiCommand::onSelectionChanged(
    const wyap::SelectionSet&,
    const wyap::SelectionSet&,
    const wyap::SelectionSet& currSS)
{
    GizmoFactory* pGizmoFactory = ApplicationImpl::instance().getGizmoFactory();
    wyap::GizmoManager* pGizmoMgr = ApplicationImpl::instance().getGizmoManager();
    assert(pGizmoFactory && pGizmoMgr);

    std::list<wyap::GizmoSPtr> gizmos;
    pGizmoFactory->createSelectionGizmos(currSS, gizmos);

    pGizmoMgr->beginChange();
    pGizmoMgr->clearGizmos();
    for (const wyap::GizmoSPtr& pGizmo : gizmos)
    {
        pGizmoMgr->addGizmo(pGizmo);
    }
    pGizmoMgr->endChange();
}

wyap::CmdExecution::StartResult GuiCommand::onStart()
{
    wyap::CmdExecution::StartResult ret = __baseClass::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    ApplicationImpl::instance().getGizmoFactory()->clearSelectionCreators();

    // 默认设置特征树不可选
    // 目前只有建模环境下的选择命令需要支持在特征树上选择
    ApplicationImpl::instance().getDockPanelManager()->findWidgetAs<FeatureTreeWidget>(
        DockPanelIds::FeatureTree)->setSelectable(false);
    // 默认属性框只读
    // 目前只有选择命令需要支持属性框可编辑
    ApplicationImpl::instance().getDockPanelManager()->findWidgetAs<PropertyEditorWidget>(
        DockPanelIds::Property)->setReadOnly(true);

    // 命令配置:选择
    SelectOptions selectOptions;
    this->initializeSelectOptions(selectOptions);
    this->applySelectOptions(selectOptions);
    // 重置状态栏
    ApplicationImpl::instance().getStatusBar()->reset();
    // 重置捕捉系统
    ApplicationImpl::instance().getSnapSystem()->reset();
    // 设置鼠标样式
    ApplicationImpl::instance().setCursor(CursorType::Select);

    // added by wangyao 2025.05.06 {
    // 清除动态生成的面&边
    ApplicationImpl::instance().getActiveScene()->endNoBatchRender();
    // }

    return wyap::CmdExecution::StartResult::Succeeded;
}

void GuiCommand::onEnd()
{
    SelectOptions options;
    this->applySelectOptions(options);

    Scene* pActiveScene = ApplicationImpl::instance().getActiveScene();
    if (pActiveScene)
    {
        pActiveScene->clearTransients();
    }

    ApplicationImpl::instance().getGizmoManager()->beginChange();
    ApplicationImpl::instance().getGizmoManager()->clearGizmos();
    ApplicationImpl::instance().getGizmoManager()->endChange();

    this->cleanUp();
}

void GuiCommand::onAbort(wyap::CmdExecution::AbortCause cause)
{
    SelectOptions options;
    this->applySelectOptions(options);

    Scene* pActiveScene = ApplicationImpl::instance().getActiveScene();
    if (pActiveScene)
    {
        pActiveScene->clearTransients();
    }

    ApplicationImpl::instance().getGizmoManager()->beginChange();
    ApplicationImpl::instance().getGizmoManager()->clearGizmos();
    ApplicationImpl::instance().getGizmoManager()->endChange();

    this->cleanUp();
}

void GuiCommand::onRightMouseUp(const MouseEvent& event)
{
    ContextMenu menu;

    this->configureContextMenu(menu);
    if (!menu.actions().isEmpty() && !menu.actions().last()->isSeparator())
    {
        menu.addSeparator();
    }

    // Complete Selection / Clear Selection
    bool added(false);
    if (menu.isCompleteSelectionVisible())
    {
        QAction* pActionCompleteSelection = new QAction(
            QCoreApplication::translate("GuiCommand", "Complete Selection"), &menu);
        menu.addAction(pActionCompleteSelection);
        QObject::connect(pActionCompleteSelection, &QAction::triggered, pActionCompleteSelection,
            [this]() { this->onCompleteSelectionMenuItem(); });
        added = true;
    }
    if (menu.isClearSelectionVisible())
    {
        QAction* pActionClearSelection = new QAction(
            QCoreApplication::translate("GuiCommand", "Clear Selection"), &menu);
        menu.addAction(pActionClearSelection);
        QObject::connect(pActionClearSelection, &QAction::triggered, pActionClearSelection,
            [this]() { this->onClearSelectionMenuItem(); });
        added = true;
    }
    if (added) menu.addSeparator();

    // Environment actions
    wyap::Environment* pEnv = ApplicationImpl::instance().getEnvManager()->getActiveEnvironment();
    if (IEnvironmentUIHost* pEnvUIHost = dynamic_cast<IEnvironmentUIHost*>(pEnv))
    {
        EnvironmentUI* pEnvUI = pEnvUIHost->getEnvironmentUI();
        if (pEnvUI && pEnvUI->addContextMenuActions(menu)) menu.addSeparator();
    }

    // Property panel
    QAction* pActionPropertyWidget = new QAction(
        QCoreApplication::translate("GuiCommand", "Property"), &menu);
    pActionPropertyWidget->setCheckable(true);
    pActionPropertyWidget->setChecked(
        ApplicationImpl::instance().getDockPanelManager()->isPanelVisible(DockPanelIds::Property));
    menu.addAction(pActionPropertyWidget);
    QObject::connect(pActionPropertyWidget, &QAction::toggled, pActionPropertyWidget,
        onPropertyWidgetToggled);

    // World coordinate system (hidden when there is no active scene)
    Scene* pScene = ApplicationImpl::instance().getActiveScene();
    if (pScene)
    {
        QAction* pActionWCS = new QAction(
            QCoreApplication::translate("GuiCommand", "World Coordinate System"), &menu);
        pActionWCS->setCheckable(true);
        pActionWCS->setChecked(pScene->isWCSVisible());
        menu.addAction(pActionWCS);
        QObject::connect(pActionWCS, &QAction::toggled, pActionWCS, onWCSToggled);
    }

    menu.move(QCursor::pos());
    menu.exec();
}

void GuiCommand::onKeyDown(const KeyEvent& event)
{
    switch (event.key)
    {
    case KeyCode::Enter:
    case KeyCode::KP_Enter:
        this->onEnterKey();
        break;

    case KeyCode::Space:
        this->onSpaceKey();
        break;

    case KeyCode::Escape:
        this->onEscapeKey();
        break;
    }
}

void GuiCommand::applySelectOptions(SelectOptions options)
{
    BaseView* pView = ApplicationImpl::instance().getActiveView();
    if (!pView) return;
    GuiEventDispatcher* pGuiEventDispatcher = pView->getGuiEventDispatcher();
    if (!pGuiEventDispatcher) return;
    SelectHandler* pSelHandler = pGuiEventDispatcher->getSelectHandler();
    if (!pSelHandler)
    {
        assert(false);
        return;
    }
    pSelHandler->reset();
    pSelHandler->enablePointSelect(options.pointSelect);
    pSelHandler->enableBoxSelect(options.boxSelect);
    pSelHandler->setAllowedElemTypes(options.allowedElemTypes);
    pSelHandler->setSelectionType(options.selectionType);
    pSelHandler->setPreFilterFunctor(options.preFilter);
    pSelHandler->setPostFilterFunctor(options.postFilter);
    pSelHandler->enablePreview(options.preview);
    pSelHandler->setSelectMode(options.selectMode);
}

GuiCommand::SelectOptions GuiCommand::getSelectOptions()
{
    BaseView* pView = ApplicationImpl::instance().getActiveView();
    if (!pView) return SelectOptions();
    GuiEventDispatcher* pGuiEventDispatcher = pView->getGuiEventDispatcher();
    if (!pGuiEventDispatcher) return SelectOptions();
    SelectHandler* pSelHandler = pGuiEventDispatcher->getSelectHandler();
    if (!pSelHandler)
    {
        assert(false);
        return SelectOptions();
    }

    SelectOptions options;
    options.pointSelect = pSelHandler->isSupportsPointSelect();
    options.boxSelect = pSelHandler->isSupportsBoxSelect();
    options.allowedElemTypes = pSelHandler->getAllowedElemTypes();
    options.selectionType = pSelHandler->getSelectionType();
    options.preFilter = pSelHandler->getPreFilterFunctor();
    options.postFilter = pSelHandler->getPostFilterFunctor();
    options.preview = pSelHandler->isSupportPreview();
    options.selectMode = pSelHandler->getSelectMode();
    return options;
}

} // namespace wy3dApp
