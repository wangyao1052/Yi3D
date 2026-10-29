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

#include <cassert>
#include <utility>

#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dApp/DraggableGizmo.h>
#include <wy3dApp/KeyCode.h>
#include "application/ApplicationImpl.h"

namespace wy3dApp {

class DraggableGizmoImpl
{
public:
    ~DraggableGizmoImpl()
    {
        this->abortTopTransaction();
    }

    bool hasTopTransaction() const
    {
        return _pTopTransaction != nullptr;
    }

    bool startTopTransaction()
    {
        wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
        if (!pDb)
        {
            assert(false);
            return false;
        }
        wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();
        assert(pTransMgr);
        _pTopTransaction = pTransMgr->startTransactionGroup();
        if (_pTopTransaction)
        {
            return true;
        }
        else
        {
            assert(false);
            return false;
        }
    }

    bool commitTopTransaction()
    {
        if (_pTopTransaction)
        {
            wydb::Database* pDb = _pTopTransaction->getDatabase();
            assert(pDb);
            wy::ErrorStatus error = pDb->getTransactionManager()->endTransaction();
            _pTopTransaction = nullptr;
            if (wy::ErrorStatus::Ok == error)
            {
                return true;
            }
            else
            {
                assert(false);
                return false;
            }
        }
        else
        {
            return false;
        }
    }

    bool abortTopTransaction()
    {
        if (_pTopTransaction)
        {
            wydb::Database* pDb = _pTopTransaction->getDatabase();
            assert(pDb);
            wy::ErrorStatus error = pDb->getTransactionManager()->abortTransaction();
            _pTopTransaction = nullptr;
            if (wy::ErrorStatus::Ok == error)
            {
                return true;
            }
            else
            {
                assert(false);
                return false;
            }
        }
        else
        {
            return false;
        }
    }

    wydb::Database* getDatabase() const
    {
        if (_pTopTransaction) return _pTopTransaction->getDatabase();
        else return nullptr;
    }

private:
    wydb::Transaction* _pTopTransaction = nullptr;
};

DraggableGizmo::DraggableGizmo(wydb::ElementId id)
    : Gizmo(id),
      _pImpl(std::make_unique<DraggableGizmoImpl>())
{
}

DraggableGizmo::DraggableGizmo(std::vector<wydb::ElementId>&& ids)
    : Gizmo(std::move(ids)),
      _pImpl(std::make_unique<DraggableGizmoImpl>())
{
}

DraggableGizmo::DraggableGizmo(const std::vector<wydb::ElementId>& ids)
    : Gizmo(ids),
      _pImpl(std::make_unique<DraggableGizmoImpl>())
{
}

DraggableGizmo::~DraggableGizmo()
{
    const bool hasTopTransaction = _pImpl->hasTopTransaction();
    if (hasTopTransaction && _pRenderer)
        _pRenderer->onCancelDrag(); // 异常销毁兜底：还原 ghost 与实体显示
    assert(!hasTopTransaction);
}

void DraggableGizmo::onLeftMouseDown(const MouseEvent& event)
{
    if (_pImpl->hasTopTransaction())
    {
        assert(false);
        this->cancelDrag();
        return;
    }

    if (!_pImpl->startTopTransaction())
    {
        assert(false);
        return;
    }
    wydb::Database* pDb = _pImpl->getDatabase();
    assert(pDb);

    StepResult dragResult = this->onStartDrag(pDb, event);
    if (!dragResult.result)
    {
        bool ret = _pImpl->abortTopTransaction();
        assert(ret);
        return;
    }

    if (_pRenderer) _pRenderer->onStartDrag(dragResult.position);
    this->onDragStarted();
}

void DraggableGizmo::onLeftMouseDrag(const MouseEvent& event)
{
    if (!_pImpl->hasTopTransaction())
    {
        return;
    }

    assert(_pImpl->getDatabase());
    StepResult dragResult = this->onDragging(_pImpl->getDatabase(), event);
    if (!dragResult.result)
    {
        this->cancelDrag();
        return;
    }

    if (_pRenderer) _pRenderer->onDragging(dragResult.position);
    this->onDragMoved();
}

void DraggableGizmo::onLeftMouseUp(const MouseEvent& event)
{
    if (!_pImpl->hasTopTransaction())
    {
        return;
    }

    assert(_pImpl->getDatabase());
    StepResult dragResult = this->onEndDrag(_pImpl->getDatabase(), event);
    if (!dragResult.result)
    {
        this->cancelDrag();
        return;
    }
    bool topTransRet = _pImpl->commitTopTransaction();
    assert(topTransRet);

    if (_pRenderer) _pRenderer->onEndDrag(dragResult.position);
    this->onDragEnded();
}

void DraggableGizmo::onKeyDown(const KeyEvent& event)
{
    Gizmo::onKeyDown(event);

    switch (event.key)
    {
    case KeyCode::Escape:
        if (_pImpl->hasTopTransaction()) this->cancelDrag();
        break;

    default:
        break;
    }
}

void DraggableGizmo::onDeactivate()
{
    Gizmo::onDeactivate();

    if (_pImpl->hasTopTransaction())
    {
        this->cancelDrag();
        return;
    }
}

void DraggableGizmo::cancelDrag()
{
    if (!_pImpl->hasTopTransaction())
    {
        assert(false);
        return;
    }

    assert(_pImpl->getDatabase());
    this->onCancelDrag(_pImpl->getDatabase());
    bool abortTransRet = _pImpl->abortTopTransaction();
    assert(abortTransRet);

    if (_pRenderer) _pRenderer->onCancelDrag();

    this->onDragCancelled();
}

} // namespace wy3dApp
