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

#include "headers.h"

#include <array>
#include <cmath>

#include "utils/SketchRectangleDimensionMath.h"
#include "utils/SketchRectangleDragMath.h"

namespace
{
    using wy::Vector2;
    using wy3dApp::SketchRectangleDimension;

    constexpr double kPi = 3.14159265358979323846;
    constexpr double kMinExtent = 1e-3;

    std::array<Vector2, 4> makeAxisAligned(double halfWidth = 5.0,
                                           double halfHeight = 2.0,
                                           const Vector2& center = Vector2(1.0, 2.0))
    {
        return wy3dApp::computeRectangleCorners(
            center, halfWidth, halfHeight, Vector2(1.0, 0.0), Vector2(0.0, 1.0));
    }

    std::array<Vector2, 4> makeRotated(double angleRadians,
                                       double halfWidth = 5.0,
                                       double halfHeight = 2.0)
    {
        const Vector2 u(std::cos(angleRadians), std::sin(angleRadians));
        const Vector2 v(-u.y(), u.x());
        return wy3dApp::computeRectangleCorners(Vector2(0.0, 0.0), halfWidth, halfHeight, u, v);
    }

    void expectPoint(const Vector2& actual, const Vector2& expected, double tol = 1e-12)
    {
        EXPECT_NEAR(actual.x(), expected.x(), tol);
        EXPECT_NEAR(actual.y(), expected.y(), tol);
    }
}

TEST(SketchRectangleDimensionMath, AxisAlignedRectangleReportsWidthAndHeight)
{
    const auto corners = makeAxisAligned(); // x in [-4, 6], y in [0, 4]

    const auto width = wy3dApp::computeRectangleDimensionLabel(
        corners, SketchRectangleDimension::Width);
    EXPECT_TRUE(width.isValid);
    EXPECT_NEAR(width.value, 10.0, 1e-12);
    expectPoint(width.edgeMid, Vector2(1.0, 0.0));

    const auto height = wy3dApp::computeRectangleDimensionLabel(
        corners, SketchRectangleDimension::Height);
    EXPECT_TRUE(height.isValid);
    EXPECT_NEAR(height.value, 4.0, 1e-12);
    expectPoint(height.edgeMid, Vector2(6.0, 2.0));
}

TEST(SketchRectangleDimensionMath, RingOrderDoesNotChangeTheAnchors)
{
    // Same rectangle, ring walked from the vertical edge: the bottom/right edges
    // still come from the geometry, not from the index.
    const std::array<Vector2, 4> corners = {
        Vector2(10.0, 0.0), Vector2(10.0, 20.0), Vector2(0.0, 20.0), Vector2(0.0, 0.0) };

    const auto width = wy3dApp::computeRectangleDimensionLabel(
        corners, SketchRectangleDimension::Width);
    EXPECT_TRUE(width.isValid);
    EXPECT_NEAR(width.value, 10.0, 1e-12);
    expectPoint(width.edgeMid, Vector2(5.0, 0.0));

    const auto height = wy3dApp::computeRectangleDimensionLabel(
        corners, SketchRectangleDimension::Height);
    EXPECT_TRUE(height.isValid);
    EXPECT_NEAR(height.value, 20.0, 1e-12);
    expectPoint(height.edgeMid, Vector2(10.0, 10.0));
}

TEST(SketchRectangleDimensionMath, RotatedRectangleUsesThePairFacingUpAndDown)
{
    const double sqrt3 = std::sqrt(3.0);

    // 30 degrees: the first pair (10 wide) is still the one facing up/down
    {
        const auto corners = makeRotated(30.0 * kPi / 180.0);
        const auto width = wy3dApp::computeRectangleDimensionLabel(
            corners, SketchRectangleDimension::Width);
        EXPECT_TRUE(width.isValid);
        EXPECT_NEAR(width.value, 10.0, 1e-12);
        expectPoint(width.edgeMid, Vector2(1.0, -sqrt3));

        const auto height = wy3dApp::computeRectangleDimensionLabel(
            corners, SketchRectangleDimension::Height);
        EXPECT_TRUE(height.isValid);
        EXPECT_NEAR(height.value, 4.0, 1e-12);
        expectPoint(height.edgeMid, Vector2(5.0 * std::cos(30.0 * kPi / 180.0), 2.5));
    }

    // 60 degrees: the pair facing up/down is now the short one, so width and
    // height trade places
    {
        const auto corners = makeRotated(60.0 * kPi / 180.0);
        const auto width = wy3dApp::computeRectangleDimensionLabel(
            corners, SketchRectangleDimension::Width);
        EXPECT_TRUE(width.isValid);
        EXPECT_NEAR(width.value, 4.0, 1e-12);
        expectPoint(width.edgeMid, Vector2(-2.5, -2.5 * sqrt3));

        const auto height = wy3dApp::computeRectangleDimensionLabel(
            corners, SketchRectangleDimension::Height);
        EXPECT_TRUE(height.isValid);
        EXPECT_NEAR(height.value, 10.0, 1e-12);
        expectPoint(height.edgeMid, Vector2(sqrt3, -1.0));
    }

    // 45 degrees, built from an exactly symmetric direction: the tie keeps the
    // first pair on width
    {
        const Vector2 u = Vector2(1.0, 1.0).normalized();
        const Vector2 v(-u.y(), u.x());
        const auto corners = wy3dApp::computeRectangleCorners(
            Vector2(0.0, 0.0), 5.0, 2.0, u, v);

        const auto width = wy3dApp::computeRectangleDimensionLabel(
            corners, SketchRectangleDimension::Width);
        EXPECT_NEAR(width.value, 10.0, 1e-12);
        const auto height = wy3dApp::computeRectangleDimensionLabel(
            corners, SketchRectangleDimension::Height);
        EXPECT_NEAR(height.value, 4.0, 1e-12);
    }
}

TEST(SketchRectangleDimensionMath, RightAngleRotationSwapsWidthAndHeight)
{
    const auto corners = makeRotated(kPi * 0.5); // width along y, height along x

    const auto width = wy3dApp::computeRectangleDimensionLabel(
        corners, SketchRectangleDimension::Width);
    EXPECT_TRUE(width.isValid);
    EXPECT_NEAR(width.value, 4.0, 1e-12); // 2*halfHeight
    expectPoint(width.edgeMid, Vector2(0.0, -5.0));

    const auto height = wy3dApp::computeRectangleDimensionLabel(
        corners, SketchRectangleDimension::Height);
    EXPECT_TRUE(height.isValid);
    EXPECT_NEAR(height.value, 10.0, 1e-12);
    expectPoint(height.edgeMid, Vector2(2.0, 0.0));
}

TEST(SketchRectangleDimensionMath, DegenerateEdgeIsInvalid)
{
    const std::array<Vector2, 4> corners = {
        Vector2(1.0, 1.0), Vector2(1.0, 1.0), Vector2(1.0, 1.0), Vector2(1.0, 1.0) };
    const auto label = wy3dApp::computeRectangleDimensionLabel(
        corners, SketchRectangleDimension::Width);
    EXPECT_FALSE(label.isValid);
}

TEST(SketchRectangleDimensionMath, ResizeWidthKeepsCenterAndCornerIdentity)
{
    const auto corners = makeAxisAligned(); // x in [-4, 6], y in [0, 4]

    const auto result = wy3dApp::computeRectangleResizedCorners(
        corners, SketchRectangleDimension::Width, 20.0, kMinExtent);

    expectPoint(result[0], Vector2(-9.0, 0.0));
    expectPoint(result[1], Vector2(11.0, 0.0));
    expectPoint(result[2], Vector2(11.0, 4.0));
    expectPoint(result[3], Vector2(-9.0, 4.0));
}

TEST(SketchRectangleDimensionMath, ResizeHeightKeepsCenterAndCornerIdentity)
{
    const auto corners = makeAxisAligned(); // x in [-4, 6], y in [0, 4]

    const auto result = wy3dApp::computeRectangleResizedCorners(
        corners, SketchRectangleDimension::Height, 10.0, kMinExtent);

    expectPoint(result[0], Vector2(-4.0, -3.0));
    expectPoint(result[1], Vector2(6.0, -3.0));
    expectPoint(result[2], Vector2(6.0, 7.0));
    expectPoint(result[3], Vector2(-4.0, 7.0));
}

TEST(SketchRectangleDimensionMath, ResizeClampsNegativeAndTinyValues)
{
    const auto corners = makeAxisAligned(); // center (1, 2)

    for (const double value : { -5.0, 0.0, kMinExtent * 0.25 })
    {
        const auto result = wy3dApp::computeRectangleResizedCorners(
            corners, SketchRectangleDimension::Width, value, kMinExtent);
        EXPECT_NEAR(result[0].x(), 1.0 - 0.5 * kMinExtent, 1e-12);
        EXPECT_NEAR(result[1].x(), 1.0 + 0.5 * kMinExtent, 1e-12);
        EXPECT_NEAR(result[0].y(), 0.0, 1e-12);
        EXPECT_NEAR(result[1].y(), 0.0, 1e-12);
    }
}

TEST(SketchRectangleDimensionMath, ResizeRotatedWidthKeepsCenterAndCornerIdentity)
{
    // 30 degrees: the edited axis is the rectangle's own long axis
    {
        const double angle = 30.0 * kPi / 180.0;
        const Vector2 u(std::cos(angle), std::sin(angle));
        const Vector2 v(-u.y(), u.x());

        const auto result = wy3dApp::computeRectangleResizedCorners(
            makeRotated(angle), SketchRectangleDimension::Width, 20.0, kMinExtent);
        const auto expected = wy3dApp::computeRectangleCorners(
            Vector2(0.0, 0.0), 10.0, 2.0, u, v);
        for (int i = 0; i < 4; ++i)
        {
            expectPoint(result[i], expected[i]);
        }
    }

    // 60 degrees: width now names the short pair, so the edited axis is v
    {
        const double angle = 60.0 * kPi / 180.0;
        const Vector2 u(std::cos(angle), std::sin(angle));
        const Vector2 v(-u.y(), u.x());

        const auto result = wy3dApp::computeRectangleResizedCorners(
            makeRotated(angle), SketchRectangleDimension::Width, 10.0, kMinExtent);
        const auto expected = wy3dApp::computeRectangleCorners(
            Vector2(0.0, 0.0), 5.0, 5.0, u, v);
        for (int i = 0; i < 4; ++i)
        {
            expectPoint(result[i], expected[i]);
        }
    }
}
