///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 Wang Yao <wangyao1052@163.com>
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

#include "SketchSlotGeometry.h"

#include <cmath>

namespace wy3dApp {

namespace {

constexpr double kPi = 3.14159265358979323846264338327950288;
constexpr double kTwoPi = 2.0 * kPi;
constexpr double kHalfPi = 0.5 * kPi;

double normalizeAngle(double angle)
{
    angle = std::fmod(angle, kTwoPi);
    if (angle < 0.0)
    {
        angle += kTwoPi;
    }
    return angle;
}

SlotPoint offset(const SlotPoint& point, double nx, double ny, double distance)
{
    SlotPoint result;
    result.x = point.x + nx * distance;
    result.y = point.y + ny * distance;
    return result;
}

} // namespace

bool SlotGeometry::compute(
    const SlotPoint& firstCenter,
    const SlotPoint& secondCenter,
    double width,
    SlotGeometry& out,
    double tolerance)
{
    if (!std::isfinite(firstCenter.x) || !std::isfinite(firstCenter.y) ||
        !std::isfinite(secondCenter.x) || !std::isfinite(secondCenter.y) ||
        !std::isfinite(width))
    {
        return false;
    }

    if (tolerance <= 0.0)
    {
        tolerance = 1.0e-9;
    }

    const double deltaX = secondCenter.x - firstCenter.x;
    const double deltaY = secondCenter.y - firstCenter.y;
    const double centerDistance = std::hypot(deltaX, deltaY);
    if (centerDistance <= tolerance)
    {
        return false;
    }

    const double radius = 0.5 * width;
    if (radius <= tolerance)
    {
        return false;
    }

    const double directionX = deltaX / centerDistance;
    const double directionY = deltaY / centerDistance;
    // Left normal of the centerline.
    const double normalX = -directionY;
    const double normalY = directionX;
    const double theta = std::atan2(directionY, directionX);

    SlotGeometry geometry;

    // Right cap: B - n*r -> B + n*r, CCW around B.
    geometry.rightArc.center = secondCenter;
    geometry.rightArc.radius = radius;
    geometry.rightArc.startAngle = normalizeAngle(theta - kHalfPi);
    geometry.rightArc.endAngle = normalizeAngle(theta + kHalfPi);

    geometry.upperStart = offset(secondCenter, normalX, normalY, radius);
    geometry.upperEnd = offset(firstCenter, normalX, normalY, radius);

    // Left cap: A + n*r -> A - n*r, CCW around A.
    geometry.leftArc.center = firstCenter;
    geometry.leftArc.radius = radius;
    geometry.leftArc.startAngle = normalizeAngle(theta + kHalfPi);
    geometry.leftArc.endAngle = normalizeAngle(theta + 3.0 * kHalfPi);

    geometry.lowerStart = offset(firstCenter, normalX, normalY, -radius);
    geometry.lowerEnd = offset(secondCenter, normalX, normalY, -radius);

    out = geometry;
    return true;
}

} // namespace wy3dApp
