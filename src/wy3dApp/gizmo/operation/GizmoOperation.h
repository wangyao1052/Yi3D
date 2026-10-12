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

#ifndef WY3DAPP_GIZMO_OPERATION_H
#define WY3DAPP_GIZMO_OPERATION_H

#include <wyVector3.h>
#include <wy3dVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class BaseGizmo;

class GizmoOperation
{
public:
    GizmoOperation(wydb::Database* pDb, BaseGizmo* pGizmo);
    virtual ~GizmoOperation();

    virtual bool onBeginDrag(const wy::Vector3& startPos) = 0;
    virtual bool onDragging(const wy::Vector3& curPos) = 0;
    virtual void onEndDrag(const wy::Vector3& endPos) = 0;
    virtual void onCancelDrag() = 0;

protected:
    void commitTopTransaction();
    void abortTopTransaction();

protected:
    wydb::Database* _pDb;
    wydb::Transaction* _pTopTransaction;
    BaseGizmo* _pGizmo;
};

} // namespace wy3dApp

#endif // WY3DAPP_GIZMO_OPERATION_H
