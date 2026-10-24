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

#include "utils/SketchPolygonGeometry.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

// The radius label reads the inscribed circle and sits at the midpoint of the
// edge picked at recognition (labelEdge), pointing outwards
struct PolygonDimensionLabel
{
    bool isValid = false;
    double value = 0.0;
    wy::Vector2 edgeMid;
    wy::Vector2 radialDirection;
    int edge = -1;
};

inline PolygonDimensionLabel computePolygonDimensionLabel(
    const std::vector<wy::Vector2>& corners,
    const wy::Vector2& center,
    int labelEdge)
{
    PolygonDimensionLabel label;

    const int sides = static_cast<int>(corners.size());
    if (sides < kMinPolygonSides || labelEdge < 0 || labelEdge >= sides) return label;

    const wy::Vector2 edgeMid = (corners[labelEdge] + corners[(labelEdge + 1) % sides]) * 0.5;
    const wy::Vector2 radial = edgeMid - center;
    const double distance = radial.length();
    if (!std::isfinite(distance) || distance <= 0.0) return label;

    label.isValid = true;
    label.value = distance;
    label.edgeMid = edgeMid;
    label.radialDirection = radial / distance;
    label.edge = labelEdge;
    return label;
}

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_POLYGON_DIMENSION_MATH_H
