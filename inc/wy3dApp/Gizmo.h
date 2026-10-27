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

#ifndef WY3DAPP_GIZMO_H
#define WY3DAPP_GIZMO_H

#include <cassert>
#include <memory>
#include <utility>
#include <vector>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapGizmo.h>

#include <wy3dApp/GizmoOperation.h>
#include <wy3dApp/GizmoRenderer.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

struct MouseEvent;
struct KeyEvent;

class WY3DAPP_EXPORT Gizmo : public wyap::Gizmo
{
public:
    explicit Gizmo(wydb::ElementId id) : _ids{ id } {}
    explicit Gizmo(std::vector<wydb::ElementId>&& ids) : _ids(std::move(ids))
    {
        assert(!_ids.empty());
    }
    explicit Gizmo(const std::vector<wydb::ElementId>& ids) : _ids(ids)
    {
        assert(!_ids.empty());
    }
    virtual ~Gizmo();

    virtual void onLeftMouseDown(const MouseEvent& event) {}
    virtual void onLeftMouseDrag(const MouseEvent& event) {}
    virtual void onLeftMouseUp(const MouseEvent& event) {}
    virtual void onEscapeKey() {}

    GizmoRenderer* getRenderer() const { return _pRenderer.get(); }

    const std::vector<wydb::ElementId>& getElementIds() const { return _ids; }

    wydb::ElementId getFirstElementId() const
    {
        return _ids.empty() ? wydb::ElementId::kNull : _ids.front();
    }

    // 刷新视觉状态（委托给 Renderer）
    virtual void refresh() override { if (_pRenderer) _pRenderer->refresh(); }

protected:
    virtual void onActivate() override;
    virtual void onDeactivate() override;

    void setElementIds(const std::vector<wydb::ElementId>& ids)
    {
        _ids = ids;
        assert(!_ids.empty());
    }
    void setElementIds(std::vector<wydb::ElementId>&& ids)
    {
        _ids = std::move(ids);
        assert(!_ids.empty());
    }

    // 关联的元素ID（1 个或多个）
    std::vector<wydb::ElementId> _ids;

    // Gizmo 操作（拖拽事务管理）
    std::unique_ptr<GizmoOperation> _pGizmoOp;
    // Gizmo 渲染器（拖拽视觉反馈）
    // 子类必须把它设置为 OsgGizmoRenderer 或其派生类：Scene 的挂载/拾取和
    // 事件分发都依赖这一契约。
    std::unique_ptr<GizmoRenderer> _pRenderer;
};

#define PICK_GIZMO_MASK 0x0000FFFF

} // namespace wy3dApp

#endif // WY3DAPP_BASE_GIZMO_H
