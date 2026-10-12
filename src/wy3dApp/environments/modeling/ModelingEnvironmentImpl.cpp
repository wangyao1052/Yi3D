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

#include "ModelingEnvironmentImpl.h"
#include "ModelingEnvironmentUI.h"

#include <memory>
#include <string>
#include <vector>

#include <wydbTransaction.h>

#include <wy3dApp/Application.h>
#include "commands/CommandNames.h"
#include "wy3dAppDefs.h"

namespace wy3dApp {

ModelingEnvironmentImpl::ModelingEnvironmentImpl()
    : _pUI(std::make_unique<ModelingEnvironmentUI>())
{
    setName("modeling");
}

ModelingEnvironmentImpl::~ModelingEnvironmentImpl()
{
}

void ModelingEnvironmentImpl::onCommandStartFailed(
    wyap::Command* pCmd,
    wyap::CmdExecution::StartResult startResult)
{
    EnvironmentBase::onCommandStartFailed(pCmd, startResult);
}

void ModelingEnvironmentImpl::onCommandStarted(wyap::Command* pCmd)
{
    EnvironmentBase::onCommandStarted(pCmd);
}

void ModelingEnvironmentImpl::onCommandEnded(wyap::Command* pCmd)
{
    EnvironmentBase::onCommandEnded(pCmd);
}

void ModelingEnvironmentImpl::onCommandAborted(
    wyap::Command* pCmd,
    wyap::CmdExecution::AbortCause abortCause)
{
    EnvironmentBase::onCommandAborted(pCmd, abortCause);
}

void ModelingEnvironmentImpl::onEnter()
{
    wyap::DocumentEnvironment::onEnter();
    EnvironmentBase::onEnter();

    this->registerCommands();
    _pUI->initialize(this);

    Application::instance().getCmdManager()->postCommand(CommandNames::Select);

    this->updateCommandActionStates();

    // 同步显示模式按钮状态
    this->syncDisplayModeAction();
}

void ModelingEnvironmentImpl::onExit(ExitCode exitCode)
{
    _pUI->teardown(this);
    this->removeCommands();

    EnvironmentBase::onExit(exitCode);
    wyap::DocumentEnvironment::onExit(exitCode);
}

void ModelingEnvironmentImpl::onSuspend()
{
    _pUI->teardown(this);
    this->removeCommands();

    EnvironmentBase::onSuspend();
    wyap::DocumentEnvironment::onSuspend();
}

void ModelingEnvironmentImpl::onResume()
{
    wyap::DocumentEnvironment::onResume();
    EnvironmentBase::onResume();

    this->registerCommands();
    _pUI->initialize(this);

    Application::instance().getCmdManager()->postCommand(CommandNames::Select);

    this->updateCommandActionStates();

    // 同步显示模式按钮状态
    this->syncDisplayModeAction();
}

void ModelingEnvironmentImpl::onDocumentActivated(wyap::Document* pActivatedDoc)
{
    wyap::DocumentEnvironment::onDocumentActivated(pActivatedDoc);
    this->syncDisplayModeAction();
}

void ModelingEnvironmentImpl::updateUndoRedoActionStates()
{
    QAction* pUndoAction = this->findCommandAction(CommandNames::Undo);
    QAction* pRedoAction = this->findCommandAction(CommandNames::Redo);
    if (!pUndoAction || !pRedoAction)
    {
        assert(false);
        return;
    }

    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        pUndoAction->setEnabled(false);
        pRedoAction->setEnabled(false);
        return;
    }

    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();
    wydb::Transaction* pActiveTrans = pTransMgr->getActiveTransaction();
    if (pActiveTrans)
    {
        pUndoAction->setEnabled(false);
        pRedoAction->setEnabled(false);
        return;
    }

    pUndoAction->setEnabled(pTransMgr->canUndo() ? true : false);
    pRedoAction->setEnabled(pTransMgr->canRedo() ? true : false);
}

void ModelingEnvironmentImpl::updateFileActionStates()
{
    const std::vector<std::string> fileCommandNames =
    {
        CommandNames::SaveFile,
        CommandNames::SaveAsFile,
        CommandNames::ImportFile,
        CommandNames::ExportFile,
    };

    bool enabled(false);
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (pDb)
    {
        wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();
        enabled = (nullptr == pTransMgr->getActiveTransaction());
    }

    for (const std::string& commandName : fileCommandNames)
    {
        QAction* pAction = this->findCommandAction(commandName);
        if (!pAction)
        {
            assert(false);
            continue;
        }
        pAction->setEnabled(enabled);
    }
}

}
