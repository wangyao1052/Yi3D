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

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include "utils/SketchSlotGeometry.h"

namespace {

constexpr double kPi = 3.14159265358979323846264338327950288;
constexpr double kTwoPi = 2.0 * kPi;
constexpr double kTolerance = 1.0e-9;

using wy3dApp::SlotArc;
using wy3dApp::SlotGeometry;
using wy3dApp::SlotPoint;

SlotPoint arcPoint(const SlotArc& arc, bool start)
{
    const double angle = start ? arc.startAngle : arc.endAngle;
    SlotPoint result;
    result.x = arc.center.x + arc.radius * std::cos(angle);
    result.y = arc.center.y + arc.radius * std::sin(angle);
    return result;
}

double distance(const SlotPoint& lhs, const SlotPoint& rhs)
{
    return std::hypot(lhs.x - rhs.x, lhs.y - rhs.y);
}

double normalizedSweep(const SlotArc& arc)
{
    double sweep = std::fmod(arc.endAngle - arc.startAngle, kTwoPi);
    if (sweep < 0.0) sweep += kTwoPi;
    return sweep;
}

void checkClosedLoop(const SlotGeometry& slot)
{
    EXPECT_NEAR(distance(arcPoint(slot.rightArc, false), slot.upperStart), 0.0, kTolerance);
    EXPECT_GT(distance(slot.upperStart, slot.upperEnd), 0.0);
    EXPECT_NEAR(distance(slot.upperEnd, arcPoint(slot.leftArc, true)), 0.0, kTolerance);
    EXPECT_NEAR(distance(arcPoint(slot.leftArc, false), slot.lowerStart), 0.0, kTolerance);
    EXPECT_GT(distance(slot.lowerStart, slot.lowerEnd), 0.0);
    EXPECT_NEAR(distance(slot.lowerEnd, arcPoint(slot.rightArc, true)), 0.0, kTolerance);

    EXPECT_NEAR(normalizedSweep(slot.rightArc), kPi, kTolerance);
    EXPECT_NEAR(normalizedSweep(slot.leftArc), kPi, kTolerance);
}

} // namespace

TEST(StraightSlotGeometry, HorizontalSlot)
{
    SlotPoint a{0.0, 0.0};
    SlotPoint b{10.0, 0.0};
    SlotGeometry slot;
    ASSERT_TRUE(SlotGeometry::compute(a, b, 4.0, slot));
    EXPECT_DOUBLE_EQ(slot.rightArc.radius, 2.0);
    EXPECT_DOUBLE_EQ(slot.leftArc.radius, 2.0);

    EXPECT_NEAR(slot.upperStart.x, 10.0, kTolerance);
    EXPECT_NEAR(slot.upperStart.y, 2.0, kTolerance);
    EXPECT_NEAR(slot.upperEnd.x, 0.0, kTolerance);
    EXPECT_NEAR(slot.upperEnd.y, 2.0, kTolerance);
    EXPECT_NEAR(slot.lowerStart.x, 0.0, kTolerance);
    EXPECT_NEAR(slot.lowerStart.y, -2.0, kTolerance);
    EXPECT_NEAR(slot.lowerEnd.x, 10.0, kTolerance);
    EXPECT_NEAR(slot.lowerEnd.y, -2.0, kTolerance);

    checkClosedLoop(slot);
}

TEST(StraightSlotGeometry, DiagonalSlot)
{
    SlotPoint a{1.0, 2.0};
    SlotPoint b{4.0, 6.0};
    SlotGeometry slot;
    ASSERT_TRUE(SlotGeometry::compute(a, b, 3.0, slot));
    checkClosedLoop(slot);

    // All boundary points must be radius away from one of the end centers.
    const SlotPoint samples[] = {
        arcPoint(slot.rightArc, true),
        arcPoint(slot.rightArc, false),
        slot.upperStart,
        slot.upperEnd,
        arcPoint(slot.leftArc, true),
        arcPoint(slot.leftArc, false),
        slot.lowerStart,
        slot.lowerEnd};
    for (const SlotPoint& sample : samples)
    {
        const double distanceToA = distance(sample, a);
        const double distanceToB = distance(sample, b);
        const bool onCap = std::fabs(distanceToA - 1.5) < kTolerance
            || std::fabs(distanceToB - 1.5) < kTolerance;
        EXPECT_TRUE(onCap);
    }
}

TEST(StraightSlotGeometry, ReversedDirection)
{
    SlotPoint a{4.0, 6.0};
    SlotPoint b{1.0, 2.0};
    SlotGeometry slot;
    ASSERT_TRUE(SlotGeometry::compute(a, b, 3.0, slot));
    checkClosedLoop(slot);
}

TEST(StraightSlotGeometry, InvalidInput)
{
    SlotGeometry slot;
    const SlotPoint a{0.0, 0.0};
    const SlotPoint b{10.0, 0.0};

    EXPECT_FALSE(SlotGeometry::compute(a, b, 0.0, slot));
    EXPECT_FALSE(SlotGeometry::compute(a, b, -1.0, slot));
    EXPECT_FALSE(SlotGeometry::compute(a, a, 4.0, slot));
    EXPECT_FALSE(SlotGeometry::compute(
        a, b, std::numeric_limits<double>::quiet_NaN(), slot));

    SlotPoint invalid = b;
    invalid.x = std::numeric_limits<double>::infinity();
    EXPECT_FALSE(SlotGeometry::compute(a, invalid, 4.0, slot));
}
