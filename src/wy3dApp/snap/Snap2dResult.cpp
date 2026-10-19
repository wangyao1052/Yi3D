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

#include "Snap2dResultFactory.h"

#include <cassert>

namespace wy3dApp {

namespace {

class Snap2dResultImpl final : public Snap2dResult
{
public:
    Snap2dResultImpl(
        const wy3d::Plane& workPlane,
        const wy::Vector2& position,
        const std::vector<Snap2dHit>& hits)
        : _workPlane(workPlane), _position(position), _hits(hits)
    {
    }

    Snap2dResultImpl(
        const wy3d::Plane& workPlane,
        const wy::Vector2& position,
        const Snap2dHit& hit)
        : _workPlane(workPlane), _position(position)
    {
        _hits.reserve(3);
        _hits.emplace_back(hit);
    }

    Snap2dResultImpl(
        const wy3d::Plane& workPlane,
        const wy::Vector2& position,
        const Snap2dHit& hit,
        const Snap2dHit& otherHit)
        : _workPlane(workPlane), _position(position)
    {
        _hits.reserve(3);
        _hits.emplace_back(hit);
        _hits.emplace_back(otherHit);
    }

    Snap2dResultImpl(
        const wy3d::Plane& workPlane,
        const wy::Vector2& position)
        : _workPlane(workPlane), _position(position)
    {
    }

    wy::Vector2 getPosition() const override { return _position; }

    const std::vector<Snap2dHit>& getHits() const override { return _hits; }

    const wy3d::Plane& getWorkPlane() const override { return _workPlane; }

private:
    wy3d::Plane _workPlane;
    wy::Vector2 _position;
    std::vector<Snap2dHit> _hits;
};

} // namespace

Snap2dResult::Snap2dResult() = default;

Snap2dResult::~Snap2dResult() = default;

Snap2dHit toSnap2dHit(const Snap2dObject& snapObject, const wy::Vector2& position)
{
    Snap2dHit result;
    result.type = snapObject.getType();
    result.element = snapObject.getId();
    switch (result.type)
    {
    case Snap2dType::AlignX:
    case Snap2dType::AlignY:
        result.point = snapObject.getRefPoint();
        break;
    default:
        result.point = position;
        break;
    }
    return result;
}

Snap2dResultSPtr makeSnap2dResult(
    const wy3d::Plane& sketchPlane, const wy::Vector2& position,
    const Snap2dObjectSPtr& pSnapObject)
{
    assert(pSnapObject);
    return std::make_shared<Snap2dResultImpl>(
        sketchPlane, position, toSnap2dHit(*pSnapObject, position));
}

Snap2dResultSPtr makeSnap2dResult(
    const wy3d::Plane& sketchPlane, const wy::Vector2& position,
    const Snap2dObjectSPtr& pSnapObject, const Snap2dObjectSPtr& pOtherSnapObject)
{
    assert(pSnapObject);
    assert(pOtherSnapObject);
    return std::make_shared<Snap2dResultImpl>(sketchPlane, position,
        toSnap2dHit(*pSnapObject, position), toSnap2dHit(*pOtherSnapObject, position));
}

Snap2dResultSPtr makeSnap2dResult(
    const wy3d::Plane& workPlane, const wy::Vector2& position,
    const std::vector<Snap2dHit>& hits)
{
    return std::make_shared<Snap2dResultImpl>(workPlane, position, hits);
}

Snap2dResultSPtr makeSnap2dResult(
    const wy3d::Plane& workPlane, const wy::Vector2& position)
{
    return std::make_shared<Snap2dResultImpl>(workPlane, position);
}

} // namespace wy3dApp
