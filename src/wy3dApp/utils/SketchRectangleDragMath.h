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

#ifndef WY3DAPP_SKETCH_RECTANGLE_DRAG_MATH_H
#define WY3DAPP_SKETCH_RECTANGLE_DRAG_MATH_H

#include <algorithm>
#include <array>
#include <cmath>

#include <wyVector2.h>

#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

enum class SketchRectangleHandle
{
    Corner = 0,
    EdgeMid = 1,
    Center = 2,
};

// 规范参数化角点：
// c0 = C - a*U - b*V, c1 = C + a*U - b*V, c2 = C + a*U + b*V, c3 = C - a*U + b*V
inline void computeRectangleCorners(const wy::Vector2& center,
                                    double halfWidth,
                                    double halfHeight,
                                    const wy::Vector2& u,
                                    const wy::Vector2& v,
                                    std::array<wy::Vector2, 4>& outCorners)
{
    outCorners[0] = center - halfWidth * u - halfHeight * v;
    outCorners[1] = center + halfWidth * u - halfHeight * v;
    outCorners[2] = center + halfWidth * u + halfHeight * v;
    outCorners[3] = center - halfWidth * u + halfHeight * v;
}

inline std::array<wy::Vector2, 4> computeRectangleCorners(const wy::Vector2& center,
                                                           double halfWidth,
                                                           double halfHeight,
                                                           const wy::Vector2& u,
                                                           const wy::Vector2& v)
{
    std::array<wy::Vector2, 4> corners{};
    computeRectangleCorners(center, halfWidth, halfHeight, u, v, corners);
    return corners;
}

// 根据拖拽开始时的 4 角点/朝向/半尺寸，计算拖拽后的 4 角点。
// 角点：对角点固定，同时改变宽高；
// 边中点：对边固定，该边沿法向平移；
// 中心：整体平移。
// 允许拖过对角点/对边：矩形会翻转并从固定角点/对边向反方向生长。
// 尺寸钳制到最小尺寸（2*minHalfSize），不会退化为零。
inline std::array<wy::Vector2, 4> computeRectangleDragCorners(
    const std::array<wy::Vector2, 4>& startCorners,
    const wy::Vector2& u,
    const wy::Vector2& v,
    double halfWidth,
    double halfHeight,
    SketchRectangleHandle handle,
    int index,
    const wy::Vector2& curPos,
    double minHalfSize)
{
    const double minExtent = 2.0 * minHalfSize;

    switch (handle)
    {
    case SketchRectangleHandle::Corner:
    {
        const int fixedIndex = (index + 2) % 4;
        const wy::Vector2 fixed = startCorners[fixedIndex];
        const wy::Vector2 delta = curPos - fixed;

        // 有符号的整条边长，允许跨过固定角点后变为负值（翻转）
        double du = delta.dot(u);
        double dv = delta.dot(v);
        du = (du >= 0.0) ? std::max(du, minExtent) : std::min(du, -minExtent);
        dv = (dv >= 0.0) ? std::max(dv, minExtent) : std::min(dv, -minExtent);

        // 由起始角点判断哪条邻边沿 u 方向、哪条沿 v 方向
        const int adjacent1 = (index + 1) % 4;
        const int adjacent3 = (index + 3) % 4;
        const bool uAdjacentIs1 =
            std::fabs((startCorners[adjacent1] - fixed).dot(u)) >
            std::fabs((startCorners[adjacent3] - fixed).dot(u));
        const int uIndex = uAdjacentIs1 ? adjacent1 : adjacent3;
        const int vIndex = uAdjacentIs1 ? adjacent3 : adjacent1;

        std::array<wy::Vector2, 4> corners = startCorners;
        corners[fixedIndex] = fixed;
        corners[uIndex] = fixed + du * u;
        corners[vIndex] = fixed + dv * v;
        corners[index] = fixed + du * u + dv * v;
        return corners;
    }
    case SketchRectangleHandle::EdgeMid:
    {
        const wy::Vector2 fixedMid =
            (startCorners[(index + 2) % 4] + startCorners[(index + 3) % 4]) * 0.5;
        const wy::Vector2 draggedMid =
            (startCorners[index] + startCorners[(index + 1) % 4]) * 0.5;

        wy::Vector2 axis = draggedMid - fixedMid;
        const double startSeparation = axis.length();
        if (startSeparation < 1e-12)
        {
            return startCorners;
        }
        axis = axis / startSeparation;

        // 有符号的边间距，允许跨过对边后变为负值（翻转）
        double separation = (curPos - fixedMid).dot(axis);
        separation = (separation >= 0.0)
            ? std::max(separation, minExtent)
            : std::min(separation, -minExtent);

        const wy::Vector2 offset = (separation - startSeparation) * axis;
        std::array<wy::Vector2, 4> corners = startCorners;
        corners[index] = startCorners[index] + offset;
        corners[(index + 1) % 4] = startCorners[(index + 1) % 4] + offset;
        return corners;
    }
    case SketchRectangleHandle::Center:
    default:
    {
        // 中心手柄的位置就是 4 角点平均，拖动时直接跟随当前点
        return computeRectangleCorners(curPos, halfWidth, halfHeight, u, v);
    }
    }
}

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_RECTANGLE_DRAG_MATH_H
