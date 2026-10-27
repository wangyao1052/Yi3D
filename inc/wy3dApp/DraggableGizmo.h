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

#ifndef WY3DAPP_DRAGGABLE_GIZMO_H
#define WY3DAPP_DRAGGABLE_GIZMO_H

#include <utility>
#include <vector>

#include <wyVector3.h>
#include <wy3dApp/Gizmo.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

// 可拖拽 Gizmo 的公共基类：在 Gizmo 之上提供 Operation 与 Renderer 的拖拽编排。
// 非拖拽 Gizmo 直接派生 Gizmo，不引入这些接口。
class WY3DAPP_EXPORT DraggableGizmo : public Gizmo
{
public:
    explicit DraggableGizmo(wydb::ElementId id) : Gizmo(id) {}
    explicit DraggableGizmo(std::vector<wydb::ElementId>&& ids) : Gizmo(std::move(ids)) {}
    explicit DraggableGizmo(const std::vector<wydb::ElementId>& ids) : Gizmo(ids) {}
    virtual ~DraggableGizmo();

protected:
    // 拖拽编排：子类创建 _pGizmoOp 后调用，统一 Operation 与 Renderer 的调用顺序。
    bool beginDrag(const wy::Vector3& startPos);
    bool dragTo(const wy::Vector3& curPos);
    bool endDrag(const wy::Vector3& endPos);
    bool cancelDrag();
};

} // namespace wy3dApp

#endif // WY3DAPP_DRAGGABLE_GIZMO_H
