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

#ifndef WY3DAPP_SNAP2D_RESULT_H
#define WY3DAPP_SNAP2D_RESULT_H

#include <vector>
#include <memory>
#include <wyVector2.h>
#include <wy3dVector2.h>
#include <wy3dPlane.h>
#include "Snap2dObject.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

// One item of a snap result: a value snapshot of one snapped element.
struct Snap2dResultItem
{
    Snap2dType type = Snap2dType::Undefined;
    wydb::ElementId highlightId;  // element to highlight
    wy::Vector2 point;            // point carried by the snap object (tangent point, curve end, ...)
    wy::Vector2 refPoint;         // guide-line reference (alignment point)

    explicit Snap2dResultItem(Snap2dType argType, const wydb::ElementId& argHighlightId,
        const wy::Vector2& argPoint, const wy::Vector2& argRefPoint)
        : type(argType), highlightId(argHighlightId), point(argPoint),
          refPoint(argRefPoint) {}
};

class Snap2dResult
{
public:
    Snap2dResult(
        const wy3d::Plane& sketchPlane,
        const wy::Vector2& position,
        Snap2dObjectSPtr pSnapObject);
    Snap2dResult(
        const wy3d::Plane& sketchPlane,
        const wy::Vector2& position,
        Snap2dObjectSPtr pSnapObject,
        Snap2dObjectSPtr pOtherSnapObject);
    // Result built from one item (external rule hits).
    Snap2dResult(
        const wy3d::Plane& sketchPlane,
        const wy::Vector2& position,
        const Snap2dResultItem& snapItem);
    // A hit with nothing for the sketch layer to display.
    Snap2dResult(
        const wy3d::Plane& sketchPlane,
        const wy::Vector2& position);

    // 获取位置坐标
    wy::Vector2 getPosition() const
    {
        return _position;
    }

    // 获取捕捉到的项
    const std::vector<Snap2dResultItem>& getItems() const
    {
        return _items;
    }

    // 获取草图平面(显示点/点划线时使用)
    const wy3d::Plane& getSketchPlane() const
    {
        return _sketchPlane;
    }

private:
    wy3d::Plane _sketchPlane;
    wy::Vector2 _position;
    std::vector<Snap2dResultItem> _items;
};
typedef std::shared_ptr<Snap2dResult> Snap2dResultSPtr;

} // namespace wy3dApp

#endif // WY3DAPP_SNAP2D_RESULT_H
