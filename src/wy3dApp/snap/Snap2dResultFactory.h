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

#ifndef WY3DAPP_SNAP2D_RESULT_FACTORY_H
#define WY3DAPP_SNAP2D_RESULT_FACTORY_H

#include <wy3dApp/Snap2dResult.h>
#include "Snap2dObject.h"

namespace wy3dApp {

// Flattens a private snap object into its public value snapshot.
Snap2dHit toSnap2dHit(const Snap2dObject& snapObject, const wy::Vector2& position);

// Non-exported factories used by the snap implementation.
Snap2dResultSPtr makeSnap2dResult(
    const wy3d::Plane& sketchPlane,
    const wy::Vector2& position,
    const Snap2dObjectSPtr& pSnapObject);

Snap2dResultSPtr makeSnap2dResult(
    const wy3d::Plane& sketchPlane,
    const wy::Vector2& position,
    const Snap2dObjectSPtr& pSnapObject,
    const Snap2dObjectSPtr& pOtherSnapObject);

Snap2dResultSPtr makeSnap2dResult(
    const wy3d::Plane& workPlane,
    const wy::Vector2& position,
    const std::vector<Snap2dHit>& hits);

// Display-less result (no items).
Snap2dResultSPtr makeSnap2dResult(
    const wy3d::Plane& workPlane,
    const wy::Vector2& position);

} // namespace wy3dApp

#endif // WY3DAPP_SNAP2D_RESULT_FACTORY_H
