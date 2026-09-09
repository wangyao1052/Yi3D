///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 Wang Yao <wangyao1052@163.com>
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

#include "Sketch3DEnvironment.h"
#include "Sketch3DEnvironmentUI.h"

#include <cassert>

#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dSketch3D.h>

#include "application/Application.h"
#include "commands/CommandNames.h"

Sketch3DEnvironment::Sketch3DEnvironment()
    : wyap::TaskEnvironment()
    , _pUI(std::make_unique<Sketch3DEnvironmentUI>())
    , _op(Operation::New)
    , _sketch3dId(wydb::ElementId::kNull)
    , _pTopTrans(nullptr)
    , _isTransCommitted(false)
{
    setName("sketch3d");
}

Sketch3DEnvironment::Sketch3DEnvironment(const wy3d::Sketch3D* pSketch3D)
    : wyap::TaskEnvironment()
    , _pUI(std::make_unique<Sketch3DEnvironmentUI>())
    , _op(Operation::Edit)
    , _sketch3dId(wydb::ElementId::kNull)
    , _pTopTrans(nullptr)
    , _isTransCommitted(false)
{
    assert(pSketch3D);
    setName("sketch3d");
    _sketch3dId = pSketch3D->getId();
}

Sketch3DEnvironment::~Sketch3DEnvironment()
{
}

void Sketch3DEnvironment::onCommandStartFailed(
    wyap::Command* pCmd,
    wyap::CmdExecution::StartResult startResult)
{
    EnvironmentBase::onCommandStartFailed(pCmd, startResult);
}

void Sketch3DEnvironment::onCommandStarted(wyap::Command* pCmd)
{
    EnvironmentBase::onCommandStarted(pCmd);
}

void Sketch3DEnvironment::onCommandEnded(wyap::Command* pCmd)
{
    EnvironmentBase::onCommandEnded(pCmd);
}

void Sketch3DEnvironment::onCommandAborted(
    wyap::Command* pCmd,
    wyap::CmdExecution::AbortCause abortCause)
{
    EnvironmentBase::onCommandAborted(pCmd, abortCause);
}

void Sketch3DEnvironment::onEnter()
{
    wyap::TaskEnvironment::onEnter();
    EnvironmentBase::onEnter();

    this->registerCommands();
    _pUI->initialize(this);

    // 清空选择集
    Application::instance().getSelManager()->beginChange();
    Application::instance().getSelManager()->clearSelections();
    Application::instance().getSelManager()->endChange();

    // 同步显示模式按钮状态
    this->syncDisplayModeAction();

    // 开启顶层事务组
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return;
    }
    wydb::Transaction* pGroupTrans = pDb->getTransactionManager()->startTransactionGroup();
    if (!pGroupTrans)
    {
        assert(false);
        return;
    }

    // 新建3D草图
    if (Operation::New == _op)
    {
        // 注:创建草图的事务在草图环境下是不能回退的(同2D草图)
        wy3d::Sketch3D* pNewSketch3D = this->newSketch3D(pDb);
        if (!pNewSketch3D)
        {
            assert(false);
            pDb->getTransactionManager()->abortTransaction(); // 回退顶级事务
            return;
        }
        _sketch3dId = pNewSketch3D->getId();
    }
    // 编辑3D草图
    else
    {
        // 校验
        const wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pDb->getElement(_sketch3dId));
        if (!pSketch3D)
        {
            assert(false);
            pDb->getTransactionManager()->abortTransaction();
            return;
        }
    }
    _pTopTrans = pGroupTrans;

    Application::instance().getCmdManager()->postCommand(CommandNames::Select);

    // Update command action states.
    this->updateCommandActionStates();
}

wy3d::Sketch3D* Sketch3DEnvironment::newSketch3D(wydb::Database* pDb)
{
    assert(pDb);

    wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
    if (!pTrans)
    {
        assert(false);
        return nullptr;
    }

    wy3d::Sketch3D* pSketch3D(nullptr);
    wy::ErrorStatus error = wy3d::Sketch3D::create(pTrans, pSketch3D);
    if (wy::ErrorStatus::Ok != error)
    {
        assert(false);
        return nullptr;
    }
    assert(pSketch3D);

    error = pDb->getTransactionManager()->endTransaction();
    assert(wy::ErrorStatus::Ok == error);

    return pSketch3D;
}

void Sketch3DEnvironment::onExit(ExitCode exitCode)
{
    // 清空选择集
    Application::instance().getSelManager()->beginChange();
    Application::instance().getSelManager()->clearSelections();
    Application::instance().getSelManager()->endChange();

    wydb::Database* pDb = Application::instance().getActiveDatabase();
    assert(pDb);
    if (pDb)
    {
        // 提交3D草图
        this->commitSketch3D(pDb, ExitCode::Ok == exitCode ? true : false);
    }

    _pUI->teardown(this);
    this->removeCommands();

    EnvironmentBase::onExit(exitCode);
    wyap::TaskEnvironment::onExit(exitCode);
}

void Sketch3DEnvironment::commitSketch3D(wydb::Database* pDb, bool ok)
{
    assert(pDb);
    assert(!_isTransCommitted);
    if (!ok)
    {
        pDb->getTransactionManager()->abortTransaction();
        return;
    }

    const wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pDb->getElement(_sketch3dId));
    if (!pSketch3D)
    {
        assert(false);
        pDb->getTransactionManager()->abortTransaction();
        return;
    }

    // 3D草图为空则取消事务
    if (pSketch3D->createIterator().isDone())
    {
        pDb->getTransactionManager()->abortTransaction();
    }
    // 提交事务
    else
    {
        // mark sketch dirty to execute chain updaters
        // (refreshes the container node's render data so newly drawn entities show up)
        wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
        assert(pTrans);
        if (pTrans)
        {
            wy3d::Sketch3D* pSketch3DWrite = wy3d::Sketch3D::cast(pTrans->getElementForWrite(_sketch3dId));
            assert(pSketch3DWrite);
            pSketch3DWrite->regenerate();
            pDb->getTransactionManager()->endTransaction();
        }

        pDb->getTransactionManager()->endTransaction();
        _isTransCommitted = true;
    }
}

void Sketch3DEnvironment::onSuspend()
{
    _pUI->teardown(this);
    this->removeCommands();

    // 清空选择集
    Application::instance().getSelManager()->beginChange();
    Application::instance().getSelManager()->clearSelections();
    Application::instance().getSelManager()->endChange();

    EnvironmentBase::onSuspend();
    wyap::TaskEnvironment::onSuspend();
}

void Sketch3DEnvironment::onResume()
{
    wyap::TaskEnvironment::onResume();
    EnvironmentBase::onResume();

    this->registerCommands();
    _pUI->initialize(this);

    // 清空选择集
    Application::instance().getSelManager()->beginChange();
    Application::instance().getSelManager()->clearSelections();
    Application::instance().getSelManager()->endChange();

    this->updateCommandActionStates();

    // 同步显示模式按钮状态
    this->syncDisplayModeAction();
}

void Sketch3DEnvironment::updateUndoRedoActionStates()
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
        assert(false);
        pUndoAction->setEnabled(false);
        pRedoAction->setEnabled(false);
        return;
    }

    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();
    wydb::Transaction* pActiveTrans = pTransMgr->getActiveTransaction();
    if (!pActiveTrans)
    {
        pUndoAction->setEnabled(false);
        pRedoAction->setEnabled(false);
        return;
    }

    if (this->getSketchTransaction() == pActiveTrans)
    {
        // Creating a new sketch.
        if (Sketch3DEnvironment::Operation::New == this->getOperation())
        {
            // In a new-sketch session, the first child transaction in the top-level
            // transaction group creates the sketch itself and is not undoable.
            size_t numUndoRecords = pTransMgr->getUndoRecordCount();
            pUndoAction->setEnabled((pTransMgr->canUndo() && numUndoRecords > 1) ? true : false);
            pRedoAction->setEnabled(pTransMgr->canRedo() ? true : false);
        }
        // Editing an existing sketch.
        else
        {
            pUndoAction->setEnabled(pTransMgr->canUndo() ? true : false);
            pRedoAction->setEnabled(pTransMgr->canRedo() ? true : false);
        }
    }
    else
    {
        pUndoAction->setEnabled(false);
        pRedoAction->setEnabled(false);
    }
}

void Sketch3DEnvironment::updateFileActionStates()
{
    // 3D草图环境不支持文件操作
}
