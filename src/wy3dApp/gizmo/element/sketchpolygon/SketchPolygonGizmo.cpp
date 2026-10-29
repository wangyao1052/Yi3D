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

#include "SketchPolygonGizmo.h"

#include <cassert>
#include <cmath>
#include <limits>
#include <utility>

#include <wy3dImpl.h>

#include <wy3dApp/Application.h>
#include "application/ApplicationImpl.h"
#include "utils/SketchClosedLoop.h"
#include "utils/SketchPolygonGeometry.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

SketchPolygonGizmo::SketchPolygonGizmo(std::shared_ptr<const SketchPolygonInfo> polygon,
                                       Type type,
                                       int index)
    : SketchEntityGripGizmo(polygon->sketchId, polygon->lineIds)
    , _polygon(std::move(polygon))
    , _type(type)
    , _index(index)
{
    assert(_polygon);
    assert(_polygon && _index >= 0 && _index < _polygon->sides);
}

SketchPolygonGizmo::~SketchPolygonGizmo()
{
}

wy::Vector2 SketchPolygonGizmo::getGripPosition() const
{
    const int sides = _polygon->sides;
    const wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();

    if (Type::Center == _type)
    {
        if (!pDb) return _polygon->center;

        std::vector<wy::Vector2> liveCorners;
        if (!readSketchClosedLoopCorners(pDb, _polygon->lineIds, liveCorners,
                                         nullptr, _polygon->distTol)
            || static_cast<int>(liveCorners.size()) != sides)
        {
            return _polygon->center;
        }
        return computePolygonCenter(liveCorners);
    }

    if (_index < 0 || _index >= sides) return _polygon->center;

    if (Type::EdgeMid == _type)
    {
        const wy::Vector2 fallback =
            (_polygon->corners[_index] + _polygon->corners[(_index + 1) % sides]) * 0.5;
        if (!pDb) return fallback;

        const wy3d::SketchLine* pLine =
            wy3d::SketchLine::cast(pDb->getElement(_polygon->lineIds[_index]));
        if (!pLine) return fallback;
        return (pLine->getStartPoint() + pLine->getEndPoint()) * 0.5;
    }

    if (!pDb) return _polygon->corners[_index];

    // Corner: the shared endpoint of the previous line and this line
    const wy3d::SketchLine* pPrevious = wy3d::SketchLine::cast(
        pDb->getElement(_polygon->lineIds[(_index + sides - 1) % sides]));
    const wy3d::SketchLine* pNext = wy3d::SketchLine::cast(
        pDb->getElement(_polygon->lineIds[_index]));
    if (pPrevious && pNext)
    {
        const wy::Vector2 previousEnds[2] = {
            pPrevious->getStartPoint(), pPrevious->getEndPoint() };
        const wy::Vector2 nextEnds[2] = { pNext->getStartPoint(), pNext->getEndPoint() };

        double bestDistance2 = std::numeric_limits<double>::max();
        wy::Vector2 bestPoint;
        for (const wy::Vector2& previousEnd : previousEnds)
        {
            for (const wy::Vector2& nextEnd : nextEnds)
            {
                const double distance2 = (previousEnd - nextEnd).length2();
                if (distance2 < bestDistance2)
                {
                    bestDistance2 = distance2;
                    bestPoint = (previousEnd + nextEnd) * 0.5;
                }
            }
        }
        if (bestDistance2 <= _polygon->distTol * _polygon->distTol) return bestPoint;
    }
    return _polygon->corners[_index];
}

bool SketchPolygonGizmo::onStartEdit(wydb::Database* pDb)
{
    assert(pDb);
    if (!pDb) return false;

    std::vector<wy::Vector2> corners;
    std::vector<bool> lineStartAtCorner;
    if (!readSketchClosedLoopCorners(pDb, _polygon->lineIds, corners, &lineStartAtCorner,
                                     _polygon->distTol))
    {
        return false;
    }

    // The drag math assumes a counter-clockwise ring starting at corners[0]
    const PolygonRegularity regularity =
        validateRegularPolygon(corners, _polygon->angleTol, _polygon->distTol);
    if (!regularity.isValid) return false;
    if (computeClosedLoopSignedArea(corners, regularity.center) <= 0.0) return false;

    _dragCorners = std::move(corners);
    _dragLineStartAtCorner = std::move(lineStartAtCorner);
    _dragCenter = regularity.center;
    _dragRotation = computePolygonRotation(_dragCorners, _dragCenter);
    return true;
}

bool SketchPolygonGizmo::onApplyEdit(wydb::Transaction* pTrans, const wy::Vector2& curPos)
{
    assert(pTrans);
    if (!pTrans) return false;

    const int sides = _polygon->sides;
    const double minInradius = computePolygonMinInradius(sides, wy3d::kMinValue);

    std::vector<wy::Vector2> newCorners;
    switch (_type)
    {
    case Type::Corner:
        newCorners = computePolygonCornerDragVertices(
            _dragCenter, _dragRotation, sides, _index, curPos, minInradius);
        break;
    case Type::EdgeMid:
        newCorners = computePolygonEdgeMidDragVertices(
            _dragCenter, _dragRotation, sides, _index, curPos, minInradius);
        break;
    case Type::Center:
        newCorners = computePolygonCenterDragVertices(_dragCorners, _dragCenter, curPos);
        break;
    default:
        assert(false);
        return false;
    }
    if (static_cast<int>(newCorners.size()) != sides) return false;

    return writeSketchClosedLoopCorners(
        pTrans, _polygon->lineIds, _dragLineStartAtCorner, newCorners);
}

} // namespace wy3dApp
