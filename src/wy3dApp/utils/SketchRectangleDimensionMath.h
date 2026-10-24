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

#ifndef WY3DAPP_SKETCH_RECTANGLE_DIMENSION_MATH_H
#define WY3DAPP_SKETCH_RECTANGLE_DIMENSION_MATH_H

#include <algorithm>
#include <array>
#include <cmath>

#include <wyMath.h>
#include <wyVector2.h>

#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

enum class SketchRectangleDimension
{
    Width = 0,
    Height = 1,
};

// Label of one rectangle dimension. The rectangle does not have to be axis
// aligned: of the two pairs of parallel edges, the pair whose direction is
// closer to the sketch X axis carries the width, the other pair the height. The
// label sits at the midpoint of the lower edge of the width pair, respectively
// of the right edge of the height pair.
struct RectangleDimensionLabel
{
    bool isValid = false;
    double value = 0.0;
    wy::Vector2 edgeMid;
};

// Index of the edge (corner i -> corner i+1) that carries the given dimension.
// Width picks the lower edge of the pair whose direction is closer to the
// sketch X axis, Height picks the right edge of the other pair. The two picks
// are perpendicular, so they never describe the same pair of edges. Returns -1
// for a degenerate rectangle.
inline int selectRectangleDimensionEdge(const std::array<wy::Vector2, 4>& corners,
                                        SketchRectangleDimension dim)
{
    const wy::Vector2 firstEdge = corners[1] - corners[0];
    const wy::Vector2 secondEdge = corners[2] - corners[1];
    if (firstEdge.length() < wy::EPS || secondEdge.length() < wy::EPS)
    {
        return -1;
    }

    // The ring alternates between the two pairs (edges 0/2 and 1/3); a tie at
    // 45 degrees keeps the first pair on width
    const bool isFirstPairWidth = std::fabs(firstEdge.x()) >= std::fabs(firstEdge.y());
    const bool isFirstPairEdited =
        (SketchRectangleDimension::Width == dim) == isFirstPairWidth;
    const int first = isFirstPairEdited ? 0 : 1;
    const int second = first + 2;

    const wy::Vector2 firstMid = (corners[first] + corners[(first + 1) % 4]) * 0.5;
    const wy::Vector2 secondMid = (corners[second] + corners[(second + 1) % 4]) * 0.5;
    if (SketchRectangleDimension::Width == dim)
    {
        return firstMid.y() <= secondMid.y() ? first : second;
    }
    return firstMid.x() >= secondMid.x() ? first : second;
}

inline RectangleDimensionLabel computeRectangleDimensionLabel(
    const std::array<wy::Vector2, 4>& corners,
    SketchRectangleDimension dim)
{
    RectangleDimensionLabel label;

    const int edge = selectRectangleDimensionEdge(corners, dim);
    if (edge < 0)
    {
        return label;
    }

    const wy::Vector2& start = corners[edge];
    const wy::Vector2& end = corners[(edge + 1) % 4];
    label.value = (end - start).length();
    label.edgeMid = (start + end) * 0.5;
    label.isValid = true;
    return label;
}

// Resize the rectangle about its center along its own axis: both edges of the
// edited pair move outwards/inwards by half the change. Corner identity and ring
// order are preserved; the extent is clamped to minValue.
inline std::array<wy::Vector2, 4> computeRectangleResizedCorners(
    const std::array<wy::Vector2, 4>& corners,
    SketchRectangleDimension dim,
    double newValue,
    double minValue)
{
    const int edge = selectRectangleDimensionEdge(corners, dim);
    if (edge < 0)
    {
        return corners;
    }

    const wy::Vector2 direction = (corners[(edge + 1) % 4] - corners[edge]).normalized();
    const wy::Vector2 center = (corners[0] + corners[1] + corners[2] + corners[3]) * 0.25;
    const double halfExtent = 0.5 * std::max(newValue, minValue);

    std::array<wy::Vector2, 4> result = corners;
    for (int i = 0; i < 4; ++i)
    {
        const double offset = (corners[i] - center).dot(direction);
        const double target = offset < 0.0 ? -halfExtent : halfExtent;
        result[i] = corners[i] + (target - offset) * direction;
    }
    return result;
}

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_RECTANGLE_DIMENSION_MATH_H
