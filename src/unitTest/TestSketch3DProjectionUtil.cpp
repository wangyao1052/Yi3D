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
#include <utils/wy3dSketch3DProjectionUtil.h>
#include <wy3dMath.h>
#include <wy3dSketch.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchPlane.h>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepPrimAPI_MakeTorus.hxx>
#include <BRep_Tool.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>

#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

namespace
{

using Result = wy3d::Sketch3DProjectionUtil::Result;
using Options = wy3d::Sketch3DProjectionUtil::Options;

const double kTol = 1e-6;
const double kCylinderRadius = 5.0;

// --- fixtures ---

// Sketch-local coordinates are world coordinates: the sketch sits on the XY plane through the
// origin, so every expected value in this file can be written down literally.
wydb::ElementId createSketchOnXY(wy3d::Database* pDb,
    const std::function<void(wydb::Transaction*, wy3d::Sketch*)>& addCurves)
{
    const wydb::ElementId kNull = wydb::ElementId::kNull;
    wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
    wy3d::SketchPlane plane(wy::Vector3::kZero, wy::Vector3::kZAxis, wy::Vector3::kXAxis);
    wy3d::Sketch* pSketch(nullptr);
    EXPECT_EQ(wy3d::Sketch::create(pTrans, plane, pSketch), wy::ErrorStatus::Ok);
    if (!pSketch)
    {
        pDb->getTransactionManager()->abortTransaction();
        return kNull;
    }

    addCurves(pTrans, pSketch);

    EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
    return pSketch->getId();
}

void addLine(wydb::Transaction* pTrans, wy3d::Sketch* pSketch,
    const wy::Vector2& start, const wy::Vector2& end)
{
    wy3d::SketchLine* pLine(nullptr);
    EXPECT_EQ(wy3d::SketchLine::create(pTrans, start, end, pLine), wy::ErrorStatus::Ok);
    if (pLine) EXPECT_EQ(pSketch->addEntity(pLine), wy::ErrorStatus::Ok);
}

// --- shapes ---

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

// The top face of a 20 x 20 x 10 box: the +Z target the parallel plane cases project onto.
TopoDS_Face makeBoxTopFace()
{
    return findPlanarFace(BRepPrimAPI_MakeBox(20.0, 20.0, 10.0).Shape(),
        gp_Pnt(10.0, 10.0, 10.0), gp_Dir(0.0, 0.0, 1.0));
}

// A cylinder lying along X, so its lateral face satisfies y^2 + z^2 = r^2 and a sketch on the XY
// plane reaches it along +Z and along -Z, at two different places. It runs from x = -10 to x = +10
// so that the sources these tests use sit strictly inside it: the face is bounded, and a source
// reaching past its end would be clipped, which would silently shorten the expected lengths.
TopoDS_Face makeCrossCylinderLateralFace()
{
    const gp_Ax2 axis(gp_Pnt(-10.0, 0.0, 0.0), gp_Dir(1.0, 0.0, 0.0));
    return findFaceOfType(BRepPrimAPI_MakeCylinder(axis, kCylinderRadius, 20.0).Shape(),
        GeomAbs_Cylinder);
}

// --- measurements ---

GeomAbs_CurveType curveType(const TopoDS_Edge& edge)
{
    BRepAdaptor_Curve curve(edge);
    return curve.GetType();
}

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

// Every point of the result, in one spread: used to say "the whole projection sits at z = 5".
std::pair<double, double> zRange(const std::vector<TopoDS_Edge>& edges)
{
    double lo(0.0), hi(0.0);
    bool first = true;
    for (const TopoDS_Edge& edge : edges)
    {
        for (const gp_Pnt& pnt : sampleEdge(edge))
        {
            if (first) { lo = hi = pnt.Z(); first = false; }
            lo = std::min(lo, pnt.Z());
            hi = std::max(hi, pnt.Z());
        }
    }
    return std::make_pair(lo, hi);
}

} // namespace

// --- source collection ---

TEST(Sketch3DProjectionUtil, ConstructionGeometryIsSkipped)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    const wydb::ElementId sketchId = createSketchOnXY(pDb.get(),
        [](wydb::Transaction* pTrans, wy3d::Sketch* pSketch)
        {
            addLine(pTrans, pSketch, wy::Vector2(0.0, 5.0), wy::Vector2(10.0, 5.0));

            wy3d::SketchLine* pConstruction(nullptr);
            EXPECT_EQ(wy3d::SketchLine::create(pTrans, wy::Vector2(0.0, 8.0), wy::Vector2(10.0, 8.0), pConstruction),
                wy::ErrorStatus::Ok);
            if (pConstruction)
            {
                EXPECT_EQ(pConstruction->setConstruction(true), wy::ErrorStatus::Ok);
                EXPECT_EQ(pSketch->addEntity(pConstruction), wy::ErrorStatus::Ok);
            }

            wy3d::SketchCenterLine* pCenterLine(nullptr);
            EXPECT_EQ(wy3d::SketchCenterLine::create(pTrans, wy::Vector2(0.0, 12.0), wy::Vector2(10.0, 12.0), pCenterLine),
                wy::ErrorStatus::Ok);
            if (pCenterLine) EXPECT_EQ(pSketch->addEntity(pCenterLine), wy::ErrorStatus::Ok);
        });
    ASSERT_FALSE(sketchId.isNull());

    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(sketchId));
    ASSERT_NE(pSketch, nullptr);

    std::vector<TopoDS_Edge> edges;
    EXPECT_EQ(wy3d::Sketch3DProjectionUtil::collectSourceEdges(pSketch, edges), Result::Ok);
    EXPECT_EQ(edges.size(), 1u);
}

// A sketch holding nothing but construction geometry reads as "nothing to project", which is not
// the same news as a caller mistake and must not be reported as one.
TEST(Sketch3DProjectionUtil, SketchWithNothingToProjectIsReadable)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    const wydb::ElementId sketchId = createSketchOnXY(pDb.get(),
        [](wydb::Transaction* pTrans, wy3d::Sketch* pSketch)
        {
            wy3d::SketchLine* pConstruction(nullptr);
            EXPECT_EQ(wy3d::SketchLine::create(pTrans, wy::Vector2(0.0, 8.0), wy::Vector2(10.0, 8.0), pConstruction),
                wy::ErrorStatus::Ok);
            if (pConstruction)
            {
                EXPECT_EQ(pConstruction->setConstruction(true), wy::ErrorStatus::Ok);
                EXPECT_EQ(pSketch->addEntity(pConstruction), wy::ErrorStatus::Ok);
            }
        });
    ASSERT_FALSE(sketchId.isNull());

    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(sketchId));
    ASSERT_NE(pSketch, nullptr);

    std::vector<TopoDS_Edge> edges;
    EXPECT_EQ(wy3d::Sketch3DProjectionUtil::collectSourceEdges(pSketch, edges), Result::Ok);
    EXPECT_TRUE(edges.empty());
}

// --- projection ---

// One case pins three things at once: the analytic result survives, the sweep keeps the side the
// flags ask for, and a target on the far side is an empty projection rather than an error.
TEST(Sketch3DProjectionUtil, LineOntoParallelPlaneStaysALine)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    const wydb::ElementId sketchId = createSketchOnXY(pDb.get(),
        [](wydb::Transaction* pTrans, wy3d::Sketch* pSketch)
        {
            addLine(pTrans, pSketch, wy::Vector2(0.0, 5.0), wy::Vector2(10.0, 5.0));
        });
    ASSERT_FALSE(sketchId.isNull());

    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(sketchId));
    ASSERT_NE(pSketch, nullptr);

    std::vector<TopoDS_Edge> sourceEdges;
    ASSERT_EQ(wy3d::Sketch3DProjectionUtil::collectSourceEdges(pSketch, sourceEdges), Result::Ok);
    ASSERT_EQ(sourceEdges.size(), 1u);

    const TopoDS_Face targetFace = makeBoxTopFace();
    ASSERT_FALSE(targetFace.IsNull());

    std::vector<TopoDS_Edge> edges;
    EXPECT_EQ(wy3d::Sketch3DProjectionUtil::project(pSketch->getPlane(), sourceEdges, targetFace, Options(), edges),
        Result::Ok);
    ASSERT_EQ(edges.size(), 1u);
    EXPECT_EQ(curveType(edges[0]), GeomAbs_Line);
    EXPECT_NEAR(edgeLength(edges[0]), 10.0, kTol);
    EXPECT_NEAR(zRange(edges).first, 10.0, kTol);
    EXPECT_NEAR(zRange(edges).second, 10.0, kTol);

    // The face is entirely on the +Z side, so the reverse sweep has nowhere to go.
    std::vector<TopoDS_Edge> reversed;
    Options reverse;
    reverse.reverse = true;
    EXPECT_EQ(wy3d::Sketch3DProjectionUtil::project(pSketch->getPlane(), sourceEdges, targetFace, reverse, reversed),
        Result::NoProjection);
    EXPECT_TRUE(reversed.empty());
}

TEST(Sketch3DProjectionUtil, CircleOntoParallelPlaneStaysACircle)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    const wydb::ElementId sketchId = createSketchOnXY(pDb.get(),
        [](wydb::Transaction* pTrans, wy3d::Sketch* pSketch)
        {
            wy3d::SketchCircle* pCircle(nullptr);
            EXPECT_EQ(wy3d::SketchCircle::create(pTrans, wy::Vector2(10.0, 10.0), 3.0, pCircle), wy::ErrorStatus::Ok);
            if (pCircle) EXPECT_EQ(pSketch->addEntity(pCircle), wy::ErrorStatus::Ok);
        });
    ASSERT_FALSE(sketchId.isNull());

    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(sketchId));
    ASSERT_NE(pSketch, nullptr);

    std::vector<TopoDS_Edge> sourceEdges;
    ASSERT_EQ(wy3d::Sketch3DProjectionUtil::collectSourceEdges(pSketch, sourceEdges), Result::Ok);
    ASSERT_EQ(sourceEdges.size(), 1u);

    const TopoDS_Face targetFace = makeBoxTopFace();
    ASSERT_FALSE(targetFace.IsNull());

    std::vector<TopoDS_Edge> edges;
    EXPECT_EQ(wy3d::Sketch3DProjectionUtil::project(pSketch->getPlane(), sourceEdges, targetFace, Options(), edges),
        Result::Ok);
    ASSERT_EQ(edges.size(), 1u);
    EXPECT_EQ(curveType(edges[0]), GeomAbs_Circle);
    EXPECT_NEAR(edgeLength(edges[0]), wy3d::TWO_PI * 3.0, 1e-4);

    const std::vector<gp_Pnt> points = sampleEdge(edges[0]);
    for (const gp_Pnt& pnt : points)
    {
        EXPECT_NEAR(pnt.Z(), 10.0, kTol);
        EXPECT_NEAR(std::hypot(pnt.X() - 10.0, pnt.Y() - 10.0), 3.0, 1e-5);
    }
}

// A target the sweep meets on both sides of the sketch plane: the two flags have to tell the two
// hits apart, and only bidirectional keeps both.
TEST(Sketch3DProjectionUtil, CylinderTargetKeepsTheTwoSidesApart)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    const wydb::ElementId sketchId = createSketchOnXY(pDb.get(),
        [](wydb::Transaction* pTrans, wy3d::Sketch* pSketch)
        {
            addLine(pTrans, pSketch, wy::Vector2(-5.0, 0.0), wy::Vector2(5.0, 0.0));
        });
    ASSERT_FALSE(sketchId.isNull());

    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(sketchId));
    ASSERT_NE(pSketch, nullptr);

    std::vector<TopoDS_Edge> sourceEdges;
    ASSERT_EQ(wy3d::Sketch3DProjectionUtil::collectSourceEdges(pSketch, sourceEdges), Result::Ok);
    ASSERT_EQ(sourceEdges.size(), 1u);

    const TopoDS_Face targetFace = makeCrossCylinderLateralFace();
    ASSERT_FALSE(targetFace.IsNull());

    Options both;
    both.bidirectional = true;
    std::vector<TopoDS_Edge> edges;
    ASSERT_EQ(wy3d::Sketch3DProjectionUtil::project(pSketch->getPlane(), sourceEdges, targetFace, both, edges),
        Result::Ok);
    // Asserted on the total and the spread rather than on the edge count: a seam is free to split
    // one ruling in two.
    EXPECT_NEAR(totalLength(edges), 20.0, 1e-4);
    EXPECT_NEAR(zRange(edges).first, -kCylinderRadius, 1e-5);
    EXPECT_NEAR(zRange(edges).second, kCylinderRadius, 1e-5);
    for (const TopoDS_Edge& edge : edges) EXPECT_EQ(curveType(edge), GeomAbs_Line);

    std::vector<TopoDS_Edge> forward;
    ASSERT_EQ(wy3d::Sketch3DProjectionUtil::project(pSketch->getPlane(), sourceEdges, targetFace, Options(), forward),
        Result::Ok);
    EXPECT_NEAR(totalLength(forward), 10.0, 1e-4);
    EXPECT_NEAR(zRange(forward).first, kCylinderRadius, 1e-5);
    EXPECT_NEAR(zRange(forward).second, kCylinderRadius, 1e-5);

    Options reverse;
    reverse.reverse = true;
    std::vector<TopoDS_Edge> backward;
    ASSERT_EQ(wy3d::Sketch3DProjectionUtil::project(pSketch->getPlane(), sourceEdges, targetFace, reverse, backward),
        Result::Ok);
    EXPECT_NEAR(totalLength(backward), 10.0, 1e-4);
    EXPECT_NEAR(zRange(backward).first, -kCylinderRadius, 1e-5);
    EXPECT_NEAR(zRange(backward).second, -kCylinderRadius, 1e-5);
}

// A circle swept into a tube along Z, cut by a tube along X: no closed form, so the result is a
// spline - and it has to stay on the target, and inside the degree the sketch can hold.
TEST(Sketch3DProjectionUtil, CircleOntoCylinderIsASupportedCurveOnTheSurface)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    const wydb::ElementId sketchId = createSketchOnXY(pDb.get(),
        [](wydb::Transaction* pTrans, wy3d::Sketch* pSketch)
        {
            wy3d::SketchCircle* pCircle(nullptr);
            EXPECT_EQ(wy3d::SketchCircle::create(pTrans, wy::Vector2(0.0, 0.0), 3.0, pCircle), wy::ErrorStatus::Ok);
            if (pCircle) EXPECT_EQ(pSketch->addEntity(pCircle), wy::ErrorStatus::Ok);
        });
    ASSERT_FALSE(sketchId.isNull());

    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(sketchId));
    ASSERT_NE(pSketch, nullptr);

    std::vector<TopoDS_Edge> sourceEdges;
    ASSERT_EQ(wy3d::Sketch3DProjectionUtil::collectSourceEdges(pSketch, sourceEdges), Result::Ok);
    ASSERT_EQ(sourceEdges.size(), 1u);

    const TopoDS_Face targetFace = makeCrossCylinderLateralFace();
    ASSERT_FALSE(targetFace.IsNull());

    Options both;
    both.bidirectional = true;
    std::vector<TopoDS_Edge> edges;
    const Result result = wy3d::Sketch3DProjectionUtil::project(pSketch->getPlane(), sourceEdges, targetFace, both, edges);
    EXPECT_EQ(result, Result::Ok);
    ASSERT_FALSE(edges.empty());

    for (const TopoDS_Edge& edge : edges)
    {
        // Whatever came back has to be something the sketch can hold as it is; the collector
        // refuses anything else, so this also proves the guard did not fire spuriously.
        EXPECT_TRUE(wy3d::Sketch3DIntersectionUtil::isSupported(edge));
        for (const gp_Pnt& pnt : sampleEdge(edge))
        {
            EXPECT_NEAR(std::hypot(pnt.Y(), pnt.Z()), kCylinderRadius, 1e-4);
        }
    }
}

TEST(Sketch3DProjectionUtil, NoHitIsReported)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    const wydb::ElementId sketchId = createSketchOnXY(pDb.get(),
        [](wydb::Transaction* pTrans, wy3d::Sketch* pSketch)
        {
            addLine(pTrans, pSketch, wy::Vector2(0.0, 5.0), wy::Vector2(10.0, 5.0));
        });
    ASSERT_FALSE(sketchId.isNull());

    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(sketchId));
    ASSERT_NE(pSketch, nullptr);

    std::vector<TopoDS_Edge> sourceEdges;
    ASSERT_EQ(wy3d::Sketch3DProjectionUtil::collectSourceEdges(pSketch, sourceEdges), Result::Ok);

    // A face well away from the sweep, on a different part of the plane.
    const TopoDS_Face farFace = findPlanarFace(BRepPrimAPI_MakeBox(gp_Pnt(100.0, 0.0, 0.0), 10.0, 10.0, 10.0).Shape(),
        gp_Pnt(105.0, 5.0, 10.0), gp_Dir(0.0, 0.0, 1.0));
    ASSERT_FALSE(farFace.IsNull());

    std::vector<TopoDS_Edge> edges;
    EXPECT_EQ(wy3d::Sketch3DProjectionUtil::project(pSketch->getPlane(), sourceEdges, farFace, Options(), edges),
        Result::NoProjection);
    EXPECT_TRUE(edges.empty());
}

// On a planar target there is nothing behind the sketch plane to duplicate, so bidirectional has
// to give exactly what the plain forward sweep gives.
TEST(Sketch3DProjectionUtil, BidirectionalAddsNothingOnAPlanarTarget)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    const wydb::ElementId sketchId = createSketchOnXY(pDb.get(),
        [](wydb::Transaction* pTrans, wy3d::Sketch* pSketch)
        {
            addLine(pTrans, pSketch, wy::Vector2(0.0, 5.0), wy::Vector2(10.0, 5.0));
        });
    ASSERT_FALSE(sketchId.isNull());

    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(sketchId));
    ASSERT_NE(pSketch, nullptr);

    std::vector<TopoDS_Edge> sourceEdges;
    ASSERT_EQ(wy3d::Sketch3DProjectionUtil::collectSourceEdges(pSketch, sourceEdges), Result::Ok);

    const TopoDS_Face targetFace = makeBoxTopFace();
    ASSERT_FALSE(targetFace.IsNull());

    Options both;
    both.bidirectional = true;
    std::vector<TopoDS_Edge> edges;
    ASSERT_EQ(wy3d::Sketch3DProjectionUtil::project(pSketch->getPlane(), sourceEdges, targetFace, both, edges),
        Result::Ok);
    ASSERT_EQ(edges.size(), 1u);
    EXPECT_NEAR(edgeLength(edges[0]), 10.0, kTol);
    EXPECT_NEAR(zRange(edges).first, 10.0, kTol);
}

// --- projection onto a datum plane's plane ---

// The three things the face case pins, now against a target that has no boundary of its own: the
// analytic result survives, the sweep keeps the side the flags ask for, and the far side is an
// empty projection rather than an error.
TEST(Sketch3DProjectionUtil, LineOntoADatumPlaneStaysALine)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    const wydb::ElementId sketchId = createSketchOnXY(pDb.get(),
        [](wydb::Transaction* pTrans, wy3d::Sketch* pSketch)
        {
            addLine(pTrans, pSketch, wy::Vector2(0.0, 5.0), wy::Vector2(10.0, 5.0));
        });
    ASSERT_FALSE(sketchId.isNull());

    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(sketchId));
    ASSERT_NE(pSketch, nullptr);

    std::vector<TopoDS_Edge> sourceEdges;
    ASSERT_EQ(wy3d::Sketch3DProjectionUtil::collectSourceEdges(pSketch, sourceEdges), Result::Ok);
    ASSERT_EQ(sourceEdges.size(), 1u);

    // The plane z = 10, which is what a datum plane in that place stands for.
    const wy3d::SketchPlane targetPlane(
        wy::Vector3(0.0, 0.0, 10.0), wy::Vector3::kZAxis, wy::Vector3::kXAxis);

    std::vector<TopoDS_Edge> edges;
    EXPECT_EQ(wy3d::Sketch3DProjectionUtil::project(pSketch->getPlane(), sourceEdges, targetPlane, Options(), edges),
        Result::Ok);
    ASSERT_EQ(edges.size(), 1u);
    EXPECT_EQ(curveType(edges[0]), GeomAbs_Line);
    EXPECT_NEAR(edgeLength(edges[0]), 10.0, kTol);
    EXPECT_NEAR(zRange(edges).first, 10.0, kTol);
    EXPECT_NEAR(zRange(edges).second, 10.0, kTol);

    // The plane is entirely on the +Z side, so the reverse sweep has nowhere to go - the rule the
    // face case follows as well.
    std::vector<TopoDS_Edge> reversed;
    Options reverse;
    reverse.reverse = true;
    EXPECT_EQ(wy3d::Sketch3DProjectionUtil::project(pSketch->getPlane(), sourceEdges, targetPlane, reverse, reversed),
        Result::NoProjection);
    EXPECT_TRUE(reversed.empty());
}

// The difference between the two kinds of target, in one comparison: the same source onto a face
// and onto the plane that face lies in. The face trims the result at its own boundary, the plane
// keeps all of it.
TEST(Sketch3DProjectionUtil, ADatumPlaneKeepsWhatAFaceWouldTrim)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    const wydb::ElementId sketchId = createSketchOnXY(pDb.get(),
        [](wydb::Transaction* pTrans, wy3d::Sketch* pSketch)
        {
            // Sixty long, while the box top face reaches y = 20 at most.
            addLine(pTrans, pSketch, wy::Vector2(10.0, -30.0), wy::Vector2(10.0, 30.0));
        });
    ASSERT_FALSE(sketchId.isNull());

    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(sketchId));
    ASSERT_NE(pSketch, nullptr);

    std::vector<TopoDS_Edge> sourceEdges;
    ASSERT_EQ(wy3d::Sketch3DProjectionUtil::collectSourceEdges(pSketch, sourceEdges), Result::Ok);
    ASSERT_EQ(sourceEdges.size(), 1u);

    const TopoDS_Face targetFace = makeBoxTopFace();
    ASSERT_FALSE(targetFace.IsNull());

    std::vector<TopoDS_Edge> trimmed;
    ASSERT_EQ(wy3d::Sketch3DProjectionUtil::project(pSketch->getPlane(), sourceEdges, targetFace, Options(), trimmed),
        Result::Ok);
    EXPECT_NEAR(totalLength(trimmed), 20.0, kTol);

    const wy3d::SketchPlane targetPlane(
        wy::Vector3(0.0, 0.0, 10.0), wy::Vector3::kZAxis, wy::Vector3::kXAxis);
    std::vector<TopoDS_Edge> whole;
    ASSERT_EQ(wy3d::Sketch3DProjectionUtil::project(pSketch->getPlane(), sourceEdges, targetPlane, Options(), whole),
        Result::Ok);
    EXPECT_NEAR(totalLength(whole), 60.0, kTol);
}

// A plane the sweep meets at an angle: the circle comes back as an ellipse rather than as an
// approximation, and it comes back whole. The plane here is z = y, so a source point at y travels
// exactly y to reach it.
TEST(Sketch3DProjectionUtil, CircleOntoATiltedDatumPlaneIsAnEllipse)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    const wydb::ElementId sketchId = createSketchOnXY(pDb.get(),
        [](wydb::Transaction* pTrans, wy3d::Sketch* pSketch)
        {
            // Clear of the plane's own line through the sketch, so the whole circle sweeps forward.
            wy3d::SketchCircle* pCircle(nullptr);
            EXPECT_EQ(wy3d::SketchCircle::create(pTrans, wy::Vector2(0.0, 10.0), 3.0, pCircle), wy::ErrorStatus::Ok);
            if (pCircle) EXPECT_EQ(pSketch->addEntity(pCircle), wy::ErrorStatus::Ok);
        });
    ASSERT_FALSE(sketchId.isNull());

    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(sketchId));
    ASSERT_NE(pSketch, nullptr);

    std::vector<TopoDS_Edge> sourceEdges;
    ASSERT_EQ(wy3d::Sketch3DProjectionUtil::collectSourceEdges(pSketch, sourceEdges), Result::Ok);
    ASSERT_EQ(sourceEdges.size(), 1u);

    // Forty five degrees about the X axis, through the origin.
    const wy3d::SketchPlane targetPlane(wy::Vector3::kZero,
        wy::Vector3(0.0, -std::sqrt(0.5), std::sqrt(0.5)), wy::Vector3::kXAxis);

    std::vector<TopoDS_Edge> edges;
    ASSERT_EQ(wy3d::Sketch3DProjectionUtil::project(pSketch->getPlane(), sourceEdges, targetPlane, Options(), edges),
        Result::Ok);
    ASSERT_EQ(edges.size(), 1u);
    EXPECT_EQ(curveType(edges[0]), GeomAbs_Ellipse);

    // The stretch is the 1 / cos(45) of an oblique projection, along the direction the plane rises
    // in: a radius of 3 becomes 3*sqrt(2), across the same 3 as before.
    const gp_Elips ellipse = BRepAdaptor_Curve(edges[0]).Ellipse();
    EXPECT_NEAR(ellipse.MajorRadius(), 3.0 * std::sqrt(2.0), 1e-6);
    EXPECT_NEAR(ellipse.MinorRadius(), 3.0, 1e-6);

    // Every point on the plane, and the extremes of a closed curve: an arc, which is what a target
    // that could only be reached from one side would give, stops short of them.
    const int kSamples = 256;
    double xLo(1e9), xHi(-1e9), yLo(1e9), yHi(-1e9), offPlane(0.0);
    for (const gp_Pnt& pnt : sampleEdge(edges[0], kSamples))
    {
        xLo = std::min(xLo, pnt.X());
        xHi = std::max(xHi, pnt.X());
        yLo = std::min(yLo, pnt.Y());
        yHi = std::max(yHi, pnt.Y());
        offPlane = std::max(offPlane, std::abs(pnt.Z() - pnt.Y()));
    }
    EXPECT_NEAR(offPlane, 0.0, kTol);
    EXPECT_NEAR(xLo, -3.0, 0.02);
    EXPECT_NEAR(xHi, 3.0, 0.02);
    EXPECT_NEAR(yLo, 7.0, 0.02);
    EXPECT_NEAR(yHi, 13.0, 0.02);
}

// A source lying across the line where the two planes meet, so part of it has to be swept one way
// and part of it the other. What comes back is one side at a time, which is what the two flags are
// for: a single sided sweep covers the sketch plane's forward side and nothing behind it, so the
// half of the source on the far side of the junction is the other flag's to ask for.
TEST(Sketch3DProjectionUtil, AStraddlingSourceIsSweptOneWayAtATime)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    const wydb::ElementId sketchId = createSketchOnXY(pDb.get(),
        [](wydb::Transaction* pTrans, wy3d::Sketch* pSketch)
        {
            // Across the origin, so the plane below cuts it in half.
            addLine(pTrans, pSketch, wy::Vector2(0.0, -5.0), wy::Vector2(0.0, 5.0));
        });
    ASSERT_FALSE(sketchId.isNull());

    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(sketchId));
    ASSERT_NE(pSketch, nullptr);

    std::vector<TopoDS_Edge> sourceEdges;
    ASSERT_EQ(wy3d::Sketch3DProjectionUtil::collectSourceEdges(pSketch, sourceEdges), Result::Ok);
    ASSERT_EQ(sourceEdges.size(), 1u);

    // z = y, as above: a source point at y travels exactly y to reach it, so the half at y < 0 wants
    // the sweep backwards and the half at y > 0 wants it forwards.
    const wy3d::SketchPlane targetPlane(wy::Vector3::kZero,
        wy::Vector3(0.0, -std::sqrt(0.5), std::sqrt(0.5)), wy::Vector3::kXAxis);

    // One side of the junction is half the source line, at the 45 degrees it is projected across.
    const double kHalf = 5.0 * std::sqrt(2.0);

    std::vector<TopoDS_Edge> edges;
    ASSERT_EQ(wy3d::Sketch3DProjectionUtil::project(pSketch->getPlane(), sourceEdges, targetPlane, Options(), edges),
        Result::Ok);
    EXPECT_NEAR(totalLength(edges), kHalf, kTol);
    for (const TopoDS_Edge& edge : edges)
    {
        EXPECT_EQ(curveType(edge), GeomAbs_Line);
        for (const gp_Pnt& pnt : sampleEdge(edge, 8))
        {
            EXPECT_NEAR(pnt.Z() - pnt.Y(), 0.0, kTol);
            EXPECT_GE(pnt.Y(), -kTol);
        }
    }

    Options reverse;
    reverse.reverse = true;
    edges.clear();
    ASSERT_EQ(wy3d::Sketch3DProjectionUtil::project(pSketch->getPlane(), sourceEdges, targetPlane, reverse, edges),
        Result::Ok);
    EXPECT_NEAR(totalLength(edges), kHalf, kTol);
    for (const TopoDS_Edge& edge : edges)
    {
        for (const gp_Pnt& pnt : sampleEdge(edge, 8)) EXPECT_LE(pnt.Y(), kTol);
    }

    // Both at once, and there is nothing left over: the whole line, projected whole.
    Options bidirectional;
    bidirectional.bidirectional = true;
    edges.clear();
    ASSERT_EQ(wy3d::Sketch3DProjectionUtil::project(pSketch->getPlane(), sourceEdges, targetPlane, bidirectional, edges),
        Result::Ok);
    EXPECT_NEAR(totalLength(edges), 2.0 * kHalf, kTol);
}

// The two ways a target plane cannot be met, and a target that is not a plane at all. Neither of
// the first two is an error: the sweep never meets the plane, which is what the user is told.
TEST(Sketch3DProjectionUtil, TargetsTheSweepCannotMeetAreNoProjection)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    const wydb::ElementId sketchId = createSketchOnXY(pDb.get(),
        [](wydb::Transaction* pTrans, wy3d::Sketch* pSketch)
        {
            addLine(pTrans, pSketch, wy::Vector2(0.0, 5.0), wy::Vector2(10.0, 5.0));
        });
    ASSERT_FALSE(sketchId.isNull());

    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(sketchId));
    ASSERT_NE(pSketch, nullptr);

    std::vector<TopoDS_Edge> sourceEdges;
    ASSERT_EQ(wy3d::Sketch3DProjectionUtil::collectSourceEdges(pSketch, sourceEdges), Result::Ok);
    ASSERT_FALSE(sourceEdges.empty());

    std::vector<TopoDS_Edge> edges;

    // The sketch's own plane: there is nothing to sweep to, and the two planes meet everywhere.
    const wy3d::SketchPlane coincident(wy::Vector3::kZero, wy::Vector3::kZAxis, wy::Vector3::kXAxis);
    EXPECT_EQ(wy3d::Sketch3DProjectionUtil::project(pSketch->getPlane(), sourceEdges, coincident, Options(), edges),
        Result::NoProjection);
    EXPECT_TRUE(edges.empty());

    // The same case on the other kind of target: a face lying in the sketch plane. The rule is
    // about where the target is, not about which of the two was picked - and a projection of the
    // sketch onto the plane it already lies in would only be a duplicate of the sketch.
    const TopoDS_Face levelFace = findPlanarFace(
        BRepPrimAPI_MakeBox(gp_Pnt(0.0, 0.0, -10.0), 20.0, 20.0, 10.0).Shape(),
        gp_Pnt(10.0, 10.0, 0.0), gp_Dir(0.0, 0.0, 1.0));
    ASSERT_FALSE(levelFace.IsNull());
    EXPECT_EQ(wy3d::Sketch3DProjectionUtil::project(pSketch->getPlane(), sourceEdges, levelFace, Options(), edges),
        Result::NoProjection);
    EXPECT_TRUE(edges.empty());

    // A plane the sweep runs along rather than into: it stays parallel to it however far it goes.
    const wy3d::SketchPlane alongSide(wy::Vector3::kZero, wy::Vector3::kXAxis, wy::Vector3::kYAxis);
    EXPECT_EQ(wy3d::Sketch3DProjectionUtil::project(pSketch->getPlane(), sourceEdges, alongSide, Options(), edges),
        Result::NoProjection);
    EXPECT_TRUE(edges.empty());

    // A plane that is not a plane.
    const wy3d::SketchPlane bad(wy::Vector3::kZero, wy::Vector3::kZero, wy::Vector3::kZero);
    ASSERT_FALSE(bad.isValid());
    EXPECT_EQ(wy3d::Sketch3DProjectionUtil::project(pSketch->getPlane(), sourceEdges, bad, Options(), edges),
        Result::InvalidInput);
    EXPECT_TRUE(edges.empty());

    // And a target of either kind with nothing to project.
    const wy3d::SketchPlane targetPlane(
        wy::Vector3(0.0, 0.0, 10.0), wy::Vector3::kZAxis, wy::Vector3::kXAxis);
    EXPECT_EQ(wy3d::Sketch3DProjectionUtil::project(pSketch->getPlane(), std::vector<TopoDS_Edge>(),
                  targetPlane, Options(), edges),
        Result::InvalidInput);
    EXPECT_TRUE(edges.empty());
}

// The guarantee the faces are held to, extended to planes: no target may produce broken geometry,
// and whatever does come back has to be something the sketch can hold.
TEST(Sketch3DProjectionUtil, EveryDatumPlaneIsHandled)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    const wydb::ElementId sketchId = createSketchOnXY(pDb.get(),
        [](wydb::Transaction* pTrans, wy3d::Sketch* pSketch)
        {
            wy3d::SketchCircle* pCircle(nullptr);
            EXPECT_EQ(wy3d::SketchCircle::create(pTrans, wy::Vector2(0.0, 0.0), 3.0, pCircle), wy::ErrorStatus::Ok);
            if (pCircle) EXPECT_EQ(pSketch->addEntity(pCircle), wy::ErrorStatus::Ok);
            addLine(pTrans, pSketch, wy::Vector2(-8.0, 0.0), wy::Vector2(8.0, 0.0));
        });
    ASSERT_FALSE(sketchId.isNull());

    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(sketchId));
    ASSERT_NE(pSketch, nullptr);

    std::vector<TopoDS_Edge> sourceEdges;
    ASSERT_EQ(wy3d::Sketch3DProjectionUtil::collectSourceEdges(pSketch, sourceEdges), Result::Ok);
    ASSERT_EQ(sourceEdges.size(), 2u);

    const wy::Vector3 oblique(0.0, -std::sqrt(0.5), std::sqrt(0.5));
    std::vector<wy3d::SketchPlane> targets;
    targets.push_back(wy3d::SketchPlane(wy::Vector3(0.0, 0.0, 10.0), wy::Vector3::kZAxis, wy::Vector3::kXAxis));
    targets.push_back(wy3d::SketchPlane(wy::Vector3(0.0, 0.0, -10.0), wy::Vector3::kZAxis, wy::Vector3::kXAxis));
    targets.push_back(wy3d::SketchPlane(wy::Vector3(3.0, 3.0, 10.0), wy::Vector3::kZAxis, wy::Vector3(1.0, 0.0, 0.0)));
    targets.push_back(wy3d::SketchPlane(wy::Vector3::kZero, oblique, wy::Vector3::kXAxis));
    targets.push_back(wy3d::SketchPlane(wy::Vector3::kZero, wy::Vector3::kXAxis, wy::Vector3::kYAxis));
    targets.push_back(wy3d::SketchPlane(wy::Vector3::kZero, wy::Vector3::kZAxis, wy::Vector3::kXAxis));

    int projected(0), missed(0);
    for (const wy3d::SketchPlane& target : targets)
    {
        for (int mode = 0; mode < 3; ++mode)
        {
            Options options;
            options.reverse = (1 == mode);
            options.bidirectional = (2 == mode);

            std::vector<TopoDS_Edge> edges;
            const Result result = wy3d::Sketch3DProjectionUtil::project(
                pSketch->getPlane(), sourceEdges, target, options, edges);

            EXPECT_NE(result, Result::InvalidGeometry);
            EXPECT_NE(result, Result::AlgorithmFailed);
            if (Result::Ok == result)
            {
                ASSERT_FALSE(edges.empty());
                for (const TopoDS_Edge& edge : edges)
                {
                    EXPECT_TRUE(wy3d::Sketch3DIntersectionUtil::isSupported(edge));
                    EXPECT_FALSE(BRep_Tool::Degenerated(edge));

                    wy3d::Sketch3DEdgeUtil::CurveSpec spec;
                    EXPECT_EQ(wy3d::Sketch3DEdgeUtil::makeCurveSpec(edge, spec),
                        wy3d::Sketch3DEdgeUtil::Result::Ok);
                }
                ++projected;
            }
            else
            {
                EXPECT_EQ(result, Result::NoProjection);
                EXPECT_TRUE(edges.empty());
                ++missed;
            }
        }
    }

    std::cout << "[projection onto planes] projected=" << projected << " missed=" << missed << std::endl;
    EXPECT_GT(projected, 0);
}

TEST(Sketch3DProjectionUtil, InvalidInputIsRejected)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    const wydb::ElementId sketchId = createSketchOnXY(pDb.get(),
        [](wydb::Transaction* pTrans, wy3d::Sketch* pSketch)
        {
            addLine(pTrans, pSketch, wy::Vector2(0.0, 5.0), wy::Vector2(10.0, 5.0));
        });
    ASSERT_FALSE(sketchId.isNull());

    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(sketchId));
    ASSERT_NE(pSketch, nullptr);

    std::vector<TopoDS_Edge> sourceEdges;
    ASSERT_EQ(wy3d::Sketch3DProjectionUtil::collectSourceEdges(pSketch, sourceEdges), Result::Ok);
    ASSERT_FALSE(sourceEdges.empty());

    const TopoDS_Face targetFace = makeBoxTopFace();
    ASSERT_FALSE(targetFace.IsNull());

    std::vector<TopoDS_Edge> edges;
    // Null face
    EXPECT_EQ(wy3d::Sketch3DProjectionUtil::project(pSketch->getPlane(), sourceEdges, TopoDS_Face(), Options(), edges),
        Result::InvalidInput);
    // An invalid plane. Not the default constructor: that one is the XY plane through the origin
    // and is perfectly usable, so it would only prove the call can succeed.
    wy3d::SketchPlane badPlane(wy::Vector3::kZero, wy::Vector3::kZero, wy::Vector3::kZero);
    ASSERT_FALSE(badPlane.isValid());
    EXPECT_EQ(wy3d::Sketch3DProjectionUtil::project(badPlane, sourceEdges, targetFace, Options(), edges),
        Result::InvalidInput);
    // Empty source
    EXPECT_EQ(wy3d::Sketch3DProjectionUtil::project(pSketch->getPlane(), std::vector<TopoDS_Edge>(), targetFace, Options(), edges),
        Result::InvalidInput);
    // Null sketch
    EXPECT_EQ(wy3d::Sketch3DProjectionUtil::collectSourceEdges(nullptr, edges), Result::InvalidInput);
    EXPECT_TRUE(edges.empty());
}

// The whole point of routing through the section machinery is that a primitive's faces do not
// surprise it: nothing may come back as broken geometry, and whatever does come back has to be
// something the sketch can hold.
TEST(Sketch3DProjectionUtil, EveryTargetFaceIsHandled)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    const wydb::ElementId sketchId = createSketchOnXY(pDb.get(),
        [](wydb::Transaction* pTrans, wy3d::Sketch* pSketch)
        {
            wy3d::SketchCircle* pCircle(nullptr);
            EXPECT_EQ(wy3d::SketchCircle::create(pTrans, wy::Vector2(0.0, 0.0), 3.0, pCircle), wy::ErrorStatus::Ok);
            if (pCircle) EXPECT_EQ(pSketch->addEntity(pCircle), wy::ErrorStatus::Ok);
            addLine(pTrans, pSketch, wy::Vector2(-8.0, 0.0), wy::Vector2(8.0, 0.0));
        });
    ASSERT_FALSE(sketchId.isNull());

    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(sketchId));
    ASSERT_NE(pSketch, nullptr);

    std::vector<TopoDS_Edge> sourceEdges;
    ASSERT_EQ(wy3d::Sketch3DProjectionUtil::collectSourceEdges(pSketch, sourceEdges), Result::Ok);
    ASSERT_EQ(sourceEdges.size(), 2u);

    std::vector<TopoDS_Shape> targets;
    targets.push_back(BRepPrimAPI_MakeBox(20.0, 20.0, 10.0).Shape());
    targets.push_back(BRepPrimAPI_MakeCylinder(kCylinderRadius, 10.0).Shape());
    targets.push_back(BRepPrimAPI_MakeSphere(5.0).Shape());
    targets.push_back(BRepPrimAPI_MakeCone(5.0, 2.0, 10.0).Shape());
    targets.push_back(BRepPrimAPI_MakeTorus(5.0, 1.5).Shape());

    int projected(0), missed(0);
    for (const TopoDS_Shape& target : targets)
    {
        TopTools_IndexedMapOfShape faceMap;
        TopExp::MapShapes(target, TopAbs_FACE, faceMap);
        for (Standard_Integer i = 1; i <= faceMap.Extent(); ++i)
        {
            const TopoDS_Face& face = TopoDS::Face(faceMap.FindKey(i));
            for (int mode = 0; mode < 3; ++mode)
            {
                Options options;
                options.reverse = (1 == mode);
                options.bidirectional = (2 == mode);

                std::vector<TopoDS_Edge> edges;
                const Result result = wy3d::Sketch3DProjectionUtil::project(
                    pSketch->getPlane(), sourceEdges, face, options, edges);

                EXPECT_NE(result, Result::InvalidGeometry);
                EXPECT_NE(result, Result::AlgorithmFailed);
                if (Result::Ok == result)
                {
                    ASSERT_FALSE(edges.empty());
                    for (const TopoDS_Edge& edge : edges)
                    {
                        EXPECT_TRUE(wy3d::Sketch3DIntersectionUtil::isSupported(edge));
                        EXPECT_FALSE(BRep_Tool::Degenerated(edge));

                        // The real contract, and a stronger one than any length floor: the
                        // geometry layer may only hand over edges that turning into a sketch
                        // entity actually succeeds on. A grazing section leaves slivers here,
                        // which is exactly what the collector has to have dropped.
                        wy3d::Sketch3DEdgeUtil::CurveSpec spec;
                        EXPECT_EQ(wy3d::Sketch3DEdgeUtil::makeCurveSpec(edge, spec),
                            wy3d::Sketch3DEdgeUtil::Result::Ok);
                    }
                    ++projected;
                }
                else
                {
                    EXPECT_EQ(result, Result::NoProjection);
                    EXPECT_TRUE(edges.empty());
                    ++missed;
                }
            }
        }
    }

    std::cout << "[projection sweep] projected=" << projected << " missed=" << missed << std::endl;
    EXPECT_GT(projected, 0);
}
