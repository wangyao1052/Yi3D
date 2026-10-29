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

#ifndef WY3DAPP_GIZMO_RENDERER_H
#define WY3DAPP_GIZMO_RENDERER_H

#include <wyVector3.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class Gizmo;
class Scene;

class WY3DAPP_EXPORT GizmoRenderer
{
public:
    explicit GizmoRenderer(Gizmo* pGizmo) : _pGizmo(pGizmo) {}
    virtual ~GizmoRenderer() {}

    virtual void refresh() = 0;
    virtual void onActivate() = 0;
    virtual void onDeactivate() = 0;

    virtual void attachToScene(Scene* pScene) = 0;
    virtual void detachFromScene(Scene* pScene) = 0;

    virtual bool onStartDrag(const wy::Vector3&) { return true; }
    virtual bool onDragging(const wy::Vector3&) { return true; }
    virtual bool onEndDrag(const wy::Vector3&) { return true; }
    virtual void onCancelDrag() {}

protected:
    Gizmo* _pGizmo;
};

} // namespace wy3dApp

#endif // WY3DAPP_GIZMO_RENDERER_H
