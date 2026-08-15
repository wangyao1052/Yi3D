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

#include "ModelingEnvironment.h"
#include "ModelingEnvironmentUI.h"

#include <memory>
#include <string>
#include <vector>

#include <wydbTransaction.h>

#include "application/Application.h"
#include "commands/CommandNames.h"

ModelingEnvironment::ModelingEnvironment()
    : wyap::DocumentEnvironment()
    , _pUI(std::make_unique<ModelingEnvironmentUI>())
{
    setName("modeling");
}

ModelingEnvironment::~ModelingEnvironment()
{
}

void ModelingEnvironment::onCommandStartFailed(
    wyap::Command* pCmd,
    wyap::CmdExecution::StartResult startResult)
{
    EnvironmentBase::onCommandStartFailed(pCmd, startResult);
}

void ModelingEnvironment::onCommandStarted(wyap::Command* pCmd)
{
    EnvironmentBase::onCommandStarted(pCmd);
}

void ModelingEnvironment::onCommandEnded(wyap::Command* pCmd)
{
    EnvironmentBase::onCommandEnded(pCmd);
}

void ModelingEnvironment::onCommandAborted(
    wyap::Command* pCmd,
    wyap::CmdExecution::AbortCause abortCause)
{
    EnvironmentBase::onCommandAborted(pCmd, abortCause);
}

void ModelingEnvironment::onEnter()
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

void ModelingEnvironment::onExit(ExitCode exitCode)
{
    _pUI->teardown(this);
    this->removeCommands();

    EnvironmentBase::onExit(exitCode);
    wyap::DocumentEnvironment::onExit(exitCode);
}

void ModelingEnvironment::onSuspend()
{
    _pUI->teardown(this);
    this->removeCommands();

    EnvironmentBase::onSuspend();
    wyap::DocumentEnvironment::onSuspend();
}

void ModelingEnvironment::onResume()
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

void ModelingEnvironment::onDocumentActivated(wyap::Document* pActivatedDoc)
{
    wyap::DocumentEnvironment::onDocumentActivated(pActivatedDoc);
    this->syncDisplayModeAction();
}

void ModelingEnvironment::updateUndoRedoActionStates()
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

void ModelingEnvironment::updateFileActionStates()
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