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

#include "headers.h"

#include <utils/wy3dSketch3DEdgeUtil.h>
#include <utils/wy3dSketch3DIntersectionUtil.h>
#include <wy3dSketchPlane.h>
#include <wy3dMath.h>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepPrimAPI_MakeTorus.hxx>
#include <BRep_Tool.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <GeomAPI_PointsToBSplineSurface.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_BSplineSurface.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <TColStd_Array1OfInteger.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TColgp_Array2OfPnt.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>

#include <cmath>
#include <iostream>
#include <random>
#include <vector>

namespace
{

using Result = wy3d::Sketch3DIntersectionUtil::Result;
using Source = wy3d::Sketch3DIntersectionUtil::Source;
using Mode = wy3d::Sketch3DIntersectionUtil::Mode;

const double kTol = 1e-6;

const double kCylinderRadius = 5.0;
const double kCylinderHeight = 10.0;
const double kCrossCylinderRadius = 4.0;

Source faceSource(const TopoDS_Shape& face)
{
    return Source{false, face, wy3d::SketchPlane()};
}

wy3d::SketchPlane makePlane(const gp_Pnt& origin, const gp_Dir& normal)
{
    // A zero xDir is fine: the SketchPlane constructor derives one from the normal.
    return wy3d::SketchPlane(wy::Vector3(origin.X(), origin.Y(), origin.Z()),
        wy::Vector3(normal.X(), normal.Y(), normal.Z()), wy::Vector3(0.0, 0.0, 0.0));
}

Source planeSource(const gp_Pnt& origin, const gp_Dir& normal)
{
    return Source{true, TopoDS_Shape(), makePlane(origin, normal)};
}

// Faces are matched by geometry rather than by index: MapShapes order is not contractual.
TopoDS_Face findPlanarFace(const TopoDS_Shape& shape, const gp_Pnt& pointOnPlane, const gp_Dir& normal)
{
    TopTools_IndexedMapOfShape faceMap;
    TopExp::MapShapes(shape, TopAbs_FACE, faceMap);
    for (Standard_Integer i = 1; i <= faceMap.Extent(); ++i)
    {
        const TopoDS_Face& face = TopoDS::Face(faceMap.FindKey(i));
        BRepAdaptor_Surface surface(face);
        if (GeomAbs_Plane != surface.GetType()) continue;

        const gp_Pln& plane = surface.Plane();
        if (std::abs(plane.Axis().Direction().Dot(normal)) < 1.0 - 1e-9) continue;
        if (plane.Distance(pointOnPlane) > kTol) continue;
        return face;
    }
    return TopoDS_Face();
}

// The lateral face of a primitive, i.e. the one that is not a cap.
TopoDS_Face findFaceOfType(const TopoDS_Shape& shape, GeomAbs_SurfaceType type)
{
    TopTools_IndexedMapOfShape faceMap;
    TopExp::MapShapes(shape, TopAbs_FACE, faceMap);
    for (Standard_Integer i = 1; i <= faceMap.Extent(); ++i)
    {
        const TopoDS_Face& face = TopoDS::Face(faceMap.FindKey(i));
        BRepAdaptor_Surface surface(face);
        if (type == surface.GetType()) return face;
    }
    return TopoDS_Face();
}

GeomAbs_CurveType curveType(const TopoDS_Edge& edge)
{
    BRepAdaptor_Curve curve(edge);
    return curve.GetType();
}

// Points taken along the edge, for checks that need positions rather than a length.
std::vector<gp_Pnt> sampleEdge(const TopoDS_Edge& edge, int count = 32)
{
    BRepAdaptor_Curve curve(edge);
    const double first = curve.FirstParameter(), last = curve.LastParameter();

    std::vector<gp_Pnt> points;
    points.reserve(static_cast<std::size_t>(count) + 1);
    for (int i = 0; i <= count; ++i)
        points.push_back(curve.Value(first + (last - first) * i / count));
    return points;
}

double edgeLength(const TopoDS_Edge& edge)
{
    BRepAdaptor_Curve curve(edge);
    return GCPnts_AbscissaPoint::Length(curve);
}

double totalLength(const std::vector<TopoDS_Edge>& edges)
{
    double total(0.0);
    for (const TopoDS_Edge& edge : edges) total += edgeLength(edge);
    return total;
}

void expectAllNonDegenerate(const std::vector<TopoDS_Edge>& edges)
{
    for (const TopoDS_Edge& edge : edges)
    {
        EXPECT_FALSE(BRep_Tool::Degenerated(edge));
        EXPECT_GT(edgeLength(edge), kTol);
    }
}

TopoDS_Shape makeCylinder()
{
    return BRepPrimAPI_MakeCylinder(kCylinderRadius, kCylinderHeight).Shape();
}

// A clamped spline of the given degree over as few poles as that degree allows, which is the
// shape both degree limits are about. Only the degree and the rational flag matter here.
TopoDS_Edge makeSplineEdge(int degree, bool rational)
{
    TColgp_Array1OfPnt poles(1, degree + 1);
    for (int i = 1; i <= degree + 1; ++i)
        poles.SetValue(i, gp_Pnt((i - 1) * 5.0, std::sin(i * 0.8) * 4.0, 0.0));

    TColStd_Array1OfReal knots(1, 2);
    knots.SetValue(1, 0.0);
    knots.SetValue(2, 1.0);
    TColStd_Array1OfInteger multiplicities(1, 2);
    multiplicities.SetValue(1, degree + 1);
    multiplicities.SetValue(2, degree + 1);

    Handle(Geom_BSplineCurve) curve;
    if (rational)
    {
        TColStd_Array1OfReal weights(1, degree + 1);
        for (int i = 1; i <= degree + 1; ++i) weights.SetValue(i, 1.0 + 0.5 * (i % 3));
        curve = new Geom_BSplineCurve(poles, weights, knots, multiplicities, degree);
    }
    else
    {
        curve = new Geom_BSplineCurve(poles, knots, multiplicities, degree);
    }

    BRepBuilderAPI_MakeEdge makeEdge(curve);
    return makeEdge.Edge();
}

TopoDS_Edge makeCircleEdge()
{
    BRepBuilderAPI_MakeEdge makeEdge(gp_Circ(gp_Ax2(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0)), 5.0));
    return makeEdge.Edge();
}

// Degree read the way the guard reads it. Zero for anything that is not a spline: asking an
// analytic curve for its degree is a different question, and the adaptor does not answer it.
int curveDegree(const TopoDS_Edge& edge)
{
    BRepAdaptor_Curve curve(edge);
    if (GeomAbs_BSplineCurve != curve.GetType()) return 0;
    return curve.Degree();
}

int countPolylines(const std::vector<TopoDS_Edge>& edges)
{
    int count(0);
    for (const TopoDS_Edge& edge : edges)
    {
        if (1 == curveDegree(edge)) ++count;
    }
    return count;
}

// Endpoints that have no partner within the tolerance, i.e. what would be a gap rather than a
// closed loop. Greedy nearest pair joining, the way the profile fuser reads the same edges.
int countFreeEndpoints(const std::vector<TopoDS_Edge>& edges, double tolerance)
{
    std::vector<gp_Pnt> ends;
    for (const TopoDS_Edge& edge : edges)
    {
        BRepAdaptor_Curve curve(edge);
        ends.push_back(curve.Value(curve.FirstParameter()));
        ends.push_back(curve.Value(curve.LastParameter()));
    }

    std::vector<bool> joined(ends.size(), false);
    int freeEnds(0);
    for (std::size_t i = 0; i < ends.size(); ++i)
    {
        if (joined[i]) continue;
        for (std::size_t j = i + 1; j < ends.size(); ++j)
        {
            if (joined[j] || ends[i].Distance(ends[j]) > tolerance) continue;
            joined[i] = true;
            joined[j] = true;
            break;
        }
        if (!joined[i]) ++freeEnds;
    }
    return freeEnds;
}

} // namespace


// The test that pins both assumptions at once: Approximation(false) has to keep the
// analytic circle, and a datum plane has to act as an unbounded plane for the circle to
// come back whole rather than clipped to some default rectangle.
TEST(Sketch3DIntersectionUtil, PlaneThroughCylinderIsExactCircle)
{
    std::vector<TopoDS_Edge> edges;
    const Source plane = planeSource(gp_Pnt(0.0, 0.0, 4.0), gp_Dir(0.0, 0.0, 1.0));

    EXPECT_EQ(Result::Ok, wy3d::Sketch3DIntersectionUtil::intersect(plane, faceSource(makeCylinder()), edges));
    ASSERT_EQ(1u, edges.size());
    EXPECT_EQ(GeomAbs_Circle, curveType(edges.front()));
    EXPECT_NEAR(2.0 * wy3d::PI * kCylinderRadius, edgeLength(edges.front()), 1e-6);

    BRepAdaptor_Curve curve(edges.front());
    const gp_Circ circle = curve.Circle();
    EXPECT_NEAR(kCylinderRadius, circle.Radius(), 1e-9);
    EXPECT_NEAR(0.0, circle.Location().X(), 1e-9);
    EXPECT_NEAR(0.0, circle.Location().Y(), 1e-9);
    EXPECT_NEAR(4.0, circle.Location().Z(), 1e-9);
    EXPECT_TRUE(curve.IsClosed());
}

TEST(Sketch3DIntersectionUtil, ObliquePlaneThroughCylinderIsEllipse)
{
    // The lateral face alone: sectioning the whole solid would also clip the caps, and the
    // plane grazes both of them at a single point.
    const TopoDS_Face lateral = findFaceOfType(makeCylinder(), GeomAbs_Cylinder);
    ASSERT_FALSE(lateral.IsNull());

    const gp_Dir normal(gp_Vec(1.0, 0.0, 1.0));
    const Source plane = planeSource(gp_Pnt(0.0, 0.0, 5.0), normal);

    std::vector<TopoDS_Edge> edges;
    EXPECT_EQ(Result::Ok, wy3d::Sketch3DIntersectionUtil::intersect(plane, faceSource(lateral), edges));
    ASSERT_FALSE(edges.empty());

    // A plane at 45 degrees to the axis cuts an ellipse whose major radius is r / cos(45).
    // The cylinder seam may split it, so check every edge rather than the count.
    for (const TopoDS_Edge& edge : edges)
    {
        EXPECT_EQ(GeomAbs_Ellipse, curveType(edge));
        BRepAdaptor_Curve curve(edge);
        EXPECT_NEAR(kCylinderRadius * std::sqrt(2.0), curve.Ellipse().MajorRadius(), 1e-6);
        EXPECT_NEAR(kCylinderRadius, curve.Ellipse().MinorRadius(), 1e-6);
    }
}

TEST(Sketch3DIntersectionUtil, PlaneThroughBoxIsFourLines)
{
    const TopoDS_Shape box = BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape();
    // The plane x + z = 20 is parallel to Y, so it leaves the box as a parallelogram: two
    // edges of length 20 along Y, and two of length sqrt(200) across the XZ section.
    const Source plane = planeSource(gp_Pnt(5.0, 10.0, 15.0), gp_Dir(gp_Vec(1.0, 0.0, 1.0)));

    std::vector<TopoDS_Edge> edges;
    EXPECT_EQ(Result::Ok, wy3d::Sketch3DIntersectionUtil::intersect(plane, faceSource(box), edges));
    ASSERT_EQ(4u, edges.size());
    for (const TopoDS_Edge& edge : edges) EXPECT_EQ(GeomAbs_Line, curveType(edge));
    EXPECT_NEAR(2.0 * (20.0 + std::sqrt(200.0)), totalLength(edges), 1e-6);
}

TEST(Sketch3DIntersectionUtil, PlaneThroughSphereIsCircle)
{
    const TopoDS_Shape sphere = BRepPrimAPI_MakeSphere(5.0).Shape();
    const Source plane = planeSource(gp_Pnt(0.0, 0.0, 2.0), gp_Dir(0.0, 0.0, 1.0));

    std::vector<TopoDS_Edge> edges;
    EXPECT_EQ(Result::Ok, wy3d::Sketch3DIntersectionUtil::intersect(plane, faceSource(sphere), edges));
    ASSERT_FALSE(edges.empty());
    for (const TopoDS_Edge& edge : edges) EXPECT_EQ(GeomAbs_Circle, curveType(edge));
    // The sphere seam may split the circle, so assert the total rather than the edge count.
    EXPECT_NEAR(2.0 * wy3d::PI * std::sqrt(21.0), totalLength(edges), 1e-6);
}

TEST(Sketch3DIntersectionUtil, ObliquePlaneThroughConeIsEllipse)
{
    // Again the lateral face only: the plane also crosses the base disc, and that part of
    // the section is a line.
    const TopoDS_Shape cone = BRepPrimAPI_MakeCone(5.0, 2.0, 8.0).Shape();
    const TopoDS_Face lateral = findFaceOfType(cone, GeomAbs_Cone);
    ASSERT_FALSE(lateral.IsNull());

    const Source plane = planeSource(gp_Pnt(0.0, 0.0, 4.0), gp_Dir(gp_Vec(1.0, 0.0, 1.0)));
    std::vector<TopoDS_Edge> edges;
    EXPECT_EQ(Result::Ok, wy3d::Sketch3DIntersectionUtil::intersect(plane, faceSource(lateral), edges));
    ASSERT_FALSE(edges.empty());
    for (const TopoDS_Edge& edge : edges) EXPECT_EQ(GeomAbs_Ellipse, curveType(edge));
    EXPECT_GT(totalLength(edges), kTol);
}

// Adjacent faces of one body legitimately meet in their shared edge, and that is a section
// like any other.
TEST(Sketch3DIntersectionUtil, TwoAdjacentBoxFacesMeetInSharedEdge)
{
    const TopoDS_Shape box = BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape();
    const TopoDS_Face top = findPlanarFace(box, gp_Pnt(5.0, 10.0, 30.0), gp_Dir(0.0, 0.0, 1.0));
    const TopoDS_Face right = findPlanarFace(box, gp_Pnt(10.0, 10.0, 15.0), gp_Dir(1.0, 0.0, 0.0));
    ASSERT_FALSE(top.IsNull());
    ASSERT_FALSE(right.IsNull());

    std::vector<TopoDS_Edge> edges;
    EXPECT_EQ(Result::Ok, wy3d::Sketch3DIntersectionUtil::intersect(faceSource(top), faceSource(right), edges));
    ASSERT_EQ(1u, edges.size());
    EXPECT_EQ(GeomAbs_Line, curveType(edges.front()));
    EXPECT_NEAR(20.0, edgeLength(edges.front()), 1e-9);
}

// A face stays bounded: a curve that fits inside it comes back whole.
TEST(Sketch3DIntersectionUtil, FaceAndCurvedFaceMeetInACircle)
{
    const TopoDS_Shape box = BRepPrimAPI_MakeBox(20.0, 20.0, 20.0).Shape();
    const TopoDS_Face top = findPlanarFace(box, gp_Pnt(10.0, 10.0, 20.0), gp_Dir(0.0, 0.0, 1.0));
    ASSERT_FALSE(top.IsNull());

    const gp_Ax2 axis(gp_Pnt(10.0, 10.0, 0.0), gp_Dir(0.0, 0.0, 1.0));
    const TopoDS_Shape cylinder = BRepPrimAPI_MakeCylinder(axis, 3.0, 30.0).Shape();

    std::vector<TopoDS_Edge> edges;
    EXPECT_EQ(Result::Ok, wy3d::Sketch3DIntersectionUtil::intersect(faceSource(top), faceSource(cylinder), edges));
    ASSERT_EQ(1u, edges.size());
    EXPECT_EQ(GeomAbs_Circle, curveType(edges.front()));
    EXPECT_NEAR(2.0 * wy3d::PI * 3.0, edgeLength(edges.front()), 1e-6);
}

TEST(Sketch3DIntersectionUtil, ArgumentOrderDoesNotMatter)
{
    const TopoDS_Shape box = BRepPrimAPI_MakeBox(20.0, 20.0, 20.0).Shape();
    const TopoDS_Face top = findPlanarFace(box, gp_Pnt(10.0, 10.0, 20.0), gp_Dir(0.0, 0.0, 1.0));
    const gp_Ax2 axis(gp_Pnt(10.0, 10.0, 0.0), gp_Dir(0.0, 0.0, 1.0));
    const TopoDS_Shape cylinder = BRepPrimAPI_MakeCylinder(axis, 3.0, 30.0).Shape();

    std::vector<TopoDS_Edge> forward;
    std::vector<TopoDS_Edge> backward;
    ASSERT_EQ(Result::Ok,
        wy3d::Sketch3DIntersectionUtil::intersect(faceSource(top), faceSource(cylinder), forward));
    ASSERT_EQ(Result::Ok,
        wy3d::Sketch3DIntersectionUtil::intersect(faceSource(cylinder), faceSource(top), backward));

    ASSERT_EQ(forward.size(), backward.size());
    EXPECT_NEAR(totalLength(forward), totalLength(backward), 1e-9);
}

TEST(Sketch3DIntersectionUtil, FreeFormSurfaceSectionIsSpline)
{
    TColgp_Array2OfPnt grid(1, 5, 1, 5);
    for (int i = 1; i <= 5; ++i)
    {
        for (int j = 1; j <= 5; ++j)
        {
            const double z = 3.0 * std::sin(i * 0.9) * std::cos(j * 0.7);
            grid.SetValue(i, j, gp_Pnt(i * 2.0, j * 2.0, z));
        }
    }
    GeomAPI_PointsToBSplineSurface fit(grid);
    const TopoDS_Face face = BRepBuilderAPI_MakeFace(fit.Surface(), 1e-6);
    ASSERT_FALSE(face.IsNull());

    const Source plane = planeSource(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0));
    std::vector<TopoDS_Edge> edges;
    EXPECT_EQ(Result::Ok, wy3d::Sketch3DIntersectionUtil::intersect(plane, faceSource(face), edges));
    ASSERT_FALSE(edges.empty());
    for (const TopoDS_Edge& edge : edges) EXPECT_EQ(GeomAbs_BSplineCurve, curveType(edge));
    EXPECT_GT(totalLength(edges), kTol);
}

// Two cylinders also meet in a curve with no closed form, so this is the case the intersector
// can only hand back as a polyline. Both cylinders are known in closed form here, which lets
// the result be checked against the surfaces themselves: every point of the curve has to lie
// on both of them, whatever curve the intersector chose to describe it with.
TEST(Sketch3DIntersectionUtil, SectionOfTwoCylindersStaysOnBothSurfaces)
{
    const TopoDS_Shape cylinder = makeCylinder();
    const TopoDS_Shape crossCylinder = BRepPrimAPI_MakeCylinder(
        gp_Ax2(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(1.0, 0.0, 0.0)), kCrossCylinderRadius, 30.0).Shape();

    const TopoDS_Face first = findFaceOfType(cylinder, GeomAbs_Cylinder);
    const TopoDS_Face second = findFaceOfType(crossCylinder, GeomAbs_Cylinder);
    ASSERT_FALSE(first.IsNull());
    ASSERT_FALSE(second.IsNull());

    std::vector<TopoDS_Edge> edges;
    ASSERT_EQ(Result::Ok,
        wy3d::Sketch3DIntersectionUtil::intersect(faceSource(first), faceSource(second), edges));
    ASSERT_FALSE(edges.empty());

    for (const TopoDS_Edge& edge : edges)
    {
        ASSERT_EQ(GeomAbs_BSplineCurve, curveType(edge));

        for (const gp_Pnt& point : sampleEdge(edge))
        {
            EXPECT_NEAR(kCylinderRadius, std::sqrt(point.X() * point.X() + point.Y() * point.Y()), 2e-3);
            EXPECT_NEAR(kCrossCylinderRadius, std::sqrt(point.Y() * point.Y() + point.Z() * point.Z()), 2e-3);
        }
    }
}

TEST(Sketch3DIntersectionUtil, PlaneOutsideCylinderIsEmpty)
{
    const Source plane = planeSource(gp_Pnt(0.0, 0.0, 20.0), gp_Dir(0.0, 0.0, 1.0));
    std::vector<TopoDS_Edge> edges;
    EXPECT_EQ(Result::NoIntersection,
        wy3d::Sketch3DIntersectionUtil::intersect(plane, faceSource(makeCylinder()), edges));
    EXPECT_TRUE(edges.empty());
}

// Tangent is numerically delicate, so the contract is robustness: either refuse, or hand
// back edges that are actually usable.
TEST(Sketch3DIntersectionUtil, TangentPlaneIsEitherEmptyOrUsable)
{
    const Source plane = planeSource(gp_Pnt(kCylinderRadius, 0.0, 5.0), gp_Dir(1.0, 0.0, 0.0));
    std::vector<TopoDS_Edge> edges;
    const Result result = wy3d::Sketch3DIntersectionUtil::intersect(plane, faceSource(makeCylinder()), edges);
    EXPECT_TRUE(Result::Ok == result || Result::NoIntersection == result);
    expectAllNonDegenerate(edges);
}

TEST(Sketch3DIntersectionUtil, CoplanarPlanesAreRejected)
{
    const gp_Pln plane(gp_Pnt(0.0, 0.0, 5.0), gp_Dir(0.0, 0.0, 1.0));
    const TopoDS_Face first = BRepBuilderAPI_MakeFace(plane, 0.0, 10.0, 0.0, 10.0);
    const TopoDS_Face second = BRepBuilderAPI_MakeFace(plane, 20.0, 30.0, 0.0, 10.0);

    std::vector<TopoDS_Edge> edges;
    EXPECT_EQ(Result::ParallelSources,
        wy3d::Sketch3DIntersectionUtil::intersect(faceSource(first), faceSource(second), edges));
    EXPECT_TRUE(edges.empty());
}

TEST(Sketch3DIntersectionUtil, ParallelDatumPlanesDoNotMeet)
{
    const Source a = planeSource(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0));
    const Source b = planeSource(gp_Pnt(0.0, 0.0, 7.0), gp_Dir(0.0, 0.0, 1.0));

    std::vector<TopoDS_Edge> edges;
    EXPECT_EQ(Result::NoIntersection, wy3d::Sketch3DIntersectionUtil::intersect(a, b, edges));
    EXPECT_TRUE(edges.empty());
}

TEST(Sketch3DIntersectionUtil, PerpendicularDatumPlanesAreInfinite)
{
    const Source a = planeSource(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0));
    const Source b = planeSource(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(1.0, 0.0, 0.0));

    std::vector<TopoDS_Edge> edges;
    EXPECT_EQ(Result::InfiniteSection, wy3d::Sketch3DIntersectionUtil::intersect(a, b, edges));
    EXPECT_TRUE(edges.empty());
}

TEST(Sketch3DIntersectionUtil, SameSourceTwiceIsRejected)
{
    const TopoDS_Shape box = BRepPrimAPI_MakeBox(10.0, 10.0, 10.0).Shape();
    const TopoDS_Face face = findPlanarFace(box, gp_Pnt(5.0, 5.0, 10.0), gp_Dir(0.0, 0.0, 1.0));
    ASSERT_FALSE(face.IsNull());

    std::vector<TopoDS_Edge> edges;
    EXPECT_EQ(Result::InvalidInput,
        wy3d::Sketch3DIntersectionUtil::intersect(faceSource(face), faceSource(face), edges));
    EXPECT_TRUE(edges.empty());

    const gp_Pnt origin(1.0, 2.0, 3.0);
    const gp_Dir normal(0.0, 0.0, 1.0);
    EXPECT_EQ(Result::InvalidInput,
        wy3d::Sketch3DIntersectionUtil::intersect(planeSource(origin, normal), planeSource(origin, normal), edges));
    EXPECT_TRUE(edges.empty());
}

TEST(Sketch3DIntersectionUtil, InvalidInputIsRejected)
{
    std::vector<TopoDS_Edge> edges;
    const Source plane = planeSource(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0));

    EXPECT_EQ(Result::InvalidInput,
        wy3d::Sketch3DIntersectionUtil::intersect(faceSource(TopoDS_Shape()), plane, edges));

    // A default constructed SketchPlane is the XY plane and perfectly valid; a zero normal
    // is what makes one unusable.
    EXPECT_TRUE(wy3d::SketchPlane().isValid());
    const Source invalidPlane{true, TopoDS_Shape(),
        wy3d::SketchPlane(wy::Vector3(0.0, 0.0, 0.0), wy::Vector3(0.0, 0.0, 0.0), wy::Vector3(0.0, 0.0, 0.0))};
    EXPECT_FALSE(invalidPlane.plane.isValid());
    EXPECT_EQ(Result::InvalidInput,
        wy3d::Sketch3DIntersectionUtil::intersect(invalidPlane, faceSource(makeCylinder()), edges));
}

namespace
{

// A free form face with no analytic section, from a random point grid. Nearly flat grids are
// in range on purpose: they exercise the planar degeneracy checks on the way through.
TopoDS_Shape makeRandomBSplineFace(std::mt19937& rng)
{
    std::uniform_int_distribution<int> countDist(4, 6);
    std::uniform_real_distribution<double> spacingDist(1.0, 6.0);
    std::uniform_real_distribution<double> amplitudeDist(0.0, 6.0);
    std::uniform_real_distribution<double> frequencyDist(0.2, 1.4);

    const int numU = countDist(rng), numV = countDist(rng);
    const double stepU = spacingDist(rng), stepV = spacingDist(rng);
    const double amplitude = amplitudeDist(rng);
    const double frequencyU = frequencyDist(rng), frequencyV = frequencyDist(rng);

    TColgp_Array2OfPnt grid(1, numU, 1, numV);
    for (int i = 1; i <= numU; ++i)
    {
        for (int j = 1; j <= numV; ++j)
        {
            const double z = amplitude * std::sin(i * frequencyU) * std::cos(j * frequencyV);
            grid.SetValue(i, j, gp_Pnt(i * stepU, j * stepV, z));
        }
    }

    GeomAPI_PointsToBSplineSurface fit(grid);
    BRepBuilderAPI_MakeFace makeFace(fit.Surface(), 1e-6);
    if (!makeFace.IsDone() || makeFace.Face().IsNull()) return TopoDS_Shape();
    return makeFace.Face();
}

// gp_Dir rejects a null vector, so keep drawing until one is long enough to normalize.
gp_Dir randomDirection(std::mt19937& rng)
{
    std::normal_distribution<double> distribution(0.0, 1.0);
    for (int i = 0; i < 64; ++i)
    {
        const gp_Vec candidate(distribution(rng), distribution(rng), distribution(rng));
        if (candidate.Magnitude() > 0.1) return gp_Dir(candidate);
    }
    return gp_Dir(0.0, 0.0, 1.0);
}

} // namespace

// A sweep over pairs of sources no hand written case would think to try: every face of a few
// primitives, a handful of free form faces, and a spread of skewed datum planes, all crossed
// with each other. The seed is fixed, so anything found here is reproducible.
//
// What is being watched is that nothing comes back as InvalidGeometry, which is what the
// geometry layer reports when OCCT threw, and that every curve it does return is one the
// sketch can hold. Every pair is run in both approximation modes, since the user picks one.
TEST(Sketch3DIntersectionUtil, EverySourcePairIsHandled)
{
    std::mt19937 rng(20260912);

    std::vector<Source> faces;
    const TopoDS_Shape primitives[] = {
        BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape(),
        makeCylinder(),
        BRepPrimAPI_MakeSphere(5.0).Shape(),
        BRepPrimAPI_MakeCone(5.0, 0.0, 10.0).Shape(),
        BRepPrimAPI_MakeTorus(8.0, 2.0).Shape(),
    };
    for (const TopoDS_Shape& primitive : primitives)
    {
        TopTools_IndexedMapOfShape faceMap;
        TopExp::MapShapes(primitive, TopAbs_FACE, faceMap);
        for (Standard_Integer i = 1; i <= faceMap.Extent(); ++i)
            faces.push_back(faceSource(TopoDS::Face(faceMap.FindKey(i))));
    }
    for (int i = 0; i < 4; ++i)
    {
        const TopoDS_Shape face = makeRandomBSplineFace(rng);
        if (!face.IsNull()) faces.push_back(faceSource(face));
    }

    std::vector<Source> planes;
    std::uniform_real_distribution<double> originDist(-12.0, 12.0);
    for (int i = 0; i < 6; ++i)
    {
        planes.push_back(planeSource(gp_Pnt(originDist(rng), originDist(rng), originDist(rng)),
            randomDirection(rng)));
    }
    // Straight through the box, so the coincident and parallel cases are in the sweep too.
    planes.push_back(planeSource(gp_Pnt(5.0, 10.0, 15.0), gp_Dir(0.0, 0.0, 1.0)));

    int sections(0), refusals(0), refusalsByGeometry(0);
    int curves(0), unconvertible(0), degenerateEdges(0);
    int polylines[2] = { 0, 0 };
    int unsupported[2] = { 0, 0 };

    auto check = [&](const Source& a, const Source& b, Mode mode)
    {
        std::vector<TopoDS_Edge> edges;
        const Result result = wy3d::Sketch3DIntersectionUtil::intersect(a, b, edges, mode);
        if (Result::Ok != result)
        {
            ++refusals;
            if (Result::InvalidGeometry == result) ++refusalsByGeometry;
            if (Result::UnsupportedCurve == result) ++unsupported[static_cast<int>(mode)];
            EXPECT_TRUE(edges.empty());
            return;
        }

        ++sections;
        EXPECT_FALSE(edges.empty());
        for (const TopoDS_Edge& edge : edges)
        {
            ++curves;
            EXPECT_FALSE(edge.IsNull());
            if (1 == curveDegree(edge)) ++polylines[static_cast<int>(mode)];

            // Whatever the geometry layer hands over has to be something the sketch can hold.
            wy3d::Sketch3DEdgeUtil::CurveSpec spec;
            const wy3d::Sketch3DEdgeUtil::Result converted =
                wy3d::Sketch3DEdgeUtil::makeCurveSpec(edge, spec);
            switch (converted)
            {
            case wy3d::Sketch3DEdgeUtil::Result::Ok:
                break;
            case wy3d::Sketch3DEdgeUtil::Result::Degenerate:
                // A sliver that collapsed to a point. The command refuses the whole pair, so
                // this is a legitimate outcome rather than a broken curve.
                ++degenerateEdges;
                break;
            default:
                ++unconvertible;
                break;
            }
        }
    };

    for (std::size_t i = 0; i < faces.size(); ++i)
    {
        for (std::size_t j = i + 1; j < faces.size(); ++j)
        {
            check(faces[i], faces[j], Mode::Interpolate);
            check(faces[i], faces[j], Mode::Fit);
        }
        for (const Source& plane : planes)
        {
            check(faces[i], plane, Mode::Interpolate);
            check(faces[i], plane, Mode::Fit);
        }
    }

    std::cout << "[ stress ] faces " << faces.size() << " planes " << planes.size()
              << " | sections " << sections << " refused " << refusals
              << " (by geometry " << refusalsByGeometry << ")"
              << " | curves " << curves << " | degenerate " << degenerateEdges
              << " unconvertible " << unconvertible
              << " | polylines interpolate " << polylines[0] << " fit " << polylines[1]
              << " | unsupported interpolate " << unsupported[0] << " fit " << unsupported[1]
              << std::endl;

    EXPECT_EQ(0, refusalsByGeometry);
    EXPECT_EQ(0, unconvertible);
    // The mode is every bit as decisive across the sweep as it is on the hand written cases: on
    // this seed fit leaves no polyline behind, where interpolation is full of them, and the
    // guard fires on neither path.
    EXPECT_GT(polylines[0], 0);
    EXPECT_EQ(0, polylines[1]);
    EXPECT_EQ(0, unsupported[0]);
    EXPECT_EQ(0, unsupported[1]);
}

// The guard is for the fit path, where OCCT could in principle hand back a curve the sketch has
// no room for. Its contract is read straight off the edge, so it is pinned with curves built by
// hand rather than hunted for in the wild - the sweep above never reaches it.
TEST(Sketch3DIntersectionUtil, SupportedCurvesArePinned)
{
    EXPECT_TRUE(wy3d::Sketch3DIntersectionUtil::isSupported(makeCircleEdge()));
    EXPECT_TRUE(wy3d::Sketch3DIntersectionUtil::isSupported(makeSplineEdge(8, false)));
    EXPECT_FALSE(wy3d::Sketch3DIntersectionUtil::isSupported(makeSplineEdge(9, false)));
    EXPECT_FALSE(wy3d::Sketch3DIntersectionUtil::isSupported(makeSplineEdge(3, true)));
}

// Two cylinders meet in a curve with no closed form, so the mode is the whole difference: the
// interpolated result can only be the polyline through the computed points, the fitted one is
// free to leave them. They describe the same curve, which is what the lengths say.
TEST(Sketch3DIntersectionUtil, TwoCylindersInterpolateIsPolylineFitIsSpline)
{
    const TopoDS_Shape cylinder = makeCylinder();
    const TopoDS_Shape crossCylinder = BRepPrimAPI_MakeCylinder(
        gp_Ax2(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(1.0, 0.0, 0.0)), kCrossCylinderRadius, 30.0).Shape();
    const TopoDS_Face first = findFaceOfType(cylinder, GeomAbs_Cylinder);
    const TopoDS_Face second = findFaceOfType(crossCylinder, GeomAbs_Cylinder);
    ASSERT_FALSE(first.IsNull());
    ASSERT_FALSE(second.IsNull());

    std::vector<TopoDS_Edge> interpolated;
    std::vector<TopoDS_Edge> fitted;
    ASSERT_EQ(Result::Ok,
        wy3d::Sketch3DIntersectionUtil::intersect(faceSource(first), faceSource(second), interpolated));
    ASSERT_EQ(Result::Ok, wy3d::Sketch3DIntersectionUtil::intersect(
        faceSource(first), faceSource(second), fitted, Mode::Fit));
    ASSERT_FALSE(interpolated.empty());
    ASSERT_FALSE(fitted.empty());

    for (const TopoDS_Edge& edge : interpolated) EXPECT_EQ(1, curveDegree(edge));
    for (const TopoDS_Edge& edge : fitted) EXPECT_GE(curveDegree(edge), 2);
    EXPECT_NEAR(totalLength(interpolated), totalLength(fitted), 1e-3);
}

// A free form surface is the same story, with the two representations equally faithful: the
// measured deviations are 4.3e-09 for the polyline and 4.4e-09 for the fit.
TEST(Sketch3DIntersectionUtil, FreeFormSurfaceSectionUnderBothModes)
{
    TColgp_Array2OfPnt grid(1, 5, 1, 5);
    for (int i = 1; i <= 5; ++i)
    {
        for (int j = 1; j <= 5; ++j)
        {
            const double z = 3.0 * std::sin(i * 0.9) * std::cos(j * 0.7);
            grid.SetValue(i, j, gp_Pnt(i * 2.0, j * 2.0, z));
        }
    }
    GeomAPI_PointsToBSplineSurface fit(grid);
    const TopoDS_Face face = BRepBuilderAPI_MakeFace(fit.Surface(), 1e-6);
    ASSERT_FALSE(face.IsNull());

    const Source plane = planeSource(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0));
    std::vector<TopoDS_Edge> interpolated;
    std::vector<TopoDS_Edge> fitted;
    ASSERT_EQ(Result::Ok, wy3d::Sketch3DIntersectionUtil::intersect(plane, faceSource(face), interpolated));
    ASSERT_EQ(Result::Ok,
        wy3d::Sketch3DIntersectionUtil::intersect(plane, faceSource(face), fitted, Mode::Fit));
    ASSERT_FALSE(interpolated.empty());
    ASSERT_FALSE(fitted.empty());

    for (const TopoDS_Edge& edge : interpolated)
    {
        EXPECT_EQ(GeomAbs_BSplineCurve, curveType(edge));
        EXPECT_EQ(1, curveDegree(edge));
    }
    for (const TopoDS_Edge& edge : fitted)
    {
        EXPECT_EQ(GeomAbs_BSplineCurve, curveType(edge));
        EXPECT_GE(curveDegree(edge), 2);
    }
    EXPECT_NEAR(totalLength(interpolated), totalLength(fitted), 1e-3);
}

// Where the section has a closed form the mode is out of the picture: the intersector keeps the
// analytic curve and both paths hand back the same thing, which is the premise the whole choice
// rests on. Measured identical on every analytic case, not just these two.
TEST(Sketch3DIntersectionUtil, AnalyticSectionsIgnoreTheMode)
{
    const Source plane = planeSource(gp_Pnt(0.0, 0.0, 4.0), gp_Dir(0.0, 0.0, 1.0));
    const Source oblique = planeSource(gp_Pnt(0.0, 0.0, 5.0), gp_Dir(gp_Vec(1.0, 0.0, 1.0)));
    const TopoDS_Face lateral = findFaceOfType(makeCylinder(), GeomAbs_Cylinder);
    ASSERT_FALSE(lateral.IsNull());

    std::vector<TopoDS_Edge> circleA, circleB, ellipseA, ellipseB;
    ASSERT_EQ(Result::Ok,
        wy3d::Sketch3DIntersectionUtil::intersect(plane, faceSource(makeCylinder()), circleA));
    ASSERT_EQ(Result::Ok, wy3d::Sketch3DIntersectionUtil::intersect(
        plane, faceSource(makeCylinder()), circleB, Mode::Fit));
    ASSERT_EQ(Result::Ok,
        wy3d::Sketch3DIntersectionUtil::intersect(oblique, faceSource(lateral), ellipseA));
    ASSERT_EQ(Result::Ok,
        wy3d::Sketch3DIntersectionUtil::intersect(oblique, faceSource(lateral), ellipseB, Mode::Fit));

    ASSERT_EQ(1u, circleA.size());
    ASSERT_EQ(1u, circleB.size());
    ASSERT_FALSE(ellipseA.empty());
    ASSERT_FALSE(ellipseB.empty());

    EXPECT_EQ(GeomAbs_Circle, curveType(circleA.front()));
    EXPECT_EQ(GeomAbs_Circle, curveType(circleB.front()));
    EXPECT_NEAR(edgeLength(circleA.front()), edgeLength(circleB.front()), 1e-9);

    ASSERT_EQ(ellipseA.size(), ellipseB.size());
    for (std::size_t i = 0; i < ellipseA.size(); ++i)
    {
        EXPECT_EQ(GeomAbs_Ellipse, curveType(ellipseA[i]));
        EXPECT_EQ(GeomAbs_Ellipse, curveType(ellipseB[i]));
        EXPECT_NEAR(edgeLength(ellipseA[i]), edgeLength(ellipseB[i]), 1e-9);
    }
}

// The plane has to stay off center: through the middle of the torus it would cut two circles,
// which is the analytic case again. Offset, the two loops have no closed form at all, and the
// fit is at its weakest here - a hook of about 8e-3 where an edge meets its own seam, which is
// why the user gets to choose. What must not change is the closure: the loops still meet
// exactly, which is what the filled sheet needs.
TEST(Sketch3DIntersectionUtil, TorusLoopsStayClosedUnderFit)
{
    const TopoDS_Shape torus = BRepPrimAPI_MakeTorus(8.0, 2.0).Shape();
    const Source plane = planeSource(gp_Pnt(0.0, 0.0, 1.0), gp_Dir(0.0, 0.0, 1.0));

    std::vector<TopoDS_Edge> interpolated;
    std::vector<TopoDS_Edge> fitted;
    ASSERT_EQ(Result::Ok,
        wy3d::Sketch3DIntersectionUtil::intersect(plane, faceSource(torus), interpolated));
    ASSERT_EQ(Result::Ok,
        wy3d::Sketch3DIntersectionUtil::intersect(plane, faceSource(torus), fitted, Mode::Fit));
    ASSERT_FALSE(interpolated.empty());
    ASSERT_FALSE(fitted.empty());

    EXPECT_EQ(0, countFreeEndpoints(interpolated, 1e-5));
    EXPECT_EQ(0, countFreeEndpoints(fitted, 1e-5));
}
