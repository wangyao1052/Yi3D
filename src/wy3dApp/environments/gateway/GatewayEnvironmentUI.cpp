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

#include <QCoreApplication>
#include <QIcon>

#include "GatewayEnvironment.h"
#include "commands/CommandAction.h"
#include "commands/CommandNames.h"
#include "ui/MenuBarNames.h"
#include "ui/ToolBarNames.h"

namespace
{
struct GatewayActions
{
    CommandAction* pActionNewFile;
    CommandAction* pActionOpenFile;
    CommandAction* pActionHelpDocumentation;
    CommandAction* pActionShortcutKeys;
    CommandAction* pActionAbout;
};

GatewayActions createActions(GatewayEnvironment* pEnv)
{
    assert(pEnv);

    GatewayActions actions = {};
    actions.pActionNewFile = pEnv->newCommandAction(
        CommandNames::NewFile,
        QCoreApplication::translate("MainWindow", "New"),
        QIcon(":/images/Document_New.svg"));

    actions.pActionOpenFile = pEnv->newCommandAction(
        CommandNames::OpenFile,
        QCoreApplication::translate("MainWindow", "Open"),
        QIcon(":/images/Document_Open.svg"));

    actions.pActionHelpDocumentation = pEnv->newCommandAction(
        CommandNames::HelpDocumentation,
        QCoreApplication::translate("MainWindow", "Help Documentation"),
        QIcon());

    actions.pActionShortcutKeys = pEnv->newCommandAction(
        CommandNames::ShortcutKeys,
        QCoreApplication::translate("MainWindow", "Shortcut Keys"),
        QIcon());

    actions.pActionAbout = pEnv->newCommandAction(
        CommandNames::About,
        QCoreApplication::translate("MainWindow", "About"),
        QIcon());

    return actions;
}
} // namespace

GatewayEnvironmentUI::GatewayEnvironmentUI()
    : _pMenuFile(nullptr)
    , _pMenuHelp(nullptr)
    , _pToolBarBasic(nullptr)
{
}

GatewayEnvironmentUI::~GatewayEnvironmentUI()
{
}

void GatewayEnvironmentUI::initialize(GatewayEnvironment* pEnv)
{
    if (!pEnv)
    {
        assert(false);
        return;
    }

    const GatewayActions actions = createActions(pEnv);
    this->createMenus(pEnv);
    this->createToolBars(pEnv);
    assert(_pMenuFile);
    assert(_pMenuHelp);
    assert(_pToolBarBasic);
    _pMenuFile->addAction(actions.pActionNewFile);
    _pMenuFile->addAction(actions.pActionOpenFile);
    _pMenuHelp->addAction(actions.pActionHelpDocumentation);
    _pMenuHelp->addAction(actions.pActionShortcutKeys);
    _pMenuHelp->addAction(actions.pActionAbout);
    _pToolBarBasic->addAction(actions.pActionNewFile);
    _pToolBarBasic->addAction(actions.pActionOpenFile);

    pEnv->restoreUiState();
}

void GatewayEnvironmentUI::teardown(GatewayEnvironment* pEnv)
{
    if (!pEnv)
    {
        assert(false);
        return;
    }

    pEnv->destroyUI();
    this->clear();
}

void GatewayEnvironmentUI::clear()
{
    _pMenuFile = nullptr;
    _pMenuHelp = nullptr;
    _pToolBarBasic = nullptr;
}

void GatewayEnvironmentUI::createMenus(GatewayEnvironment* pEnv)
{
    _pMenuFile = pEnv->addMenu(
        QCoreApplication::translate("MainWindow", "File"),
        wy3dApp::MenuBarNames::File);

    _pMenuHelp = pEnv->addMenu(
        QCoreApplication::translate("MainWindow", "Help"),
        wy3dApp::MenuBarNames::Help);
}

void GatewayEnvironmentUI::createToolBars(GatewayEnvironment* pEnv)
{
    _pToolBarBasic = pEnv->addToolBar(
        QCoreApplication::translate("MainWindow", "Basic"),
        wy3dApp::ToolBarNames::Basic);
}
