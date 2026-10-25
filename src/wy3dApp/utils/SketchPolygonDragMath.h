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

#ifndef WY3DAPP_SKETCH_POLYGON_DRAG_MATH_H
#define WY3DAPP_SKETCH_POLYGON_DRAG_MATH_H

#include <algorithm>
#include <cmath>
#include <vector>

#include <wyMath.h>
#include <wyVector2.h>

#include "utils/SketchPolygonGeometry.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

enum class SketchPolygonHandle
{
    Corner = 0,
    EdgeMid = 1,
    Center = 2,
};

// Vertex i sits at rotation + i*step on the circumscribed circle
inline std::vector<wy::Vector2> computePolygonVertices(const wy::Vector2& center,
                                                       double circumRadius,
                                                       double rotation,
                                                       int sides)
{
    std::vector<wy::Vector2> corners;
    if (sides < kMinPolygonSides) return corners;

    const double step = computePolygonStepAngle(sides);
    corners.resize(sides);
    for (int i = 0; i < sides; ++i)
    {
        const double angle = rotation + i * step;
        corners[i] = center + circumRadius * wy::Vector2(std::cos(angle), std::sin(angle));
    }
    return corners;
}

// Corner drag about a fixed center: the dragged vertex follows the cursor, so
// both the radius and the rotation change. A cursor at the center keeps the
// previous rotation; the radius is always clamped to minInradius.
inline std::vector<wy::Vector2> computePolygonCornerDragVertices(
    const wy::Vector2& center,
    double rotation,
    int sides,
    int index,
    const wy::Vector2& curPos,
    double minInradius)
{
    if (sides < kMinPolygonSides) return {};

    const double minCircumRadius = computePolygonCircumradius(minInradius, sides);
    const wy::Vector2 offset = curPos - center;
    if (!std::isfinite(offset.x()) || !std::isfinite(offset.y()))
    {
        return computePolygonVertices(center, minCircumRadius, rotation, sides);
    }

    const double radius = offset.length();
    double newRotation = rotation;
    if (radius > wy::EPS)
    {
        newRotation = std::atan2(offset.y(), offset.x())
            - index * computePolygonStepAngle(sides);
    }
    return computePolygonVertices(center, std::max(radius, minCircumRadius),
                                  newRotation, sides);
}

// Edge midpoint drag: only the inradius follows the cursor along the fixed
// outward normal of that edge; the rotation never changes and moving across
// the center is clamped instead of flipping the polygon.
inline std::vector<wy::Vector2> computePolygonEdgeMidDragVertices(
    const wy::Vector2& center,
    double rotation,
    int sides,
    int index,
    const wy::Vector2& curPos,
    double minInradius)
{
    if (sides < kMinPolygonSides) return {};

    const double step = computePolygonStepAngle(sides);
    const double midAngle = rotation + (index + 0.5) * step;
    const wy::Vector2 normal(std::cos(midAngle), std::sin(midAngle));

    const wy::Vector2 offset = curPos - center;
    double inRadius = offset.dot(normal);
    if (!std::isfinite(inRadius)) inRadius = minInradius;
    inRadius = std::max(inRadius, minInradius);

    const double circumRadius = computePolygonCircumradius(inRadius, sides);
    return computePolygonVertices(center, circumRadius, rotation, sides);
}

// Center drag: translate the whole polygon with the cursor. The snapshot
// corners are shifted, never rebuilt from the parameters, so a live ring that
// is only regular within tolerance does not jump. A non-finite cursor keeps
// the polygon where it is.
inline std::vector<wy::Vector2> computePolygonCenterDragVertices(
    const std::vector<wy::Vector2>& startCorners,
    const wy::Vector2& startCenter,
    const wy::Vector2& curPos)
{
    if (static_cast<int>(startCorners.size()) < kMinPolygonSides) return {};

    const wy::Vector2 offset = curPos - startCenter;
    if (!std::isfinite(offset.x()) || !std::isfinite(offset.y()))
    {
        return startCorners;
    }

    std::vector<wy::Vector2> corners;
    corners.reserve(startCorners.size());
    for (const wy::Vector2& corner : startCorners)
    {
        corners.push_back(corner + offset);
    }
    return corners;
}

// Radius edit (text label): scale about the center, keep the rotation
inline std::vector<wy::Vector2> computePolygonResizedVertices(const wy::Vector2& center,
                                                             double rotation,
                                                             int sides,
                                                             double inRadius,
                                                             double minInradius)
{
    if (sides < kMinPolygonSides || !std::isfinite(inRadius)) return {};

    const double clamped = std::max(inRadius, minInradius);
    const double circumRadius = computePolygonCircumradius(clamped, sides);
    return computePolygonVertices(center, circumRadius, rotation, sides);
}

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_POLYGON_DRAG_MATH_H
