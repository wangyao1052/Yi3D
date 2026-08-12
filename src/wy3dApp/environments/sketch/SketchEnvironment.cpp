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

#include "SketchEnvironment.h"
#include "SketchEnvironmentUI.h"

#include <cassert>

#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dSketch.h>

#include "application/Application.h"
#include "commands/CommandNames.h"
#include "scene/Scene.h"

SketchEnvironment::SketchEnvironment(const wy3d::SketchPlane& sketchPlane)
    : wyap::TaskEnvironment()
    , _pUI(std::make_unique<SketchEnvironmentUI>())
    , _op(Operation::New)
    , _sketchId(wydb::ElementId::kNull)
    , _sketchPlane(sketchPlane)
    , _pTopTrans(nullptr)
    , _isTransCommitted(false)
{
    assert(_sketchPlane.isValid());
    setName("sketch");
}

SketchEnvironment::SketchEnvironment(const wy3d::Sketch* pSketch)
    : wyap::TaskEnvironment()
    , _pUI(std::make_unique<SketchEnvironmentUI>())
    , _op(Operation::Edit)
    , _sketchId(wydb::ElementId::kNull)
    , _sketchPlane()
    , _pTopTrans(nullptr)
    , _isTransCommitted(false)
{
    assert(pSketch);
    setName("sketch");
    _sketchId = pSketch->getId();
    _sketchPlane = pSketch->getPlane();
}

SketchEnvironment::~SketchEnvironment()
{
}

void SketchEnvironment::onCommandStartFailed(
    wyap::Command* pCmd,
    wyap::CmdExecution::StartResult startResult)
{
    EnvironmentBase::onCommandStartFailed(pCmd, startResult);
}

void SketchEnvironment::onCommandStarted(wyap::Command* pCmd)
{
    EnvironmentBase::onCommandStarted(pCmd);
}

void SketchEnvironment::onCommandEnded(wyap::Command* pCmd)
{
    EnvironmentBase::onCommandEnded(pCmd);
}

void SketchEnvironment::onCommandAborted(
    wyap::Command* pCmd,
    wyap::CmdExecution::AbortCause abortCause)
{
    EnvironmentBase::onCommandAborted(pCmd, abortCause);
}

void SketchEnvironment::onEnter()
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

    // 新建草图
    if (Operation::New == _op)
    {
        // 创建草图
        // 注:创建草图的事务在草图环境下是不能回退的(具体可查看函数UndoRedoUtil::refreshUndoRedoActionState())
        wy3d::Sketch* pNewSketch = this->newSketch(pDb);
        if (!pNewSketch)
        {
            assert(false);
            pDb->getTransactionManager()->abortTransaction(); // 回退顶级事务
            return;
        }
        _sketchId = pNewSketch->getId();

        // 初始化草图捕捉系统
        _pSketchSnapSys = std::make_shared<SketchSnapSystem>(pNewSketch);
        pDb->addReactor(_pSketchSnapSys.get());
    }
    // 编辑草图
    else
    {
        // 校验
        const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(_sketchId));
        if (!pSketch)
        {
            assert(false);
            pDb->getTransactionManager()->abortTransaction();
            return;
        }

        // 初始化草图捕捉系统
        _pSketchSnapSys = std::make_shared<SketchSnapSystem>(pSketch);
        pDb->addReactor(_pSketchSnapSys.get());
    }
    _pTopTrans = pGroupTrans;

    // 草图视图
    Application::instance().getCmdManager()->postCommand(CommandNames::OrientToSketch);
    Application::instance().getCmdManager()->postCommand(CommandNames::Select);

    // Update command action states.
    this->updateCommandActionStates();
}

wy3d::Sketch* SketchEnvironment::newSketch(wydb::Database* pDb)
{
    assert(pDb);

    wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
    if (!pTrans)
    {
        assert(false);
        return nullptr;
    }

    wy3d::Sketch* pSketch(nullptr);
    wy::ErrorStatus error = wy3d::Sketch::create(pTrans, _sketchPlane, pSketch);
    if (wy::ErrorStatus::Ok != error)
    {
        assert(false);
        return nullptr;
    }
    assert(pSketch);

    error = pDb->getTransactionManager()->endTransaction();
    assert(wy::ErrorStatus::Ok == error);

    return pSketch;
}

void SketchEnvironment::onExit(ExitCode exitCode)
{
    // 清空选择集
    Application::instance().getSelManager()->beginChange();
    Application::instance().getSelManager()->clearSelections();
    Application::instance().getSelManager()->endChange();

    wydb::Database* pDb = Application::instance().getActiveDatabase();
    assert(pDb);
    if (pDb)
    {
        // remove db reactor
        if (_pSketchSnapSys) pDb->removeReactor(_pSketchSnapSys.get());

        // 提交草图
        this->commitSketch(pDb, ExitCode::Ok == exitCode ? true : false);
    }

    // 草图视图
    Application::instance().getCmdManager()->postCommand(CommandNames::IsometricView);

    _pUI->teardown(this);
    this->removeCommands();

    EnvironmentBase::onExit(exitCode);
    wyap::TaskEnvironment::onExit(exitCode);
}

void SketchEnvironment::commitSketch(wydb::Database* pDb, bool ok)
{
    assert(pDb);
    assert(!_isTransCommitted);
    if (!ok)
    {
        pDb->getTransactionManager()->abortTransaction();
        return;
    }

    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(_sketchId));
    if (!pSketch)
    {
        assert(false);
        pDb->getTransactionManager()->abortTransaction();
        return;
    }

    // 草图为空则取消事务
    if (pSketch->createIterator().isDone())
    {
        pDb->getTransactionManager()->abortTransaction();
    }
    // 提交事务
    else
    {
        // mark sketch shape dirty to execute chain updaters
        wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
        assert(pTrans);
        if (pTrans)
        {
            wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(_sketchId));
            assert(pSketch);
            pSketch->regenerate();
            pDb->getTransactionManager()->endTransaction();
        }

        pDb->getTransactionManager()->endTransaction();
        _isTransCommitted = true;
    }
}

void SketchEnvironment::onSuspend()
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

void SketchEnvironment::onResume()
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

void SketchEnvironment::updateUndoRedoActionStates()
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
        if (SketchEnvironment::Operation::New == this->getOperation())
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

