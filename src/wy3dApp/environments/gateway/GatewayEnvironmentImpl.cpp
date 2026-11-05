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

#include "GatewayEnvironmentImpl.h"
#include "GatewayEnvironmentUI.h"
#include <wy3dApp/Application.h>
#include <wy3dApp/CommandAction.h>
#include "application/ApplicationImpl.h"
#include "commands/CommandNames.h"
#include <wy3dApp/wy3dAppDefs.h>
#include <wy3dApp/DocumentEnvironment.h>
#include <wy3dApp/TaskEnvironment.h>

namespace wy3dApp {

GatewayEnvironmentImpl::GatewayEnvironmentImpl()
{
    setName("gateway");
    _pUI = std::make_unique<GatewayEnvironmentUI>(this);
}

GatewayEnvironmentImpl::~GatewayEnvironmentImpl()
{
}

EnvironmentUI* GatewayEnvironmentImpl::getUI() const
{
    return _pUI.get();
}

void GatewayEnvironmentImpl::onDocumentDeactivated(wyap::Document* pDeactivatedDoc)
{
    wy3dApp::GatewayEnvironment::onDocumentDeactivated(pDeactivatedDoc);

    assert(pDeactivatedDoc);
    wy::ErrorStatus error = pDeactivatedDoc->getDatabase()->getTransactionManager()->removeReactor(this);
    assert(wy::ErrorStatus::Ok == error);

    this->updateCommandActionStates();
}

void GatewayEnvironmentImpl::onDocumentActivated(wyap::Document* pActivatedDoc)
{
    wy3dApp::GatewayEnvironment::onDocumentActivated(pActivatedDoc);

    assert(pActivatedDoc);
    wy::ErrorStatus error = pActivatedDoc->getDatabase()->getTransactionManager()->addReactor(this);
    assert(wy::ErrorStatus::Ok == error);

    this->updateCommandActionStates();
}

void GatewayEnvironmentImpl::onTransactionStarted(wydb::Transaction* pTrans)
{
    this->updateCommandActionStates();
}

void GatewayEnvironmentImpl::onTransactionEnded(wydb::Transaction* pTrans)
{
    this->updateCommandActionStates();
}

void GatewayEnvironmentImpl::onTransactionAborted(wydb::Transaction* pTrans)
{
    this->updateCommandActionStates();
}

void GatewayEnvironmentImpl::onTransactionUndone(wydb::Transaction* pTrans)
{
    this->updateCommandActionStates();
}

void GatewayEnvironmentImpl::onTransactionRedone(wydb::Transaction* pTrans)
{
    this->updateCommandActionStates();
}

void GatewayEnvironmentImpl::onCommandStartFailed(
    wyap::Command* pCmd,
    wyap::CmdExecution::StartResult startResult)
{
    _pUI->onCommandStartFailed(pCmd, startResult);
}

void GatewayEnvironmentImpl::onCommandStarted(wyap::Command* pCmd)
{
    _pUI->onCommandStarted(pCmd);
}

void GatewayEnvironmentImpl::onCommandEnded(wyap::Command* pCmd)
{
    _pUI->onCommandEnded(pCmd);
}

void GatewayEnvironmentImpl::onCommandAborted(
    wyap::Command* pCmd,
    wyap::CmdExecution::AbortCause abortCause)
{
    _pUI->onCommandAborted(pCmd, abortCause);
}

void GatewayEnvironmentImpl::onEnter()
{
    wy3dApp::GatewayEnvironment::onEnter();

    this->registerCommands();

    _pUI->build();
}

void GatewayEnvironmentImpl::onExit(ExitCode exitCode)
{
    _pUI->destroy();

    this->removeCommands();

    wy3dApp::GatewayEnvironment::onExit(exitCode);
}

void GatewayEnvironmentImpl::onSuspend()
{
    wy3dApp::GatewayEnvironment::onSuspend();
}

void GatewayEnvironmentImpl::onResume()
{
    wy3dApp::GatewayEnvironment::onResume();
}

void GatewayEnvironmentImpl::updateCommandActionStates()
{
    this->updateFileCommandActionStates();

    wyap::Environment* pActiveEnv = ApplicationImpl::instance().getEnvManager()->getActiveEnvironment();
    if (!pActiveEnv)
    {
        return;
    }

    wy3dApp::DocumentEnvironment* pDocEnv = dynamic_cast<wy3dApp::DocumentEnvironment*>(pActiveEnv);
    if (pDocEnv)
    {
        pDocEnv->updateCommandActionStates();
    }

    wy3dApp::TaskEnvironment* pTaskEnv = dynamic_cast<wy3dApp::TaskEnvironment*>(pActiveEnv);
    if (pTaskEnv)
    {
        pTaskEnv->updateCommandActionStates();
    }
}

void GatewayEnvironmentImpl::updateFileCommandActionStates()
{
    QAction* pNewFileAction = _pUI->findCommandAction(CommandNames::NewFile);
    QAction* pOpenFileAction = _pUI->findCommandAction(CommandNames::OpenFile);
    if (!pNewFileAction || !pOpenFileAction)
    {
        return;
    }

    bool enabled(true);
    wyap::Document* pActiveDoc = ApplicationImpl::instance().getActiveDocument();
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

} // namespace wy3dApp
