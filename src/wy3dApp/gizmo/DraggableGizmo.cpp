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

#include <wy3dApp/DraggableGizmo.h>

namespace wy3dApp {

DraggableGizmo::~DraggableGizmo() = default;

bool DraggableGizmo::beginDrag(const wy::Vector3& startPos)
{
    assert(_pGizmoOp);
    if (!_pGizmoOp) return false;
    if (!_pGizmoOp->onBeginDrag(startPos))
    {
        _pGizmoOp = nullptr;
        return false;
    }
    if (_pRenderer) _pRenderer->onBeginDrag(startPos);
    return true;
}

bool DraggableGizmo::dragTo(const wy::Vector3& curPos)
{
    if (!_pGizmoOp) return false;
    if (!_pGizmoOp->onDragging(curPos)) return false;
    if (_pRenderer) _pRenderer->onDragging(curPos);
    return true;
}

bool DraggableGizmo::endDrag(const wy::Vector3& endPos)
{
    // 先移出成员：回调期间重入 cancelDrag() 不会二次取消同一个 operation。
    auto pOp = std::move(_pGizmoOp);
    if (!pOp) return false;
    pOp->onEndDrag(endPos);
    if (_pRenderer) _pRenderer->onEndDrag(endPos);
    return true;
}

bool DraggableGizmo::cancelDrag()
{
    // 先移出成员：回调期间重入 cancelDrag() 不会二次取消同一个 operation。
    auto pOp = std::move(_pGizmoOp);
    if (!pOp) return false;
    pOp->onCancelDrag();
    if (_pRenderer) _pRenderer->onCancelDrag();
    return true;
}

} // namespace wy3dApp
