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

#include <utils/wy3dSketch3DCurveIntersectionUtil.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <utility>

#include <GeomAPI_ExtremaCurveCurve.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_Curve.hxx>
#include <Geom_Line.hxx>
#include <Standard_Failure.hxx>
#include <gp_Dir.hxx>
#include <gp_Lin.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <wy3dMath.h>
#include <wy3dSketchArc3D.h>
#include <wy3dSketchCircle3D.h>
#include <wy3dSketchCurve3D.h>
#include <wy3dSketchEllipse3D.h>
#include <wy3dSketchEllipseArc3D.h>
#include <wy3dSketchLine3D.h>
#include <wy3dSketchSpline3D.h>

#include "topo/Sketch3DTopoBuilder.h"

NS_WY3D_BEG

namespace
{
// How many samples the coarse pass of the refinement takes per operand.
constexpr int kRefineSamples = 32;

// How many places the refinement may look at. A crossing is a single dip of the distance
// function, so a handful is plenty and it keeps a pathological pair from running away.
constexpr int kMaxRefineWindows = 6;

// One piece of an operand: a curve plus the interval the operand occupies on it. Almost every
// operand is a single piece. A spline being extended is three - its body and the two tangent rays.
struct Piece
{
    Handle(Geom_Curve) curve;
    double first = 0.0;
    double last = 0.0;
};

double pointDistance(const gp_Pnt& p1, const gp_Pnt& p2)
{
    return p1.Distance(p2);
}

wy::Vector3 toVector(const gp_Pnt& pnt)
{
    return wy::Vector3(pnt.X(), pnt.Y(), pnt.Z());
}

// Only one point is ever stored for a pair of curves, so the residual of the extrema is split
// evenly between them. That keeps the counterpart-validity tests on both sides inside tolerance.
wy::Vector3 midpointOf(const gp_Pnt& p1, const gp_Pnt& p2)
{
    return wy::Vector3(
        (p1.X() + p2.X()) * 0.5, (p1.Y() + p2.Y()) * 0.5, (p1.Z() + p2.Z()) * 0.5);
}

void appendUnique(std::vector<wy::Vector3>& points, const wy::Vector3& pnt, double tol)
{
    for (const wy::Vector3& existing : points)
    {
        if ((pnt - existing).length() <= tol)
        {
            return;
        }
    }
    points.push_back(pnt);
}

double sampleParam(const Piece& piece, int index)
{
    return piece.first + (piece.last - piece.first) * static_cast<double>(index)
        / static_cast<double>(kRefineSamples - 1);
}

// The tangent ray at one end of a spline, as a segment of `reach` leaving that end. A spline
// cannot be widened analytically, so this is what extending one means.
bool makeTangentRay(const wy3d::SketchSpline3D* pSpline, bool atStart, double reach, Piece& outPiece)
{
    assert(pSpline);
    assert(reach > 0.0);

    Handle(Geom_BSplineCurve) pCurve = pSpline->getOccSpline();
    if (pCurve.IsNull())
    {
        assert(false);
        return false;
    }

    try
    {
        gp_Pnt pnt;
        gp_Vec tangent;
        pCurve->D1(atStart ? pCurve->FirstParameter() : pCurve->LastParameter(), pnt, tangent);
        if (tangent.Magnitude() < wy3d::EPS)
        {
            return false;
        }
        tangent.Normalize();
        if (atStart)
        {
            tangent.Reverse();
        }
        outPiece.curve = new Geom_Line(pnt, gp_Dir(tangent));
        outPiece.first = 0.0;
        outPiece.last = reach;
        return true;
    }
    catch (const Standard_Failure&)
    {
        return false;
    }
}

bool buildPieces(const Sketch3DCurveIntersectionUtil::Operand& operand, std::vector<Piece>& outPieces)
{
    if (!operand.pCurve)
    {
        return false;
    }

    wy3d::Sketch3DTopoBuilder builder;
    Piece piece;
    piece.curve = builder.toGeomCurveWidened(operand.pCurve, operand.reach, piece.first, piece.last);
    if (piece.curve.IsNull())
    {
        return false;
    }
    outPieces.push_back(piece);

    if (operand.reach > 0.0)
    {
        if (const wy3d::SketchSpline3D* pSpline = wy3d::SketchSpline3D::cast(operand.pCurve))
        {
            if (operand.extendStart && makeTangentRay(pSpline, true, operand.reach, piece))
            {
                outPieces.push_back(piece);
            }
            if (operand.extendEnd && makeTangentRay(pSpline, false, operand.reach, piece))
            {
                outPieces.push_back(piece);
            }
        }
    }
    return true;
}

// A conic reduced to what deciding "same support" needs. A circle and an arc carry no meaningful
// major-axis direction - the entity's xDir is arbitrary - so the axis is only compared between
// two ellipses.
struct Conic
{
    bool isEllipse = false;
    wy::Vector3 center;
    wy::Vector3 normal;
    wy::Vector3 xDir;
    double majorRadius = 0.0;
    double minorRadius = 0.0;
};

bool toConic(const SketchCurve3D* pCurve, Conic& outConic)
{
    if (const wy3d::SketchCircle3D* pCircle = wy3d::SketchCircle3D::cast(pCurve))
    {
        outConic.center = pCircle->getCenter();
        outConic.normal = pCircle->getNormal();
        outConic.majorRadius = pCircle->getRadius();
        outConic.minorRadius = pCircle->getRadius();
        return true;
    }
    else if (const wy3d::SketchArc3D* pArc = wy3d::SketchArc3D::cast(pCurve))
    {
        outConic.center = pArc->getCenter();
        outConic.normal = pArc->getNormal();
        outConic.majorRadius = pArc->getRadius();
        outConic.minorRadius = pArc->getRadius();
        return true;
    }
    else if (const wy3d::SketchEllipse3D* pEllipse = wy3d::SketchEllipse3D::cast(pCurve))
    {
        outConic.isEllipse = true;
        outConic.center = pEllipse->getCenter();
        outConic.normal = pEllipse->getNormal();
        outConic.xDir = pEllipse->getXDir();
        outConic.majorRadius = pEllipse->getMajorRadius();
        outConic.minorRadius = pEllipse->getMinorRadius();
        return true;
    }
    else if (const wy3d::SketchEllipseArc3D* pEllipseArc = wy3d::SketchEllipseArc3D::cast(pCurve))
    {
        outConic.isEllipse = true;
        outConic.center = pEllipseArc->getCenter();
        outConic.normal = pEllipseArc->getNormal();
        outConic.xDir = pEllipseArc->getXDir();
        outConic.majorRadius = pEllipseArc->getMajorRadius();
        outConic.minorRadius = pEllipseArc->getMinorRadius();
        return true;
    }
    else
    {
        return false;
    }
}

// True when every point of one operand already lies on the other. Trimming a circle into arcs
// makes this the common case, and without the test such a pair yields meaningless knots that
// then have to be filtered out downstream.
bool isSameSupport(const SketchCurve3D* pCurveA, const SketchCurve3D* pCurveB, double tol)
{
    Conic conicA;
    Conic conicB;
    if (!toConic(pCurveA, conicA) || !toConic(pCurveB, conicB))
    {
        return false;
    }

    if ((conicA.center - conicB.center).length() > tol)
    {
        return false;
    }
    if (std::fabs(std::fabs(conicA.normal.dot(conicB.normal)) - 1.0) > 1e-9)
    {
        return false;
    }
    if (std::fabs(conicA.majorRadius - conicB.majorRadius) > tol
        || std::fabs(conicA.minorRadius - conicB.minorRadius) > tol)
    {
        return false;
    }
    if (conicA.isEllipse && conicB.isEllipse
        && std::fabs(std::fabs(conicA.xDir.dot(conicB.xDir)) - 1.0) > 1e-9)
    {
        return false;
    }
    return true;
}

enum class LineLineResult
{
    Crossing,
    Collinear,
    Apart,
};

// Closest approach of two lines restricted to the given parameter intervals, where the parameter
// is the distance along the line's own direction. This is the 3D reading of the 2D
// wy3d::intersectLineLine, and it exists for a reason beyond speed: a crossing is exactly where
// the extremum system of two lines degenerates, so it is the pair where an iterated solver is
// most likely to come back empty without any sign of trouble.
LineLineResult intersectLineLine(const gp_Lin& line1, double lo1, double hi1,
    const gp_Lin& line2, double lo2, double hi2, double tol, gp_Pnt& outPnt)
{
    const gp_Vec dir1(line1.Direction());
    const gp_Vec dir2(line2.Direction());
    const gp_Vec r(line2.Location(), line1.Location()); // r = P1 - P2

    const double a = dir1.Dot(dir1);
    const double e = dir2.Dot(dir2);
    const double b = dir1.Dot(dir2);
    const double c = dir1.Dot(r);
    const double f = dir2.Dot(r);
    const double denom = a * e - b * b;

    if (std::fabs(denom) <= wy3d::EPS)
    {
        // Parallel. Collinear when the offset between them has no component across the direction.
        if (r.Crossed(dir1).Magnitude() <= tol)
        {
            return LineLineResult::Collinear;
        }
        return LineLineResult::Apart;
    }

    double s = std::clamp((b * f - c * e) / denom, lo1, hi1);
    double t = (b * s + f) / e;
    if (t < lo2)
    {
        t = lo2;
        s = std::clamp((b * t - c) / a, lo1, hi1);
    }
    else if (t > hi2)
    {
        t = hi2;
        s = std::clamp((b * t - c) / a, lo1, hi1);
    }

    const gp_Pnt pnt1 = line1.Location().Translated(dir1 * s);
    const gp_Pnt pnt2 = line2.Location().Translated(dir2 * t);
    if (pointDistance(pnt1, pnt2) > tol)
    {
        return LineLineResult::Apart;
    }
    outPnt = gp_Pnt((pnt1.X() + pnt2.X()) * 0.5, (pnt1.Y() + pnt2.Y()) * 0.5,
        (pnt1.Z() + pnt2.Z()) * 0.5);
    return LineLineResult::Crossing;
}

void collectFromExtrema(const Handle(Geom_Curve)& curveA, double aFirst, double aLast,
    const Handle(Geom_Curve)& curveB, double bFirst, double bLast, double tol,
    std::vector<wy::Vector3>& outPoints)
{
    GeomAPI_ExtremaCurveCurve extrema(curveA, curveB, aFirst, aLast, bFirst, bLast);

    const Standard_Integer numExtrema = extrema.NbExtrema();
    for (Standard_Integer i = 1; i <= numExtrema; ++i)
    {
        if (extrema.Distance(i) > tol)
        {
            continue;
        }
        gp_Pnt pnt1;
        gp_Pnt pnt2;
        extrema.Points(i, pnt1, pnt2);
        appendUnique(outPoints, midpointOf(pnt1, pnt2), tol);
    }

    // NbExtrema covers the computed extrema only. Two curves that merely touch at an end may
    // come back from it with nothing at all, so the endpoint-aware answer is asked for too.
    try
    {
        if (extrema.TotalLowerDistance() <= tol)
        {
            gp_Pnt pnt1;
            gp_Pnt pnt2;
            if (extrema.TotalNearestPoints(pnt1, pnt2))
            {
                appendUnique(outPoints, midpointOf(pnt1, pnt2), tol);
            }
        }
    }
    catch (const Standard_Failure&)
    {
        // No total answer available; the computed extrema above are all there is.
    }
}

// Second chance for the pairs the extrema call misses. The distance function of a high degree
// spline has many local minima, and a narrow dip can be stepped over. Sampling first finds where
// the two operands come close, and re-running the extrema on a window around that place has far
// fewer competing roots than the full call had.
void refineBySampling(const Piece& pieceA, const Piece& pieceB, double tol,
    std::vector<wy::Vector3>& outPoints)
{
    const double sizeA = std::fabs(pieceA.last - pieceA.first);
    const double sizeB = std::fabs(pieceB.last - pieceB.first);
    if (sizeA <= wy3d::EPS || sizeB <= wy3d::EPS)
    {
        return;
    }

    std::vector<gp_Pnt> samplesA(kRefineSamples);
    std::vector<gp_Pnt> samplesB(kRefineSamples);
    try
    {
        for (int i = 0; i < kRefineSamples; ++i)
        {
            samplesA[i] = pieceA.curve->Value(sampleParam(pieceA, i));
            samplesB[i] = pieceB.curve->Value(sampleParam(pieceB, i));
        }
    }
    catch (const Standard_Failure&)
    {
        return;
    }

    // The sampling resolution: the widest gap between consecutive samples. A point of the curve
    // is never further than that from the nearest sample, so a crossing always has a sample pair
    // no wider apart than twice it. Gating on this rather than on a fraction of the piece size
    // keeps the trigger tied to what the sampling can actually see - on a piece whose samples
    // are far apart, a tight gate would skip straight past the crossing being looked for.
    double resolution = 0.0;
    for (int i = 0; i + 1 < kRefineSamples; ++i)
    {
        resolution = std::max(resolution, samplesA[i].Distance(samplesA[i + 1]));
        resolution = std::max(resolution, samplesB[i].Distance(samplesB[i + 1]));
    }
    const double slack = 2.0 * resolution;

    // Candidate places, nearest first. Each refinement claims the samples around it, which keeps
    // one crossing from being refined once per nearby sample pair.
    std::vector<std::pair<double, std::pair<int, int>>> candidates;
    for (int i = 0; i < kRefineSamples; ++i)
    {
        for (int j = 0; j < kRefineSamples; ++j)
        {
            const double distance = pointDistance(samplesA[i], samplesB[j]);
            if (distance <= tol + slack)
            {
                candidates.emplace_back(distance, std::make_pair(i, j));
            }
        }
    }
    if (candidates.empty())
    {
        return;
    }
    std::sort(candidates.begin(), candidates.end());

    const double stepA = sizeA / static_cast<double>(kRefineSamples - 1);
    const double stepB = sizeB / static_cast<double>(kRefineSamples - 1);
    std::vector<std::pair<double, double>> windows; // already refined centres
    int numWindows = 0;

    for (const auto& candidate : candidates)
    {
        if (numWindows >= kMaxRefineWindows)
        {
            break;
        }
        const int i = candidate.second.first;
        const int j = candidate.second.second;
        const double centreA = sampleParam(pieceA, i);
        const double centreB = sampleParam(pieceB, j);
        bool claimed = false;
        for (const auto& window : windows)
        {
            if (std::fabs(window.first - centreA) <= stepA && std::fabs(window.second - centreB) <= stepB)
            {
                claimed = true;
                break;
            }
        }
        if (claimed)
        {
            continue;
        }
        windows.emplace_back(centreA, centreB);
        ++numWindows;

        const double loA = std::max(pieceA.first, centreA - stepA);
        const double hiA = std::min(pieceA.last, centreA + stepA);
        const double loB = std::max(pieceB.first, centreB - stepB);
        const double hiB = std::min(pieceB.last, centreB + stepB);
        if (hiA - loA <= wy3d::EPS || hiB - loB <= wy3d::EPS)
        {
            continue;
        }
        collectFromExtrema(pieceA.curve, loA, hiA, pieceB.curve, loB, hiB, tol, outPoints);
    }
}

} // namespace

Sketch3DCurveIntersectionUtil::Operand Sketch3DCurveIntersectionUtil::bounded(const SketchCurve3D* pCurve)
{
    Operand operand;
    operand.pCurve = pCurve;
    return operand;
}

Sketch3DCurveIntersectionUtil::Operand Sketch3DCurveIntersectionUtil::extended(
    const SketchCurve3D* pCurve, double reach, bool atStart, bool atEnd)
{
    Operand operand;
    operand.pCurve = pCurve;
    operand.reach = std::max(reach, 0.0);
    operand.extendStart = atStart;
    operand.extendEnd = atEnd;
    return operand;
}

Sketch3DCurveIntersectionUtil::Result Sketch3DCurveIntersectionUtil::intersect(
    const Operand& a, const Operand& b, std::vector<wy::Vector3>& outPoints, double tol)
{
    assert(tol > 0.0);

    if (!a.pCurve || !b.pCurve)
    {
        return Result::DegenerateInput;
    }
    if (a.pCurve == b.pCurve)
    {
        return Result::Coincident;
    }

    // Two straight lines are settled in closed form, which also spares the rest of the code the
    // case it handles worst.
    if (wy3d::SketchLine3D::cast(a.pCurve) && wy3d::SketchLine3D::cast(b.pCurve))
    {
        std::vector<Piece> piecesA;
        std::vector<Piece> piecesB;
        if (!buildPieces(a, piecesA) || !buildPieces(b, piecesB))
        {
            return Result::DegenerateInput;
        }
        assert(piecesA.size() == 1 && piecesB.size() == 1);

        Handle(Geom_Line) geomLineA = Handle(Geom_Line)::DownCast(piecesA.front().curve);
        Handle(Geom_Line) geomLineB = Handle(Geom_Line)::DownCast(piecesB.front().curve);
        assert(!geomLineA.IsNull() && !geomLineB.IsNull());

        gp_Pnt pnt;
        switch (intersectLineLine(geomLineA->Lin(), piecesA.front().first, piecesA.front().last,
            geomLineB->Lin(), piecesB.front().first, piecesB.front().last, tol, pnt))
        {
        case LineLineResult::Crossing:
            appendUnique(outPoints, toVector(pnt), tol);
            return Result::Ok;
        case LineLineResult::Collinear:
            return Result::Coincident;
        case LineLineResult::Apart:
        default:
            return Result::NoIntersection;
        }
    }

    if (isSameSupport(a.pCurve, b.pCurve, tol))
    {
        return Result::Coincident;
    }

    std::vector<Piece> piecesA;
    std::vector<Piece> piecesB;
    if (!buildPieces(a, piecesA) || !buildPieces(b, piecesB))
    {
        return Result::DegenerateInput;
    }

    bool isInvalid = false;
    for (const Piece& pieceA : piecesA)
    {
        for (const Piece& pieceB : piecesB)
        {
            try
            {
                collectFromExtrema(pieceA.curve, pieceA.first, pieceA.last,
                    pieceB.curve, pieceB.first, pieceB.last, tol, outPoints);
            }
            catch (const Standard_Failure&)
            {
                isInvalid = true;
            }
            catch (...)
            {
                isInvalid = true;
            }
        }
    }

    // The refinement runs even when the first pass found something: a spline crossing another
    // curve twice is exactly the case where the extrema call returns one of the two, and a
    // partial answer is not something the caller can detect on its own. Duplicates it turns up
    // are merged away, and a pair that never comes close costs one sampling sweep.
    if (!isInvalid)
    {
        for (const Piece& pieceA : piecesA)
        {
            for (const Piece& pieceB : piecesB)
            {
                try
                {
                    refineBySampling(pieceA, pieceB, tol, outPoints);
                }
                catch (const Standard_Failure&)
                {
                    isInvalid = true;
                }
                catch (...)
                {
                    isInvalid = true;
                }
            }
        }
    }

    if (!outPoints.empty())
    {
        return Result::Ok;
    }
    if (isInvalid)
    {
        return Result::InvalidGeometry;
    }
    return Result::NoIntersection;
}

NS_WY3D_END
