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

#include "SketchEntityGripGizmoImpl.h"

#include <cassert>
#include <utility>

#include "application/ApplicationImpl.h"
#include <wyapGizmoManager.h>

namespace wy3dApp {

namespace {

// 夹点重合判定容差（与矩形识别默认 distTol 一致）
constexpr double kCoincidentTol = 1e-6;

} // namespace

bool SketchEntityGripGizmoImpl::begin(
    SketchEntityGripGizmo& owner, wydb::Database* pDb)
{
    this->clear();
    if (!owner.onStartEdit(pDb)) return false;

    this->collect(owner, pDb);
    return true;
}

bool SketchEntityGripGizmoImpl::update(
    SketchEntityGripGizmo& owner,
    wydb::Transaction* pTrans,
    const wy::Vector2& curPos)
{
    if (!owner.onApplyEdit(pTrans, curPos)) return false;

    for (const std::weak_ptr<SketchEntityGripGizmo>& weakOther : _grips)
    {
        std::shared_ptr<SketchEntityGripGizmo> pOther = weakOther.lock();
        if (!pOther) continue;
        if (!pOther->onApplyEdit(pTrans, curPos)) return false;
    }
    return true;
}

void SketchEntityGripGizmoImpl::end(
    SketchEntityGripGizmo& owner, wydb::Database* pDb, bool commit)
{
    owner.onEndEdit(pDb, commit);

    for (const std::weak_ptr<SketchEntityGripGizmo>& weakOther : _grips)
    {
        std::shared_ptr<SketchEntityGripGizmo> pOther = weakOther.lock();
        if (pOther) pOther->onEndEdit(pDb, commit);
    }

    this->clear();
}

void SketchEntityGripGizmoImpl::clear()
{
    _grips.clear();
    _snapExcludeIds.clear();
}

const std::set<wydb::ElementId>& SketchEntityGripGizmoImpl::snapExcludeIds() const
{
    return _snapExcludeIds;
}

void SketchEntityGripGizmoImpl::collect(
    SketchEntityGripGizmo& owner, wydb::Database* pDb)
{
    if (!pDb)
    {
        assert(false);
        return;
    }

    wyap::GizmoManager* pGizmoMgr = ApplicationImpl::instance().getGizmoManager();
    assert(pGizmoMgr);
    if (!pGizmoMgr) return;

    if (sketchId.isNull()) return;

    const wy::Vector2 gripPos = owner.getGripPosition();
    const double tol2 = kCoincidentTol * kCoincidentTol;

    std::set<wydb::ElementId> excludeIds;
    for (auto iter = pGizmoMgr->createIterator(); !iter.isDone(); iter.moveNext())
    {
        std::shared_ptr<SketchEntityGripGizmo> pOther =
            std::dynamic_pointer_cast<SketchEntityGripGizmo>(iter.current());
        if (!pOther || pOther.get() == &owner) continue;
        if (pOther->_pImpl->sketchId != sketchId) continue;
        if ((pOther->getGripPosition() - gripPos).length2() > tol2) continue;
        if (!pOther->onStartEdit(pDb)) continue;

        _grips.push_back(pOther);
        const std::vector<wydb::ElementId>& ids = pOther->getElementIds();
        excludeIds.insert(ids.begin(), ids.end());
    }

    _snapExcludeIds = std::move(excludeIds);
}

} // namespace wy3dApp
