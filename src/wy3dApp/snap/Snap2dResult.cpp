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

#include "Snap2dResult.h"

#include <cassert>
#include <wyVector2.h>
#include <wy3dPlane.h>

namespace wy3dApp {

namespace {

Snap2dResultItem makeResultItem(const SketchSnapObject& snapObject)
{
    wy::Vector2 point;
    if (const PointSnapObject* pPointSnapObject = dynamic_cast<const PointSnapObject*>(&snapObject))
        point = pPointSnapObject->getValue();
    return Snap2dResultItem(snapObject.getType(), snapObject.getId(), point, snapObject.getRefPoint());
}

} // namespace

Snap2dResult::Snap2dResult(
    const wy3d::Plane& sketchPlane,
    const wy::Vector2& position,
    SketchSnapObjectSPtr pSnapObject)
    : _sketchPlane(sketchPlane), _position(position)
{
    assert(pSnapObject);

    _items.reserve(3);
    _items.emplace_back(makeResultItem(*pSnapObject));
}

Snap2dResult::Snap2dResult(
    const wy3d::Plane& sketchPlane,
    const wy::Vector2& position,
    SketchSnapObjectSPtr pSnapObject, SketchSnapObjectSPtr pOtherSnapObject)
    : _sketchPlane(sketchPlane), _position(position)
{
    assert(pSnapObject);
    assert(pOtherSnapObject);

    _items.reserve(3);
    _items.emplace_back(makeResultItem(*pSnapObject));
    _items.emplace_back(makeResultItem(*pOtherSnapObject));
}

Snap2dResult::Snap2dResult(
    const wy3d::Plane& sketchPlane,
    const wy::Vector2& position,
    const Snap2dResultItem& snapItem)
    : _sketchPlane(sketchPlane), _position(position)
{
    _items.reserve(3);
    _items.emplace_back(snapItem);
}

Snap2dResult::Snap2dResult(
    const wy3d::Plane& sketchPlane,
    const wy::Vector2& position)
    : _sketchPlane(sketchPlane), _position(position)
{
}

} // namespace wy3dApp
