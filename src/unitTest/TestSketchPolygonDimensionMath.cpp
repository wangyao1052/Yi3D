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

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include <wyMath.h>
#include <wyVector2.h>

#include "utils/SketchPolygonDimensionMath.h"
#include "utils/SketchPolygonDragMath.h"

namespace
{
    using wy::Vector2;

    double wrapAngle(double angle)
    {
        while (angle > wy::PI) angle -= wy::TWO_PI;
        while (angle <= -wy::PI) angle += wy::TWO_PI;
        return angle;
    }
}

TEST(SketchPolygonDimensionMath, LabelReadsInscribedRadius)
{
    const Vector2 center(3.0, -2.0);
    const double radius = 10.0;
    const double rotation = 0.2;
    const int sides = 5;
    const int labelEdge = 2;
    const std::vector<Vector2> corners =
        wy3dApp::computePolygonVertices(center, radius, rotation, sides);

    const wy3dApp::PolygonDimensionLabel label =
        wy3dApp::computePolygonDimensionLabel(
            corners, center, wy3dApp::SketchPolygonDimension::InscribedRadius, labelEdge);

    ASSERT_TRUE(label.isValid);
    EXPECT_EQ(label.edge, labelEdge);
    EXPECT_NEAR(label.value, radius * std::cos(wy::PI / sides), 1e-12);

    const Vector2 expectedMid = (corners[labelEdge] + corners[(labelEdge + 1) % sides]) * 0.5;
    EXPECT_NEAR((label.anchor - expectedMid).length(), 0.0, 1e-12);
    EXPECT_NEAR(label.radialDirection.length(), 1.0, 1e-12);
    EXPECT_NEAR(label.radialDirection.dot((expectedMid - center).normalized()), 1.0, 1e-12);

    // Every edge is at the same distance: the reading is the inscribed radius
    for (int edge = 0; edge < sides; ++edge)
    {
        const wy3dApp::PolygonDimensionLabel other =
            wy3dApp::computePolygonDimensionLabel(
                corners, center, wy3dApp::SketchPolygonDimension::InscribedRadius, edge);
        ASSERT_TRUE(other.isValid);
        EXPECT_NEAR(other.value, label.value, 1e-12);
    }
}

TEST(SketchPolygonDimensionMath, LabelReadsCircumscribedRadius)
{
    const Vector2 center(-3.0, 2.0);
    const double radius = 10.0;
    const int sides = 5;
    const std::vector<Vector2> corners =
        wy3dApp::computePolygonVertices(center, radius, 0.2, sides);

    const wy3dApp::PolygonDimensionLabel label =
        wy3dApp::computePolygonDimensionLabel(
            corners, center, wy3dApp::SketchPolygonDimension::CircumscribedRadius, 4);

    ASSERT_TRUE(label.isValid);
    EXPECT_EQ(label.edge, -1);
    EXPECT_NEAR((label.anchor - corners[0]).length(), 0.0, 1e-12);
    EXPECT_NEAR(label.value, radius, 1e-12);
    EXPECT_NEAR(label.radialDirection.length(), 1.0, 1e-12);
    EXPECT_NEAR(label.radialDirection.dot((corners[0] - center).normalized()), 1.0, 1e-12);

    // The anchor is the canonical first vertex, whatever the label edge is
    for (const int edge : { 0, 2, 4 })
    {
        const wy3dApp::PolygonDimensionLabel other =
            wy3dApp::computePolygonDimensionLabel(
                corners, center, wy3dApp::SketchPolygonDimension::CircumscribedRadius, edge);
        ASSERT_TRUE(other.isValid);
        EXPECT_NEAR((other.anchor - corners[0]).length(), 0.0, 1e-12);
    }
}

TEST(SketchPolygonDimensionMath, CircumscribedAnchorFollowsRotation)
{
    const Vector2 center(1.0, 2.0);
    const double radius = 6.0;
    const int sides = 7;
    for (const double rotation : { 0.0, 0.3, 1.1, -0.7 })
    {
        const std::vector<Vector2> corners =
            wy3dApp::computePolygonVertices(center, radius, rotation, sides);
        const wy3dApp::PolygonDimensionLabel label =
            wy3dApp::computePolygonDimensionLabel(
                corners, center, wy3dApp::SketchPolygonDimension::CircumscribedRadius, 0);
        ASSERT_TRUE(label.isValid);
        EXPECT_NEAR((label.anchor - corners[0]).length(), 0.0, 1e-12);
        EXPECT_NEAR(label.value, radius, 1e-12);
    }
}

TEST(SketchPolygonDimensionMath, LabelEdgeIsClosestToX)
{
    const Vector2 center(-4.0, 1.0);
    const double radius = 6.0;
    const double rotation = 0.2;
    const int sides = 5;
    const std::vector<Vector2> corners =
        wy3dApp::computePolygonVertices(center, radius, rotation, sides);

    const int edge = wy3dApp::selectPolygonLabelEdge(corners, center);
    const int start = wy3dApp::selectCanonicalPolygonStart(corners, center);
    EXPECT_EQ(start, 0);

    // Brute force: the chosen midpoint direction has the largest +X component
    double bestX = -2.0;
    for (int i = 0; i < sides; ++i)
    {
        const Vector2 mid = (corners[i] + corners[(i + 1) % sides]) * 0.5 - center;
        bestX = std::max(bestX, mid.normalized().x());
    }
    const Vector2 chosenMid =
        ((corners[edge] + corners[(edge + 1) % sides]) * 0.5 - center).normalized();
    EXPECT_NEAR(chosenMid.x(), bestX, 1e-12);
    EXPECT_EQ(edge, 4);
}

TEST(SketchPolygonDimensionMath, TiesPreferPlusYSide)
{
    // Rotation exactly half a step: two vertices sit symmetrically around +X
    const Vector2 center(1.0, 1.0);
    const double radius = 5.0;
    const int sides = 5;
    const double step = wy::TWO_PI / sides;
    const std::vector<Vector2> corners =
        wy3dApp::computePolygonVertices(center, radius, 0.5 * step, sides);

    // The +Y side vertex wins the tie
    EXPECT_EQ(wy3dApp::selectCanonicalPolygonStart(corners, center), 0);
    // The edge that straddles +X carries the label
    EXPECT_EQ(wy3dApp::selectPolygonLabelEdge(corners, center), sides - 1);
}

TEST(SketchPolygonDimensionMath, RejectsDegenerateInput)
{
    const Vector2 center;
    const std::vector<Vector2> corners =
        wy3dApp::computePolygonVertices(center, 5.0, 0.0, 5);

    EXPECT_FALSE(wy3dApp::computePolygonDimensionLabel(
        {}, center, wy3dApp::SketchPolygonDimension::InscribedRadius, 0).isValid);
    EXPECT_FALSE(wy3dApp::computePolygonDimensionLabel(
        corners, center, wy3dApp::SketchPolygonDimension::InscribedRadius, -1).isValid);
    EXPECT_FALSE(wy3dApp::computePolygonDimensionLabel(
        corners, center, wy3dApp::SketchPolygonDimension::InscribedRadius, 5).isValid);
    EXPECT_FALSE(wy3dApp::computePolygonDimensionLabel(
        { Vector2(0.0, 0.0), Vector2(1.0, 0.0) }, center,
        wy3dApp::SketchPolygonDimension::InscribedRadius, 0).isValid);

    // Degenerate edge (midpoint coincides with the center)
    const std::vector<Vector2> collapsed = {
        Vector2(1.0, 0.0), Vector2(-1.0, 0.0), Vector2(0.0, 0.0) };
    EXPECT_FALSE(wy3dApp::computePolygonDimensionLabel(
        collapsed, center, wy3dApp::SketchPolygonDimension::InscribedRadius, 0).isValid);

    // The circumscribed anchor is the first vertex itself
    EXPECT_FALSE(wy3dApp::computePolygonDimensionLabel(
        { center, center, center }, center,
        wy3dApp::SketchPolygonDimension::CircumscribedRadius, 0).isValid);
    EXPECT_FALSE(wy3dApp::computePolygonDimensionLabel(
        { Vector2(0.0, 0.0), Vector2(1.0, 0.0) }, center,
        wy3dApp::SketchPolygonDimension::CircumscribedRadius, 0).isValid);
}

TEST(SketchPolygonDimensionMath, MinValueFollowsTheSelectedRadius)
{
    const int sides = 5;
    const double minLength = 0.5;
    const double minInradius = wy3dApp::computePolygonMinInradius(sides, minLength);

    EXPECT_NEAR(wy3dApp::computePolygonMinDimensionValue(
        wy3dApp::SketchPolygonDimension::InscribedRadius, sides, minLength),
        minInradius, 1e-12);
    EXPECT_NEAR(wy3dApp::computePolygonMinDimensionValue(
        wy3dApp::SketchPolygonDimension::CircumscribedRadius, sides, minLength),
        wy3dApp::computePolygonCircumradius(minInradius, sides), 1e-12);
    EXPECT_NEAR(wy3dApp::computePolygonMinDimensionValue(
        wy3dApp::SketchPolygonDimension::CircumscribedRadius, sides, minLength),
        minInradius / std::cos(wy::PI / sides), 1e-12);
}

TEST(SketchPolygonDimensionMath, ResizeUsesTheSelectedRadius)
{
    const Vector2 center(2.0, -3.0);
    const double rotation = 0.4;
    const int sides = 5;
    const double step = wy::TWO_PI / sides;
    const double minInradius = 1.0;

    // Inscribed reading: every edge midpoint is `value` away from the center
    const std::vector<Vector2> inscribed =
        wy3dApp::computePolygonDimensionResizedVertices(
            center, rotation, sides,
            wy3dApp::SketchPolygonDimension::InscribedRadius, 4.0, minInradius);
    ASSERT_EQ(static_cast<int>(inscribed.size()), sides);
    for (int i = 0; i < sides; ++i)
    {
        const Vector2 mid = (inscribed[i] + inscribed[(i + 1) % sides]) * 0.5;
        EXPECT_NEAR((mid - center).length(), 4.0, 1e-12);
    }

    // Circumscribed reading: every vertex is `value` away from the center
    const double circumRadius = 7.0;
    const std::vector<Vector2> circumscribed =
        wy3dApp::computePolygonDimensionResizedVertices(
            center, rotation, sides,
            wy3dApp::SketchPolygonDimension::CircumscribedRadius,
            circumRadius, minInradius);
    ASSERT_EQ(static_cast<int>(circumscribed.size()), sides);
    for (int i = 0; i < sides; ++i)
    {
        const Vector2 offset = circumscribed[i] - center;
        EXPECT_NEAR(offset.length(), circumRadius, 1e-12);
        EXPECT_NEAR(wrapAngle(std::atan2(offset.y(), offset.x()) - rotation - i * step),
                    0.0, 1e-12);
    }

    // Values below the minimum clamp at the same minimum inradius
    const double minCircumRadius = wy3dApp::computePolygonCircumradius(minInradius, sides);
    const std::vector<Vector2> clamped =
        wy3dApp::computePolygonDimensionResizedVertices(
            center, rotation, sides,
            wy3dApp::SketchPolygonDimension::CircumscribedRadius, 0.5, minInradius);
    ASSERT_EQ(static_cast<int>(clamped.size()), sides);
    for (int i = 0; i < sides; ++i)
    {
        EXPECT_NEAR((clamped[i] - center).length(), minCircumRadius, 1e-12);
    }

    // Non-finite input is refused
    EXPECT_TRUE(wy3dApp::computePolygonDimensionResizedVertices(
        center, rotation, sides, wy3dApp::SketchPolygonDimension::CircumscribedRadius,
        std::numeric_limits<double>::infinity(), minInradius).empty());
}
