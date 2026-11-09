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

#include "GatewayEnvironmentUI.h"

#include <cassert>

#include <QAction>
#include <QCoreApplication>
#include <QIcon>

#include <wy3dApp/GatewayEnvironment.h>
#include <wy3dApp/CommandAction.h>
#include "commands/CommandNames.h"
#include "commands/PluginManagerCommands.h"
#include "ui/MenuBarNames.h"
#include "ui/ToolBarNames.h"
#include "utils/IconUtil.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

namespace
{
struct GatewayActions
{
    CommandAction* pActionNewFile;
    CommandAction* pActionOpenFile;
    CommandAction* pActionOptions;
    QAction* pActionManagePlugins;
    CommandAction* pActionHelpDocumentation;
    CommandAction* pActionShortcutKeys;
    CommandAction* pActionContactUs;
    CommandAction* pActionAbout;
};

GatewayActions createActions(EnvironmentUIImpl* pUI)
{
    assert(pUI);

    GatewayActions actions = {};
    actions.pActionNewFile = pUI->newCommandAction(
        CommandNames::NewFile,
        QCoreApplication::translate("MainWindow", "New"),
        IconUtil::get(":/images/Document_New.svg"));

    actions.pActionOpenFile = pUI->newCommandAction(
        CommandNames::OpenFile,
        QCoreApplication::translate("MainWindow", "Open"),
        IconUtil::get(":/images/Document_Open.svg"));

    actions.pActionOptions = pUI->newCommandAction(
        CommandNames::Options,
        QCoreApplication::translate("MainWindow", "Options"),
        IconUtil::get(":/images/Options.svg"));

    actions.pActionManagePlugins = new QAction(
        QCoreApplication::translate("MainWindow", "Plugin Management"), pUI);
    actions.pActionManagePlugins->setIcon(
        QIcon(QStringLiteral(":/images/Plugin_Manage.svg")));
    actions.pActionManagePlugins->setToolTip(
        QCoreApplication::translate("MainWindow", "Plugin Management"));
    actions.pActionManagePlugins->setStatusTip(
        actions.pActionManagePlugins->toolTip());
    QObject::connect(
        actions.pActionManagePlugins, &QAction::triggered,
        actions.pActionManagePlugins, []() { showPluginManagerDialog(); });

    actions.pActionHelpDocumentation = pUI->newCommandAction(
        CommandNames::HelpDocumentation,
        QCoreApplication::translate("MainWindow", "Help Documentation"),
        QIcon());

    actions.pActionShortcutKeys = pUI->newCommandAction(
        CommandNames::ShortcutKeys,
        QCoreApplication::translate("MainWindow", "Shortcut Keys"),
        QIcon());

    actions.pActionContactUs = pUI->newCommandAction(
        CommandNames::ContactUs,
        QCoreApplication::translate("MainWindow", "Contact Us"),
        QIcon());

    actions.pActionAbout = pUI->newCommandAction(
        CommandNames::About,
        QCoreApplication::translate("MainWindow", "About"),
        QIcon());

    return actions;
}
} // namespace

GatewayEnvironmentUI::GatewayEnvironmentUI(const GatewayEnvironment* pEnv)
    : EnvironmentUIImpl(pEnv)
    , _pMenuFile(nullptr)
    , _pMenuTools(nullptr)
    , _pMenuHelp(nullptr)
    , _pToolBarFile(nullptr)
{
}

GatewayEnvironmentUI::~GatewayEnvironmentUI()
{
}

void GatewayEnvironmentUI::build()
{
    const GatewayActions actions = createActions(this);
    _pNewFileAction = actions.pActionNewFile;
    _pOpenFileAction = actions.pActionOpenFile;
    this->createMenus();
    this->createToolBars();
    assert(_pMenuFile);
    assert(_pMenuTools);
    assert(_pMenuHelp);
    assert(_pToolBarFile);
    _pMenuFile->addAction(actions.pActionNewFile);
    _pMenuFile->addAction(actions.pActionOpenFile);
    _pMenuTools->addAction(actions.pActionOptions);
    _pMenuTools->addAction(actions.pActionManagePlugins);
    _pMenuHelp->addAction(actions.pActionHelpDocumentation);
    _pMenuHelp->addAction(actions.pActionShortcutKeys);
    _pMenuHelp->addAction(actions.pActionContactUs);
    _pMenuHelp->addAction(actions.pActionAbout);
    _pToolBarFile->addAction(actions.pActionNewFile);
    _pToolBarFile->addAction(actions.pActionOpenFile);
}

void GatewayEnvironmentUI::setFileActionsEnabled(bool enabled)
{
    if (!_pNewFileAction || !_pOpenFileAction) return;

    _pNewFileAction->setEnabled(enabled);
    _pOpenFileAction->setEnabled(enabled);
}

void GatewayEnvironmentUI::createMenus()
{
    _pMenuFile = this->newMenu(
        QCoreApplication::translate("MainWindow", "File"),
        wy3dApp::MenuBarNames::File);

    _pMenuTools = this->newMenu(
        QCoreApplication::translate("MainWindow", "Tools"),
        wy3dApp::MenuBarNames::Tools);

    _pMenuHelp = this->newMenu(
        QCoreApplication::translate("MainWindow", "Help"),
        wy3dApp::MenuBarNames::Help);
}

void GatewayEnvironmentUI::createToolBars()
{
    _pToolBarFile = this->newToolBar(
        QCoreApplication::translate("MainWindow", "File"),
        wy3dApp::ToolBarNames::File);
}

} // namespace wy3dApp
