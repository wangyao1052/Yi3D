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

#ifndef WY3D_SKETCH3D_CHAMFER_ALGO_H
#define WY3D_SKETCH3D_CHAMFER_ALGO_H

#include <wyVector3.h>
#include <wy3dDefs.h>

NS_WY3D_BEG

class SketchCurve3D;

// Where a chamfer between two straight 3D sketch curves lands, and how much of each curve is left.
// Every parameter is normalized to [0,1] over the curve's own extent, as SketchChamferAlgo's are,
// and may fall outside that range where the curve has to be extended rather than trimmed.
struct Sketch3DChamferData
{
    // first curve
    double startParam1st;
    double endParam1st;

    // second curve
    double startParam2nd;
    double endParam2nd;

    // Start of the chamfer segment
    wy::Vector3 chamferStartPnt;
    wy::Vector3 chamferEndPnt;
};

// The 3D reading of SketchChamferAlgo::chamferLineLine, and close to a verbatim one: two straight
// curves are always coplanar, so nothing here needs a plane, a frame or a projection.
//
// The one substitution is the corner. A 3D line is unbounded geometry and the two may be skew, so
// the corner comes from Sketch3DCurveIntersectionUtil::intersectInfiniteLines instead of a planar
// intersection, and a pair of skew lines further apart than tol comes back as Parallel: from the
// chamfer's point of view a corner it cannot reach is a corner that is not there.
//
// Each distance is laid off along a curve's own direction from the corner, and which of the two
// halves of a curve survives is decided by the pick, never by the curve's own orientation.
class WY3D_EXPORT Sketch3DChamferAlgo
{
public:
    enum class Result
    {
        Ok = 0,
        NotStraight,  // an operand is null or is not a straight line
        Parallel,     // the two supports have no single corner within tol
        TooShort,     // a distance, a surviving range, or the chamfer segment itself is too short
    };

    // The pick positions need not lie on the curves, and are used as they are. The only thing asked
    // of a pick is which side of the corner it fell on, and projecting it onto its curve first can
    // be shown not to change that answer: with p0 on both lines, the raw and the projected test both
    // reduce to -|lineVec|^2 * t0 * (t - t0).
    static Result chamferLineLine(double D1, double D2, double tol,
        const SketchCurve3D* pCurve1st, const wy::Vector3& pickPos1st,
        const SketchCurve3D* pCurve2nd, const wy::Vector3& pickPos2nd,
        Sketch3DChamferData& outData);
};

NS_WY3D_END

#endif // WY3D_SKETCH3D_CHAMFER_ALGO_H
