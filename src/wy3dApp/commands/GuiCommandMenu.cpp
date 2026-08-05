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

#include <QCoreApplication>
#include <QMouseEvent>
#include "commands/GuiCommandMenu.h"
#include "commands/GuiCommand.h"
#include "commands/CommandNames.h"
#include "application/Application.h"
#include "widgets/panels/DockPanelIds.h"
#include "widgets/panels/DockPanelManager.h"
#include "environments/EnvironmentBase.h"
#include "scene/Scene.h"
#include "environments/sketch/SketchEnvironment.h"

namespace
{
CommandAction* findActiveEnvironmentCommandAction(const std::string& commandName)
{
    wyap::Environment* pEnv = Application::instance().getEnvManager()->getActiveEnvironment();
    EnvironmentBase* pEnvBase = dynamic_cast<EnvironmentBase*>(pEnv);
    if (!pEnvBase)
    {
        assert(false);
        return nullptr;
    }

    CommandAction* pAction = pEnvBase->findCommandAction(commandName);
    if (!pAction)
    {
        assert(false);
        return nullptr;
    }

    return pAction;
}
}

GuiCmdMenu::GuiCmdMenu(GuiCommand* pCmd)
    : QObject()
    , _pCmd(pCmd)
{
    assert(_pCmd);
}

void GuiCmdMenu::exec(const QPoint& pos)
{
    GuiCmdQMenu menu;
    this->init(&menu);
    menu.move(pos);
    menu.exec();
}

void GuiCmdMenu::init(QMenu* menu)
{
    assert(menu);
    assert(_pCmd);
    if (!_pCmd) return;

    // Custom menu actions — header
    if (this->initCustomHeaderActions(menu))
    {
        menu->addSeparator();
    }

    // 完成选择 & 取消选择
    {
        bool added(false);

        // 完成选择
        if (_pCmd->isContextMenuActionVisible_CompleteSelection())
        {
            // 完成选择
            QAction* pActionCompleteSelection = new QAction(tr("Complete Selection"), menu);
            menu->addAction(pActionCompleteSelection);
            this->connect(pActionCompleteSelection, &QAction::triggered, this, &GuiCmdMenu::onCompleteSelection);
            added = true;
        }

        // 取消选择
        if (_pCmd->isContextMenuActionVisible_ClearSelection())
        {
            QAction* pActionClearSelection = new QAction(tr("Clear Selection"), menu);
            menu->addAction(pActionClearSelection);
            this->connect(pActionClearSelection, &QAction::triggered, this, &GuiCmdMenu::onClearSelection);
            added = true;
        }

        // 分隔符
        if (added) menu->addSeparator();
    }

    
    // 草绘环境
    wyap::Environment* pEnv = Application::instance().getEnvManager()->getActiveEnvironment();
    if (dynamic_cast<SketchEnvironment*>(pEnv))
    {
        bool added(false);

        if (CommandAction* pActionOrientToSketch = findActiveEnvironmentCommandAction(CommandNames::OrientToSketch))
        {
            menu->addAction(pActionOrientToSketch);
            added = true;
        }

        if (CommandAction* pActionEndSketch = findActiveEnvironmentCommandAction(CommandNames::EndSketch))
        {
            menu->addAction(pActionEndSketch);
            added = true;
        }

        if (CommandAction* pActionCancelSketch = findActiveEnvironmentCommandAction(CommandNames::CancelSketch))
        {
            menu->addAction(pActionCancelSketch);
            added = true;
        }

        if (added) menu->addSeparator();
    }

    // Custom menu actions — middle
    if (this->initCustomMiddleActions(menu))
    {
        menu->addSeparator();
    }

    // 属性窗口
    QAction* pActionPropertyWidget = new QAction(tr("Property Widget"), menu);
    pActionPropertyWidget->setCheckable(true);
    if (Application::instance().getDockPanelManager()->isPanelVisible(DockPanelIds::Property))
    {
        pActionPropertyWidget->setChecked(true);
    }
    else
    {
        pActionPropertyWidget->setChecked(false);
    }
    menu->addAction(pActionPropertyWidget);
    this->connect(pActionPropertyWidget, &QAction::toggled, this, &GuiCmdMenu::onPropertyWidgetToggled);

    // 世界坐标系
    QAction* pActionWCS = new QAction(tr("World CSYS"), menu);
    pActionWCS->setCheckable(true);
    Scene* pScene = Application::instance().getActiveScene();
    if (pActionWCS && pScene)
    {
        if (pScene->isWCSVisible())
            pActionWCS->setChecked(true);
        else
            pActionWCS->setChecked(false);
        menu->addAction(pActionWCS);
        this->connect(pActionWCS, &QAction::toggled, this, &GuiCmdMenu::onWCSToggled);
    }

    // Custom menu actions — footer
    if (this->initCustomFooterActions(menu))
    {
        menu->addSeparator();
    }
}

bool GuiCmdMenu::initCustomHeaderActions(QMenu* menu)
{
    return false;
}

bool GuiCmdMenu::initCustomMiddleActions(QMenu* menu)
{
    return false;
}

bool GuiCmdMenu::initCustomFooterActions(QMenu* menu)
{
    return false;
}

void GuiCmdMenu::onCompleteSelection()
{
    assert(_pCmd);
    if (_pCmd) _pCmd->onContextMenuAction_CompleteSelection();
}

void GuiCmdMenu::onClearSelection()
{
    assert(_pCmd);
    if (_pCmd) _pCmd->onContextMenuAction_ClearSelection();
}

void GuiCmdMenu::onPropertyWidgetToggled(bool checked)
{
    if (checked)
    {
        Application::instance().getDockPanelManager()->showPanel(DockPanelIds::Property);
    }
    else
    {
        Application::instance().getDockPanelManager()->hidePanel(DockPanelIds::Property);
    }
}

void GuiCmdMenu::onWCSToggled(bool checked)
{
    Scene* pScene = Application::instance().getActiveScene();
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

GuiCmdQMenu::GuiCmdQMenu(QWidget* parent)
    : QMenu(parent)
{
    this->connect(this, &QMenu::aboutToShow, this, &GuiCmdQMenu::installEventFilter);
    this->connect(this, &QMenu::aboutToHide, this, &GuiCmdQMenu::removeEventFilter);
}

void GuiCmdQMenu::installEventFilter()
{
    if (QCoreApplication* app = QCoreApplication::instance())
    {
        app->installEventFilter(this);
    }
}

void GuiCmdQMenu::removeEventFilter()
{
    if (QCoreApplication* app = QCoreApplication::instance())
    {
        app->removeEventFilter(this);
    }
}

bool GuiCmdQMenu::eventFilter(QObject* obj, QEvent* event)
{
    Q_UNUSED(obj);

    if (event->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent* mouseEvent = dynamic_cast<QMouseEvent*>(event);
        assert(mouseEvent);
        if (mouseEvent->button() == Qt::LeftButton)
        {
            QPoint globalPos = mouseEvent->globalPos();
            if (!this->geometry().contains(globalPos) && this->isVisible())
            {
                this->hide();
                return true;
            }
        }
    }

    return QMenu::eventFilter(obj, event);
}
