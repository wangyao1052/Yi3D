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

#include "GatewayEnvironment.h"
#include "GatewayEnvironmentUI.h"
#include "application/Application.h"
#include "commands/CommandNames.h"
#include "environments/ICommandActionStateHost.h"


GatewayEnvironment::GatewayEnvironment()
    : wyap::GatewayEnvironment()
    , _pUI(std::make_unique<GatewayEnvironmentUI>())
{
    setName("gateway");
}

GatewayEnvironment::~GatewayEnvironment()
{
}

void GatewayEnvironment::onDocumentDeactivated(wyap::Document* pDeactivatedDoc)
{
    wyap::GatewayEnvironment::onDocumentDeactivated(pDeactivatedDoc);

    assert(pDeactivatedDoc);
    wy::ErrorStatus error = pDeactivatedDoc->getDatabase()->getTransactionManager()->removeReactor(this);
    assert(wy::ErrorStatus::Ok == error);

    this->updateCommandActionStates();
    this->updateFileActionStates();
}

void GatewayEnvironment::onDocumentActivated(wyap::Document* pActivatedDoc)
{
    wyap::GatewayEnvironment::onDocumentActivated(pActivatedDoc);

    assert(pActivatedDoc);
    wy::ErrorStatus error = pActivatedDoc->getDatabase()->getTransactionManager()->addReactor(this);
    assert(wy::ErrorStatus::Ok == error);

    this->updateCommandActionStates();
    this->updateFileActionStates();
}

void GatewayEnvironment::onTransactionStarted(wydb::Transaction* pTrans)
{
    this->updateCommandActionStates();
    this->updateFileActionStates();
}

void GatewayEnvironment::onTransactionEnded(wydb::Transaction* pTrans)
{
    this->updateCommandActionStates();
    this->updateFileActionStates();
}

void GatewayEnvironment::onTransactionAborted(wydb::Transaction* pTrans)
{
    this->updateCommandActionStates();
    this->updateFileActionStates();
}

void GatewayEnvironment::onCommandStartFailed(
    wyap::Command* pCmd,
    wyap::CmdExecution::StartResult startResult)
{
    EnvironmentBase::onCommandStartFailed(pCmd, startResult);
}

void GatewayEnvironment::onCommandStarted(wyap::Command* pCmd)
{
    EnvironmentBase::onCommandStarted(pCmd);
}

void GatewayEnvironment::onCommandEnded(wyap::Command* pCmd)
{
    EnvironmentBase::onCommandEnded(pCmd);
}

void GatewayEnvironment::onCommandAborted(
    wyap::Command* pCmd,
    wyap::CmdExecution::AbortCause abortCause)
{
    EnvironmentBase::onCommandAborted(pCmd, abortCause);
}

void GatewayEnvironment::onEnter()
{
    wyap::GatewayEnvironment::onEnter();

    this->registerCommands();
    _pUI->initialize(this);
}

void GatewayEnvironment::onExit(ExitCode exitCode)
{
    _pUI->teardown(this);
    this->removeCommands();

    wyap::GatewayEnvironment::onExit(exitCode);
}

void GatewayEnvironment::onSuspend()
{
    wyap::GatewayEnvironment::onSuspend();
}

void GatewayEnvironment::onResume()
{
    wyap::GatewayEnvironment::onResume();
}

void GatewayEnvironment::updateCommandActionStates()
{
    wyap::EnvManager* pEnvMgr = Application::instance().getEnvManager();
    wyap::Environment* pActiveEnv = pEnvMgr->getActiveEnvironment();
    if (pActiveEnv)
    {
        ICommandActionStateHost* pCmdActionStateHost = dynamic_cast<ICommandActionStateHost*>(pActiveEnv);
        if (pCmdActionStateHost)
        {
            pCmdActionStateHost->updateCommandActionStates();
        }
    }
}

void GatewayEnvironment::updateFileActionStates()
{
    QAction* pNewFileAction = this->findCommandAction(CommandNames::NewFile);
    QAction* pOpenFileAction = this->findCommandAction(CommandNames::OpenFile);
    if (!pNewFileAction || !pOpenFileAction)
    {
        return;
    }

    bool enabled(true);
    wyap::Document* pActiveDoc = Application::instance().getActiveDocument();
    if (pActiveDoc)
    {
        wydb::Database* pDb = pActiveDoc->getDatabase();
        assert(pDb);
        if (pDb)
        {
            wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();
            enabled = (nullptr == pTransMgr->getActiveTransaction());
        }
    }

    pNewFileAction->setEnabled(enabled);
    pOpenFileAction->setEnabled(enabled);
}
