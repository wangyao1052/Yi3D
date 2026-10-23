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
#include "application/ApplicationImpl.h"
#include "widgets/panels/property/PropertyEditorWidget.h"
#include "utils/MathUtils.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

namespace {

// 夹点重合判定容差（与矩形识别默认 distTol 一致）
constexpr double kCoincidentTol = 1e-6;

} // namespace

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
    if (!_pGripGizmo->onBeginDrag(_pDb)) return false;

    this->collectSynchronizedGrips();
    return true;
}

void SketchEntityDragOperation::collectSynchronizedGrips()
{
    _synchronizedGrips.clear();
    this->setSnapExcludeIds({});

    wyap::GizmoManager* pGizmoMgr = ApplicationImpl::instance().getGizmoManager();
    assert(pGizmoMgr);
    if (!pGizmoMgr) return;

    const wydb::ElementId sketchId = _pGripGizmo->_sketchId;
    if (sketchId.isNull()) return;

    const wy::Vector2 gripPos = _pGripGizmo->getGripPosition();
    const double tol2 = kCoincidentTol * kCoincidentTol;

    std::set<wydb::ElementId> excludeIds;
    for (auto iter = pGizmoMgr->createIterator(); !iter.isDone(); iter.moveNext())
    {
        std::shared_ptr<SketchEntityGripGizmo> pOther =
            std::dynamic_pointer_cast<SketchEntityGripGizmo>(iter.current());
        if (!pOther || pOther.get() == _pGripGizmo) continue;
        if (pOther->_sketchId != sketchId) continue;
        if ((pOther->getGripPosition() - gripPos).length2() > tol2) continue;
        if (!pOther->onBeginDrag(_pDb)) continue;

        _synchronizedGrips.push_back(pOther);
        const std::vector<wydb::ElementId>& ids = pOther->getElementIds();
        excludeIds.insert(ids.begin(), ids.end());
    }

    this->setSnapExcludeIds(std::move(excludeIds));
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
    const wy::Vector2 curPos2 = MathUtils::toVector2(curPos);
    bool ret = _pGripGizmo->onDragging(pTrans, curPos2);
    if (ret)
    {
        for (const std::weak_ptr<SketchEntityGripGizmo>& weakOther : _synchronizedGrips)
        {
            std::shared_ptr<SketchEntityGripGizmo> pOther = weakOther.lock();
            if (!pOther) continue;
            if (!pOther->onDragging(pTrans, curPos2))
            {
                ret = false;
                break;
            }
        }
    }
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
    for (const std::weak_ptr<SketchEntityGripGizmo>& weakOther : _synchronizedGrips)
    {
        std::shared_ptr<SketchEntityGripGizmo> pOther = weakOther.lock();
        if (pOther) pOther->onEndDrag(_pDb, true);
    }
    _synchronizedGrips.clear();
    this->setSnapExcludeIds({});
    this->commitTopTransaction();
}

void SketchEntityDragOperation::onCancelDrag()
{
    assert(_pGripGizmo);
    assert(_pDb);
    _pGripGizmo->onEndDrag(_pDb, false);
    for (const std::weak_ptr<SketchEntityGripGizmo>& weakOther : _synchronizedGrips)
    {
        std::shared_ptr<SketchEntityGripGizmo> pOther = weakOther.lock();
        if (pOther) pOther->onEndDrag(_pDb, false);
    }
    _synchronizedGrips.clear();
    this->setSnapExcludeIds({});
    this->abortTopTransaction();
}

} // namespace wy3dApp
