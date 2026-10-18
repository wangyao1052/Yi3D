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

#ifndef WY3DAPP_SNAP2D_TYPE_H
#define WY3DAPP_SNAP2D_TYPE_H

namespace wy3dApp {

// 2D snap type
enum class Snap2dType
{
    Undefined = 0,
    // Horizontal
    Horizontal,
    // Vertical
    Vertical,
    // Parallel
    Parallel,
    // Perpendicular
    Perpendicular,
    // Equal length
    EqualLength,
    // Equal radius
    EqualRadius,
    // X alignment
    AlignX,
    // Y alignment
    AlignY,
    // Nearest point on curve
    Nearest,
    // Coordinate origin
    Origin,
    // External: end point
    EndPoint,
    // External: middle point
    MiddlePoint,
    // External: center point
    CenterPoint,
    // Perpendicular foot
    PerpendicularPoint,
    // Tangent point
    TangentPoint,
    // Intersection point
    IntersectionPoint,
    // 17 was KeyPoint (removed); keep the gap for plugin compatibility
    // Quadrant point
    QuadrantPoint = 18,
    // Generic point (spline first point)
    GenericPoint = 19,
};

} // namespace wy3dApp

#endif // WY3DAPP_SNAP2D_TYPE_H
