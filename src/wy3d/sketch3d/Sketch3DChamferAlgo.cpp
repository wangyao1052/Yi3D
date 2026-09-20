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

#include <utils/wy3dSketch3DChamferAlgo.h>

#include <algorithm>
#include <cassert>
#include <cmath>

#include <wyVector3.h>
#include <wy3dSketchCurve3D.h>
#include <wy3dSketchLine3D.h>
#include <utils/wy3dSketch3DCurveIntersectionUtil.h>

NS_WY3D_BEG

Sketch3DChamferAlgo::Result Sketch3DChamferAlgo::chamferLineLine(double D1, double D2, double tol,
    const SketchCurve3D* pCurve1st, const wy::Vector3& pickPos1st,
    const SketchCurve3D* pCurve2nd, const wy::Vector3& pickPos2nd,
    Sketch3DChamferData& outData)
{
    assert(D1 > 0.0);
    assert(D2 > 0.0);
    assert(tol > 0.0);

    const SketchLine3D* pLine1st = SketchLine3D::cast(pCurve1st);
    const SketchLine3D* pLine2nd = SketchLine3D::cast(pCurve2nd);
    if (!pLine1st || !pLine2nd || pLine1st == pLine2nd)
    {
        return Result::NotStraight;
    }
    if (D1 <= tol || D2 <= tol)
    {
        return Result::TooShort;
    }

    const wy::Vector3 startPnt1st = pLine1st->getStartPoint();
    const wy::Vector3 endPnt1st = pLine1st->getEndPoint();
    const wy::Vector3 startPnt2nd = pLine2nd->getStartPoint();
    const wy::Vector3 endPnt2nd = pLine2nd->getEndPoint();

    // A segment that collapsed to a point
    const wy::Vector3 lineVec1st = endPnt1st - startPnt1st;
    const wy::Vector3 lineVec2nd = endPnt2nd - startPnt2nd;
    const double lineLen1st = lineVec1st.length();
    const double lineLen2nd = lineVec2nd.length();
    if (lineLen1st <= tol || lineLen2nd <= tol)
    {
        return Result::TooShort;
    }

    // Unit directions of the two segments
    wy::Vector3 lineDir1st = lineVec1st;
    lineDir1st.normalize();
    wy::Vector3 lineDir2nd = lineVec2nd;
    lineDir2nd.normalize();

    // The corner where the two lines' supports meet
    wy::Vector3 p0;
    if (!Sketch3DCurveIntersectionUtil::intersectInfiniteLines(pCurve1st, pCurve2nd, p0, tol))
    {
        return Result::Parallel;
    }

    // Segment 1
    double t1(0.0); // The end that survives (0.0 or 1.0)
    double param1st = (p0 - startPnt1st).dot(lineDir1st) / lineLen1st; // Where the corner sits on segment 1
    if (std::fabs(param1st) <= tol) // Corner at the start
    {
        t1 = 1.0;
    }
    else if (std::fabs(param1st - 1.0) <= tol) // Corner at the end
    {
        t1 = 0.0;
    }
    else if (param1st > 0.0 && param1st < 1.0) // Corner inside the segment
    {
        if ((pickPos1st - p0).dot(startPnt1st - p0) >= 0.0)
            t1 = 0.0;
        else
            t1 = 1.0;
    }
    else // Corner outside the segment
    {
        t1 = param1st > 1.0 ? 0.0 : 1.0;
    }
    wy::Vector3 p1 = p0;
    if (t1 == 1.0) p1 += D1 * lineDir1st;
    else p1 -= D1 * lineDir1st;
    param1st = (p1 - startPnt1st).dot(lineDir1st) / lineLen1st;

    // Result
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
        return Result::TooShort;
    }

    // Segment 2
    double t2(0.0); // The end that survives (0.0 or 1.0)
    double param2nd = (p0 - startPnt2nd).dot(lineDir2nd) / lineLen2nd; // Where the corner sits on segment 2
    if (std::fabs(param2nd) <= tol) // Corner at the start
    {
        t2 = 1.0;
    }
    else if (std::fabs(param2nd - 1.0) <= tol) // Corner at the end
    {
        t2 = 0.0;
    }
    else if (param2nd > 0.0 && param2nd < 1.0) // Corner inside the segment
    {
        if ((pickPos2nd - p0).dot(startPnt2nd - p0) >= 0.0)
            t2 = 0.0;
        else
            t2 = 1.0;
    }
    else // Corner outside the segment
    {
        t2 = param2nd > 1.0 ? 0.0 : 1.0;
    }
    wy::Vector3 p2 = p0;
    if (t2 == 1.0) p2 += D2 * lineDir2nd;
    else p2 -= D2 * lineDir2nd;
    param2nd = (p2 - startPnt2nd).dot(lineDir2nd) / lineLen2nd;

    // Result
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
        return Result::TooShort;
    }

    // The chamfer segment is too short
    if ((p1 - p2).length() <= tol)
    {
        return Result::TooShort;
    }

    // Result
    outData.startParam1st = startParam1st;
    outData.endParam1st = endParam1st;
    outData.startParam2nd = startParam2nd;
    outData.endParam2nd = endParam2nd;
    outData.chamferStartPnt = p1;
    outData.chamferEndPnt = p2;

    return Result::Ok;
}

NS_WY3D_END
