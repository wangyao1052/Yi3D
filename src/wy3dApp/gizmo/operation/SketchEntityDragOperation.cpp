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

#include <wyVector3.h>
#include "SketchEntityDragOperation.h"
#include "gizmo/element/SketchEntityGripGizmo.h"
#include <wy3dApp/Application.h>
#include "widgets/panels/property/PropertyEditorWidget.h"
#include "utils/MathUtils.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

SketchEntityDragOperation::SketchEntityDragOperation(wydb::Database* pDb, SketchEntityGripGizmo* pGizmo)
    : GizmoOperation(pDb, pGizmo), _pGripGizmo(pGizmo)
{
    assert(_pGripGizmo);
}

SketchEntityDragOperation::~SketchEntityDragOperation()
{
}

bool SketchEntityDragOperation::onBeginDrag(const wy::Vector3& startPos)
{
    assert(_pDb);
    assert(_pGripGizmo);
    return _pGripGizmo->onBeginDrag(_pDb);
}

bool SketchEntityDragOperation::onDragging(const wy::Vector3& curPos)
{
    // 启动事务
    assert(_pDb);
    wydb::TransactionManager* pTransMgr = _pDb->getTransactionManager();
    assert(pTransMgr);
    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = pTransMgr->startTransaction("", option);
    if (!pTrans) return false;

    // 更新
    assert(_pGripGizmo);
    bool ret = _pGripGizmo->onDragging(pTrans, MathUtils::toVector2(curPos));
    if (ret)
    {
        if (wy::ErrorStatus::Ok == pTransMgr->endTransaction())
        {
            pTransMgr->mergeTransaction();
        }
    }
    else
    {
        pTransMgr->abortTransaction();
    }
    return ret;
}

void SketchEntityDragOperation::onEndDrag(const wy::Vector3& endPos)
{
    this->onDragging(endPos);
    assert(_pGripGizmo);
    assert(_pDb);
    _pGripGizmo->onEndDrag(_pDb, true);
    this->commitTopTransaction();
}

void SketchEntityDragOperation::onCancelDrag()
{
    assert(_pGripGizmo);
    assert(_pDb);
    _pGripGizmo->onEndDrag(_pDb, false);
    this->abortTopTransaction();
}

} // namespace wy3dApp
