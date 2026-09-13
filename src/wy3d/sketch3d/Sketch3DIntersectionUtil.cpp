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

#include <cmath>
#include <vector>
#include <BRepAlgoAPI_Section.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRep_Tool.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <Precision.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Ax3.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>

#include <utils/wy3dSketch3DIntersectionUtil.h>
#include <wy3dImpl.h>

NS_WY3D_BEG

namespace
{

using Result = Sketch3DIntersectionUtil::Result;
using Source = Sketch3DIntersectionUtil::Source;

// The dot product of two unit normals is 1 - theta^2 / 2 for a small angle theta, so this
// rejects anything beyond roughly 4.5e-5 rad. Anything closer than that counts as parallel,
// and the caller gets ParallelSources rather than whatever the intersector makes of it.
const double kParallelDotTol = 1e-9;

gp_Pln toGpPln(const SketchPlane& plane)
{
    const wy::Vector3 origin = plane.getOrigin();
    const wy::Vector3 normal = plane.getNormal();
    return gp_Pln(gp_Pnt(origin.x(), origin.y(), origin.z()),
        gp_Dir(normal.x(), normal.y(), normal.z()));
}

// A source is planar when it is a datum plane, or a face lying on a plane. Faces on other
// surfaces have no plane and are never part of a degeneracy check.
bool toPlanar(const Source& source, gp_Pln& plane)
{
    if (source.isPlane)
    {
        plane = toGpPln(source.plane);
        return true;
    }

    try
    {
        BRepAdaptor_Surface surface(TopoDS::Face(source.shape));
        if (GeomAbs_Plane != surface.GetType()) return false;
        plane = surface.Plane();
        return true;
    }
    catch (const Standard_Failure&)
    {
        return false;
    }
}

bool areParallelPlanes(const gp_Pln& a, const gp_Pln& b)
{
    return std::abs(a.Axis().Direction().Dot(b.Axis().Direction())) >= 1.0 - kParallelDotTol;
}

// Coincident planes have no section at all: the intersector would either return nothing or
// hand back the overlapping boundary of the two faces.
bool areCoincidentPlanes(const gp_Pln& a, const gp_Pln& b)
{
    return areParallelPlanes(a, b) && a.Distance(b.Location()) <= wy3d::TOL;
}

Result collectEdges(const TopoDS_Shape& section, std::vector<TopoDS_Edge>& outEdges)
{
    TopTools_IndexedMapOfShape edgeMap;
    TopExp::MapShapes(section, TopAbs_EDGE, edgeMap);
    for (Standard_Integer i = 1; i <= edgeMap.Extent(); ++i)
    {
        const TopoDS_Edge& edge = TopoDS::Edge(edgeMap.FindKey(i));
        if (edge.IsNull() || BRep_Tool::Degenerated(edge)) continue;

        Standard_Real first(0.0), last(0.0);
        Handle(Geom_Curve) pCurve;
        try
        {
            pCurve = BRep_Tool::Curve(edge, first, last);
        }
        catch (const Standard_Failure&)
        {
            continue;
        }

        if (pCurve.IsNull()) continue;
        // An unbounded edge cannot become a sketch entity, so it is not a usable result.
        if (Precision::IsInfinite(first) || Precision::IsInfinite(last)) continue;
        if (last - first <= Precision::PConfusion()) continue;
        // A refusal drops the edges collected so far with it: a caller must never see a partial
        // section next to a non Ok result.
        if (!Sketch3DIntersectionUtil::isSupported(edge))
        {
            outEdges.clear();
            return Result::UnsupportedCurve;
        }

        outEdges.push_back(edge);
    }
    return Result::Ok;
}

Result runSection(BRepAlgoAPI_Section& section, std::vector<TopoDS_Edge>& outEdges,
    Sketch3DIntersectionUtil::Mode mode)
{
    // Off keeps every point the intersector computed. Where the section has no closed form that
    // leaves only the polyline through those points, which comes back as a degree 1 BSpline and
    // reaches the sketch as an order 2 spline. On lets the intersector leave them and fit a
    // smooth spline instead - of its own degree and tolerance, measured at degree 5 to 6, with
    // no knob to change either. Neither flag touches a section that does have a closed form:
    // circles, ellipses and lines come back identical in the two modes.
    section.Approximation(Sketch3DIntersectionUtil::Mode::Fit == mode ? Standard_True : Standard_False);
    section.Build();
    if (!section.IsDone()) return Result::AlgorithmFailed;

    const Result collected = collectEdges(section.Shape(), outEdges);
    if (collected != Result::Ok || !outEdges.empty()) return collected;
    return section.HasErrors() ? Result::AlgorithmFailed : Result::NoIntersection;
}

} // namespace

bool Sketch3DIntersectionUtil::isSupported(const TopoDS_Edge& edge)
{
    // The degree limit SketchSpline3D accepts. Above it the entity refuses the curve and
    // Sketch3DEdgeUtil falls back to resampling it into a 64 point cubic, which loses far more
    // than saying so does.
    const int kMaxDegree = 8;

    try
    {
        // The adaptor reads the type, degree and rational flag straight off the edge's own curve,
        // trimmed or not, without copying it as BSpline() would.
        BRepAdaptor_Curve curve(edge);
        if (GeomAbs_BSplineCurve != curve.GetType()) return true;
        return !curve.IsRational() && curve.Degree() <= kMaxDegree;
    }
    catch (const Standard_Failure&)
    {
        return false;
    }
}

Sketch3DIntersectionUtil::Result Sketch3DIntersectionUtil::intersect(
    const Source& a, const Source& b, std::vector<TopoDS_Edge>& outEdges, Mode mode)
{
    try
    {
        return Sketch3DIntersectionUtil::intersectImpl(a, b, outEdges, mode);
    }
    catch (const Standard_Failure&)
    {
        // All or nothing even when the section comes apart: a caller must never see a partial
        // result next to a non Ok code.
        outEdges.clear();
        return Result::InvalidGeometry;
    }
    catch (...)
    {
        outEdges.clear();
        return Result::InvalidGeometry;
    }
}

Sketch3DIntersectionUtil::Result Sketch3DIntersectionUtil::intersectImpl(
    const Source& a, const Source& b, std::vector<TopoDS_Edge>& outEdges, Mode mode)
{
    outEdges.clear();

    if (a.isPlane && !a.plane.isValid()) return Result::InvalidInput;
    if (b.isPlane && !b.plane.isValid()) return Result::InvalidInput;
    if (!a.isPlane && a.shape.IsNull()) return Result::InvalidInput;
    if (!b.isPlane && b.shape.IsNull()) return Result::InvalidInput;

    if (!a.isPlane && !b.isPlane && a.shape.IsSame(b.shape)) return Result::InvalidInput;
    if (a.isPlane && b.isPlane && a.plane == b.plane) return Result::InvalidInput;

    gp_Pln planeA;
    gp_Pln planeB;
    const bool aPlanar = toPlanar(a, planeA);
    const bool bPlanar = toPlanar(b, planeB);
    if (aPlanar && bPlanar && areParallelPlanes(planeA, planeB))
    {
        return areCoincidentPlanes(planeA, planeB) ? Result::ParallelSources : Result::NoIntersection;
    }

    // Two non-parallel planes meet in an unbounded line, which is no more a sketch entity
    // than a circle of infinite radius. Rejected here so the intersector never sees it.
    if (a.isPlane && b.isPlane) return Result::InfiniteSection;

    // The section builders are where OCCT is expected to throw. Catching them here says so at
    // the spot; the wrapper above is what guarantees nothing escapes the class either way.
    try
    {
        if (!a.isPlane && !b.isPlane)
        {
            BRepAlgoAPI_Section section(a.shape, b.shape, Standard_False);
            return runSection(section, outEdges, mode);
        }
        if (!a.isPlane)
        {
            BRepAlgoAPI_Section section(a.shape, planeB, Standard_False);
            return runSection(section, outEdges, mode);
        }
        BRepAlgoAPI_Section section(b.shape, planeA, Standard_False);
        return runSection(section, outEdges, mode);
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

NS_WY3D_END
