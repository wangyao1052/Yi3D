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

#include "UndoRedoCommands.h"
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapCmdManager.h>
#include <wyapSelManager.h>
#include <wyapEnvironment.h>
#include <wyapEnvManager.h>
#include "application/Application.h"
#include <wy3dApp/SketchEnvironment.h>
#include "commands/CommandNames.h"
#include "scene/Scene.h"


int UndoCommand::run()
{
    // Get the active database.
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return 0;
    }

    // Undo.
    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();
    if (pTransMgr->canUndo())
    {
        wy::ErrorStatus error = pTransMgr->undo();
        assert(wy::ErrorStatus::Ok == error);

        // Clear selections.
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();
    }

    // Update command action states.
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

    return 0;
}

int RedoCommand::run()
{
    // Get the active database.
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return 0;
    }

    // Redo.
    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();
    if (pTransMgr->canRedo())
    {
        pTransMgr->redo();

        // Clear selections.
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();
    }

    // Update command action states.
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

    return 0;
}