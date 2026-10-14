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

#include <wy3dApp/Application.h>
#include <wyVector2.h>
#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapSelManager.h>
#include "widgets/panels/DockPanelIds.h"
#include "widgets/panels/DockPanelManager.h"
#include "widgets/panels/featureTree/FeatureTreeWidget.h"
#include "widgets/panels/property/PropertyEditorWidget.h"
#include "widgets/frame/MainWindow.h"

#include <wy3dApp/Scene.h>
#include "scene/RenderConst.h"
#include "snap/SnapContext.h"
#include <wy3dApp/SketchEnvironment.h>
#include "snap/SnapTipWidget.h"
#include "commands/GuiCommandMenu.h"
#include "commands/GuiEventDispatcher.h"
#include "select/SelectHandler.h"
#include "view/BaseView.h"
#include "select/SelectFilterFunctors.h"
#include <wy3dApp/GuiCommandUtil.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

GuiCommand::GuiCommand() : wyap::InteractiveCmdExecution()
{
}

GuiCommand::~GuiCommand()
{
}

wyap::CmdExecution::StartResult GuiCommand::onStart()
{
    // 基类:如果不使用预选择集则会清空选择集
    wyap::CmdExecution::StartResult ret = __baseClass::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    // 默认设置特征树不可选
    // 目前只有建模环境下的选择命令需要支持在特征树上选择
    Application::instance().getDockPanelManager()->findWidgetAs<FeatureTreeWidget>(
        DockPanelIds::FeatureTree)->setSelectable(false);
    // 默认属性框只读
    // 目前只有选择命令需要支持属性框可编辑
    Application::instance().getDockPanelManager()->findWidgetAs<PropertyEditorWidget>(
        DockPanelIds::Property)->setReadOnly(true);

    // 命令配置:选择
    this->configSelect(_options);
    // 重置状态栏
    Application::instance().getStatusBar()->reset();
    // 重置捕捉系统
    Application::instance().getSnapSystem()->reset();
    // 设置鼠标样式
    Application::instance().setCursor(CursorType::Select);

    // added by wangyao 2025.05.06 {
    // 清除动态生成的面&边
    Application::instance().getActiveScene()->endNoBatchRender();
    // }

    return wyap::CmdExecution::StartResult::Succeeded;
}

void GuiCommand::onEnd()
{
    SelectOptions options;
    this->configSelect(options);

    Scene* pActiveScene = Application::instance().getActiveScene();
    if (pActiveScene)
    {
        pActiveScene->clearTransients();
    }

    Application::instance().getGizmoManager()->beginChange();
    Application::instance().getGizmoManager()->clearGizmos();
    Application::instance().getGizmoManager()->endChange();

    this->cleanUp();
}

void GuiCommand::onAbort(wyap::CmdExecution::AbortCause cause)
{
    SelectOptions options;
    this->configSelect(options);

    Scene* pActiveScene = Application::instance().getActiveScene();
    if (pActiveScene)
    {
        pActiveScene->clearTransients();
    }

    Application::instance().getGizmoManager()->beginChange();
    Application::instance().getGizmoManager()->clearGizmos();
    Application::instance().getGizmoManager()->endChange();

    this->cleanUp();
}

GuiCmdMenu* GuiCommand::createContextMenu()
{
    return new GuiCmdMenu(this);
}

void GuiCommand::onRightMouseUp(const MouseEvent& event)
{
    GuiCmdMenu* pContextMenu = this->createContextMenu();
    if (pContextMenu)
    {
        pContextMenu->exec(QCursor::pos());
        delete pContextMenu;
    }
    else
    {
        assert(false);
    }
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

void GuiCommand::configSelect(SelectOptions options)
{
    BaseView* pView = Application::instance().getActiveView();
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

} // namespace wy3dApp
