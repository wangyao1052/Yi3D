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
#include <vector>
#include <BRepAdaptor_Curve.hxx>
#include <BRepAlgoAPI_Section.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepSweep_Prism.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <Precision.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include "topo/SketchTopoBuilder.h"

#include <utils/wy3dSketch3DIntersectionUtil.h>
#include <utils/wy3dSketch3DProjectionUtil.h>
#include <wydbDatabase.h>
#include <wy3dImpl.h>
#include <wy3dSketch.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dSketchCurve.h>

NS_WY3D_BEG

namespace
{

using Result = Sketch3DProjectionUtil::Result;
using Options = Sketch3DProjectionUtil::Options;

// What the sweep is cut against. A face of a solid or a sheet is bounded, and the section is
// trimmed to it; a datum plane is handed to the intersector as the plane itself, so nothing trims
// the section and the whole projection comes back. Those two sentences are the entire difference
// between project()'s two overloads.
struct Target
{
    bool isPlane = false;
    TopoDS_Face face;
    SketchPlane plane;
};

// Two unit normals whose dot product falls below this are treated as parallel, the same value and
// the same reasoning as Sketch3DIntersectionUtil's own parallel test: it rejects anything past
// roughly 4.5e-5 rad. For a projection that means a sweep running along the target plane instead
// of into it, which never meets it however long it is.
const double kParallelDotTol = 1e-9;

gp_Pln toGpPln(const SketchPlane& plane)
{
    const wy::Vector3 origin = plane.getOrigin();
    const wy::Vector3 normal = plane.getNormal();
    return gp_Pln(gp_Pnt(origin.x(), origin.y(), origin.z()),
        gp_Dir(normal.x(), normal.y(), normal.z()));
}

// How far the sweep has to run past the target, as a relative margin. What was measured is the
// exact requirement, but an outermost point landing exactly on the prism's far end would only meet
// it in a degenerate edge, so the sweep is given a hair more room.
double sweepMargin(double span)
{
    return span * 1e-6 + Precision::Confusion();
}

// How big a span has to be before it counts as one. Both measurements below read a bounding box,
// and BRepBndLib inflates a bounding box by each shape's tolerance - plus the mesh deflection, for
// a shape that carries a mesh - so a target lying exactly in the sketch plane measures a hair of a
// span rather than none at all. Sweeping that hair sections into the source curve itself, a hair
// from where it started: the user sees a projection that did nothing, and the sketch gets a
// duplicate of itself. Anything the gap alone can account for is that hair and not a span.
//
// The corners are pushed out by the gap along every axis, so a span measured between two of them
// can be twice the gap out in each of the three directions; the factor covers the three, the
// softening term is ordinary arithmetic noise.
double noiseFloor(double gap, double scale)
{
    return 4.0 * gap / scale + Precision::Confusion();
}

// How far the face extends beyond the sketch plane along the direction, each way. A projected point
// q is on the swept surface exactly when q = p + t*dir for a source point p, and every source point
// lies in the sketch plane, so t is simply (q - origin).dir - the source curve's own size never
// enters it. Measuring the bounding box's corners is therefore both exact and enough.
//
// BRepProj_Projection asks the same question with DistanceIn(), which sums the two bounding box
// diagonals on top of the gap between them. That is a valid upper bound, but on an elongated model
// it makes the swept surface an order of magnitude bigger than the face it has to reach, and the
// user pays for it on every recompute - the panel recomputes as soon as a flag is toggled.
Result measureFaceExtent(const TopoDS_Face& targetFace, const gp_Dir& dir, const gp_Pnt& planePnt,
    double& forward, double& backward, double& noise)
{
    forward = 0.0;
    backward = 0.0;
    noise = 0.0;

    Bnd_Box box;
    BRepBndLib::Add(targetFace, box);
    if (box.IsVoid()) return Result::InvalidGeometry;
    noise = noiseFloor(box.GetGap(), 1.0);

    double x0(0.0), y0(0.0), z0(0.0), x1(0.0), y1(0.0), z1(0.0);
    box.Get(x0, y0, z0, x1, y1, z1);

    const gp_Vec v(dir);
    for (int i = 0; i < 8; ++i)
    {
        const gp_Pnt corner((i & 1) ? x1 : x0, (i & 2) ? y1 : y0, (i & 4) ? z1 : z0);
        const double d = gp_Vec(planePnt, corner).Dot(v);
        forward = std::max(forward, d);
        backward = std::max(backward, -d);
    }
    return Result::Ok;
}

// How far the sweep has to run to meet a datum plane, each way. There is no bounding box to measure
// this time - the target is the whole plane - so the question is turned around: a source point p
// meets the plane at t = (planeOrigin - p).n / (dir.n). That t does depend on p, since a source
// point lying in the sketch plane says nothing about where it lies along the target's normal, but t
// is affine in p, so the eight corners of the source's bounding box carry its extremes - the same
// corners, standing in for the face's own in the other case.
//
// Planes that coincide come out as a span no bigger than the corners' own slop, which the caller
// reads as NoProjection through the noise floor, so they need nothing of their own here.
Result measurePlaneExtent(const SketchPlane& targetPlane,
    const std::vector<TopoDS_Edge>& sourceEdges, const gp_Dir& dir, double& forward, double& backward,
    double& noise)
{
    forward = 0.0;
    backward = 0.0;
    noise = 0.0;

    const wy::Vector3 normal = targetPlane.getNormal();
    const gp_Dir planeNormal(normal.x(), normal.y(), normal.z());
    const double denom = dir.Dot(planeNormal);
    if (std::abs(denom) < kParallelDotTol) return Result::NoProjection;

    Bnd_Box box;
    for (const TopoDS_Edge& edge : sourceEdges)
    {
        if (!edge.IsNull()) BRepBndLib::Add(edge, box);
    }
    if (box.IsVoid()) return Result::InvalidGeometry;

    // A t comes from this box's corners through a division by denom, so the gap travels the same
    // way a t does.
    noise = noiseFloor(box.GetGap(), std::abs(denom));

    double x0(0.0), y0(0.0), z0(0.0), x1(0.0), y1(0.0), z1(0.0);
    box.Get(x0, y0, z0, x1, y1, z1);

    const wy::Vector3 o = targetPlane.getOrigin();
    const gp_Pnt planeOrigin(o.x(), o.y(), o.z());
    const gp_Vec normalVec(planeNormal);

    for (int i = 0; i < 8; ++i)
    {
        const gp_Pnt corner((i & 1) ? x1 : x0, (i & 2) ? y1 : y0, (i & 4) ? z1 : z0);
        // gp_Vec(a, b) is b - a, so this is (planeOrigin - corner).normal / dir.normal.
        const double t = gp_Vec(corner, planeOrigin).Dot(normalVec) / denom;
        forward = std::max(forward, t);
        backward = std::max(backward, -t);
    }
    return Result::Ok;
}

// Mirrors Sketch3DIntersectionUtil's own collector: the same edges are unusable for the same
// reasons, and a refusal has to drop what was collected so far with it.
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
        if (Precision::IsInfinite(first) || Precision::IsInfinite(last)) continue;
        if (last - first <= Precision::PConfusion()) continue;

        // The same floor makeCurveSpec refuses at. A section grazing a face tangentially leaves
        // slivers that are real curves but far too short to be anything, and handing one over
        // would make the whole projection fail on geometry the user cannot even see.
        BRepAdaptor_Curve adaptor(edge);
        if (GCPnts_AbscissaPoint::Length(adaptor, first, last) < wy3d::TOL) continue;

        if (!Sketch3DIntersectionUtil::isSupported(edge))
        {
            outEdges.clear();
            return Result::UnsupportedCurve;
        }

        outEdges.push_back(edge);
    }
    return Result::Ok;
}

// The parameter set BRepProj_Projection's own BuildSection uses. Approximation(true) only reaches
// sections with no closed form: IntTools_FaceFace builds Geom_Line, Geom_Circle and Geom_Ellipse
// directly for the analytic patches and consults its approximation flag only in the Walking case,
// so a planar or cylindrical target still arrives as an exact curve.
//
// The p-curve and OBB settings are for a section between two shapes of the model, so a plane target
// leaves them off, the way Sketch3DIntersectionUtil's own datum plane path - the one place a plane
// gets sectioned today - does without them.
Result runSection(BRepAlgoAPI_Section& section, bool bothShapes, std::vector<TopoDS_Edge>& outEdges)
{
    section.Approximation(Standard_True);
    if (bothShapes)
    {
        section.ComputePCurveOn1(Standard_True);
        section.ComputePCurveOn2(Standard_True);
        section.SetUseOBB(Standard_True);
    }
    section.Build();

    // A failed builder and a section with no edges are both !IsDone, and they are different news
    // for the user: HasErrors is what tells them apart.
    if (!section.IsDone())
    {
        return section.HasErrors() ? Result::AlgorithmFailed : Result::Ok;
    }

    return collectEdges(section.Shape(), outEdges);
}

// One source edge, swept by "length" along "dir" starting "offset" away from its own position, cut
// against the target. The prism carries no caps - the generator is an edge, not a face - so the
// only section there is is the projection itself.
Result sweepAndSection(const TopoDS_Edge& source, const gp_Dir& dir, double offset, double length,
    const Target& target, std::vector<TopoDS_Edge>& outEdges)
{
    TopoDS_Shape base = source;
    if (std::abs(offset) > Precision::Confusion())
    {
        gp_Trsf trsf;
        trsf.SetTranslation(gp_Vec(dir.XYZ().Multiplied(offset)));
        BRepBuilderAPI_Transform transform(source, trsf, Standard_True);
        base = transform.Shape();
        if (base.IsNull()) return Result::Ok;
    }

    // Copy is true because the sweep copies the generating curve before it puts p-curves on it.
    // The false path assumes it may modify the shape handed to it, and these edges outlive the
    // call: the command builds them once and sweeps them again on every recompute.
    BRepSweep_Prism prism(base, gp_Vec(dir.XYZ().Multiplied(length)), Standard_True);
    const TopoDS_Shape swept = prism.Shape();
    if (swept.IsNull()) return Result::Ok;

    if (target.isPlane)
    {
        // Handed over as the plane itself rather than as a shape, so nothing trims the section:
        // that is the whole difference between a datum plane and a face in the same place.
        BRepAlgoAPI_Section section(swept, toGpPln(target.plane), Standard_False);
        return runSection(section, false, outEdges);
    }

    BRepAlgoAPI_Section section(target.face, swept, Standard_False);
    return runSection(section, true, outEdges);
}

// Both project() overloads land here: the target is the only thing that differs, and it differs in
// exactly two places - how far the sweep has to run, and what it is sectioned against.
Result projectImpl(const SketchPlane& sketchPlane, const std::vector<TopoDS_Edge>& sourceEdges,
    const Target& target, const Options& options, std::vector<TopoDS_Edge>& outEdges)
{
    outEdges.clear();

    if (!sketchPlane.isValid()) return Result::InvalidInput;
    if (target.isPlane ? !target.plane.isValid() : target.face.IsNull()) return Result::InvalidInput;
    if (sourceEdges.empty()) return Result::InvalidInput;

    const wy::Vector3 normal = sketchPlane.getNormal();
    const wy::Vector3 origin = sketchPlane.getOrigin();
    if (normal.length() < 0.5) return Result::InvalidInput;

    const gp_Dir dir(normal.x(), normal.y(), normal.z());
    const gp_Pnt planePnt(origin.x(), origin.y(), origin.z());

    double forward(0.0), backward(0.0), noise(0.0);
    const Result measured = target.isPlane
        ? measurePlaneExtent(target.plane, sourceEdges, dir, forward, backward, noise)
        : measureFaceExtent(target.face, dir, planePnt, forward, backward, noise);
    if (Result::Ok != measured) return measured;

    // The sweep runs one way by default, and that single sided sweep is what carries the two
    // flags: there is no hit behind the sketch plane to filter out afterwards, because there is
    // no prism there to make one. A target level with the sketch plane leaves nothing to sweep to,
    // which is NoProjection rather than an error - and a target that merely measures a hair is
    // that same target, which is what the noise floor is for.
    double offset(0.0), length(0.0);
    if (options.bidirectional)
    {
        if (forward + backward <= noise) return Result::NoProjection;
        const double backSpan = backward + sweepMargin(backward);
        const double forwardSpan = forward + sweepMargin(forward);
        offset = -backSpan;
        length = backSpan + forwardSpan;
    }
    else
    {
        const double span = options.reverse ? backward : forward;
        if (span <= noise) return Result::NoProjection;
        length = span + sweepMargin(span);
        offset = options.reverse ? -length : 0.0;
    }

    for (const TopoDS_Edge& source : sourceEdges)
    {
        if (source.IsNull()) continue;

        const Result swept = sweepAndSection(source, dir, offset, length, target, outEdges);
        if (Result::Ok != swept)
        {
            outEdges.clear();
            return swept;
        }
    }

    if (outEdges.empty()) return Result::NoProjection;
    return Result::Ok;
}

// The two overloads differ only in the target they hand over, so the exception guard that keeps a
// partial result from reaching a caller lives here, once.
Result projectTarget(const SketchPlane& sketchPlane, const std::vector<TopoDS_Edge>& sourceEdges,
    const Target& target, const Options& options, std::vector<TopoDS_Edge>& outEdges)
{
    try
    {
        return projectImpl(sketchPlane, sourceEdges, target, options, outEdges);
    }
    catch (const Standard_Failure&)
    {
        // All or nothing even when the sweep comes apart: a caller must never see a partial
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

} // namespace

Sketch3DProjectionUtil::Result Sketch3DProjectionUtil::collectSourceEdges(
    const Sketch* pSketch, std::vector<TopoDS_Edge>& outEdges)
{
    outEdges.clear();

    if (!pSketch) return Result::InvalidInput;
    if (!pSketch->getPlane().isValid()) return Result::InvalidInput;

    const wydb::Database* pDb = pSketch->getDatabase();
    if (!pDb) return Result::InvalidInput;

    try
    {
        SketchTopoBuilder builder(pSketch, false);
        for (auto iter = pSketch->createIterator(); !iter.isDone(); iter.moveNext())
        {
            const wydb::ElementId id = iter.current();
            if (id.isNull()) continue;

            const wydb::Element* pElem = pDb->getElement(id);
            const SketchCurve* pCurve = SketchCurve::cast(pElem);
            if (!pCurve) continue;
            // Construction geometry and center lines, the same two filters the model applies
            // everywhere else: they are scaffolding, not something to project.
            if (pCurve->isConstruction()) continue;
            if (pCurve->isKindOf(SketchCenterLine::classInfo())) continue;

            const TopoDS_Edge edge = builder.makeEdge(pCurve);
            if (edge.IsNull()) continue;

            outEdges.push_back(edge);
        }
    }
    catch (const Standard_Failure&)
    {
        outEdges.clear();
        return Result::InvalidGeometry;
    }
    catch (...)
    {
        outEdges.clear();
        return Result::InvalidGeometry;
    }

    return Result::Ok;
}

Sketch3DProjectionUtil::Result Sketch3DProjectionUtil::project(const SketchPlane& sketchPlane,
    const std::vector<TopoDS_Edge>& sourceEdges, const TopoDS_Face& targetFace,
    const Options& options, std::vector<TopoDS_Edge>& outEdges)
{
    Target target;
    target.face = targetFace;
    return projectTarget(sketchPlane, sourceEdges, target, options, outEdges);
}

Sketch3DProjectionUtil::Result Sketch3DProjectionUtil::project(const SketchPlane& sketchPlane,
    const std::vector<TopoDS_Edge>& sourceEdges, const SketchPlane& targetPlane,
    const Options& options, std::vector<TopoDS_Edge>& outEdges)
{
    Target target;
    target.isPlane = true;
    target.plane = targetPlane;
    return projectTarget(sketchPlane, sourceEdges, target, options, outEdges);
}

NS_WY3D_END
