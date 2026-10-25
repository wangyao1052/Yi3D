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

#ifndef WY3DAPP_SKETCH_POLYGON_GEOMETRY_H
#define WY3DAPP_SKETCH_POLYGON_GEOMETRY_H

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include <wyMath.h>
#include <wyVector2.h>

#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

constexpr int kMinPolygonSides = 3;
constexpr int kMaxPolygonSides = 100;

// Angle between the center and two adjacent vertices
inline double computePolygonStepAngle(int sides)
{
    return wy::TWO_PI / static_cast<double>(sides);
}

// Smallest inradius that keeps every edge at least minLength long; a polygon
// shrunk below it would no longer be recognized after it is deselected
inline double computePolygonMinInradius(int sides, double minLength)
{
    if (sides < kMinPolygonSides || minLength <= 0.0) return minLength;
    const double halfStep = wy::PI / static_cast<double>(sides);
    return std::max(minLength, minLength / (2.0 * std::tan(halfStep)));
}

// A regular polygon is defined by its circumscribed circle
inline double computePolygonInradius(double circumRadius, int sides)
{
    return circumRadius * std::cos(wy::PI / static_cast<double>(sides));
}

inline double computePolygonCircumradius(double inRadius, int sides)
{
    return inRadius / std::cos(wy::PI / static_cast<double>(sides));
}

// Vertex average; exact for a regular polygon. Empty input yields the origin.
inline wy::Vector2 computePolygonCenter(const std::vector<wy::Vector2>& corners)
{
    wy::Vector2 center;
    for (const wy::Vector2& corner : corners) center += corner;
    if (!corners.empty())
    {
        center = center / static_cast<double>(corners.size());
    }
    return center;
}

struct PolygonRegularity
{
    bool isValid = false;
    wy::Vector2 center;
    double circumRadius = 0.0;
    double inRadius = 0.0;
};

// Verifies that the ring is equilateral, equiangular and convex. The winding
// direction may be either; callers normalize it. The center is the vertex
// average, which is exact for a regular polygon.
inline PolygonRegularity validateRegularPolygon(const std::vector<wy::Vector2>& corners,
                                                double angleTol,
                                                double distTol)
{
    PolygonRegularity result;

    const int sides = static_cast<int>(corners.size());
    if (sides < kMinPolygonSides || angleTol <= 0.0 || distTol <= 0.0) return result;

    std::vector<double> lengths(sides);
    std::vector<wy::Vector2> units(sides);
    double lengthSum = 0.0;
    for (int i = 0; i < sides; ++i)
    {
        const wy::Vector2 edge = corners[(i + 1) % sides] - corners[i];
        lengths[i] = edge.length();
        if (!std::isfinite(lengths[i]) || lengths[i] <= distTol) return result;
        units[i] = edge / lengths[i];
        lengthSum += lengths[i];
    }

    const double meanLength = lengthSum / static_cast<double>(sides);
    const double lengthTol = std::max(distTol, 2.0 * std::sin(angleTol) * meanLength);
    for (int i = 0; i < sides; ++i)
    {
        if (std::fabs(lengths[i] - meanLength) > lengthTol) return result;
    }

    // The turn at every vertex must equal the center angle, all with one sign
    // (a convex ring); a folded or self-intersecting ring changes sign
    const double step = computePolygonStepAngle(sides);
    double turnSign = 0.0;
    for (int i = 0; i < sides; ++i)
    {
        const wy::Vector2& previous = units[(i + sides - 1) % sides];
        const wy::Vector2& next = units[i];
        const double turn = std::atan2(previous.cross(next), previous.dot(next));
        if (0 == i)
        {
            if (std::fabs(std::fabs(turn) - step) > angleTol) return result;
            turnSign = turn < 0.0 ? -1.0 : 1.0;
        }
        else if (std::fabs(turn - turnSign * step) > angleTol)
        {
            return result;
        }
    }

    const wy::Vector2 center = computePolygonCenter(corners);

    double radiusSum = 0.0;
    std::vector<double> radii(sides);
    for (int i = 0; i < sides; ++i)
    {
        radii[i] = (corners[i] - center).length();
        radiusSum += radii[i];
    }
    const double circumRadius = radiusSum / static_cast<double>(sides);
    if (!std::isfinite(circumRadius) || circumRadius <= distTol) return result;

    const double radiusTol = std::max(distTol, 2.0 * std::sin(angleTol) * circumRadius);
    for (int i = 0; i < sides; ++i)
    {
        if (std::fabs(radii[i] - circumRadius) > radiusTol) return result;
    }

    result.isValid = true;
    result.center = center;
    result.circumRadius = circumRadius;
    result.inRadius = computePolygonInradius(circumRadius, sides);
    return result;
}

inline double computePolygonRotation(const std::vector<wy::Vector2>& corners,
                                     const wy::Vector2& center)
{
    if (corners.empty()) return 0.0;
    const wy::Vector2 offset = corners[0] - center;
    return std::atan2(offset.y(), offset.x());
}

// Index of the direction closest to the +X axis; a tie prefers the +Y side so
// the result does not depend on where the ring starts
inline int selectDirectionClosestToX(const std::vector<wy::Vector2>& directions)
{
    int best = -1;
    double bestAbsAngle = std::numeric_limits<double>::max();
    double bestAngle = 0.0;
    for (int i = 0; i < static_cast<int>(directions.size()); ++i)
    {
        const wy::Vector2& direction = directions[i];
        if (!std::isfinite(direction.x()) || !std::isfinite(direction.y())) continue;
        if (direction.length2() <= 0.0) continue;

        const double angle = std::atan2(direction.y(), direction.x());
        const double absAngle = std::fabs(angle);
        if (absAngle < bestAbsAngle - wy::EPS
            || (absAngle <= bestAbsAngle + wy::EPS && angle > bestAngle))
        {
            best = i;
            bestAbsAngle = absAngle;
            bestAngle = angle;
        }
    }
    return best;
}

// Canonical first vertex: the one closest to +X (tie -> +Y side)
inline int selectCanonicalPolygonStart(const std::vector<wy::Vector2>& corners,
                                       const wy::Vector2& center)
{
    std::vector<wy::Vector2> directions(corners.size());
    for (int i = 0; i < static_cast<int>(corners.size()); ++i)
    {
        directions[i] = corners[i] - center;
    }
    return selectDirectionClosestToX(directions);
}

// The edge that carries the radius label: its midpoint direction is the one
// closest to +X. Fixed once at recognition so the label follows that edge
// while the polygon rotates.
inline int selectPolygonLabelEdge(const std::vector<wy::Vector2>& corners,
                                  const wy::Vector2& center)
{
    const int sides = static_cast<int>(corners.size());
    std::vector<wy::Vector2> directions(sides);
    for (int i = 0; i < sides; ++i)
    {
        directions[i] = (corners[i] + corners[(i + 1) % sides]) * 0.5 - center;
    }

    const int edge = selectDirectionClosestToX(directions);
    return edge < 0 ? 0 : edge;
}

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_POLYGON_GEOMETRY_H
