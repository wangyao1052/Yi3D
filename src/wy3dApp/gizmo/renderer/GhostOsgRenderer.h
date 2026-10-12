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

#ifndef WY3DAPP_GHOST_OSG_RENDERER_H
#define WY3DAPP_GHOST_OSG_RENDERER_H

#include <osg/Geometry>
#include <wydbDatabase.h>
#include <wydbElement.h>

#include "OsgGizmoRenderer.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class GhostOsgRenderer : public OsgGizmoRenderer
{
public:
    explicit GhostOsgRenderer(BaseGizmo* pGizmo);
    virtual ~GhostOsgRenderer() {}

    virtual void refresh() {}
    virtual void onActivate() {}
    virtual void onDeactivate() {}

    virtual bool onBeginDrag(const wy::Vector3& startPos) override { return true; }
    virtual bool onDragging(const wy::Vector3& curPos) override { return true; }
    virtual void onEndDrag(const wy::Vector3& endPos) override {}
    virtual void onCancelDrag() override {}
};

}

#endif // WY3DAPP_GHOST_OSG_RENDERER_H
