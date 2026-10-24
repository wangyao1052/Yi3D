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

#include "utils/SketchDimensionLabelMath.h"

namespace
{
    using wy::Vector2;
    using wy3dApp::SketchDimensionPlacement;

    constexpr double kGap = 8.0;
    constexpr double kWidth = 100.0;
    constexpr double kHeight = 20.0;

    void expectPoint(const Vector2& actual, const Vector2& expected, double tol = 1e-12)
    {
        EXPECT_NEAR(actual.x(), expected.x(), tol);
        EXPECT_NEAR(actual.y(), expected.y(), tol);
    }

    Vector2 pixelOffset(SketchDimensionPlacement placement,
                        const Vector2& radialDirection = Vector2())
    {
        return wy3dApp::computeSketchDimensionPixelOffset(
            placement, kGap, kWidth, kHeight, radialDirection);
    }
}

TEST(SketchDimensionLabelMath, FixedPlacementsMatchTheRectangleLabels)
{
    expectPoint(pixelOffset(SketchDimensionPlacement::Below), Vector2(0.0, -18.0));
    expectPoint(pixelOffset(SketchDimensionPlacement::Above), Vector2(0.0, 18.0));
    expectPoint(pixelOffset(SketchDimensionPlacement::Right), Vector2(58.0, 0.0));
}

TEST(SketchDimensionLabelMath, RadialAlongAxisKeepsTheNearEdgeAtTheGap)
{
    // +X: the near edge is half the label width away
    expectPoint(pixelOffset(SketchDimensionPlacement::Radial, Vector2(3.0, 0.0)),
                Vector2(58.0, 0.0));
    // -Y: half the label height
    expectPoint(pixelOffset(SketchDimensionPlacement::Radial, Vector2(0.0, -7.0)),
                Vector2(0.0, -18.0));
}

TEST(SketchDimensionLabelMath, RadialDiagonalUsesTheBoxSupport)
{
    const double half = std::sqrt(0.5);
    const Vector2 direction(half, half);
    const double distance = kGap + 0.5 * kWidth * half + 0.5 * kHeight * half;
    expectPoint(pixelOffset(SketchDimensionPlacement::Radial, Vector2(1.0, 1.0)),
                direction * distance);
}

TEST(SketchDimensionLabelMath, RadialZeroDirectionFallsBackBelow)
{
    expectPoint(pixelOffset(SketchDimensionPlacement::Radial, Vector2()),
                Vector2(0.0, -18.0));
}

TEST(SketchDimensionLabelMath, RadialIgnoresTheDirectionLength)
{
    expectPoint(pixelOffset(SketchDimensionPlacement::Radial, Vector2(25.0, 0.0)),
                pixelOffset(SketchDimensionPlacement::Radial, Vector2(1.0, 0.0)));
}
