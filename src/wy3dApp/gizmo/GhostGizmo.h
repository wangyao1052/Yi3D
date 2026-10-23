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

#ifndef WY3DAPP_GHOST_GIZMO_H
#define WY3DAPP_GHOST_GIZMO_H

#include <memory>
#include <wydbElement.h>

#include "gizmo/BaseGizmo.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class GhostOsgRenderer;

// Ghost Gizmo — 用于显示草图图元/特征的辅助几何
// 几何生成由 GhostOsgRenderer 子类负责
class GhostGizmo : public BaseGizmo
{
public:
    explicit GhostGizmo(const wydb::Element* pElem);
    explicit GhostGizmo(std::vector<wydb::ElementId> ids) : BaseGizmo(std::move(ids)) {}
    virtual ~GhostGizmo() {}
};

} // namespace wy3dApp

#endif // WY3DAPP_GHOST_GIZMO_H
