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

#include "SketchEnvironmentImpl.h"
#include "SketchEnvironmentUI.h"

#include <cassert>

#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dSketch.h>

#include <wy3dApp/Application.h>
#include <wy3dApp/CommandAction.h>
#include "application/ApplicationImpl.h"
#include "commands/CommandNames.h"
#include <wy3dApp/Scene.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

SketchEnvironmentImpl::SketchEnvironmentImpl(const wy3d::Plane& sketchPlane)
    : _op(Operation::New)
    , _sketchId(wydb::ElementId::kNull)
    , _sketchPlane(sketchPlane)
    , _pTopTrans(nullptr)
    , _isTransCommitted(false)
{
    assert(_sketchPlane.isValid());
    setName("sketch");
    _pUI = std::make_unique<SketchEnvironmentUI>(this);
}

SketchEnvironmentImpl::SketchEnvironmentImpl(const wy3d::Sketch* pSketch)
    : _op(Operation::Edit)
    , _sketchId(wydb::ElementId::kNull)
    , _sketchPlane()
    , _pTopTrans(nullptr)
    , _isTransCommitted(false)
{
    assert(pSketch);
    setName("sketch");
    _pUI = std::make_unique<SketchEnvironmentUI>(this);
    _sketchId = pSketch->getId();
    _sketchPlane = pSketch->getPlane();
}

SketchEnvironmentImpl::~SketchEnvironmentImpl()
{
}

SketchEnvironment::Operation SketchEnvironmentImpl::getOperation() const
{
    return _op;
}

wydb::ElementId SketchEnvironmentImpl::getSketchId() const
{
    return _sketchId;
}

wydb::Transaction* SketchEnvironmentImpl::getSketchTransaction() const
{
    return _pTopTrans;
}

const wy3d::Plane& SketchEnvironmentImpl::getSketchPlane() const
{
    return _sketchPlane;
}

void SketchEnvironmentImpl::setSketchPlane(const wy3d::Plane& sketchPlane)
{
    _sketchPlane = sketchPlane;
}

bool SketchEnvironmentImpl::isTransactionCommited() const
{
    return _isTransCommitted;
}

SketchSnapSystem* SketchEnvironmentImpl::getSnapSystem() const
{
    return _pSketchSnapSys.get();
}

EnvironmentUI* SketchEnvironmentImpl::getEnvironmentUI() const
{
    return _pUI.get();
}

void SketchEnvironmentImpl::onCommandStartFailed(
    wyap::Command* pCmd,
    wyap::CmdExecution::StartResult startResult)
{
    _pUI->onCommandStartFailed(pCmd, startResult);
}

void SketchEnvironmentImpl::onCommandStarted(wyap::Command* pCmd)
{
    _pUI->onCommandStarted(pCmd);
}

void SketchEnvironmentImpl::onCommandEnded(wyap::Command* pCmd)
{
    _pUI->onCommandEnded(pCmd);
}

void SketchEnvironmentImpl::onCommandAborted(
    wyap::Command* pCmd,
    wyap::CmdExecution::AbortCause abortCause)
{
    _pUI->onCommandAborted(pCmd, abortCause);
}

void SketchEnvironmentImpl::onEnter()
{
    wyap::TaskEnvironment::onEnter();
    _pUI->onEnter();

    this->registerCommands();
    _pUI->initialize();

    // 清空选择集
    ApplicationImpl::instance().getSelManager()->beginChange();
    ApplicationImpl::instance().getSelManager()->clearSelections();
    ApplicationImpl::instance().getSelManager()->endChange();

    // Drop the leftover modeling snap marker; the 2D layer draws its own.
    ApplicationImpl::instance().getSnapSystem()->clearSnapResult();

    // 同步显示模式按钮状态
    _pUI->syncDisplayModeAction();

    // 开启顶层事务组
    wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
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
    ApplicationImpl::instance().getCmdManager()->postCommand(CommandNames::OrientToSketch);
    ApplicationImpl::instance().getCmdManager()->postCommand(CommandNames::Select);

    // Update command action states.
    this->updateCommandActionStates();
}

wy3d::Sketch* SketchEnvironmentImpl::newSketch(wydb::Database* pDb)
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

void SketchEnvironmentImpl::onExit(ExitCode exitCode)
{
    // 清空选择集
    ApplicationImpl::instance().getSelManager()->beginChange();
    ApplicationImpl::instance().getSelManager()->clearSelections();
    ApplicationImpl::instance().getSelManager()->endChange();

    wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    assert(pDb);
    if (pDb)
    {
        // remove db reactor
        if (_pSketchSnapSys) pDb->removeReactor(_pSketchSnapSys.get());

        // 提交草图
        this->commitSketch(pDb, ExitCode::Ok == exitCode ? true : false);
    }

    // 草图视图
    ApplicationImpl::instance().getCmdManager()->postCommand(CommandNames::IsometricView);

    _pUI->teardown();
    this->removeCommands();

    _pUI->onExit(exitCode);
    wyap::TaskEnvironment::onExit(exitCode);
}

void SketchEnvironmentImpl::commitSketch(wydb::Database* pDb, bool ok)
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

void SketchEnvironmentImpl::onSuspend()
{
    _pUI->teardown();
    this->removeCommands();

    // 清空选择集
    ApplicationImpl::instance().getSelManager()->beginChange();
    ApplicationImpl::instance().getSelManager()->clearSelections();
    ApplicationImpl::instance().getSelManager()->endChange();

    _pUI->onSuspend();
    wyap::TaskEnvironment::onSuspend();
}

void SketchEnvironmentImpl::onResume()
{
    wyap::TaskEnvironment::onResume();
    _pUI->onResume();

    this->registerCommands();
    _pUI->initialize();

    // 清空选择集
    ApplicationImpl::instance().getSelManager()->beginChange();
    ApplicationImpl::instance().getSelManager()->clearSelections();
    ApplicationImpl::instance().getSelManager()->endChange();

    this->updateCommandActionStates();

    // 同步显示模式按钮状态
    _pUI->syncDisplayModeAction();
}

void SketchEnvironmentImpl::updateUndoRedoActionStates()
{
    QAction* pUndoAction = _pUI->findCommandAction(CommandNames::Undo);
    QAction* pRedoAction = _pUI->findCommandAction(CommandNames::Redo);
    if (!pUndoAction || !pRedoAction)
    {
        assert(false);
        return;
    }

    wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
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

void SketchEnvironmentImpl::updateFileActionStates()
{
    // 草图环境不支持文件操作:
    // 无文件动作需要更新,文件操作动作也不存在于此环境。
}

} // namespace wy3dApp
