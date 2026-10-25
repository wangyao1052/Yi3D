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

#ifndef WY3DAPP_SKETCH_POLYGON_DIMENSION_MATH_H
#define WY3DAPP_SKETCH_POLYGON_DIMENSION_MATH_H

#include <cmath>
#include <vector>

#include <wyVector2.h>

#include "utils/SketchPolygonDragMath.h"
#include "utils/SketchPolygonGeometry.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

enum class SketchPolygonDimension
{
    InscribedRadius = 0,
    CircumscribedRadius = 1,
};

// One radius label of a regular polygon. The inscribed radius is read at the
// midpoint of the edge picked at recognition (labelEdge); the circumscribed
// radius is read at corners[0], the canonical vertex closest to +X at
// recognition. Both labels point radially outwards.
struct PolygonDimensionLabel
{
    bool isValid = false;
    double value = 0.0;
    wy::Vector2 anchor;
    wy::Vector2 radialDirection;
    int edge = -1; // inscribed only
};

inline PolygonDimensionLabel computePolygonDimensionLabel(
    const std::vector<wy::Vector2>& corners,
    const wy::Vector2& center,
    SketchPolygonDimension dimension,
    int labelEdge)
{
    PolygonDimensionLabel label;

    const int sides = static_cast<int>(corners.size());
    if (sides < kMinPolygonSides) return label;

    wy::Vector2 anchor;
    if (SketchPolygonDimension::InscribedRadius == dimension)
    {
        if (labelEdge < 0 || labelEdge >= sides) return label;
        anchor = (corners[labelEdge] + corners[(labelEdge + 1) % sides]) * 0.5;
        label.edge = labelEdge;
    }
    else
    {
        anchor = corners[0];
    }

    const wy::Vector2 radial = anchor - center;
    const double distance = radial.length();
    if (!std::isfinite(distance) || distance <= 0.0) return label;

    label.isValid = true;
    label.value = distance;
    label.anchor = anchor;
    label.radialDirection = radial / distance;
    return label;
}

// Smallest value the given reading may take while keeping the polygon
// recognizable after it is deselected; both readings clamp at the same minimum
// inscribed radius.
inline double computePolygonMinDimensionValue(SketchPolygonDimension dimension,
                                              int sides,
                                              double minLength)
{
    const double minInradius = computePolygonMinInradius(sides, minLength);
    return SketchPolygonDimension::InscribedRadius == dimension
        ? minInradius
        : computePolygonCircumradius(minInradius, sides);
}

// Radius edit from the label: scale about the center, keep the rotation. The
// value is measured on the circle the given dimension names.
inline std::vector<wy::Vector2> computePolygonDimensionResizedVertices(
    const wy::Vector2& center,
    double rotation,
    int sides,
    SketchPolygonDimension dimension,
    double value,
    double minInradius)
{
    if (sides < kMinPolygonSides || !std::isfinite(value)) return {};

    const double inRadius = SketchPolygonDimension::InscribedRadius == dimension
        ? value
        : computePolygonInradius(value, sides);
    return computePolygonResizedVertices(center, rotation, sides, inRadius, minInradius);
}

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_POLYGON_DIMENSION_MATH_H
