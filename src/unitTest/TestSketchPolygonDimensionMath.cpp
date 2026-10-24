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
#include <vector>

#include <wyMath.h>
#include <wyVector2.h>

#include "utils/SketchPolygonDimensionMath.h"
#include "utils/SketchPolygonDragMath.h"

namespace
{
    using wy::Vector2;
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
        wy3dApp::computePolygonDimensionLabel(corners, center, labelEdge);

    ASSERT_TRUE(label.isValid);
    EXPECT_EQ(label.edge, labelEdge);
    EXPECT_NEAR(label.value, radius * std::cos(wy::PI / sides), 1e-12);

    const Vector2 expectedMid = (corners[labelEdge] + corners[(labelEdge + 1) % sides]) * 0.5;
    EXPECT_NEAR((label.edgeMid - expectedMid).length(), 0.0, 1e-12);
    EXPECT_NEAR(label.radialDirection.length(), 1.0, 1e-12);
    EXPECT_NEAR(label.radialDirection.dot((expectedMid - center).normalized()), 1.0, 1e-12);

    // Every edge is at the same distance: the reading is the inscribed radius
    for (int edge = 0; edge < sides; ++edge)
    {
        const wy3dApp::PolygonDimensionLabel other =
            wy3dApp::computePolygonDimensionLabel(corners, center, edge);
        ASSERT_TRUE(other.isValid);
        EXPECT_NEAR(other.value, label.value, 1e-12);
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

    EXPECT_FALSE(wy3dApp::computePolygonDimensionLabel({}, center, 0).isValid);
    EXPECT_FALSE(wy3dApp::computePolygonDimensionLabel(corners, center, -1).isValid);
    EXPECT_FALSE(wy3dApp::computePolygonDimensionLabel(corners, center, 5).isValid);
    EXPECT_FALSE(wy3dApp::computePolygonDimensionLabel(
        { Vector2(0.0, 0.0), Vector2(1.0, 0.0) }, center, 0).isValid);

    // Degenerate edge (midpoint coincides with the center)
    const std::vector<Vector2> collapsed = {
        Vector2(1.0, 0.0), Vector2(-1.0, 0.0), Vector2(0.0, 0.0) };
    EXPECT_FALSE(wy3dApp::computePolygonDimensionLabel(collapsed, center, 0).isValid);
}
