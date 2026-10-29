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

#include <wyVector2.h>
#include <wyVector3.h>
#include <wy3dApp/SketchEntityGripGizmo.h>
#include "SketchEntityGripGizmoImpl.h"
#include <cassert>
#include <set>
#include <utility>
#include <wy3dApp/GuiCommand.h> // MouseEvent
#include "commands/OsgCoordUtil.h"
#include <wy3dApp/Application.h>
#include "application/ApplicationImpl.h"
#include "commands/OsgCoordUtil.h"
#include "utils/MathUtils.h"
#include <wy3dApp/SketchEnvironment.h>
#include "snap/SketchSnapSystem.h"
#include "gizmo/renderer/SketchEntityGripRenderer.h"
#include <wy3dApp/OsgGizmoRenderer.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

namespace {

std::vector<wydb::ElementId> normalizeIds(std::vector<wydb::ElementId> ids)
{
    if (!ids.empty()) return ids;
    return { wydb::ElementId::kNull };
}

} // namespace

SketchEntityGripGizmo::SketchEntityGripGizmo(
    wydb::ElementId sketchId,
    wydb::ElementId entityId)
    : SketchEntityGripGizmo(sketchId, std::vector<wydb::ElementId>{ entityId })
{
}

SketchEntityGripGizmo::SketchEntityGripGizmo(
    wydb::ElementId sketchId,
    std::vector<wydb::ElementId> ids)
    : DraggableGizmo(normalizeIds(std::move(ids))),
      _pImpl(std::make_unique<SketchEntityGripGizmoImpl>())
{
    _pImpl->sketchId = sketchId;

    const wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    assert(pDb);
    const wy3d::Sketch* pSketch = pDb
        ? wy3d::Sketch::cast(pDb->getElement(sketchId))
        : nullptr;
    if (!pSketch) assert(false);

    _pRenderer = std::make_unique<SketchEntityGripRenderer>(
        this,
        pSketch ? pSketch->getPlane() : wy3d::Plane());
}

SketchEntityGripGizmo::~SketchEntityGripGizmo()
{
}

static SketchSnapSystem* getCurSketchSnapSystem()
{
    wyap::Environment* pCurEnv = ApplicationImpl::instance().getEnvManager()->getActiveEnvironment();
    const SketchEnvironment* pSketchEnv = dynamic_cast<const SketchEnvironment*>(pCurEnv);
    if (pSketchEnv)
    {
        return pSketchEnv->getSnapSystem();
    }
    else
    {
        assert(false);
        return nullptr;
    }
}

DraggableGizmo::StepResult SketchEntityGripGizmo::onStartDrag(
    wydb::Database* pDb, const MouseEvent&)
{
    if (!pDb)
    {
        assert(false);
        return { false, wy::Vector3() };
    }

    wy::Vector2 startPos2 = this->getGripPosition();

    if (!_pImpl->begin(*this, pDb))
    {
        return { false, wy::Vector3() };
    }

    return { true, wy::Vector3(startPos2.x(), startPos2.y(), 0.0) };
}

void SketchEntityGripGizmo::onCancelDrag(wydb::Database* pDb)
{
    _pImpl->end(*this, pDb, false);
}

DraggableGizmo::StepResult SketchEntityGripGizmo::onDragging(wydb::Database* pDb, const MouseEvent& event)
{
    SketchSnapSystem* pSketchSnapSys = getCurSketchSnapSystem();
    if (!pSketchSnapSys)
    {
        return { false, wy::Vector3() };
    }
    std::set<wydb::ElementId> excludeIds(_ids.begin(), _ids.end());
    if (!_pImpl->sketchId.isNull())
    {
        excludeIds.insert(_pImpl->sketchId);
    }
    const std::set<wydb::ElementId>& synchronizedExcludeIds = _pImpl->snapExcludeIds();
    excludeIds.insert(synchronizedExcludeIds.begin(), synchronizedExcludeIds.end());
    wy::Vector2 curPos = OsgCoordUtil::computePoint2d(
        static_cast<OsgGizmoRenderer*>(_pRenderer.get())->getOsgView(),
        event.x, event.y, excludeIds, _pImpl->snap2dContext, pSketchSnapSys).first;
    const wy::Vector3 pos = MathUtils::toVector3(curPos);

    wydb::TransactionManager* pTransMgr = pDb ? pDb->getTransactionManager() : nullptr;
    if (!pTransMgr) return { false, pos };

    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = pTransMgr->startTransaction("", option);
    if (!pTrans) return { false, pos };

    bool applied = _pImpl->update(*this, pTrans, curPos);
    if (applied)
    {
        if (wy::ErrorStatus::Ok != pTransMgr->endTransaction())
            return { false, pos }; // 事务异常：取消整次拖拽
        pTransMgr->mergeTransaction();
    }
    else
    {
        // 几何暂时非法（半径过小、半径比越界等）：只丢弃这一帧，保持拖拽继续
        pTransMgr->abortTransaction();
    }

    return { true, pos };
}

DraggableGizmo::StepResult SketchEntityGripGizmo::onEndDrag(wydb::Database* pDb, const MouseEvent& event)
{
    StepResult result = this->onDragging(pDb, event);
    if (!result.result) return result;

    _pImpl->end(*this, pDb, true);
    return result;
}

void SketchEntityGripGizmo::onDragStarted()
{
    // added by wangyao 2025.04.10 {
    _pImpl->snap2dContext = this->createSnap2dContext();
    SketchSnapSystem* pSketchSnapSys = getCurSketchSnapSystem();
    assert(pSketchSnapSys);
    if (pSketchSnapSys)
        pSketchSnapSys->clearSnapResult();
    // }
}

void SketchEntityGripGizmo::onDragEnded()
{
    _pImpl->snap2dContext = nullptr;
    _pImpl->clear();

    // added by wangyao 2025.04.10 {
    // 拖拽完成之后清空草图捕捉结果
    SketchSnapSystem* pSketchSnapSys = getCurSketchSnapSystem();
    if (pSketchSnapSys) pSketchSnapSys->clearSnapResult();
    // }
}

void SketchEntityGripGizmo::onDragCancelled()
{
    _pImpl->snap2dContext = nullptr;
    _pImpl->clear();

    // added by wangyao 2025.04.10 {
    // 取消拖拽之后清空草图捕捉结果
    SketchSnapSystem* pSketchSnapSys = getCurSketchSnapSystem();
    assert(pSketchSnapSys);
    if (pSketchSnapSys) pSketchSnapSys->clearSnapResult();
    // }
}

} // namespace wy3dApp
