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

#include "utils/SketchRectangleDragMath.h"

namespace
{
    using wy::Vector2;
    using wy3dApp::SketchRectangleHandle;

    constexpr double kMinHalf = 0.0005;

    std::array<Vector2, 4> makeAxisAligned(double halfWidth = 5.0,
                                           double halfHeight = 2.0,
                                           const Vector2& center = Vector2(0.0, 0.0))
    {
        return wy3dApp::computeRectangleCorners(
            center, halfWidth, halfHeight, Vector2(1.0, 0.0), Vector2(0.0, 1.0));
    }

    void expectPoint(const Vector2& actual, const Vector2& expected, double tol = 1e-9)
    {
        EXPECT_NEAR(actual.x(), expected.x(), tol);
        EXPECT_NEAR(actual.y(), expected.y(), tol);
    }

    void expectIsRectangle(const std::array<Vector2, 4>& corners)
    {
        for (int i = 0; i < 4; ++i)
        {
            const Vector2 a = corners[(i + 1) % 4] - corners[i];
            const Vector2 b = corners[(i + 2) % 4] - corners[(i + 1) % 4];
            EXPECT_NEAR(a.dot(b), 0.0, 1e-9);
        }
        expectPoint((corners[1] - corners[0]), (corners[2] - corners[3]));
        expectPoint((corners[3] - corners[0]), (corners[2] - corners[1]));
    }
}

TEST(SketchRectangleDragMath, CornerDrag)
{
    const auto start = makeAxisAligned();

    // 拖角点 2（对角点 c0 固定）
    const auto result = wy3dApp::computeRectangleDragCorners(
        start, Vector2(1.0, 0.0), Vector2(0.0, 1.0), 5.0, 2.0,
        SketchRectangleHandle::Corner, 2, Vector2(8.0, 3.0), kMinHalf);

    expectPoint(result[0], Vector2(-5.0, -2.0));
    expectPoint(result[1], Vector2(8.0, -2.0));
    expectPoint(result[2], Vector2(8.0, 3.0));
    expectPoint(result[3], Vector2(-5.0, 3.0));
    expectIsRectangle(result);
}

TEST(SketchRectangleDragMath, CornerDragAcrossOppositeCornerFlipsRectangle)
{
    const auto start = makeAxisAligned();

    // 越过对角点继续拖：矩形翻转，被拖角点精确跟随鼠标
    const auto result = wy3dApp::computeRectangleDragCorners(
        start, Vector2(1.0, 0.0), Vector2(0.0, 1.0), 5.0, 2.0,
        SketchRectangleHandle::Corner, 2, Vector2(-10.0, -10.0), kMinHalf);

    expectPoint(result[0], Vector2(-5.0, -2.0));   // 对角点固定
    expectPoint(result[1], Vector2(-10.0, -2.0));
    expectPoint(result[2], Vector2(-10.0, -10.0)); // 被拖角点跟随鼠标
    expectPoint(result[3], Vector2(-5.0, -10.0));
    expectIsRectangle(result);
}

TEST(SketchRectangleDragMath, CornerDragMinimumSizeWhenCrossing)
{
    const auto start = makeAxisAligned();

    // 刚越过对角点：翻转后仍保持最小尺寸，不退化为零
    const auto result = wy3dApp::computeRectangleDragCorners(
        start, Vector2(1.0, 0.0), Vector2(0.0, 1.0), 5.0, 2.0,
        SketchRectangleHandle::Corner, 2, Vector2(-5.0001, -2.0001), kMinHalf);

    // 固定角点 c0=(-5,-2)，最小全边长为 2*kMinHalf，位于固定角点的负方向一侧
    expectPoint(result[0], Vector2(-5.0, -2.0));
    expectPoint(result[1], Vector2(-5.0 - 2.0 * kMinHalf, -2.0));
    expectPoint(result[2], Vector2(-5.0 - 2.0 * kMinHalf, -2.0 - 2.0 * kMinHalf));
    expectPoint(result[3], Vector2(-5.0, -2.0 - 2.0 * kMinHalf));
    expectIsRectangle(result);
}

TEST(SketchRectangleDragMath, EdgeMidDrag)
{
    const auto start = makeAxisAligned();

    // 拖底边中点（边 0）向下：对边（y=+2）固定
    const auto result = wy3dApp::computeRectangleDragCorners(
        start, Vector2(1.0, 0.0), Vector2(0.0, 1.0), 5.0, 2.0,
        SketchRectangleHandle::EdgeMid, 0, Vector2(0.0, -5.0), kMinHalf);

    expectPoint(result[0], Vector2(-5.0, -5.0));
    expectPoint(result[1], Vector2(5.0, -5.0));
    expectPoint(result[2], Vector2(5.0, 2.0));
    expectPoint(result[3], Vector2(-5.0, 2.0));
    expectIsRectangle(result);
}

TEST(SketchRectangleDragMath, EdgeMidDragAcrossOppositeEdgeFlipsRectangle)
{
    const auto start = makeAxisAligned();

    // 拖过对边继续拖：矩形翻转，拖拽边精确跟随鼠标
    const auto result = wy3dApp::computeRectangleDragCorners(
        start, Vector2(1.0, 0.0), Vector2(0.0, 1.0), 5.0, 2.0,
        SketchRectangleHandle::EdgeMid, 0, Vector2(0.0, 100.0), kMinHalf);

    expectPoint(result[2], Vector2(5.0, 2.0));
    expectPoint(result[3], Vector2(-5.0, 2.0));
    expectPoint(result[0], Vector2(-5.0, 100.0));
    expectPoint(result[1], Vector2(5.0, 100.0));
    expectIsRectangle(result);
}

TEST(SketchRectangleDragMath, EdgeMidDragMinimumSizeWhenCrossing)
{
    const auto start = makeAxisAligned();

    // 刚越过对边：翻转后仍保持最小间距
    const auto result = wy3dApp::computeRectangleDragCorners(
        start, Vector2(1.0, 0.0), Vector2(0.0, 1.0), 5.0, 2.0,
        SketchRectangleHandle::EdgeMid, 0, Vector2(0.0, 2.0001), kMinHalf);

    expectPoint(result[2], Vector2(5.0, 2.0));
    expectPoint(result[3], Vector2(-5.0, 2.0));
    expectPoint(result[0], Vector2(-5.0, 2.0 + 2.0 * kMinHalf));
    expectPoint(result[1], Vector2(5.0, 2.0 + 2.0 * kMinHalf));
    expectIsRectangle(result);
}

TEST(SketchRectangleDragMath, CenterDragTranslatesWholeRectangle)
{
    const auto start = makeAxisAligned();

    const auto result = wy3dApp::computeRectangleDragCorners(
        start, Vector2(1.0, 0.0), Vector2(0.0, 1.0), 5.0, 2.0,
        SketchRectangleHandle::Center, 0, Vector2(3.0, 4.0), kMinHalf);

    for (int i = 0; i < 4; ++i)
    {
        expectPoint(result[i], start[i] + Vector2(3.0, 4.0));
    }
    expectIsRectangle(result);
}

TEST(SketchRectangleDragMath, RotatedRectangleKeepsOrientation)
{
    const double angle = 30.0 * 3.14159265358979323846 / 180.0;
    const Vector2 u(std::cos(angle), std::sin(angle));
    const Vector2 v(-u.y(), u.x());
    const Vector2 center(1.0, 2.0);
    const auto start = wy3dApp::computeRectangleCorners(center, 6.0, 2.0, u, v);

    const auto result = wy3dApp::computeRectangleDragCorners(
        start, u, v, 6.0, 2.0,
        SketchRectangleHandle::Corner, 0, Vector2(-10.0, -5.0), kMinHalf);

    // 对角点 c2 固定
    expectPoint(result[2], start[2]);
    expectIsRectangle(result);

    // 新矩形的边仍与 u/v 平行
    const Vector2 newU = (result[1] - result[0]).normalized();
    const Vector2 newV = (result[3] - result[0]).normalized();
    EXPECT_NEAR(std::fabs(newU.dot(u)), 1.0, 1e-12);
    EXPECT_NEAR(std::fabs(newV.dot(v)), 1.0, 1e-12);
}
