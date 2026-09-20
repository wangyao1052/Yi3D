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

#ifndef WY3D_SKETCH3D_CURVE_PARAM_H
#define WY3D_SKETCH3D_CURVE_PARAM_H

#include <Geom_BSplineCurve.hxx>

#include <wyVector3.h>
#include <wy3dDefs.h>

NS_WY3D_BEG

class SketchLine3D;
class SketchCircle3D;
class SketchArc3D;
class SketchEllipse3D;
class SketchEllipseArc3D;
class SketchSpline3D;

// The parameter a point sits at on a 3D sketch curve, each one the exact inverse of the curve's
// own getPointAt. Every function returns DBL_MAX when it cannot answer.
//
// Two things callers depend on:
//  - the parameter is normalized to [0,1] over the curve's own extent, and for an open curve it
//    is deliberately left unclamped, so a point past an end comes back as t < 0 or t > 1. That is
//    how extending finds its target.
//  - the argument is a position, not a curve parameter. It need not lie on the curve: the answer
//    is that position's projection, which is what lets a pick point be used directly.
//
// The ellipse entities store polar angles while OCCT's Geom_Ellipse is parameterized by the
// eccentric anomaly. Nothing here converts between them: getPointAt feeds a polar angle into the
// conversion, so the polar angle at parameter t is exactly the angle the entity stores, and the
// inverse is a plain atan2 in the curve's own frame. Converting here as well would be wrong.
class WY3D_EXPORT Sketch3DCurveParam
{
public:
    // The frame every conic-like entity is parameterized in: yDir = normal x xDir, matching
    // SketchCircle3D::getPointAt and its siblings.
    static void computeFrame(const wy::Vector3& normal, const wy::Vector3& xDir, wy::Vector3& yDir);

    // The angle of pos about the axis, measured from xDir towards yDir, normalized to [0, 2PI).
    static double polarAngle(const wy::Vector3& xDir, const wy::Vector3& yDir,
        const wy::Vector3& axis, const wy::Vector3& pos);

    // Snaps to 0 or 1 when within tol; otherwise it returns t unchanged.
    static double reviseT(double t, double tol = 1e-7);

    // Brings an angle into [startAngle, startAngle + 2PI) and snaps it to whichever of the two
    // ends it is within tol of. The top of the range is not clamped: an angle past endAngle comes
    // back as it is, which is what an arc's extend target relies on.
    static double reviseAngle(double angle, double startAngle, double endAngle, double tol = 1e-7);

    // The angles an arc or an ellipse arc spans once its parameter range is bent out to
    // [fromParam, toParam] - the reading of a parameter range back into the angles it covers, and
    // so the inverse of getParamOfArc.
    //
    // A periodic curve's parameter is *not* signed the way a line's is: reviseAngle wraps every
    // angle forward into [startAngle, startAngle + 2PI), so a target behind the start comes back as
    // a parameter greater than 1 rather than less than 0. The range is what places the two ends;
    // their signs never enter into it. `startAngle` is expected normalized and `totalAngle` in
    // (0, 2PI).
    static void subArcAngles(double startAngle, double totalAngle, double fromParam, double toParam,
        double& newStartAngle, double& newEndAngle);

    static double getParamOfLine(const SketchLine3D* pLine, const wy::Vector3& pos, double tol = 1e-7);
    static double getParamOfCircle(const SketchCircle3D* pCircle, const wy::Vector3& pos);
    static double getParamOfArc(const SketchArc3D* pArc, const wy::Vector3& pos, double tol = 1e-7);
    static double getParamOfEllipse(const SketchEllipse3D* pEllipse, const wy::Vector3& pos);
    static double getParamOfEllipseArc(const SketchEllipseArc3D* pEllipseArc, const wy::Vector3& pos, double tol = 1e-7);

    static double getParamOfSpline(const SketchSpline3D* pSpline, const wy::Vector3& pos, double tol = 1e-7);
    // Same, with the curve already in hand. A graph that projects many points onto one spline
    // should take the handle once: getOccSpline may have to rebuild the curve.
    static double getParamOfSpline(const Handle(Geom_BSplineCurve)& pBSpline, const wy::Vector3& pos,
        double tol = 1e-7);

    // The parameter of a point that is known to be on the spline, so the answer is clamped to
    // [0,1] and no extension ray is consulted.
    static double getPickParamOfSpline(const SketchSpline3D& sketchSpline, const wy::Vector3& pos,
        double tol = 1e-7);

    // The end points and unit end tangents of a spline. False when OCCT refuses to evaluate them.
    static bool getBSplineInfo(const Handle(Geom_BSplineCurve)& pBSpline,
        wy::Vector3& startPnt, wy::Vector3& startDir,
        wy::Vector3& endPnt, wy::Vector3& endDir);
};

NS_WY3D_END

#endif // WY3D_SKETCH3D_CURVE_PARAM_H
