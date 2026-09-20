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

#include <utils/wy3dSketch3DFilletAlgo.h>

#include <algorithm>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <memory>
#include <vector>

#include <Geom2dAPI_ProjectPointOnCurve.hxx>
#include <Geom2d_BSplineCurve.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Standard_Failure.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TColgp_Array1OfPnt2d.hxx>
#include <gp_Pnt2d.hxx>

#include <utils/wy3dSketch3DCurveParam.h>
#include <utils/wy3dSketchFilletAlgo.h>
#include <wyVector2.h>
#include <wyVector3.h>
#include <wy3dMath.h>
#include <wy3dSketchArc3D.h>
#include <wy3dSketchCircle3D.h>
#include <wy3dSketchCurve3D.h>
#include <wy3dSketchEllipse3D.h>
#include <wy3dSketchEllipseArc3D.h>
#include <wy3dSketchLine3D.h>
#include <wy3dSketchSpline3D.h>

NS_WY3D_BEG

namespace
{
    enum class CurveKind
    {
        Line,
        Conic,        // a circle or an arc: both carry a plane of their own
        Spline,
        Unsupported,  // an ellipse or an ellipse arc, which the 2D algorithm refuses too
        Missing,      // nothing there at all
    };

    // The four shapes the 2D algorithm has an entry point for, in the order its entry points are
    // written in: a pair is solved as (lower, higher) and the answer swapped back if the caller
    // named them the other way round. A circle and an arc are the same shape to the plane-finding
    // code above and different ones here, because only the arc has angles to carry.
    enum class CurveForm
    {
        Line = 0,
        Circle = 1,
        Arc = 2,
        Spline = 3,
    };

    // Whether the side question the spline entry points ask can be answered at all. They project
    // the other curve's pick onto the spline to learn which side of it that pick lies on, and
    // Geom2dAPI_ProjectPointOnCurve answers with the empty set once the foot of the perpendicular
    // falls past an end of the spline - it reports interior extrema only, not the nearest end. A
    // pick that falls there is an ordinary shape rather than a rare one, since the pick is on the
    // other curve and that curve is free to run on past where the spline stops. The entry points
    // take the empty set for an internal failure and assert on it, so the same question is asked
    // here first, with the same two arguments, and its answer becomes this function's result.
    //
    // Not asked of a circle against a spline: filletCircleSpline already answers the empty set
    // itself, by falling back to whichever end of the spline is nearer the pick.
    bool isSideAnswerable(const wy::Vector2& pickOnOtherCurve,
        const Handle(Geom2d_BSplineCurve)& pSpline)
    {
        if (pSpline.IsNull()) return false;
        Geom2dAPI_ProjectPointOnCurve projector(
            gp_Pnt2d(pickOnOtherCurve.x(), pickOnOtherCurve.y()), pSpline);
        return projector.NbPoints() > 0;
    }

    bool formOf(const SketchCurve3D* pCurve, CurveForm& outForm)
    {
        if (SketchLine3D::cast(pCurve)) outForm = CurveForm::Line;
        else if (SketchCircle3D::cast(pCurve)) outForm = CurveForm::Circle;
        else if (SketchArc3D::cast(pCurve)) outForm = CurveForm::Arc;
        else if (SketchSpline3D::cast(pCurve)) outForm = CurveForm::Spline;
        else return false;
        return true;
    }

    // Whether a range describes a piece of curve the command layer can keep. This is the rule the
    // command's own applier applies, asked here first so that the two cannot disagree: a pair the
    // applier would refuse must not be reported as a solution, or the preview would offer a fillet
    // the click cannot commit. The applier's refusal is a legitimate answer rather than a failure,
    // because it is exactly what a tangency landing on an end of a spline produces.
    //
    // A spline is the one curve whose range cannot leave its own domain: the command would have to
    // invent geometry to extend it rather than reveal it. Lines and arcs are extended instead, so
    // only emptiness is asked of them. A circle is never trimmed at all - a single tangency does not
    // cut one, and cutting the 0 degree point out of it would leave a degenerate arc - so the whole
    // circle is the only range that means anything for it.
    bool isUsableRange(CurveForm form, double startParam, double endParam, double tol)
    {
        if ((endParam - startParam) <= tol) return false;
        if (CurveForm::Spline == form) return (startParam >= 0.0 && endParam <= 1.0);
        if (CurveForm::Circle == form) return (startParam == 0.0 && endParam == 1.0);
        return true;
    }

    CurveKind kindOf(const SketchCurve3D* pCurve)
    {
        if (!pCurve) return CurveKind::Missing;
        if (SketchLine3D::cast(pCurve)) return CurveKind::Line;
        if (SketchCircle3D::cast(pCurve) || SketchArc3D::cast(pCurve)) return CurveKind::Conic;
        if (SketchSpline3D::cast(pCurve)) return CurveKind::Spline;
        return CurveKind::Unsupported;
    }

    double planeDistance(const Sketch3DFrame& frame, const wy::Vector3& pnt)
    {
        return std::fabs((pnt - frame.origin).dot(frame.normal));
    }

    // An orthonormal frame from a centre, a plane normal and an in-plane direction. The stored
    // normal and xDir of an entity are taken as they come: what comes back is orthogonal by
    // construction, which is what the angle arithmetic downstream relies on, rather than being
    // whatever the two happened to be.
    //
    // False when the direction has no component across the normal, since then there is no way to
    // tell which way in the plane xDir was meant to point.
    bool makeFrame(const wy::Vector3& origin, const wy::Vector3& normal, const wy::Vector3& xDir,
        Sketch3DFrame& outFrame)
    {
        if (normal.length() <= 0.0) return false;

        const wy::Vector3 unitNormal = normal.normalized();
        const wy::Vector3 inPlane = xDir - unitNormal * xDir.dot(unitNormal);
        if (inPlane.length() <= 0.0) return false;

        outFrame.origin = origin;
        outFrame.normal = unitNormal;
        outFrame.xDir = inPlane.normalized();
        outFrame.yDir = outFrame.normal.cross(outFrame.xDir);
        return true;
    }

    // The plane a set of points lies in. The two poles farthest apart give one direction and the
    // pole farthest from that line gives the other; their cross product is the normal. For a planar
    // set this is that plane exactly.
    //
    // The obvious alternative, Newell's method - summing the edge cross products - is the one to
    // avoid here, and it is worth saying why since it is right for a convex polygon. The sum is a
    // signed area, so a set that bulges to both sides of its own chord, which is precisely the shape
    // an S-shaped spline's poles make, has its two lobes cancel and comes back with no normal at
    // all. This construction has no such blind spot.
    //
    // False when the points have no extent across any single direction, or none along any. A
    // collinear list lies in every plane through it, so there is no plane here to report in, and a
    // spline is the only curve whose plane has to be reconstructed this way.
    bool planeThroughPoints(const std::vector<wy::Vector3>& points,
        wy::Vector3& outOrigin, wy::Vector3& outNormal)
    {
        if (points.size() < 2) return false;

        outOrigin = wy::Vector3::kZero;
        for (const wy::Vector3& pnt : points)
        {
            outOrigin += pnt;
        }
        outOrigin /= static_cast<double>(points.size());

        double extent = 0.0;
        for (const wy::Vector3& pnt : points)
        {
            extent = std::max(extent, (pnt - outOrigin).length());
        }
        if (extent <= 0.0) return false;

        double widestSpan = 0.0;
        wy::Vector3 span = wy::Vector3::kZero;
        for (std::size_t i = 0; i < points.size(); ++i)
        {
            for (std::size_t j = i + 1; j < points.size(); ++j)
            {
                const wy::Vector3 delta = points[j] - points[i];
                if (delta.length() > widestSpan)
                {
                    widestSpan = delta.length();
                    span = delta;
                }
            }
        }
        if (widestSpan <= 0.0) return false;

        // How far the set reaches across that direction. A comparison of two lengths, so the
        // tolerance needs no squaring to stay meaningful.
        const wy::Vector3 alongSpan = span / widestSpan;
        double widestAcross = 0.0;
        wy::Vector3 across = wy::Vector3::kZero;
        for (const wy::Vector3& pnt : points)
        {
            const wy::Vector3 offset = pnt - outOrigin;
            const wy::Vector3 perpendicular = offset - alongSpan * offset.dot(alongSpan);
            if (perpendicular.length() > widestAcross)
            {
                widestAcross = perpendicular.length();
                across = perpendicular;
            }
        }
        if (widestAcross <= Sketch3DFilletAlgo::kCoplanarTol * extent) return false;

        outNormal = alongSpan.cross(across).normalized();
        return true;
    }

    // The poles of a spline as points. Taken from the entity rather than from a fitted curve so
    // that the plane is the one the stored geometry describes.
    std::vector<wy::Vector3> polesOf(const SketchSpline3D& spline)
    {
        const std::vector<wy::Vector3>& points = spline.getPoints();
        return std::vector<wy::Vector3>(points.begin(), points.end());
    }

    // Whether pCurve lies in the frame's plane. A conic is tested through its own plane rather than
    // through sampled points, and a spline through its poles: a B-spline stays inside the convex
    // hull of its poles, so poles inside the plane put the curve inside it too. That direction is
    // the only one that matters - it can never accept a curve that leaves the plane - while the
    // converse would need the curve sampled, which is work no caller is waiting for.
    bool isCoplanarWithFrame(const SketchCurve3D* pCurve, CurveKind kind, const Sketch3DFrame& frame)
    {
        switch (kind)
        {
        case CurveKind::Line:
        {
            const SketchLine3D* pLine = SketchLine3D::cast(pCurve);
            // The two ends are the whole test, and both of them: a segment's distance from a plane
            // is at its largest at one of its ends, so testing both is not a sample of the segment
            // but a complete account of it. Testing one would say nothing about the other, which for
            // a short segment is where all the deviation lives.
            //
            // A third test on the direction, |d.n| <= tol, would be the natural thing to reach for
            // and is worth spelling out as absent: d is in units of length, so the same tilt passes
            // for a short segment and fails for a long one, and any segment it rejected would be one
            // whose ends both lie in the plane - that is, a segment that is in the plane by every
            // measure that matters here.
            if (planeDistance(frame, pLine->getStartPoint()) > Sketch3DFilletAlgo::kCoplanarTol)
            {
                return false;
            }
            return planeDistance(frame, pLine->getEndPoint()) <= Sketch3DFilletAlgo::kCoplanarTol;
        }

        case CurveKind::Conic:
        {
            wy::Vector3 center;
            wy::Vector3 normal;
            if (const SketchCircle3D* pCircle = SketchCircle3D::cast(pCurve))
            {
                center = pCircle->getCenter();
                normal = pCircle->getNormal();
            }
            else
            {
                const SketchArc3D* pArc = SketchArc3D::cast(pCurve);
                center = pArc->getCenter();
                normal = pArc->getNormal();
            }

            if (normal.length() <= 0.0) return false;
            // Planes are undirected, so an anti-parallel normal is the same plane and must pass.
            // Comparing the cross product against zero says so without a dot-product sign test.
            if (normal.normalized().cross(frame.normal).length() > Sketch3DFilletAlgo::kCoplanarTol)
            {
                return false;
            }
            return planeDistance(frame, center) <= Sketch3DFilletAlgo::kCoplanarTol;
        }

        case CurveKind::Spline:
        {
            const SketchSpline3D* pSpline = SketchSpline3D::cast(pCurve);
            for (const wy::Vector3& pole : pSpline->getPoints())
            {
                if (planeDistance(frame, pole) > Sketch3DFilletAlgo::kCoplanarTol) return false;
            }
            return true;
        }

        default:
            return false;
        }
    }

    // The pick as a normalized parameter on its own curve, clamped the way the 2D dispatcher clamps
    // it. False when the curve cannot answer at all: a degenerate one has no parameter to give, and
    // a pick left stale by an undo can genuinely miss. Neither is a reason to assert - both are
    // reachable - so the caller turns them into a refusal.
    bool pickParamOf(const SketchCurve3D* pCurve, CurveForm form, const wy::Vector3& pickPos,
        double& outParam)
    {
        double param = DBL_MAX;
        switch (form)
        {
        case CurveForm::Line:
            param = Sketch3DCurveParam::getParamOfLine(SketchLine3D::cast(pCurve), pickPos);
            break;
        case CurveForm::Circle:
            param = Sketch3DCurveParam::getParamOfCircle(SketchCircle3D::cast(pCurve), pickPos);
            break;
        case CurveForm::Arc:
            param = Sketch3DCurveParam::getParamOfArc(SketchArc3D::cast(pCurve), pickPos);
            break;
        case CurveForm::Spline:
            param = Sketch3DCurveParam::getPickParamOfSpline(*SketchSpline3D::cast(pCurve), pickPos);
            break;
        }

        if (DBL_MAX == param) return false;
        outParam = std::clamp(param, 0.0, 1.0);
        return true;
    }
}

wy::Vector2 Sketch3DFrame::to2D(const wy::Vector3& pnt) const
{
    const wy::Vector3 offset = pnt - origin;
    return wy::Vector2(offset.dot(xDir), offset.dot(yDir));
}

wy::Vector3 Sketch3DFrame::to3D(const wy::Vector2& pnt) const
{
    return origin + xDir * pnt.x() + yDir * pnt.y();
}

double Sketch3DFrame::deltaOf(const wy::Vector3& xDirOfConic) const
{
    const double angle = std::atan2(xDirOfConic.dot(yDir), xDirOfConic.dot(xDir));
    return angle < 0.0 ? angle + wy3d::TWO_PI : angle;
}

Sketch3DFilletAlgo::Result Sketch3DFilletAlgo::computeFrame(const SketchCurve3D* pCurve1st,
    const SketchCurve3D* pCurve2nd, Sketch3DFrame& outFrame)
{
    const CurveKind kind1st = kindOf(pCurve1st);
    const CurveKind kind2nd = kindOf(pCurve2nd);
    if (CurveKind::Missing == kind1st || CurveKind::Missing == kind2nd) return Result::DegenerateInput;
    if (CurveKind::Unsupported == kind1st || CurveKind::Unsupported == kind2nd)
    {
        return Result::UnsupportedPair;
    }
    if (pCurve1st == pCurve2nd) return Result::SameEntity;

    // The frame source is chosen by what pins a plane down best, so the answer does not depend on
    // which curve the caller picked first: a conic knows its own plane, a spline's poles describe
    // one, and two lines have to span one between them.
    const SketchCurve3D* pSourceConic = nullptr;
    const SketchCurve3D* pSourceSpline = nullptr;
    if (CurveKind::Conic == kind1st) pSourceConic = pCurve1st;
    else if (CurveKind::Conic == kind2nd) pSourceConic = pCurve2nd;
    if (CurveKind::Spline == kind1st) pSourceSpline = pCurve1st;
    else if (CurveKind::Spline == kind2nd) pSourceSpline = pCurve2nd;

    if (pSourceConic)
    {
        wy::Vector3 center;
        wy::Vector3 normal;
        wy::Vector3 xDir;
        if (const SketchCircle3D* pCircle = SketchCircle3D::cast(pSourceConic))
        {
            center = pCircle->getCenter();
            normal = pCircle->getNormal();
            xDir = pCircle->getXDir();
        }
        else
        {
            const SketchArc3D* pArc = SketchArc3D::cast(pSourceConic);
            center = pArc->getCenter();
            normal = pArc->getNormal();
            xDir = pArc->getXDir();
        }
        if (!makeFrame(center, normal, xDir, outFrame)) return Result::DegenerateInput;
    }
    else if (pSourceSpline)
    {
        const SketchSpline3D* pSpline = SketchSpline3D::cast(pSourceSpline);
        wy::Vector3 origin;
        wy::Vector3 normal;
        if (!planeThroughPoints(polesOf(*pSpline), origin, normal)) return Result::DegenerateInput;

        // Any in-plane direction will do, and the first pole far enough from the plane's origin to
        // be trusted as a direction is as good as any other.
        wy::Vector3 xDir = wy::Vector3::kZero;
        for (const wy::Vector3& pole : pSpline->getPoints())
        {
            const wy::Vector3 candidate = pole - origin;
            if (candidate.length() > kCoplanarTol)
            {
                xDir = candidate;
                break;
            }
        }
        if (!makeFrame(origin, normal, xDir, outFrame)) return Result::DegenerateInput;
    }
    else
    {
        const SketchLine3D* pLine1st = SketchLine3D::cast(pCurve1st);
        const SketchLine3D* pLine2nd = SketchLine3D::cast(pCurve2nd);
        const wy::Vector3 startPnt1st = pLine1st->getStartPoint();
        const wy::Vector3 dir1st = pLine1st->getEndPoint() - startPnt1st;
        const wy::Vector3 dir2nd = pLine2nd->getEndPoint() - pLine2nd->getStartPoint();
        if (dir1st.length() <= 0.0 || dir2nd.length() <= 0.0) return Result::DegenerateInput;

        wy::Vector3 normal = dir1st.cross(dir2nd);
        if (normal.length() > kParallelSinTol * dir1st.length() * dir2nd.length())
        {
            normal = normal.normalized();
        }
        else
        {
            // Parallel lines are still coplanar - every plane holding one and a point of the other
            // holds both - so this is not a refusal. The plane is pinned instead by the offset
            // between them, and two lines offset by nothing at all have no plane to be found.
            const wy::Vector3 offset = pLine2nd->getStartPoint() - startPnt1st;
            normal = dir1st.cross(offset);
            if (normal.length() <= 0.0) return Result::DegenerateInput;
            normal = normal.normalized();
        }
        if (!makeFrame(startPnt1st, normal, dir1st, outFrame)) return Result::DegenerateInput;
    }

    if (!isCoplanarWithFrame(pCurve1st, kind1st, outFrame)) return Result::NotCoplanar;
    if (!isCoplanarWithFrame(pCurve2nd, kind2nd, outFrame)) return Result::NotCoplanar;
    return Result::Ok;
}

Sketch3DFilletAlgo::Result Sketch3DFilletAlgo::projectCurve(const SketchCurve3D* pCurve,
    const Sketch3DFrame& frame, Sketch3DProjectedCurve& outProjected)
{
    switch (kindOf(pCurve))
    {
    case CurveKind::Missing:
        return Result::DegenerateInput;

    case CurveKind::Unsupported:
        return Result::UnsupportedPair;

    case CurveKind::Line:
    {
        const SketchLine3D* pLine = SketchLine3D::cast(pCurve);
        outProjected.kind = Sketch3DProjectedCurve::Kind::Line;
        outProjected.lineStart = frame.to2D(pLine->getStartPoint());
        outProjected.lineEnd = frame.to2D(pLine->getEndPoint());
        return Result::Ok;
    }

    case CurveKind::Conic:
    {
        wy::Vector3 center;
        wy::Vector3 xDir;
        double radius = 0.0;
        if (const SketchCircle3D* pCircle = SketchCircle3D::cast(pCurve))
        {
            center = pCircle->getCenter();
            xDir = pCircle->getXDir();
            radius = pCircle->getRadius();
            outProjected.kind = Sketch3DProjectedCurve::Kind::Circle;
        }
        else
        {
            const SketchArc3D* pArc = SketchArc3D::cast(pCurve);
            center = pArc->getCenter();
            xDir = pArc->getXDir();
            radius = pArc->getRadius();
            outProjected.kind = Sketch3DProjectedCurve::Kind::Arc;

            // An arc stores its angles against its own xDir, and the frame is not obliged to share
            // it, so both ends are carried over. Both, not one: the 2D arc entry points rebuild the
            // sweep from the pair themselves rather than take it on trust.
            const double delta = frame.deltaOf(xDir);
            outProjected.startAngle = pArc->getStartAngle() + delta;
            outProjected.endAngle = pArc->getEndAngle() + delta;
        }
        outProjected.center = frame.to2D(center);
        outProjected.radius = radius;
        return Result::Ok;
    }

    case CurveKind::Spline:
    {
        const SketchSpline3D* pSpline = SketchSpline3D::cast(pCurve);
        const Handle(Geom_BSplineCurve) pOccSpline = pSpline->getOccSpline();
        if (pOccSpline.IsNull()) return Result::DegenerateInput;

        // Every spline the entity builds is non-rational. A rational one would have to carry its
        // weights across rather than have this quietly drop them and move the curve.
        assert(!pOccSpline->IsRational());

        // The flags have to cross with the numbers. A closed spline is a periodic curve, whose
        // poles, knots and multiplicities satisfy the periodic rule instead of the clamped one, and
        // building a non-periodic curve out of them is not a shape that exists: OCCT refuses it by
        // raising. The whole construction is therefore guarded, which is how the rest of this
        // library treats OCCT - a refusal comes back as a result, not as an exception.
        outProjected.kind = Sketch3DProjectedCurve::Kind::Spline;
        try
        {
            const TColgp_Array1OfPnt& poles = pOccSpline->Poles();
            TColgp_Array1OfPnt2d flatPoles(1, poles.Length());
            for (int i = poles.Lower(); i <= poles.Upper(); ++i)
            {
                const wy::Vector3 pole(poles(i).X(), poles(i).Y(), poles(i).Z());
                const wy::Vector2 flat = frame.to2D(pole);
                flatPoles.SetValue(i, gp_Pnt2d(flat.x(), flat.y()));
            }

            // The knots and multiplicities are the originals, untouched: an isometry commutes with
            // B-spline evaluation, so a parameter that read [0,1] on the entity still reads [0,1] on
            // the projection, and the parameters the 2D algorithm hands back can be used as they are.
            outProjected.pBSpline = new Geom2d_BSplineCurve(flatPoles, pOccSpline->Knots(),
                pOccSpline->Multiplicities(), pOccSpline->Degree(), pOccSpline->IsPeriodic());
        }
        catch (const Standard_Failure&)
        {
            return Result::DegenerateInput;
        }
        return Result::Ok;
    }

    default:
        return Result::DegenerateInput;
    }
}

void Sketch3DFilletData::swap()
{
    std::swap(startParam1st, startParam2nd);
    std::swap(endParam1st, endParam2nd);
}

Sketch3DFilletAlgo::Result Sketch3DFilletAlgo::fillet(double R, double tol,
    const SketchCurve3D* pCurve1st, const wy::Vector3& pickPos1st,
    const SketchCurve3D* pCurve2nd, const wy::Vector3& pickPos2nd,
    Sketch3DFilletData& outData)
{
    assert(R > 0.0);
    assert(tol > 0.0);

    // The plane first, because it is both the refusal and the frame everything else is expressed
    // in. Nothing below can run without it.
    Sketch3DFrame frame;
    const Result frameResult = computeFrame(pCurve1st, pCurve2nd, frame);
    if (Result::Ok != frameResult) return frameResult;

    CurveForm form1st = CurveForm::Line;
    CurveForm form2nd = CurveForm::Line;
    if (!formOf(pCurve1st, form1st) || !formOf(pCurve2nd, form2nd)) return Result::UnsupportedPair;

    Sketch3DProjectedCurve flat1st;
    Sketch3DProjectedCurve flat2nd;
    Result result = projectCurve(pCurve1st, frame, flat1st);
    if (Result::Ok != result) return result;
    result = projectCurve(pCurve2nd, frame, flat2nd);
    if (Result::Ok != result) return result;

    double pickParam1st = 0.0;
    double pickParam2nd = 0.0;
    if (!pickParamOf(pCurve1st, form1st, pickPos1st, pickParam1st)) return Result::NoSolution;
    if (!pickParamOf(pCurve2nd, form2nd, pickPos2nd, pickParam2nd)) return Result::NoSolution;

    // Both picks are re-evaluated on their curve and only then projected, rather than projected as
    // they arrived. A 3D pick lands near a curve rather than on it, and what the 2D algorithm
    // reasons about is a point that is on the curve - which is also what the 2D command hands it.
    const wy::Vector2 pick1st = frame.to2D(pCurve1st->getPointAt(pickParam1st));
    const wy::Vector2 pick2nd = frame.to2D(pCurve2nd->getPointAt(pickParam2nd));

    // A spline is the one shape whose entry points can be handed a question that has no answer, and
    // answered by asserting. See isSideAnswerable; the circle is left out because that entry point
    // handles it. Asked in the caller's order rather than the canonical one, because the picks are
    // what the question is about and those belong to the caller's two curves.
    if (CurveForm::Spline == form1st && CurveForm::Circle != form2nd
        && !isSideAnswerable(pick2nd, flat1st.pBSpline)) return Result::NoSolution;
    if (CurveForm::Spline == form2nd && CurveForm::Circle != form1st
        && !isSideAnswerable(pick1st, flat2nd.pBSpline)) return Result::NoSolution;

    // Present the pair in the order the 2D entry points are written in. A pair the caller named the
    // other way round is solved by the forward entry point with its arguments swapped and the
    // answer swapped back afterwards, which is exactly what the 2D command does - and the flag is
    // what tells the algorithm that the second pick arrives from the other side.
    const bool isReversed = form2nd < form1st;
    const Sketch3DProjectedCurve& head = isReversed ? flat2nd : flat1st;
    const Sketch3DProjectedCurve& tail = isReversed ? flat1st : flat2nd;
    const CurveForm headForm = isReversed ? form2nd : form1st;
    const CurveForm tailForm = isReversed ? form1st : form2nd;
    const wy::Vector2 headPick = isReversed ? pick2nd : pick1st;
    const wy::Vector2 tailPick = isReversed ? pick1st : pick2nd;
    const bool isSecondPickPosMajor = !isReversed;

    std::shared_ptr<SketchFilletData> pData;
    switch (headForm)
    {
    case CurveForm::Line:
        switch (tailForm)
        {
        case CurveForm::Line:
            pData = SketchFilletAlgo::filletLineLine(R, tol,
                head.lineStart, head.lineEnd, headPick,
                tail.lineStart, tail.lineEnd, tailPick);
            break;
        case CurveForm::Circle:
            pData = SketchFilletAlgo::filletLineCircle(R, tol,
                head.lineStart, head.lineEnd, headPick,
                tail.center, tail.radius, tailPick, isSecondPickPosMajor);
            break;
        case CurveForm::Arc:
            pData = SketchFilletAlgo::filletLineArc(R, tol,
                head.lineStart, head.lineEnd, headPick,
                tail.center, tail.radius, tail.startAngle, tail.endAngle, tailPick,
                isSecondPickPosMajor);
            break;
        case CurveForm::Spline:
            pData = SketchFilletAlgo::filletLineSpline(R, tol,
                head.lineStart, head.lineEnd, headPick,
                tail.pBSpline, tailPick, isSecondPickPosMajor);
            break;
        }
        break;

    case CurveForm::Circle:
        switch (tailForm)
        {
        case CurveForm::Circle:
            pData = SketchFilletAlgo::filletCircleCircle(R, tol,
                head.center, head.radius, headPick,
                tail.center, tail.radius, tailPick);
            break;
        case CurveForm::Arc:
            pData = SketchFilletAlgo::filletCircleArc(R, tol,
                head.center, head.radius, headPick,
                tail.center, tail.radius, tail.startAngle, tail.endAngle, tailPick,
                isSecondPickPosMajor);
            break;
        case CurveForm::Spline:
            pData = SketchFilletAlgo::filletCircleSpline(R, tol,
                head.center, head.radius, headPick,
                tail.pBSpline, tailPick, isSecondPickPosMajor);
            break;
        case CurveForm::Line:
            break; // below the head, so the pair was presented the other way round
        }
        break;

    case CurveForm::Arc:
        switch (tailForm)
        {
        case CurveForm::Arc:
            pData = SketchFilletAlgo::filletArcArc(R, tol,
                head.center, head.radius, head.startAngle, head.endAngle, headPick,
                tail.center, tail.radius, tail.startAngle, tail.endAngle, tailPick);
            break;
        case CurveForm::Spline:
            pData = SketchFilletAlgo::filletArcSpline(R, tol,
                head.center, head.radius, head.startAngle, head.endAngle, headPick,
                tail.pBSpline, tailPick, isSecondPickPosMajor);
            break;
        case CurveForm::Line:
        case CurveForm::Circle:
            break; // below the head, so the pair was presented the other way round
        }
        break;

    case CurveForm::Spline:
        switch (tailForm)
        {
        case CurveForm::Spline:
            // The one pair with nothing to swap: both curves are the same shape, so there is no
            // other way round, and the 2D entry point's flag is left where the 2D command leaves it.
            pData = SketchFilletAlgo::filletSplineSpline(R, tol,
                head.pBSpline, headPick, tail.pBSpline, tailPick);
            break;
        case CurveForm::Line:
        case CurveForm::Circle:
        case CurveForm::Arc:
            break; // below the head, so the pair was presented the other way round
        }
        break;
    }

    if (!pData) return Result::NoSolution;

    outData.startParam1st = pData->startParam1st;
    outData.endParam1st = pData->endParam1st;
    outData.startParam2nd = pData->startParam2nd;
    outData.endParam2nd = pData->endParam2nd;
    outData.filletCenter = frame.to3D(pData->filletCenter);
    outData.filletRadius = pData->filletRadius;
    outData.filletStartAngle = pData->filletStartAngle;
    outData.filletEndAngle = pData->filletEndAngle;
    outData.frame = frame;

    if (isReversed) outData.swap();

    // The arc the command layer builds runs from filletStartAngle to filletEndAngle, and
    // SketchArc3D::getTotalAngle normalizes that difference into [0, 2PI). A sweep of nothing would
    // come back as a full turn and a sweep of a full turn as nothing, so both are refused here
    // rather than handed over to be normalized into the wrong shape of arc.
    const double sweep = outData.filletEndAngle - outData.filletStartAngle;
    if (sweep <= tol || sweep >= wy3d::TWO_PI) return Result::NoSolution;

    // The same question asked of the two ranges, and for the same reason: the command layer keeps
    // each curve over the range it is handed, and refuses the whole fillet when the range describes
    // no piece of curve. That refusal is a real answer rather than a failure - a tangency landing on
    // an end of a spline is exactly what the 2D entry points produce - so a pair the command would
    // refuse must not be reported as a solution in the first place, or the preview would offer a
    // fillet that the click cannot commit.
    if (!isUsableRange(form1st, outData.startParam1st, outData.endParam1st, tol)) return Result::NoSolution;
    if (!isUsableRange(form2nd, outData.startParam2nd, outData.endParam2nd, tol)) return Result::NoSolution;

    return Result::Ok;
}

NS_WY3D_END
