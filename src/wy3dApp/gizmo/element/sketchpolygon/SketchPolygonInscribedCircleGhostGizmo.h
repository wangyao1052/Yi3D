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

#ifndef WY3DAPP_SKETCH_POLYGON_INSCRIBED_CIRCLE_GHOST_GIZMO_H
#define WY3DAPP_SKETCH_POLYGON_INSCRIBED_CIRCLE_GHOST_GIZMO_H

#include <memory>

#include "gizmo/GhostGizmo.h"
#include "utils/SketchPolygonRecognizer.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

// Dashed inscribed circle of a regular polygon (not pickable), hinting that the
// selected lines are recognized as one polygon.
class SketchPolygonInscribedCircleGhostGizmo : public GhostGizmo
{
public:
    explicit SketchPolygonInscribedCircleGhostGizmo(
        std::shared_ptr<const SketchPolygonInfo> polygon);
    virtual ~SketchPolygonInscribedCircleGhostGizmo();

    const SketchPolygonInfo& getPolygonInfo() const { return *_polygon; }

private:
    std::shared_ptr<const SketchPolygonInfo> _polygon;
};

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_POLYGON_INSCRIBED_CIRCLE_GHOST_GIZMO_H
