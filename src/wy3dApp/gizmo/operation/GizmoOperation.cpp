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

#include <wy3dApp/GizmoOperation.h>
#include <cassert>
#include <wy3dApp/Gizmo.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

GizmoOperation::GizmoOperation(wydb::Database* pDb, Gizmo* pGizmo)
    : _pDb(pDb), _pGizmo(pGizmo), _pTopTransaction(nullptr)
{
    assert(_pDb);
    assert(_pGizmo);

    _pTopTransaction = _pDb->getTransactionManager()->startTransactionGroup();
    if (!_pTopTransaction)
    {
        assert(false);
    }
}

GizmoOperation::~GizmoOperation()
{
    if (_pTopTransaction)
    {
        this->abortTopTransaction();
    }
}

void GizmoOperation::commitTopTransaction()
{
    if (_pTopTransaction)
    {
        wy::ErrorStatus error = _pDb->getTransactionManager()->endTransaction();
        assert(wy::ErrorStatus::Ok == error);
        _pTopTransaction = nullptr;
    }
}

void GizmoOperation::abortTopTransaction()
{
    if (_pTopTransaction)
    {
        wy::ErrorStatus error = _pDb->getTransactionManager()->abortTransaction();
        assert(wy::ErrorStatus::Ok == error);
        _pTopTransaction = nullptr;
    }
}

} // namespace wy3dApp
