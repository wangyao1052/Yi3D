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

#ifndef WY3DAPP_BASE_GIZMO_H
#define WY3DAPP_BASE_GIZMO_H

#include <memory>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapGizmo.h>

#include "operation/GizmoOperation.h"
#include "renderer/GizmoRenderer.h"
#include "wy3dAppDefs.h"

namespace wy3dApp {

struct MouseEvent;
struct KeyEvent;

class BaseGizmo : public wyap::Gizmo
{
public:
    BaseGizmo(wydb::ElementId id) : _id(id) {}
    virtual ~BaseGizmo();

    virtual void onLeftMouseDown(const MouseEvent& event) {}
    virtual void onLeftMouseDrag(const MouseEvent& event) {}
    virtual void onLeftMouseUp(const MouseEvent& event) {}
    virtual void onEscapeKey() {}

    // 获取渲染器（Scene 等外部使用）
    GizmoRenderer* getRenderer() const { return _pRenderer.get(); }

    // 操作的元素ID
    wydb::ElementId getModifiedElement() const { return _id; }

    // 刷新视觉状态（委托给 Renderer）
    virtual void refresh() override { if (_pRenderer) _pRenderer->refresh(); }

protected:
    virtual void onActivate() override;
    virtual void onDeactivate() override;

    // 操作的元素ID
    wydb::ElementId _id;

    // Gizmo 操作（拖拽事务管理）
    std::unique_ptr<GizmoOperation> _pGizmoOp;
    // Gizmo 渲染器（拖拽视觉反馈）
    std::unique_ptr<GizmoRenderer> _pRenderer;
};

#define PICK_GIZMO_MASK 0x0000FFFF

}

#endif // WY3DAPP_BASE_GIZMO_H
