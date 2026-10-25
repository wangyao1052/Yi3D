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

#ifndef WY3DAPP_SKETCH_POLYGON_GIZMO_H
#define WY3DAPP_SKETCH_POLYGON_GIZMO_H

#include <memory>
#include <vector>

#include <wyVector2.h>
#include <wy3dSketchLine.h>

#include "gizmo/element/SketchEntityGripGizmo.h"
#include "utils/SketchPolygonDragMath.h"
#include "utils/SketchPolygonRecognizer.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

// N lines making up a regular polygon: one handle per corner, one per edge
// midpoint and one at the center. Dragging any handle rewrites all N lines in
// one go; the center handle translates the whole polygon.
class SketchPolygonGizmo : public SketchEntityGripGizmo
{
public:
    using Type = SketchPolygonHandle;

public:
    SketchPolygonGizmo(std::shared_ptr<const SketchPolygonInfo> polygon,
                       const std::vector<const wy3d::SketchLine*>& lines,
                       Type type,
                       int index);
    virtual ~SketchPolygonGizmo();

protected:
    // O(1): reads only the one or two lines that touch this handle
    virtual wy::Vector2 getGripPosition() const override;
    virtual bool onBeginDrag(wydb::Database* pDb) override;
    virtual bool onDragging(wydb::Transaction* pTrans, const wy::Vector2& curPos) override;

private:
    std::shared_ptr<const SketchPolygonInfo> _polygon;
    Type _type = Type::Corner;
    int _index = 0;

    // Drag snapshot, refreshed when the drag starts
    std::vector<wy::Vector2> _dragCorners;
    std::vector<bool> _dragLineStartAtCorner;
    wy::Vector2 _dragCenter;
    double _dragRotation = 0.0;
};

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_POLYGON_GIZMO_H
