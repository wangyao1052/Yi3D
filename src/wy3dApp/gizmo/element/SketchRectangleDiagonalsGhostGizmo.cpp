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

#include "SketchRectangleDiagonalsGhostGizmo.h"

#include <memory>
#include <utility>

#include "gizmo/renderer/SketchRectangleDiagonalsOsgRenderer.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

SketchRectangleDiagonalsGhostGizmo::SketchRectangleDiagonalsGhostGizmo(
    std::shared_ptr<const SketchRectangleInfo> rect)
    : GhostGizmo(std::vector<wydb::ElementId>(rect->lineIds.begin(), rect->lineIds.end()))
    , _rect(std::move(rect))
{
    _pRenderer = std::make_unique<SketchRectangleDiagonalsOsgRenderer>(this);
}

SketchRectangleDiagonalsGhostGizmo::~SketchRectangleDiagonalsGhostGizmo()
{
}

} // namespace wy3dApp
