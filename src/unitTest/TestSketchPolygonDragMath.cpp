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

#include <cmath>
#include <limits>
#include <vector>

#include <wyMath.h>
#include <wyVector2.h>

#include "utils/SketchPolygonDragMath.h"

namespace
{
    using wy::Vector2;

    double angleOf(const Vector2& point, const Vector2& center)
    {
        const Vector2 offset = point - center;
        return std::atan2(offset.y(), offset.x());
    }

    double wrapAngle(double angle)
    {
        while (angle > wy::PI) angle -= wy::TWO_PI;
        while (angle <= -wy::PI) angle += wy::TWO_PI;
        return angle;
    }

    double inradiusOf(const std::vector<Vector2>& corners, const Vector2& center, int edge)
    {
        const int sides = static_cast<int>(corners.size());
        const Vector2 mid = (corners[edge] + corners[(edge + 1) % sides]) * 0.5;
        return (mid - center).length();
    }
}

TEST(SketchPolygonDragMath, VertexGeneration)
{
    const Vector2 center(2.0, -3.0);
    const double radius = 6.0;
    const double rotation = 0.3;
    const int sides = 7;
    const std::vector<Vector2> corners =
        wy3dApp::computePolygonVertices(center, radius, rotation, sides);
    ASSERT_EQ(static_cast<int>(corners.size()), sides);

    const double step = wy::TWO_PI / sides;
    for (int i = 0; i < sides; ++i)
    {
        EXPECT_NEAR((corners[i] - center).length(), radius, 1e-12);
        EXPECT_NEAR(wrapAngle(angleOf(corners[i], center) - rotation - i * step), 0.0, 1e-12);
    }

    EXPECT_TRUE(wy3dApp::computePolygonVertices(center, radius, rotation, 2).empty());
}

TEST(SketchPolygonDragMath, CornerDragFollowsCursor)
{
    const Vector2 center(2.0, 3.0);
    const double rotation = 0.2;
    const int sides = 5;
    const int index = 2;
    const double step = wy::TWO_PI / sides;

    const double cursorAngle = 1.0;
    const double cursorRadius = 20.0;
    const Vector2 curPos = center
        + cursorRadius * Vector2(std::cos(cursorAngle), std::sin(cursorAngle));

    const std::vector<Vector2> corners = wy3dApp::computePolygonCornerDragVertices(
        center, rotation, sides, index, curPos, 0.1);
    ASSERT_EQ(static_cast<int>(corners.size()), sides);

    // The dragged vertex follows the cursor; the rest rotate with it
    EXPECT_NEAR((corners[index] - curPos).length(), 0.0, 1e-12);
    for (int i = 0; i < sides; ++i)
    {
        EXPECT_NEAR((corners[i] - center).length(), cursorRadius, 1e-12);
        const double expectedAngle = cursorAngle + (i - index) * step;
        EXPECT_NEAR(wrapAngle(angleOf(corners[i], center) - expectedAngle), 0.0, 1e-12);
    }

    // The center does not move
    Vector2 average;
    for (const Vector2& corner : corners) average += corner;
    average = average / static_cast<double>(sides);
    EXPECT_NEAR((average - center).length(), 0.0, 1e-12);
}

TEST(SketchPolygonDragMath, CornerDragNearCenterKeepsRotation)
{
    const Vector2 center(-1.0, 5.0);
    const double rotation = 0.4;
    const int sides = 5;
    const double minInradius = 1.0;
    const double minCircumRadius = wy3dApp::computePolygonCircumradius(minInradius, sides);

    const Vector2 curPos = center + Vector2(1e-14, 0.0);
    const std::vector<Vector2> corners = wy3dApp::computePolygonCornerDragVertices(
        center, rotation, sides, 1, curPos, minInradius);
    ASSERT_EQ(static_cast<int>(corners.size()), sides);

    for (int i = 0; i < sides; ++i)
    {
        EXPECT_TRUE(std::isfinite(corners[i].x()));
        EXPECT_TRUE(std::isfinite(corners[i].y()));
        EXPECT_NEAR((corners[i] - center).length(), minCircumRadius, 1e-12);
        const double expectedAngle = rotation + i * wy::TWO_PI / sides;
        EXPECT_NEAR(wrapAngle(angleOf(corners[i], center) - expectedAngle), 0.0, 1e-12);
    }
}

TEST(SketchPolygonDragMath, CornerDragNonFiniteFallsBackToMinimum)
{
    const Vector2 center;
    const double rotation = -0.7;
    const int sides = 6;
    const double minInradius = 2.0;
    const double minCircumRadius = wy3dApp::computePolygonCircumradius(minInradius, sides);

    const Vector2 curPos(std::numeric_limits<double>::quiet_NaN(), 1.0);
    const std::vector<Vector2> corners = wy3dApp::computePolygonCornerDragVertices(
        center, rotation, sides, 3, curPos, minInradius);
    ASSERT_EQ(static_cast<int>(corners.size()), sides);

    for (int i = 0; i < sides; ++i)
    {
        EXPECT_TRUE(std::isfinite(corners[i].x()));
        EXPECT_TRUE(std::isfinite(corners[i].y()));
        EXPECT_NEAR((corners[i] - center).length(), minCircumRadius, 1e-12);
    }
    EXPECT_NEAR(wrapAngle(angleOf(corners[0], center) - rotation), 0.0, 1e-12);
}

TEST(SketchPolygonDragMath, EdgeMidDragOnlyChangesInradius)
{
    const Vector2 center(1.0, -1.0);
    const double rotation = 0.2;
    const int sides = 5;
    const int index = 1;
    const double minInradius = 1.0;
    const double step = wy::TWO_PI / sides;

    const double midAngle = rotation + (index + 0.5) * step;
    const Vector2 normal(std::cos(midAngle), std::sin(midAngle));
    const Vector2 curPos = center + 7.0 * normal;

    const std::vector<Vector2> corners = wy3dApp::computePolygonEdgeMidDragVertices(
        center, rotation, sides, index, curPos, minInradius);
    ASSERT_EQ(static_cast<int>(corners.size()), sides);

    EXPECT_NEAR(inradiusOf(corners, center, index), 7.0, 1e-12);
    for (int i = 0; i < sides; ++i)
    {
        EXPECT_NEAR((corners[i] - center).length(),
            wy3dApp::computePolygonCircumradius(7.0, sides), 1e-12);
        // Rotation is unchanged
        const double expectedAngle = rotation + i * step;
        EXPECT_NEAR(wrapAngle(angleOf(corners[i], center) - expectedAngle), 0.0, 1e-12);
    }
}

TEST(SketchPolygonDragMath, EdgeMidDragTangentialNoResponse)
{
    const Vector2 center(0.0, 2.0);
    const double radius = 10.0;
    const double rotation = -0.3;
    const int sides = 6;
    const int index = 2;
    const double step = wy::TWO_PI / sides;

    const std::vector<Vector2> start =
        wy3dApp::computePolygonVertices(center, radius, rotation, sides);
    const double midAngle = rotation + (index + 0.5) * step;
    const Vector2 normal(std::cos(midAngle), std::sin(midAngle));
    const Vector2 tangent(-normal.y(), normal.x());
    const Vector2 startMid = (start[index] + start[(index + 1) % sides]) * 0.5;
    const Vector2 curPos = startMid + 5.0 * tangent;

    const std::vector<Vector2> corners = wy3dApp::computePolygonEdgeMidDragVertices(
        center, rotation, sides, index, curPos, 1.0);
    ASSERT_EQ(static_cast<int>(corners.size()), sides);
    for (int i = 0; i < sides; ++i)
    {
        EXPECT_NEAR((corners[i] - start[i]).length(), 0.0, 1e-12);
    }
}

TEST(SketchPolygonDragMath, EdgeMidDragAcrossCenterClamps)
{
    const Vector2 center(3.0, 3.0);
    const double rotation = 0.5;
    const int sides = 5;
    const int index = 3;
    const double minInradius = 2.0;
    const double step = wy::TWO_PI / sides;

    const double midAngle = rotation + (index + 0.5) * step;
    const Vector2 normal(std::cos(midAngle), std::sin(midAngle));
    const Vector2 curPos = center - 3.0 * normal;

    const std::vector<Vector2> corners = wy3dApp::computePolygonEdgeMidDragVertices(
        center, rotation, sides, index, curPos, minInradius);
    ASSERT_EQ(static_cast<int>(corners.size()), sides);

    EXPECT_NEAR(inradiusOf(corners, center, index), minInradius, 1e-12);
    // No flip: the rotation and the winding stay the same
    for (int i = 0; i < sides; ++i)
    {
        const double expectedAngle = rotation + i * step;
        EXPECT_NEAR(wrapAngle(angleOf(corners[i], center) - expectedAngle), 0.0, 1e-12);
    }
}

TEST(SketchPolygonDragMath, ResizeKeepsCenterAndRotation)
{
    const Vector2 center(-2.0, 4.0);
    const double rotation = 0.9;
    const int sides = 5;
    const double step = wy::TWO_PI / sides;

    const std::vector<Vector2> corners =
        wy3dApp::computePolygonResizedVertices(center, rotation, sides, 4.0, 1.0);
    ASSERT_EQ(static_cast<int>(corners.size()), sides);
    for (int i = 0; i < sides; ++i)
    {
        const double expectedAngle = rotation + i * step;
        EXPECT_NEAR(wrapAngle(angleOf(corners[i], center) - expectedAngle), 0.0, 1e-12);
    }
    for (int i = 0; i < sides; ++i)
    {
        EXPECT_NEAR(inradiusOf(corners, center, i), 4.0, 1e-12);
    }

    // Values below the minimum are clamped
    const std::vector<Vector2> clamped =
        wy3dApp::computePolygonResizedVertices(center, rotation, sides, 0.5, 1.0);
    ASSERT_EQ(static_cast<int>(clamped.size()), sides);
    EXPECT_NEAR(inradiusOf(clamped, center, 0), 1.0, 1e-12);

    // Non-finite input is refused
    EXPECT_TRUE(wy3dApp::computePolygonResizedVertices(
        center, rotation, sides, std::numeric_limits<double>::infinity(), 1.0).empty());
}

TEST(SketchPolygonDragMath, StableForManySides)
{
    const Vector2 center(1e5, -1e5);
    const double minInradius = 0.5;
    const Vector2 curPos = center + Vector2(10.0, 0.0);

    for (const int sides : { 3, 60, 100 })
    {
        const std::vector<Vector2> dragged = wy3dApp::computePolygonCornerDragVertices(
            center, 0.1, sides, sides / 2, curPos, minInradius);
        ASSERT_EQ(static_cast<int>(dragged.size()), sides);
        for (const Vector2& corner : dragged)
        {
            EXPECT_TRUE(std::isfinite(corner.x()));
            EXPECT_TRUE(std::isfinite(corner.y()));
            EXPECT_NEAR((corner - center).length(), 10.0, 1e-9);
        }

        const std::vector<Vector2> edgeDragged = wy3dApp::computePolygonEdgeMidDragVertices(
            center, 0.1, sides, 0, center + Vector2(5.0, 0.0), minInradius);
        ASSERT_EQ(static_cast<int>(edgeDragged.size()), sides);
        // The cursor projects onto the fixed outward normal of edge 0
        const double expectedInradius =
            5.0 * std::cos(0.1 + 0.5 * wy::TWO_PI / sides);
        const double expectedCircum =
            wy3dApp::computePolygonCircumradius(expectedInradius, sides);
        for (const Vector2& corner : edgeDragged)
        {
            EXPECT_TRUE(std::isfinite(corner.x()));
            EXPECT_NEAR((corner - center).length(), expectedCircum, 1e-9);
        }
    }
}
