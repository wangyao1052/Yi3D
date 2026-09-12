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

#include <algorithm>
#include <cmath>
#include <BRep_Tool.hxx>
#include <BRepLib.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <GeomAdaptor_Curve.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_Circle.hxx>
#include <Geom_Ellipse.hxx>
#include <Geom_Line.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <Precision.hxx>
#include <TopoDS_Edge.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include "topo/Sketch3DTopoBuilder.h"
#include "topo/SketchTopoBuilder.h"

#include <wy3dSketch3DEdgeUtil.h>
#include <wy3dImpl.h>
#include <wy3dMath.h>
#include <wy3dSketch.h>
#include <wy3dSketchCurve.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>
#include <wy3dSketchEllipse.h>
#include <wy3dSketchEllipseArc.h>
#include <wy3dSketchSpline.h>
#include <wy3dSketchEntity3D.h>
#include <wy3dSketchLine3D.h>
#include <wy3dSketchCircle3D.h>
#include <wy3dSketchArc3D.h>
#include <wy3dSketchEllipse3D.h>
#include <wy3dSketchEllipseArc3D.h>
#include <wy3dSketchSpline3D.h>

NS_WY3D_BEG

namespace
{

using Result = Sketch3DEdgeUtil::Result;
using Kind = Sketch3DEdgeUtil::Kind;
using CurveSpec = Sketch3DEdgeUtil::CurveSpec;

const int kNumSamples = 64;

wy::Vector3 toVector3(const gp_Pnt& pnt) { return wy::Vector3(pnt.X(), pnt.Y(), pnt.Z()); }
wy::Vector3 toVector3(const gp_Dir& dir) { return wy::Vector3(dir.X(), dir.Y(), dir.Z()); }
wy::Vector3 toVector3(const gp_Vec& vec) { return wy::Vector3(vec.X(), vec.Y(), vec.Z()); }

wy::Vector3 vectorBetween(const gp_Pnt& from, const gp_Pnt& to)
{
    return wy::Vector3(to.X() - from.X(), to.Y() - from.Y(), to.Z() - from.Z());
}

double computeLength(const Handle(Geom_Curve)& pCurve, double first, double last)
{
    if (pCurve.IsNull()) return 0.0;
    try
    {
        GeomAdaptor_Curve adaptor(pCurve, first, last);
        return GCPnts_AbscissaPoint::Length(adaptor, first, last);
    }
    catch (...)
    {
        return 0.0;
    }
}

// Unwraps nested trimmed curves. The ranges are intersected layer by layer: an inner
// range may be wider than the outer one, overwriting it would lose the outer trim.
Handle(Geom_Curve) unwrapTrimmed(const Handle(Geom_Curve)& pCurve, double& first, double& last)
{
    Handle(Geom_Curve) basis = pCurve;
    while (basis->IsKind(STANDARD_TYPE(Geom_TrimmedCurve)))
    {
        Handle(Geom_TrimmedCurve) pTrimmed = Handle(Geom_TrimmedCurve)::DownCast(basis);
        first = std::max(first, pTrimmed->FirstParameter());
        last = std::min(last, pTrimmed->LastParameter());
        basis = pTrimmed->BasisCurve();
    }
    return basis;
}

// A conic is a full circle/ellipse when its parameter span covers the whole period.
// An edge carries its own range on top of the base curve, so the span decides, not
// whether the curve happens to be wrapped into a trimmed curve.
bool isFullPeriod(double first, double last)
{
    return (last - first) > (wy3d::TWO_PI - wy3d::TOL);
}

Result sampleCurve(const Handle(Geom_Curve)& pCurve, double first, double last, CurveSpec& spec)
{
    std::vector<wy::Vector3> sampled;
    sampled.reserve(kNumSamples);
    const double range = last - first;
    try
    {
        for (int i = 0; i < kNumSamples; ++i)
        {
            const double param = first + range * static_cast<double>(i) / static_cast<double>(kNumSamples - 1);
            sampled.emplace_back(toVector3(pCurve->Value(param)));
        }
    }
    catch (const Standard_Failure&)
    {
        return Result::UnsupportedType;
    }

    std::vector<wy::Vector3> fitPoints;
    fitPoints.reserve(sampled.size());
    fitPoints.push_back(sampled.front());
    for (std::size_t i = 1; i < sampled.size(); ++i)
    {
        if ((sampled[i] - fitPoints.back()).length() > wy3d::TOL) fitPoints.push_back(sampled[i]);
    }
    if (fitPoints.size() < 2) return Result::Degenerate;

    spec.kind = Kind::FitPointSpline;
    spec.points = std::move(fitPoints);
    return Result::Ok;
}

Result makeLineSpec(const Handle(Geom_Curve)& pCurve, double first, double last, CurveSpec& spec)
{
    Handle(Geom_Line) pLine = Handle(Geom_Line)::DownCast(pCurve);
    const wy::Vector3 startPnt = toVector3(pLine->Value(first));
    const wy::Vector3 endPnt = toVector3(pLine->Value(last));
    if ((endPnt - startPnt).length() < wy3d::TOL) return Result::Degenerate;

    spec.kind = Kind::Line;
    spec.startPnt = startPnt;
    spec.endPnt = endPnt;
    return Result::Ok;
}

Result makeCircleSpec(const Handle(Geom_Curve)& pCurve, double first, double last, CurveSpec& spec)
{
    Handle(Geom_Circle) pCircle = Handle(Geom_Circle)::DownCast(pCurve);
    const gp_Circ& circle = pCircle->Circ();
    const double radius = circle.Radius();
    if (radius < wy3d::TOL) return Result::Degenerate;

    spec.center = toVector3(circle.Location());
    spec.normal = toVector3(circle.Position().Direction());
    spec.xDir = toVector3(circle.Position().XDirection());
    spec.radius = radius;
    if (isFullPeriod(first, last))
    {
        spec.kind = Kind::Circle;
        return Result::Ok;
    }

    spec.kind = Kind::Arc;
    spec.startAngle = wy3d::normalizeRadian(first);
    spec.endAngle = wy3d::normalizeRadian(last);
    if (wy3d::normalizeRadian(spec.endAngle - spec.startAngle) < wy3d::TOL) return Result::Degenerate;
    return Result::Ok;
}

Result makeEllipseSpec(const Handle(Geom_Curve)& pCurve, double first, double last, CurveSpec& spec)
{
    Handle(Geom_Ellipse) pEllipse = Handle(Geom_Ellipse)::DownCast(pCurve);
    const double majorRadius = pEllipse->MajorRadius();
    const double minorRadius = pEllipse->MinorRadius();
    if (minorRadius < wy3d::TOL || majorRadius < wy3d::TOL) return Result::Degenerate;

    const gp_Ax2& axis = pEllipse->Position();
    spec.center = toVector3(axis.Location());
    spec.normal = toVector3(axis.Direction());
    spec.xDir = toVector3(axis.XDirection());
    spec.majorRadius = majorRadius;
    spec.radiusRatio = minorRadius / majorRadius;
    if (isFullPeriod(first, last))
    {
        spec.kind = Kind::Ellipse;
        return Result::Ok;
    }

    // The sketch entity stores polar angles measured from the major axis, while the
    // OCCT parameter is an eccentric anomaly: derive the angles from points instead.
    const wy::Vector3 yDir = spec.normal.cross(spec.xDir);
    const gp_Pnt center = axis.Location();
    auto polarAngle = [&](double param)
    {
        const wy::Vector3 offset = vectorBetween(center, pEllipse->Value(param));
        return wy3d::normalizeRadian(std::atan2(offset.dot(yDir), offset.dot(spec.xDir)));
    };

    spec.kind = Kind::EllipseArc;
    spec.startAngle = polarAngle(first);
    spec.endAngle = polarAngle(last);
    const double midAngle = polarAngle(0.5 * (first + last));

    // An arc always sweeps counterclockwise from start to end, so the midpoint has to
    // fall inside that sweep, otherwise the two angles are swapped.
    if (wy3d::normalizeRadian(midAngle - spec.startAngle) > wy3d::normalizeRadian(spec.endAngle - spec.startAngle))
    {
        std::swap(spec.startAngle, spec.endAngle);
    }
    if (wy3d::normalizeRadian(spec.endAngle - spec.startAngle) < wy3d::TOL) return Result::Degenerate;
    return Result::Ok;
}

Result makeSplineSpec(const Handle(Geom_Curve)& pCurve, double first, double last, CurveSpec& spec)
{
    Handle(Geom_BSplineCurve) pSpline = Handle(Geom_BSplineCurve)::DownCast(pCurve);
    if (pSpline.IsNull()) return sampleCurve(pCurve, first, last, spec);

    // A rational curve needs weights and a high degree cannot be held by the sketch
    // entity, so both fall back to sampling.
    const Standard_Integer degree = pSpline->Degree();
    if (pSpline->IsRational() || degree < 1 || degree > 5) return sampleCurve(pCurve, first, last, spec);

    Handle(Geom_BSplineCurve) source = pSpline;
    if (pSpline->IsPeriodic())
    {
        source = Handle(Geom_BSplineCurve)::DownCast(pSpline->Copy());
        if (source.IsNull()) return sampleCurve(pCurve, first, last, spec);
        source->SetNotPeriodic();
        if (source->IsPeriodic()) return sampleCurve(pCurve, first, last, spec);
    }

    const bool isFullRange = std::abs(first - source->FirstParameter()) < Precision::PConfusion()
        && std::abs(last - source->LastParameter()) < Precision::PConfusion();
    if (!isFullRange)
    {
        Handle(Geom_BSplineCurve) pSegmented = Handle(Geom_BSplineCurve)::DownCast(source->Copy());
        if (pSegmented.IsNull()) return sampleCurve(pCurve, first, last, spec);
        pSegmented->Segment(first, last);
        source = pSegmented;
    }

    std::vector<wy::Vector3> poles;
    poles.reserve(static_cast<std::size_t>(source->NbPoles()));
    for (Standard_Integer i = 1; i <= source->NbPoles(); ++i)
    {
        poles.emplace_back(toVector3(source->Pole(i)));
    }

    std::vector<double> knots;
    std::vector<std::uint32_t> multiplicities;
    knots.reserve(static_cast<std::size_t>(source->NbKnots()));
    multiplicities.reserve(static_cast<std::size_t>(source->NbKnots()));
    for (Standard_Integer i = 1; i <= source->NbKnots(); ++i)
    {
        knots.emplace_back(source->Knot(i));
        multiplicities.emplace_back(static_cast<std::uint32_t>(source->Multiplicity(i)));
    }

    // Normalizing the knots to [0,1] only changes the parameterization, not the shape,
    // and keeps the vector on the same scale as the uniformly spaced one.
    const double knotFirst = knots.front();
    const double knotRange = knots.back() - knotFirst;
    if (knotRange > wy3d::TOL)
    {
        for (double& knot : knots) knot = (knot - knotFirst) / knotRange;
    }

    spec.kind = Kind::ControlPointSpline;
    spec.degree = static_cast<std::uint32_t>(degree);
    spec.points = std::move(poles);
    spec.knots = std::move(knots);
    spec.multiplicities = std::move(multiplicities);
    return Result::Ok;
}

} // namespace

Sketch3DEdgeUtil::Result Sketch3DEdgeUtil::makeCurveSpec(
    const Handle(Geom_Curve)& pCurve, double first, double last, CurveSpec& spec)
{
    spec = CurveSpec();
    if (pCurve.IsNull()) return Result::NullCurve;
    if (Precision::IsInfinite(first) || Precision::IsInfinite(last)) return Result::InfiniteCurve;
    if (last - first < Precision::PConfusion()) return Result::Degenerate;

    try
    {
        Handle(Geom_Curve) basis = unwrapTrimmed(pCurve, first, last);
        if (basis.IsNull()) return Result::NullCurve;
        if (last - first < Precision::PConfusion()) return Result::Degenerate;
        if (computeLength(basis, first, last) < wy3d::TOL) return Result::Degenerate;

        // The order matters: circles and ellipses derive from Geom_Conic, which Geom_Line
        // does not, and IsKind matches derived classes as well.
        if (basis->IsKind(STANDARD_TYPE(Geom_Line))) return makeLineSpec(basis, first, last, spec);
        if (basis->IsKind(STANDARD_TYPE(Geom_Circle))) return makeCircleSpec(basis, first, last, spec);
        if (basis->IsKind(STANDARD_TYPE(Geom_Ellipse))) return makeEllipseSpec(basis, first, last, spec);
        if (basis->IsKind(STANDARD_TYPE(Geom_BSplineCurve))) return makeSplineSpec(basis, first, last, spec);

        return sampleCurve(basis, first, last, spec);
    }
    catch (const Standard_Failure&)
    {
        return Result::InvalidGeometry;
    }
    catch (...)
    {
        return Result::InvalidGeometry;
    }
}

Sketch3DEdgeUtil::Result Sketch3DEdgeUtil::makeCurveSpec(const TopoDS_Edge& edge, CurveSpec& spec)
{
    spec = CurveSpec();
    if (edge.IsNull()) return Result::NullCurve;

    Standard_Real first(0.0), last(0.0);
    Handle(Geom_Curve) pCurve;
    try
    {
        TopoDS_Edge edgeForCurve = edge;
        BRepLib::BuildCurve3d(edgeForCurve, wy3d::TOL);
        pCurve = BRep_Tool::Curve(edgeForCurve, first, last);
    }
    catch (const Standard_Failure&)
    {
        return Result::InvalidGeometry;
    }

    if (pCurve.IsNull()) return Result::NullCurve;
    return makeCurveSpec(pCurve, first, last, spec);
}

Sketch3DEdgeUtil::Result Sketch3DEdgeUtil::createEntity(
    wydb::Transaction* pTrans, const CurveSpec& spec, SketchEntity3D*& pOutEntity)
{
    pOutEntity = nullptr;
    if (!pTrans) return Result::CreateFailed;

    wy::ErrorStatus error = wy::ErrorStatus::InvalidInput;
    switch (spec.kind)
    {
    case Kind::Line:
    {
        SketchLine3D* pLine = nullptr;
        error = SketchLine3D::create(pTrans, spec.startPnt, spec.endPnt, pLine);
        pOutEntity = pLine;
        break;
    }
    case Kind::Circle:
    {
        SketchCircle3D* pCircle = nullptr;
        error = SketchCircle3D::create(pTrans, spec.center, spec.normal, spec.xDir, spec.radius, pCircle);
        pOutEntity = pCircle;
        break;
    }
    case Kind::Arc:
    {
        SketchArc3D* pArc = nullptr;
        error = SketchArc3D::create(pTrans, spec.center, spec.normal, spec.xDir, spec.radius,
            spec.startAngle, spec.endAngle, pArc);
        pOutEntity = pArc;
        break;
    }
    case Kind::Ellipse:
    {
        SketchEllipse3D* pEllipse = nullptr;
        error = SketchEllipse3D::create(pTrans, spec.center, spec.normal, spec.xDir, spec.majorRadius,
            spec.radiusRatio, pEllipse);
        pOutEntity = pEllipse;
        break;
    }
    case Kind::EllipseArc:
    {
        SketchEllipseArc3D* pEllipseArc = nullptr;
        error = SketchEllipseArc3D::create(pTrans, spec.center, spec.normal, spec.xDir, spec.majorRadius,
            spec.radiusRatio, spec.startAngle, spec.endAngle, pEllipseArc);
        pOutEntity = pEllipseArc;
        break;
    }
    case Kind::ControlPointSpline:
    {
        SketchSpline3D* pSpline = nullptr;
        error = SketchSpline3D::create(pTrans, spec.degree, spec.points, spec.knots, spec.multiplicities, pSpline);
        pOutEntity = pSpline;
        break;
    }
    case Kind::FitPointSpline:
    {
        SketchSpline3D* pSpline = nullptr;
        error = SketchSpline3D::create(pTrans, spec.points, pSpline);
        pOutEntity = pSpline;
        break;
    }
    default:
        return Result::UnsupportedType;
    }

    return (wy::ErrorStatus::Ok == error && pOutEntity) ? Result::Ok : Result::CreateFailed;
}

Sketch3DEdgeUtil::Result Sketch3DEdgeUtil::convert(
    wydb::Transaction* pTrans, const TopoDS_Edge& edge, SketchEntity3D*& pOutEntity)
{
    pOutEntity = nullptr;

    CurveSpec spec;
    const Result result = makeCurveSpec(edge, spec);
    if (Result::Ok != result) return result;

    return createEntity(pTrans, spec, pOutEntity);
}

Sketch3DEdgeUtil::Result Sketch3DEdgeUtil::convert(
    wydb::Transaction* pTrans, const Sketch* pSketch, const SketchCurve* pCurve,
    SketchEntity3D*& pOutEntity)
{
    pOutEntity = nullptr;
    if (!pSketch || !pCurve) return Result::NullCurve;

    // Reject what the builder cannot turn into an edge before it trips its own assert
    if (!SketchLine::cast(pCurve) && !SketchCircle::cast(pCurve) && !SketchArc::cast(pCurve) &&
        !SketchEllipse::cast(pCurve) && !SketchEllipseArc::cast(pCurve) && !SketchSpline::cast(pCurve))
    {
        return Result::UnsupportedType;
    }

    // A 2D spline builds its curve cache when the transaction is committed, and the
    // builder asserts on a null cache
    if (const SketchSpline* pSplineCurve = SketchSpline::cast(pCurve))
    {
        if (pSplineCurve->getOccSpline().IsNull()) return Result::NullCurve;
    }

    CurveSpec spec;
    Result result = Result::InvalidGeometry;
    try
    {
        SketchTopoBuilder builder(pSketch);
        const TopoDS_Edge edge = builder.makeEdge(pCurve);
        result = edge.IsNull() ? Result::Degenerate : makeCurveSpec(edge, spec);
    }
    catch (const Standard_Failure&)
    {
        return Result::InvalidGeometry;
    }

    if (Result::Ok != result) return result;
    return createEntity(pTrans, spec, pOutEntity);
}

Sketch3DEdgeUtil::Result Sketch3DEdgeUtil::convert(
    wydb::Transaction* pTrans, const SketchEntity3D* pEntity, SketchEntity3D*& pOutEntity)
{
    pOutEntity = nullptr;
    if (!pEntity) return Result::NullCurve;

    if (!SketchLine3D::cast(pEntity) && !SketchCircle3D::cast(pEntity) && !SketchArc3D::cast(pEntity) &&
        !SketchEllipse3D::cast(pEntity) && !SketchEllipseArc3D::cast(pEntity) &&
        !SketchSpline3D::cast(pEntity))
    {
        return Result::UnsupportedType;
    }

    CurveSpec spec;
    Result result = Result::InvalidGeometry;
    try
    {
        Sketch3DTopoBuilder builder;
        const TopoDS_Edge edge = builder.makeEdge(pEntity);
        result = edge.IsNull() ? Result::Degenerate : makeCurveSpec(edge, spec);
    }
    catch (const Standard_Failure&)
    {
        return Result::InvalidGeometry;
    }

    if (Result::Ok != result) return result;
    return createEntity(pTrans, spec, pOutEntity);
}

NS_WY3D_END
