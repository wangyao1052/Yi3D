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

#include "SketchChamferAlgo.h"
#include <wyVector2.h>
#include <utils/wy3dCurveIntersectionUtil.h>

std::shared_ptr<SketchChamferData> SketchChamferAlgo::chamferLineLine(double D1, double D2, double tol,
    const wy::Vector2& startPnt1st, const wy::Vector2& endPnt1st, const wy::Vector2& pickPos1st,
    const wy::Vector2& startPnt2nd, const wy::Vector2& endPnt2nd, const wy::Vector2& pickPos2nd)
{
    assert(D1 > 0.0);
    assert(D2 > 0.0);
    assert(tol > 0.0);
    if (D1 <= tol || D2 <= tol) return nullptr; // 圆角过小

    // 直线段退化为点直接返回
    wy::Vector2 lineVec1st = endPnt1st - startPnt1st;
    wy::Vector2 lineVec2nd = endPnt2nd - startPnt2nd;
    double lineLen1st = lineVec1st.length();
    double lineLen2nd = lineVec2nd.length();
    if (lineLen1st <= tol || lineLen2nd <= tol)
    {
        return nullptr;
    }

    // 直线段1和2的方向向量
    wy::Vector2 lineDir1st = lineVec1st;
    lineDir1st.normalize();
    wy::Vector2 lineDir2nd = lineVec2nd;
    lineDir2nd.normalize();

    // 求两条直线的交点
    wy::Vector2 p0;
    if (!wy3d::intersectLineLine(
        startPnt1st, endPnt1st,
        startPnt2nd, endPnt2nd,
        p0))
    {
        return nullptr; // 直线平行
    }

    // 直线段1
    double t1(0.0); // 保留的端点(0.0 or 1.0)
    double param1st = (p0 - startPnt1st).dot(lineDir1st) / lineLen1st; // 交点在线段1上的参数位置
    if (std::fabs(param1st) <= tol) // 交点在起点上
    {
        t1 = 1.0;
    }
    else if (std::fabs(param1st - 1.0) <= tol) // 交点在终点上
    {
        t1 = 0.0;
    }
    else if (param1st > 0.0 && param1st < 1.0) // 交点在直线段上
    {
        if ((pickPos1st - p0).dot(startPnt1st - p0) >= 0.0)
            t1 = 0.0;
        else
            t1 = 1.0;
    }
    else // 交点在直线段外
    {
        t1 = param1st > 1.0 ? 0.0 : 1.0;
    }
    wy::Vector2 p1 = p0;
    if (t1 == 1.0) p1 += D1 * lineDir1st;
    else p1 -= D1 * lineDir1st;
    param1st = (p1 - startPnt1st).dot(lineDir1st) / lineLen1st;
    
    // 结果
    double startParam1st(0.0), endParam1st(1.0);
    if (t1 == 0.0)
    {
        startParam1st = 0.0;
        endParam1st = param1st;
    }
    else
    {
        startParam1st = param1st;
        endParam1st = 1.0;
    }
    if (endParam1st <= startParam1st || std::fabs(endParam1st - startParam1st) <= tol)
    {
        return nullptr;
    }

    // 直线段2
    double t2(0.0); // 保留的端点(0.0 or 1.0)
    double param2nd = (p0 - startPnt2nd).dot(lineDir2nd) / lineLen2nd; // 交点在线段2上的参数位置
    if (std::fabs(param2nd) <= tol) // 交点在起点上
    {
        t2 = 1.0;
    }
    else if (std::fabs(param2nd - 1.0) <= tol) // 交点在终点上
    {
        t2 = 0.0;
    }
    else if (param2nd > 0.0 && param2nd < 1.0) // 交点在直线段上
    {
        if ((pickPos2nd - p0).dot(startPnt2nd - p0) >= 0.0)
            t2 = 0.0;
        else
            t2 = 1.0;
    }
    else // 交点在直线段外
    {
        t2 = param2nd > 1.0 ? 0.0 : 1.0;
    }
    wy::Vector2 p2 = p0;
    if (t2 == 1.0) p2 += D2 * lineDir2nd;
    else p2 -= D2 * lineDir2nd;
    param2nd = (p2 - startPnt2nd).dot(lineDir2nd) / lineLen2nd;

    // 结果
    double startParam2nd(0.0), endParam2nd(1.0);
    if (t2 == 0.0)
    {
        startParam2nd = 0.0;
        endParam2nd = param2nd;
    }
    else
    {
        startParam2nd = param2nd;
        endParam2nd = 1.0;
    }
    if (endParam2nd <= startParam2nd || std::fabs(endParam2nd - startParam2nd) <= tol)
    {
        return nullptr;
    }

    // 倒角直线段距离过短
    if ((p1 - p2).length() <= tol)
    {
        return nullptr;
    }

    // 输出结果
    std::shared_ptr<SketchChamferData> pChamferData = std::make_shared<SketchChamferData>();
    pChamferData->startParam1st = startParam1st;
    pChamferData->endParam1st = endParam1st;
    pChamferData->startParam2nd = startParam2nd;
    pChamferData->endParam2nd = endParam2nd;
    pChamferData->chamferStartPnt = p1;
    pChamferData->chamferEndPnt = p2;

    return pChamferData;
}