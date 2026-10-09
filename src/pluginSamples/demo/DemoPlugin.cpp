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

#include "DemoPlugin.h"

#include <string>
#include <cassert>

#include <QAction>
#include <QCoreApplication>
#include <QIcon>
#include <QMenu>
#include <QToolBar>

#include <wyapApplication.h>
#include <wyapEnvironment.h>

#include <plugin/PluginApi.h>

#include "DemoHelloCmd.h"
#include "DemoParametersCmd.h"
#include "environments/EnvironmentBase.h"
#include <wy3dApp/SketchEnvironment.h>
#include <wy3dApp/Sketch3DEnvironment.h>

DemoPlugin::DemoPlugin() = default;

DemoPlugin::~DemoPlugin() = default;

bool DemoPlugin::initialize()
{
    //demo::log(LogLevel::Info, "demo plugin initialized");
    return true;
}

void DemoPlugin::terminate()
{
    //demo::log(LogLevel::Info, "demo plugin terminated");
}

void DemoPlugin::onEnvironmentEntered(wyap::Environment* pEnvironment)
{
    assert(pEnvironment);
    SketchEnvironment* pSketchEnv = dynamic_cast<SketchEnvironment*>(pEnvironment);
    if (pSketchEnv)
    {
        this->enterSketchEnvironment(pSketchEnv);
    }
}

void DemoPlugin::onEnvironmentToBeExited(wyap::Environment* pEnvironment)
{
    assert(pEnvironment);
    SketchEnvironment* pSketchEnv = dynamic_cast<SketchEnvironment*>(pEnvironment);
    if (pSketchEnv)
    {
        this->exitSketchEnvironment(pSketchEnv);
    }
}

void DemoPlugin::onEnvironmentToBeSuspended(wyap::Environment* pEnvironment)
{
    assert(pEnvironment);
    SketchEnvironment* pSketchEnv = dynamic_cast<SketchEnvironment*>(pEnvironment);
    if (pSketchEnv)
    {
        this->exitSketchEnvironment(pSketchEnv);
    }
}

void DemoPlugin::onEnvironmentResumed(wyap::Environment* pEnvironment)
{
    assert(pEnvironment);
    SketchEnvironment* pSketchEnv = dynamic_cast<SketchEnvironment*>(pEnvironment);
    if (pSketchEnv)
    {
        this->enterSketchEnvironment(pSketchEnv);
    }
}

void DemoPlugin::enterSketchEnvironment(SketchEnvironment* pSketchEnvironment)
{
    assert(pSketchEnvironment);

    wyap::Application* pApp = wyap::getApplication();
    assert(pApp);
    wyap::CmdStack* pCmdStack = pApp->getCmdStack();
    assert(pCmdStack);

    wy::ErrorStatus error = pCmdStack->addCommand(
        demo::kHelloCommand,
        WYAP_CMD_MODAL | WYAP_CMD_NOHISTORY,
        DemoHelloCmd::classInfo());
    if (wy::ErrorStatus::Ok != error)
    {
        //demo::log(LogLevel::Warning, "demo: addCommand failed");
    }

    error = pCmdStack->addCommand(
        demo::kParametersCommand,
        WYAP_CMD_MODAL | WYAP_CMD_NOHISTORY,
        DemoParametersCmd::classInfo());
    if (wy::ErrorStatus::Ok != error)
    {
        //demo::log(LogLevel::Warning, "demo: addCommand failed");
    }

    EnvironmentBase* pEnvBase = dynamic_cast<EnvironmentBase*>(pSketchEnvironment);
    assert(pEnvBase);

    const QString title = QCoreApplication::translate("DemoPlugin", "Demo");
    QToolBar* pToolBar = pEnvBase->addToolBar(title, "com.yi3d.demo.toolbar");
    QMenu* pMenu = pEnvBase->addMenu(title, "com.yi3d.demo.menu");
    // One shared group for all demo command actions: it is what makes them checkable,
    // and the environment base keeps the running command's action pressed through it.
    QActionGroup* pActionGroup = pEnvBase->newActionGroup();
    CommandAction* pAction = pEnvBase->newCommandAction(
        demo::kHelloCommand,
        QCoreApplication::translate("DemoPlugin", "Hello from Demo"),
        QIcon(":/plugin/com.yi3d.demo/demo_hello.svg"),
        pActionGroup);
    if (pToolBar && pAction) pToolBar->addAction(pAction);
    if (pMenu && pAction) pMenu->addAction(pAction);

    CommandAction* pParametersAction = pEnvBase->newCommandAction(
        demo::kParametersCommand,
        QCoreApplication::translate("DemoPlugin", "List Element Parameters"),
        QIcon(":/plugin/com.yi3d.demo/demo_parameters.svg"),
        pActionGroup);
    if (pToolBar && pParametersAction) pToolBar->addAction(pParametersAction);
    if (pMenu && pParametersAction) pMenu->addAction(pParametersAction);
}

void DemoPlugin::exitSketchEnvironment(SketchEnvironment* pSketchEnvironment)
{
    assert(pSketchEnvironment);

    wyap::Application* pApp = wyap::getApplication();
    assert(pApp);
    wyap::CmdStack* pCmdStack = pApp->getCmdStack();
    assert(pCmdStack);

    wy::ErrorStatus error = pCmdStack->removeCommand(demo::kHelloCommand);
    if (wy::ErrorStatus::Ok != error && wy::ErrorStatus::KeyNotFound != error)
    {
        //demo::log(LogLevel::Warning, "demo: removeCommand failed");
    }

    error = pCmdStack->removeCommand(demo::kParametersCommand);
    if (wy::ErrorStatus::Ok != error && wy::ErrorStatus::KeyNotFound != error)
    {
        //demo::log(LogLevel::Warning, "demo: removeCommand failed");
    }

    //demo::log(LogLevel::Info, "demo ui torn down");
}

WY3DAPP_IMPLEMENT_PLUGIN_ENTRY(DemoPlugin)
