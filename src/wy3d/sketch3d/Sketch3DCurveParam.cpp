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

#include <utils/wy3dSketch3DCurveParam.h>

#include <algorithm>
#include <cassert>
#include <cfloat>
#include <cmath>

#include <GeomAPI_ProjectPointOnCurve.hxx>
#include <Standard_Failure.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <wy3dMath.h>
#include <wy3dSketchArc3D.h>
#include <wy3dSketchCircle3D.h>
#include <wy3dSketchEllipse3D.h>
#include <wy3dSketchEllipseArc3D.h>
#include <wy3dSketchLine3D.h>
#include <wy3dSketchSpline3D.h>

NS_WY3D_BEG

namespace
{
// The distance a spline projection may be off by before the point counts as off the curve. This
// is deliberately wider than the 1e-7 used for the parameters: its job is to reject a knot left
// over from before a trim, not to measure precision.
constexpr double kSplineProjectionTol = 1e-5;
} // namespace

void Sketch3DCurveParam::computeFrame(const wy::Vector3& normal, const wy::Vector3& xDir, wy::Vector3& yDir)
{
    yDir = normal.cross(xDir);
}

double Sketch3DCurveParam::polarAngle(const wy::Vector3& xDir, const wy::Vector3& yDir,
    const wy::Vector3& axis, const wy::Vector3& pos)
{
    const wy::Vector3 vec = pos - axis;
    return wy3d::normalizeRadian(std::atan2(vec.dot(yDir), vec.dot(xDir)));
}

double Sketch3DCurveParam::reviseT(double t, double tol)
{
    if (std::fabs(t) <= tol)
    {
        return 0.0;
    }
    else if (std::fabs(t - 1.0) <= tol)
    {
        return 1.0;
    }
    else
    {
        return t;
    }
}

double Sketch3DCurveParam::reviseAngle(double angle, double startAngle, double endAngle, double tol)
{
    assert(angle >= 0.0 && angle < wy3d::TWO_PI);
    assert(startAngle >= 0.0 && startAngle < wy3d::TWO_PI);
    assert(endAngle > startAngle);
    assert((endAngle - startAngle) < wy3d::TWO_PI);

    if (angle < startAngle)
    {
        angle += wy3d::TWO_PI;
    }

    if ((angle - startAngle) <= tol || (angle - startAngle) >= wy3d::TWO_PI - tol)
    {
        return startAngle;
    }
    else if (std::fabs(angle - endAngle) <= tol)
    {
        return endAngle;
    }
    else
    {
        return angle;
    }
}

void Sketch3DCurveParam::subArcAngles(double startAngle, double totalAngle, double fromParam,
    double toParam, double& newStartAngle, double& newEndAngle)
{
    // A full turn is allowed: a whole circle covers one, and its end normalizes back onto its start.
    assert(totalAngle > 0.0 && totalAngle <= wy3d::TWO_PI);

    newStartAngle = wy3d::normalizeRadian(startAngle + fromParam * totalAngle);
    newEndAngle = wy3d::normalizeRadian(startAngle + toParam * totalAngle);
    // The sweep may not wrap past a full turn, which is what tells the two ends' order apart: the
    // normalized end lands on or behind the start only when the range runs forward through the
    // seam. Equality is included so a whole circle comes back as a whole turn instead of nothing.
    if (newEndAngle <= newStartAngle) newEndAngle += wy3d::TWO_PI;
}

double Sketch3DCurveParam::getParamOfLine(const SketchLine3D* pLine, const wy::Vector3& pos, double tol)
{
    assert(pLine);

    const wy::Vector3 startPnt = pLine->getStartPoint();
    const wy::Vector3 lineVec = pLine->getEndPoint() - startPnt;
    const double squaredLength = lineVec.dot(lineVec);
    if (squaredLength <= tol * tol)
    {
        return DBL_MAX;
    }

    // The true projection, not the ratio of lengths: a 3D pick lands on the polyline drawn for
    // the curve, so the position it reports is near the curve rather than on it.
    return reviseT((pos - startPnt).dot(lineVec) / squaredLength, tol);
}

double Sketch3DCurveParam::getParamOfCircle(const SketchCircle3D* pCircle, const wy::Vector3& pos)
{
    assert(pCircle);

    wy::Vector3 yDir;
    computeFrame(pCircle->getNormal(), pCircle->getXDir(), yDir);
    return polarAngle(pCircle->getXDir(), yDir, pCircle->getCenter(), pos) / wy3d::TWO_PI;
}

double Sketch3DCurveParam::getParamOfArc(const SketchArc3D* pArc, const wy::Vector3& pos, double tol)
{
    assert(pArc);

    const double totalAngle = pArc->getTotalAngle();
    if (totalAngle <= tol)
    {
        return DBL_MAX;
    }

    const double startAngle = wy3d::normalizeRadian(pArc->getStartAngle());
    const double endAngle = startAngle + totalAngle;

    wy::Vector3 yDir;
    computeFrame(pArc->getNormal(), pArc->getXDir(), yDir);
    const double angle = reviseAngle(
        polarAngle(pArc->getXDir(), yDir, pArc->getCenter(), pos), startAngle, endAngle, tol);
    return (angle - startAngle) / totalAngle;
}

double Sketch3DCurveParam::getParamOfEllipse(const SketchEllipse3D* pEllipse, const wy::Vector3& pos)
{
    assert(pEllipse);

    wy::Vector3 yDir;
    computeFrame(pEllipse->getNormal(), pEllipse->getXDir(), yDir);
    return polarAngle(pEllipse->getXDir(), yDir, pEllipse->getCenter(), pos) / wy3d::TWO_PI;
}

double Sketch3DCurveParam::getParamOfEllipseArc(const SketchEllipseArc3D* pEllipseArc,
    const wy::Vector3& pos, double tol)
{
    assert(pEllipseArc);

    const double totalAngle = pEllipseArc->getTotalAngle();
    if (totalAngle <= tol)
    {
        return DBL_MAX;
    }

    const double startAngle = wy3d::normalizeRadian(pEllipseArc->getStartAngle());
    const double endAngle = startAngle + totalAngle;

    wy::Vector3 yDir;
    computeFrame(pEllipseArc->getNormal(), pEllipseArc->getXDir(), yDir);
    const double angle = reviseAngle(
        polarAngle(pEllipseArc->getXDir(), yDir, pEllipseArc->getCenter(), pos), startAngle, endAngle, tol);
    return (angle - startAngle) / totalAngle;
}

double Sketch3DCurveParam::getParamOfSpline(const SketchSpline3D* pSpline, const wy::Vector3& pos, double tol)
{
    assert(pSpline);

    return getParamOfSpline(pSpline->getOccSpline(), pos, tol);
}

double Sketch3DCurveParam::getParamOfSpline(const Handle(Geom_BSplineCurve)& pBSpline,
    const wy::Vector3& pos, double tol)
{
    if (pBSpline.IsNull())
    {
        assert(false);
        return DBL_MAX;
    }

    const double firstParam = pBSpline->FirstParameter();
    const double lastParam = pBSpline->LastParameter();
    const double paramRange = lastParam - firstParam;
    if (paramRange <= tol)
    {
        return DBL_MAX;
    }

    wy::Vector3 startPnt, startDir;
    wy::Vector3 endPnt, endDir;
    if (!getBSplineInfo(pBSpline, startPnt, startDir, endPnt, endDir))
    {
        return DBL_MAX;
    }

    GeomAPI_ProjectPointOnCurve projector(gp_Pnt(pos.x(), pos.y(), pos.z()), pBSpline);
    if (projector.NbPoints() > 0 && projector.LowerDistance() <= kSplineProjectionTol)
    {
        const double t = (projector.LowerDistanceParameter() - firstParam) / paramRange;
        return reviseT(t, tol);
    }
    else
    {
        // Off the curve: the point is only relevant when it sits on the tangent taken past one of
        // the ends, which is how a spline gets extended. The overshoot comes back in world units.
        const wy::Vector3 startVec = pos - startPnt;
        const wy::Vector3 endVec = pos - endPnt;
        if (startVec.cross(startDir).length() <= tol && startVec.dot(startDir) < 0.0)
        {
            return startVec.dot(startDir);
        }
        else if (endVec.cross(endDir).length() <= tol && endVec.dot(endDir) > 0.0)
        {
            return 1.0 + endVec.dot(endDir);
        }
        else
        {
            return DBL_MAX;
        }
    }
}

double Sketch3DCurveParam::getPickParamOfSpline(const SketchSpline3D& sketchSpline,
    const wy::Vector3& pos, double tol)
{
    Handle(Geom_BSplineCurve) pBSpline = sketchSpline.getOccSpline();
    if (pBSpline.IsNull())
    {
        assert(false);
        return DBL_MAX;
    }

    const double firstParam = pBSpline->FirstParameter();
    const double lastParam = pBSpline->LastParameter();
    const double paramRange = lastParam - firstParam;
    if (paramRange <= tol)
    {
        assert(false);
        return DBL_MAX;
    }

    GeomAPI_ProjectPointOnCurve projector(gp_Pnt(pos.x(), pos.y(), pos.z()), pBSpline);
    if (projector.NbPoints() > 0)
    {
        const double t = (projector.LowerDistanceParameter() - firstParam) / paramRange;
        return std::clamp(t, 0.0, 1.0);
    }
    else
    {
        assert(false);
        return DBL_MAX;
    }
}

bool Sketch3DCurveParam::getBSplineInfo(const Handle(Geom_BSplineCurve)& pBSpline,
    wy::Vector3& startPnt, wy::Vector3& startDir,
    wy::Vector3& endPnt, wy::Vector3& endDir)
{
    if (pBSpline.IsNull())
    {
        assert(false);
        return false;
    }

    try
    {
        gp_Pnt startPnt3d;
        gp_Vec startDir3d;
        pBSpline->D1(pBSpline->FirstParameter(), startPnt3d, startDir3d);
        startDir3d.Normalize();

        gp_Pnt endPnt3d;
        gp_Vec endDir3d;
        pBSpline->D1(pBSpline->LastParameter(), endPnt3d, endDir3d);
        endDir3d.Normalize();

        startPnt.set(startPnt3d.X(), startPnt3d.Y(), startPnt3d.Z());
        startDir.set(startDir3d.X(), startDir3d.Y(), startDir3d.Z());
        endPnt.set(endPnt3d.X(), endPnt3d.Y(), endPnt3d.Z());
        endDir.set(endDir3d.X(), endDir3d.Y(), endDir3d.Z());
        return true;
    }
    catch (const Standard_Failure&)
    {
        return false;
    }
}

NS_WY3D_END
