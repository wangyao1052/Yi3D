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

#include <wy3dErrorCode.h>
#include <wy3dMath.h>
#include <wy3dSketchPlane.h>

#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <BRep_Tool.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <Geom_Plane.hxx>
#include <gp_Pnt.hxx>
#include <gp_Circ.hxx>
#include <gp_Ax2.hxx>
#include <gp_Pln.hxx>
#include <TopAbs.hxx>
#include <TopoDS_Compound.hxx>
#include <BRep_Builder.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepLib_FindSurface.hxx>
#include <GeomAdaptor_Surface.hxx>
#include <Precision.hxx>

#include <iostream>

#include "wy3d/topo/TopoShapeUtil.h"

#include <cmath>
#include <vector>

namespace
{
    // BRepLib_FindSurface arguments: Tol = -1 asks for the max of the shape's own edge tolerances
    // instead of a fixed value, and OnlyPlane keeps the fitted surface to a plane
    const double kUseShapeTolerance = -1.0;
    const Standard_Boolean kOnlyPlane = Standard_True;

    TopoDS_Edge makeEdge(const gp_Pnt& p1, const gp_Pnt& p2)
    {
        return TopoDS::Edge(BRepBuilderAPI_MakeEdge(p1, p2).Edge());
    }

    // 100x100 square on the XY plane
    std::vector<TopoDS_Edge> makeRectangleEdges()
    {
        const gp_Pnt p0(0, 0, 0), p1(100, 0, 0), p2(100, 100, 0), p3(0, 100, 0);
        return { makeEdge(p0, p1), makeEdge(p1, p2), makeEdge(p2, p3), makeEdge(p3, p0) };
    }

    // Same square with one corner lifted by 50
    std::vector<gp_Pnt> makeNonPlanarQuadCorners()
    {
        return { gp_Pnt(0, 0, 0), gp_Pnt(100, 0, 0), gp_Pnt(100, 100, 50), gp_Pnt(0, 100, 0) };
    }

    std::vector<TopoDS_Edge> makeNonPlanarQuadEdges()
    {
        const std::vector<gp_Pnt> corners = makeNonPlanarQuadCorners();
        std::vector<TopoDS_Edge> edges;
        for (size_t i = 0; i < corners.size(); ++i)
        {
            edges.emplace_back(makeEdge(corners[i], corners[(i + 1) % corners.size()]));
        }
        return edges;
    }

    bool hasVertexAt(const TopoDS_Face& face, const gp_Pnt& point, double tol)
    {
        TopTools_IndexedMapOfShape vertexMap;
        TopExp::MapShapes(face, TopAbs_ShapeEnum::TopAbs_VERTEX, vertexMap);
        for (int k = 1; k <= vertexMap.Extent(); ++k)
        {
            if (BRep_Tool::Pnt(TopoDS::Vertex(vertexMap(k))).Distance(point) <= tol) return true;
        }
        return false;
    }

    bool isPlaneSurface(const TopoDS_Face& face)
    {
        if (face.IsNull()) return false;
        Handle(Geom_Surface) surface = BRep_Tool::Surface(face);
        if (surface.IsNull()) return false;
        return !Handle(Geom_Plane)::DownCast(surface).IsNull();
    }

    double faceArea(const TopoDS_Face& face)
    {
        GProp_GProps props;
        BRepGProp::SurfaceProperties(face, props);
        return props.Mass();
    }

    bool faceBoundingBoxZ(const TopoDS_Face& face, double& zMin, double& zMax)
    {
        Bnd_Box box;
        BRepBndLib::Add(face, box);
        if (box.IsVoid()) return false;
        Standard_Real xMin(0), yMin(0), zMinBox(0), xMax(0), yMax(0), zMaxBox(0);
        box.Get(xMin, yMin, zMinBox, xMax, yMax, zMaxBox);
        zMin = zMinBox;
        zMax = zMaxBox;
        return true;
    }

    // Axis parallel square, edges running counter-clockwise seen from +Z
    std::vector<TopoDS_Edge> makeSquareEdges(double x0, double y0, double x1, double y1, double z = 0.0)
    {
        const gp_Pnt p0(x0, y0, z), p1(x1, y0, z), p2(x1, y1, z), p3(x0, y1, z);
        return { makeEdge(p0, p1), makeEdge(p1, p2), makeEdge(p2, p3), makeEdge(p3, p0) };
    }

    // The same square traversed clockwise: the edges themselves run that way
    std::vector<TopoDS_Edge> makeSquareEdgesClockwise(double x0, double y0, double x1, double y1, double z = 0.0)
    {
        const gp_Pnt p0(x0, y0, z), p1(x1, y0, z), p2(x1, y1, z), p3(x0, y1, z);
        return { makeEdge(p0, p3), makeEdge(p3, p2), makeEdge(p2, p1), makeEdge(p1, p0) };
    }

    // Wire in exactly the order given, so the traversal sense is the caller's
    TopoDS_Wire makeWire(const std::vector<TopoDS_Edge>& edges)
    {
        BRepBuilderAPI_MakeWire makeWire;
        for (const TopoDS_Edge& edge : edges)
        {
            makeWire.Add(edge);
        }
        return makeWire.Wire();
    }

    int wireCount(const TopoDS_Shape& shape)
    {
        TopTools_IndexedMapOfShape wireMap;
        TopExp::MapShapes(shape, TopAbs_ShapeEnum::TopAbs_WIRE, wireMap);
        return wireMap.Extent();
    }

    std::vector<TopoDS_Face> shapeFaces(const TopoDS_Shape& shape)
    {
        std::vector<TopoDS_Face> faces;
        for (TopExp_Explorer exp(shape, TopAbs_ShapeEnum::TopAbs_FACE); exp.More(); exp.Next())
        {
            faces.emplace_back(TopoDS::Face(exp.Current()));
        }
        return faces;
    }

    int shapeShellCount(const TopoDS_Shape& shape)
    {
        TopTools_IndexedMapOfShape shellMap;
        TopExp::MapShapes(shape, TopAbs_ShapeEnum::TopAbs_SHELL, shellMap);
        return shellMap.Extent();
    }

    double totalArea(const TopoDS_Shape& shape)
    {
        double area = 0.0;
        for (const TopoDS_Face& face : shapeFaces(shape))
        {
            area += faceArea(face);
        }
        return area;
    }

    const char* stateName(TopAbs_State state)
    {
        switch (state)
        {
        case TopAbs_IN: return "IN";
        case TopAbs_OUT: return "OUT";
        case TopAbs_ON: return "ON";
        default: return "UNKNOWN";
        }
    }
}

TEST(TopoShapeUtil, MakeWireFromEdges_ClosesRectangle)
{
    std::vector<TopoDS_Edge> edges = makeRectangleEdges();

    TopoDS_Wire wire;
    EXPECT_EQ(wy3d::ErrorCode::NoError, wy3d::TopoShapeUtil::makeWireFromEdges(edges, wire));
    EXPECT_FALSE(wire.IsNull());
    EXPECT_TRUE(wire.Closed());
}

TEST(TopoShapeUtil, MakeWireFromEdges_ShuffledAndReversed)
{
    const std::vector<TopoDS_Edge> edges = makeRectangleEdges();
    const std::vector<TopoDS_Edge> shuffled{
        TopoDS::Edge(edges[2].Reversed()),
        edges[3],
        TopoDS::Edge(edges[0].Reversed()),
        TopoDS::Edge(edges[1].Reversed())
    };

    TopoDS_Wire wire;
    EXPECT_EQ(wy3d::ErrorCode::NoError, wy3d::TopoShapeUtil::makeWireFromEdges(shuffled, wire));
    EXPECT_FALSE(wire.IsNull());
    EXPECT_TRUE(wire.Closed());
}

TEST(TopoShapeUtil, MakePlanarSheetFromEdges_ExactPlane)
{
    std::vector<TopoDS_Edge> edges = makeRectangleEdges();

    TopoDS_Shape shape;
    EXPECT_EQ(wy3d::ErrorCode::NoError, wy3d::TopoShapeUtil::makePlanarSheetFromEdges(edges, shape));
    ASSERT_FALSE(shape.IsNull());

    // Even a single loop comes back as a compound of one shell, the shape a sketch profile
    // produces for the same command
    EXPECT_EQ(TopAbs_ShapeEnum::TopAbs_COMPOUND, shape.ShapeType());
    EXPECT_EQ(1, shapeShellCount(shape));

    const std::vector<TopoDS_Face> faces = shapeFaces(shape);
    ASSERT_EQ(1u, faces.size());
    EXPECT_TRUE(isPlaneSurface(faces[0]));

    wy3d::SketchPlane plane;
    ASSERT_TRUE(wy3d::TopoShapeUtil::getFacePlane(faces[0], plane));
    EXPECT_NEAR(1.0, std::abs(plane.getNormal().z()), 1e-9);
    EXPECT_NEAR(0.0, plane.getOrigin().z(), 1e-9);
    EXPECT_NEAR(10000.0, faceArea(faces[0]), 1e-6);
    EXPECT_TRUE(BRepCheck_Analyzer(faces[0]).IsValid());
}

TEST(TopoShapeUtil, MakePlanarSheetFromEdges_OpenChain)
{
    const gp_Pnt p0(0, 0, 0), p1(100, 0, 0), p2(100, 100, 0), p3(0, 100, 0);
    const std::vector<TopoDS_Edge> openChain{ makeEdge(p0, p1), makeEdge(p1, p2), makeEdge(p2, p3) };

    TopoDS_Shape shape;
    EXPECT_EQ(wy3d::ErrorCode::PLANARSHEET_EdgesNotClosed,
        wy3d::TopoShapeUtil::makePlanarSheetFromEdges(openChain, shape));
    EXPECT_TRUE(shape.IsNull());
}

TEST(TopoShapeUtil, MakePlanarSheetFromEdges_ClosedLoopPlusStrayEdge)
{
    const gp_Pnt p0(0, 0, 0), p1(100, 0, 0), p2(0, 100, 0), s0(200, 200, 0), s1(300, 200, 0);
    const std::vector<TopoDS_Edge> edges{
        makeEdge(p0, p1), makeEdge(p1, p2), makeEdge(p2, p0), makeEdge(s0, s1)
    };

    // The stray edge leaves its two vertices with degree one, which the loop split rejects
    TopoDS_Shape shape;
    EXPECT_EQ(wy3d::ErrorCode::PLANARSHEET_EdgesNotClosed,
        wy3d::TopoShapeUtil::makePlanarSheetFromEdges(edges, shape));
    EXPECT_TRUE(shape.IsNull());
}

TEST(TopoShapeUtil, MakeWireFromEdges_SingleClosedEdge)
{
    const gp_Circ circle(gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), 50.0);
    const std::vector<TopoDS_Edge> edges{ TopoDS::Edge(BRepBuilderAPI_MakeEdge(circle).Edge()) };

    TopoDS_Wire wire;
    EXPECT_EQ(wy3d::ErrorCode::NoError, wy3d::TopoShapeUtil::makeWireFromEdges(edges, wire));
    EXPECT_FALSE(wire.IsNull());
    EXPECT_TRUE(wire.Closed());
}

TEST(TopoShapeUtil, MakeWireFromEdges_SolidRimEdge)
{
    // A rim edge of a real solid, i.e. the boundary of a cylindrical face: it is the one
    // edge case where a single picked edge has to close a loop on its own
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();

    const double height(200.0);
    Tube* pTube(nullptr);
    wydb::Transaction* pTrans = pTransMgr->startTransaction();
    ASSERT_EQ(Tube::create(pTrans, 50.0, 30.0, height, pTube), wy::ErrorStatus::Ok);
    pTransMgr->endTransaction();
    ASSERT_NE(pTube, nullptr);

    TopoDS_Edge rimEdge;
    for (TopExp_Explorer exp(pTube->getShape(), TopAbs_ShapeEnum::TopAbs_EDGE); exp.More(); exp.Next())
    {
        const TopoDS_Edge& edge = TopoDS::Edge(exp.Current());
        TopoDS_Vertex v1, v2;
        TopExp::Vertices(edge, v1, v2);
        if (v1.IsNull() || v2.IsNull()) continue;
        if (std::abs(BRep_Tool::Pnt(v1).Z() - height) > 1e-9) continue;
        if (std::abs(BRep_Tool::Pnt(v2).Z() - height) > 1e-9) continue;
        rimEdge = edge;
        break;
    }
    ASSERT_FALSE(rimEdge.IsNull());

    // A rim edge is a closed edge running over its own seam: both ends are the same
    // vertex, which is what lets a single picked edge close a loop
    TopoDS_Vertex rimV1, rimV2;
    TopExp::Vertices(rimEdge, rimV1, rimV2);
    EXPECT_TRUE(rimV1.IsSame(rimV2));

    const std::vector<TopoDS_Edge> edges{ rimEdge };
    TopoDS_Wire wire;
    EXPECT_EQ(wy3d::ErrorCode::NoError, wy3d::TopoShapeUtil::makeWireFromEdges(edges, wire));
    EXPECT_FALSE(wire.IsNull());
    EXPECT_TRUE(wire.Closed());
}

TEST(TopoShapeUtil, MakeWireFromEdges_NullEdgeReportsInvalidData)
{
    // A null edge is degenerate input: rejected as invalid data instead of crashing
    const std::vector<TopoDS_Edge> edges{ TopoDS_Edge() };
    TopoDS_Wire wire;
    EXPECT_EQ(wy3d::ErrorCode::PLANARSHEET_InvalidData,
        wy3d::TopoShapeUtil::makeWireFromEdges(edges, wire));
    EXPECT_TRUE(wire.IsNull());
}

TEST(TopoShapeUtil, MakeWireFromEdges_BranchReportsNotClosed)
{
    const gp_Pnt center(0, 0, 0), p1(100, 0, 0), p2(0, 100, 0), p3(-100, 0, 0);
    const std::vector<TopoDS_Edge> branch{
        makeEdge(center, p1), makeEdge(center, p2), makeEdge(center, p3)
    };

    TopoDS_Wire wire;
    EXPECT_EQ(wy3d::ErrorCode::PLANARSHEET_EdgesNotClosed,
        wy3d::TopoShapeUtil::makeWireFromEdges(branch, wire));
    EXPECT_TRUE(wire.IsNull());
}

TEST(TopoShapeUtil, MakePlanarSheetFromEdges_EmptyInput)
{
    const std::vector<TopoDS_Edge> edges;

    TopoDS_Shape shape;
    EXPECT_EQ(wy3d::ErrorCode::warnTOPOSHAPE_NullShape,
        wy3d::TopoShapeUtil::makePlanarSheetFromEdges(edges, shape));
    EXPECT_TRUE(shape.IsNull());
}

TEST(TopoShapeUtil, MakeFilledFaceFromEdges_PlanarLoopTakesPlaneShortcut)
{
    std::vector<TopoDS_Edge> edges = makeRectangleEdges();

    TopoDS_Face face;
    EXPECT_EQ(wy3d::ErrorCode::NoError, wy3d::TopoShapeUtil::makeFilledFaceFromEdges(edges, face));
    ASSERT_FALSE(face.IsNull());
    EXPECT_TRUE(isPlaneSurface(face));
    EXPECT_NEAR(10000.0, faceArea(face), 1e-6);
}

TEST(TopoShapeUtil, MakeFilledFaceFromEdges_NonPlanarLoop)
{
    std::vector<TopoDS_Edge> edges = makeNonPlanarQuadEdges();

    // The four edges share no plane, which is what the planar builder reports. The verdict
    // comes from BRepLib_FindSurface (see the KernelProbe cases), and callers must treat
    // every non-NoError from the planar builder as "this set of edges has no common plane"
    TopoDS_Shape planarShape;
    EXPECT_EQ(wy3d::ErrorCode::PLANARSHEET_EdgesNotCoplanar,
        wy3d::TopoShapeUtil::makePlanarSheetFromEdges(edges, planarShape));
    EXPECT_TRUE(planarShape.IsNull());

    TopoDS_Face face;
    EXPECT_EQ(wy3d::ErrorCode::NoError, wy3d::TopoShapeUtil::makeFilledFaceFromEdges(edges, face));
    ASSERT_FALSE(face.IsNull());
    EXPECT_FALSE(isPlaneSurface(face));
    EXPECT_GT(faceArea(face), 0.0);

    // The bounding box only bounds the patch (Bnd_Box works on the plate surface's
    // control net and overshoots), so the boundary is checked at the corners
    double zMin(0), zMax(0);
    ASSERT_TRUE(faceBoundingBoxZ(face, zMin, zMax));
    EXPECT_LE(zMin, 1e-6);
    EXPECT_GE(zMax, 50.0 - 1e-6);

    for (const gp_Pnt& corner : makeNonPlanarQuadCorners())
    {
        EXPECT_TRUE(hasVertexAt(face, corner, 1e-6)) << "missing corner (" << corner.X()
            << ", " << corner.Y() << ", " << corner.Z() << ")";
    }
}

// The KernelProbe cases below measure what the bundled OCCT 7.7 does with the three calls the
// multi-loop planar sheet builder is planned around: BRepLib_FindSurface for the common plane,
// BRepClass_FaceClassifier for the loop nesting, and MakeFace + Add for the holes. Keep them as
// regression tests once the builder lands.

TEST(TopoShapeUtil, KernelProbe_FindSurface_PlaneAndNonPlanar)
{
    // Two loops in z=0, copied into a compound the way the builder will do it
    BRep_Builder builder;
    TopoDS_Compound coplanar;
    builder.MakeCompound(coplanar);
    for (const TopoDS_Edge& edge : makeSquareEdges(0.0, 0.0, 100.0, 100.0))
    {
        builder.Add(coplanar, BRepBuilderAPI_Copy(edge).Shape());
    }
    for (const TopoDS_Edge& edge : makeSquareEdges(20.0, 20.0, 60.0, 60.0))
    {
        builder.Add(coplanar, BRepBuilderAPI_Copy(edge).Shape());
    }

    BRepLib_FindSurface coplanarFinder(coplanar, kUseShapeTolerance, kOnlyPlane);
    std::cout << "[probe] coplanar: found=" << coplanarFinder.Found()
        << " tolerance=" << coplanarFinder.Tolerance() << std::endl;
    ASSERT_TRUE(coplanarFinder.Found());

    const gp_Pln plane = GeomAdaptor_Surface(coplanarFinder.Surface()).Plane();
    const gp_Dir normal = plane.Axis().Direction();
    EXPECT_NEAR(1.0, std::abs(normal.Z()), 1e-9);
    EXPECT_NEAR(0.0, plane.Distance(gp_Pnt(0.0, 0.0, 0.0)), 1e-9);

    // Same set with the inner loop lifted off the plane
    TopoDS_Compound nonCoplanar;
    builder.MakeCompound(nonCoplanar);
    for (const TopoDS_Edge& edge : makeSquareEdges(0.0, 0.0, 100.0, 100.0))
    {
        builder.Add(nonCoplanar, BRepBuilderAPI_Copy(edge).Shape());
    }
    for (const TopoDS_Edge& edge : makeSquareEdges(20.0, 20.0, 60.0, 60.0, 50.0))
    {
        builder.Add(nonCoplanar, BRepBuilderAPI_Copy(edge).Shape());
    }

    BRepLib_FindSurface nonCoplanarFinder(nonCoplanar, kUseShapeTolerance, kOnlyPlane);
    std::cout << "[probe] non-coplanar: found=" << nonCoplanarFinder.Found() << std::endl;
    EXPECT_FALSE(nonCoplanarFinder.Found());

    // A single loop that is twisted on its own
    TopoDS_Compound twisted;
    builder.MakeCompound(twisted);
    for (const TopoDS_Edge& edge : makeNonPlanarQuadEdges())
    {
        builder.Add(twisted, BRepBuilderAPI_Copy(edge).Shape());
    }

    BRepLib_FindSurface twistedFinder(twisted, kUseShapeTolerance, kOnlyPlane);
    std::cout << "[probe] twisted single loop: found=" << twistedFinder.Found() << std::endl;
    EXPECT_FALSE(twistedFinder.Found());
}

TEST(TopoShapeUtil, KernelProbe_FindSurface_AttachesSurfaceToEdges)
{
    // The edges are handed over as they are, without a copy: if FindSurface attaches the plane
    // to them, the second call reports Existed() and the copy in the builder is mandatory
    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    for (const TopoDS_Edge& edge : makeSquareEdges(0.0, 0.0, 100.0, 100.0))
    {
        builder.Add(compound, edge);
    }

    BRepLib_FindSurface first(compound, kUseShapeTolerance, kOnlyPlane);
    BRepLib_FindSurface second(compound, kUseShapeTolerance, kOnlyPlane);
    std::cout << "[probe] without copy: first found=" << first.Found() << " existed=" << first.Existed()
        << ", second found=" << second.Found() << " existed=" << second.Existed() << std::endl;
    EXPECT_TRUE(first.Found());
    EXPECT_TRUE(second.Found());
    // Measured on the bundled OCCT 7.7: FindSurface does not attach the surface to the edges it
    // was given, so the builder can hand over the picked edges themselves (FreeCAD copies, but
    // that is defensive). If the second call ever reports Existed() the lookup mutated the model
    // and the caller must copy
    EXPECT_FALSE(second.Existed());
}

TEST(TopoShapeUtil, KernelProbe_FaceClassifier_3DPointOnOurOwnFace)
{
    // The wire comes from the production builder, so this also says whether that wire is
    // usable by the classifier
    TopoDS_Wire wire;
    ASSERT_EQ(wy3d::ErrorCode::NoError,
        wy3d::TopoShapeUtil::makeWireFromEdges(makeSquareEdges(0.0, 0.0, 100.0, 100.0), wire));

    const gp_Pln plane(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0));
    BRepBuilderAPI_MakeFace makeFace(plane, wire);
    ASSERT_TRUE(makeFace.IsDone());
    const TopoDS_Face face = makeFace.Face();
    ASSERT_FALSE(face.IsNull());

    BRepClass_FaceClassifier inside(face, gp_Pnt(50.0, 50.0, 0.0), Precision::Confusion());
    BRepClass_FaceClassifier outside(face, gp_Pnt(150.0, 50.0, 0.0), Precision::Confusion());
    BRepClass_FaceClassifier boundary(face, gp_Pnt(50.0, 0.0, 0.0), Precision::Confusion());
    std::cout << "[probe] classifier: inside=" << stateName(inside.State())
        << " outside=" << stateName(outside.State())
        << " on-edge=" << stateName(boundary.State()) << std::endl;

    EXPECT_EQ(TopAbs_IN, inside.State());
    EXPECT_EQ(TopAbs_OUT, outside.State());
    EXPECT_EQ(TopAbs_ON, boundary.State());
}

TEST(TopoShapeUtil, KernelProbe_HoleOrientationAndGProp)
{
    const gp_Pln plane(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0));
    // Both wires are built in the order given: counter-clockwise outer, and a hole traversed
    // either way, which is what shows whether the winding is load bearing
    const TopoDS_Wire outer = makeWire(makeSquareEdges(0.0, 0.0, 100.0, 100.0));
    const TopoDS_Wire holeClockwise = makeWire(makeSquareEdgesClockwise(20.0, 20.0, 60.0, 60.0));
    const TopoDS_Wire holeCounterClockwise = makeWire(makeSquareEdges(20.0, 20.0, 60.0, 60.0));

    auto buildFace = [&plane, &outer](const TopoDS_Wire& hole) {
        BRepBuilderAPI_MakeFace makeFace(plane, outer);
        try
        {
            makeFace.Add(hole);
            makeFace.Build();
        }
        catch (const Standard_Failure& failure)
        {
            std::cout << "[probe] build threw: " << failure.GetMessageString() << std::endl;
            return TopoDS_Face();
        }
        return makeFace.Face();
    };

    const TopoDS_Face face = buildFace(holeClockwise);
    const TopoDS_Face sameSenseFace = buildFace(holeCounterClockwise);
    ASSERT_FALSE(face.IsNull());
    ASSERT_FALSE(sameSenseFace.IsNull());

    BRepClass_FaceClassifier holeCentre(face, gp_Pnt(40.0, 40.0, 0.0), Precision::Confusion());
    BRepClass_FaceClassifier ring(face, gp_Pnt(10.0, 50.0, 0.0), Precision::Confusion());
    BRepClass_FaceClassifier holeEdge(face, gp_Pnt(20.0, 40.0, 0.0), Precision::Confusion());
    BRepClass_FaceClassifier sameSenseHoleCentre(sameSenseFace, gp_Pnt(40.0, 40.0, 0.0), Precision::Confusion());

    std::cout << "[probe] cw-hole: wires=" << wireCount(face) << " area=" << faceArea(face)
        << " valid=" << BRepCheck_Analyzer(face).IsValid()
        << " | hole-centre=" << stateName(holeCentre.State())
        << " ring=" << stateName(ring.State())
        << " hole-edge=" << stateName(holeEdge.State()) << std::endl;
    std::cout << "[probe] ccw-hole: wires=" << wireCount(sameSenseFace) << " area=" << faceArea(sameSenseFace)
        << " valid=" << BRepCheck_Analyzer(sameSenseFace).IsValid()
        << " | hole-centre=" << stateName(sameSenseHoleCentre.State()) << std::endl;

    EXPECT_EQ(2, wireCount(face));
    EXPECT_EQ(TopAbs_OUT, holeCentre.State());
    EXPECT_EQ(TopAbs_IN, ring.State());
    EXPECT_EQ(TopAbs_ON, holeEdge.State());
    EXPECT_TRUE(BRepCheck_Analyzer(face).IsValid());
}

TEST(TopoShapeUtil, KernelProbe_FindSurface_ToleranceBoundary)
{
    // The verdict is a distance check against one tolerance, so this measures where it flips:
    // two squares in parallel planes a distance apart, the second one lifted off the first
    auto findSurface = [](const std::vector<TopoDS_Edge>& edges, double tol, double& tolerance,
                          double& reached) {
        BRep_Builder builder;
        TopoDS_Compound compound;
        builder.MakeCompound(compound);
        for (const TopoDS_Edge& edge : edges)
        {
            builder.Add(compound, edge);
        }
        BRepLib_FindSurface finder(compound, tol, kOnlyPlane);
        tolerance = finder.Tolerance();
        reached = finder.ToleranceReached();
        return finder.Found();
    };

    auto twoSquares = [](double offset) {
        std::vector<TopoDS_Edge> edges = makeSquareEdges(0.0, 0.0, 100.0, 100.0);
        const std::vector<TopoDS_Edge> lifted = makeSquareEdges(20.0, 20.0, 60.0, 60.0, offset);
        edges.insert(edges.end(), lifted.cbegin(), lifted.cend());
        return edges;
    };

    std::cout << "[probe] default tolerance (-1):" << std::endl;
    for (const double offset : { 0.0, 1.0e-8, 1.0e-7, 1.0e-6, 1.0e-5 })
    {
        double tolerance(0), reached(0);
        const bool found = findSurface(twoSquares(offset), -1.0, tolerance, reached);
        std::cout << "[probe]   offset=" << offset << " found=" << found
            << " tolerance=" << tolerance << " reached=" << reached << std::endl;
    }

    // An explicit tolerance is the whole criterion: the same edges flip with it alone
    double tolerance(0), reached(0);
    std::cout << "[probe] explicit tolerance: offset=1e-4 found(tol=1e-3)="
        << findSurface(twoSquares(1.0e-4), 1.0e-3, tolerance, reached)
        << " reached=" << reached
        << ", found(tol=1e-5)=" << findSurface(twoSquares(1.0e-4), 1.0e-5, tolerance, reached)
        << std::endl;
}

TEST(TopoShapeUtil, KernelProbe_FindSurface_OpenEdgeSelections)
{
    // The coplanarity of a selection has to be answerable before the loops close, so that a
    // half picked set of edges already reports the plane it can never reach
    auto sharePlane = [](const std::vector<TopoDS_Edge>& edges) {
        BRep_Builder builder;
        TopoDS_Compound compound;
        builder.MakeCompound(compound);
        for (const TopoDS_Edge& edge : edges)
        {
            builder.Add(compound, edge);
        }
        return BRepLib_FindSurface(compound, kUseShapeTolerance, kOnlyPlane).Found();
    };

    // Measured on the bundled OCCT 7.7: a lone edge defines no surface, Found is false for a
    // single straight edge (a line leaves the plane free to rotate about itself) and for a
    // single circle alike. Callers that ask "can these edges still reach a common plane" must
    // therefore leave a one edge selection unjudged instead of reading it as a failure
    const gp_Circ circle(gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), 50.0);
    std::cout << "[probe] single edge: line=" << sharePlane({ makeEdge(gp_Pnt(0, 0, 0), gp_Pnt(100, 0, 0)) })
        << " circle=" << sharePlane({ TopoDS::Edge(BRepBuilderAPI_MakeEdge(circle).Edge()) }) << std::endl;
    EXPECT_FALSE(sharePlane({ makeEdge(gp_Pnt(0, 0, 0), gp_Pnt(100, 0, 0)) }));

    // Two straight edges sharing a vertex span a plane, as any two intersecting lines do
    EXPECT_TRUE(sharePlane({ makeEdge(gp_Pnt(0, 0, 0), gp_Pnt(100, 0, 0)),
        makeEdge(gp_Pnt(0, 0, 0), gp_Pnt(0, 100, 0)) }));

    // Two skew straight edges lie in no common plane
    EXPECT_FALSE(sharePlane({ makeEdge(gp_Pnt(0, 0, 0), gp_Pnt(100, 0, 0)),
        makeEdge(gp_Pnt(0, 50, 50), gp_Pnt(100, 50, 60)) }));

    // Three of the four edges of the twisted quad: open, and already past saving
    std::vector<TopoDS_Edge> threeEdges = makeNonPlanarQuadEdges();
    threeEdges.pop_back();
    EXPECT_FALSE(sharePlane(threeEdges));
}

TEST(TopoShapeUtil, MakePlanarSheetFromEdges_SingleClosedEdge)
{
    // One closed edge is a complete loop on its own, as a solid's rim edge is when picked
    const gp_Circ circle(gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), 50.0);
    const std::vector<TopoDS_Edge> edges{ TopoDS::Edge(BRepBuilderAPI_MakeEdge(circle).Edge()) };

    TopoDS_Shape shape;
    ASSERT_EQ(wy3d::ErrorCode::NoError, wy3d::TopoShapeUtil::makePlanarSheetFromEdges(edges, shape));
    EXPECT_EQ(TopAbs_ShapeEnum::TopAbs_COMPOUND, shape.ShapeType());
    EXPECT_EQ(1, shapeShellCount(shape));

    const std::vector<TopoDS_Face> faces = shapeFaces(shape);
    ASSERT_EQ(1u, faces.size());
    EXPECT_EQ(1, wireCount(faces[0]));
    EXPECT_TRUE(BRepCheck_Analyzer(faces[0]).IsValid());
    EXPECT_NEAR(wy3d::PI * 2500.0, faceArea(faces[0]), 1.0);
}

TEST(TopoShapeUtil, MakePlanarSheetFromEdges_SquareWithSquareHole)
{
    std::vector<TopoDS_Edge> edges = makeSquareEdges(0.0, 0.0, 100.0, 100.0);
    const std::vector<TopoDS_Edge> hole = makeSquareEdges(20.0, 20.0, 40.0, 40.0);
    edges.insert(edges.end(), hole.cbegin(), hole.cend());

    TopoDS_Shape shape;
    ASSERT_EQ(wy3d::ErrorCode::NoError, wy3d::TopoShapeUtil::makePlanarSheetFromEdges(edges, shape));

    // Enclosing a loop turns it into a hole of the face around it, not into a second face
    EXPECT_EQ(1, shapeShellCount(shape));
    const std::vector<TopoDS_Face> faces = shapeFaces(shape);
    ASSERT_EQ(1u, faces.size());
    EXPECT_EQ(2, wireCount(faces[0]));
    EXPECT_TRUE(BRepCheck_Analyzer(faces[0]).IsValid());
    EXPECT_NEAR(9600.0, faceArea(faces[0]), 1e-6);
}

TEST(TopoShapeUtil, MakePlanarSheetFromEdges_HoleEdgesShuffledAndReversed)
{
    // The hole arrives shuffled and running the other way round: the builder orders the loops
    // and normalises their winding, so the face comes out the same
    std::vector<TopoDS_Edge> edges = makeSquareEdges(0.0, 0.0, 100.0, 100.0);
    const std::vector<TopoDS_Edge> hole = makeSquareEdgesClockwise(20.0, 20.0, 40.0, 40.0);
    edges.emplace_back(TopoDS::Edge(hole[1].Reversed()));
    edges.emplace_back(hole[3]);
    edges.emplace_back(hole[0]);
    edges.emplace_back(TopoDS::Edge(hole[2].Reversed()));

    TopoDS_Shape shape;
    ASSERT_EQ(wy3d::ErrorCode::NoError, wy3d::TopoShapeUtil::makePlanarSheetFromEdges(edges, shape));

    const std::vector<TopoDS_Face> faces = shapeFaces(shape);
    ASSERT_EQ(1u, faces.size());
    EXPECT_EQ(2, wireCount(faces[0]));
    EXPECT_TRUE(BRepCheck_Analyzer(faces[0]).IsValid());
    EXPECT_NEAR(9600.0, faceArea(faces[0]), 1e-6);
}

TEST(TopoShapeUtil, MakePlanarSheetFromEdges_TwoDisjointSquares)
{
    std::vector<TopoDS_Edge> edges = makeSquareEdges(0.0, 0.0, 100.0, 100.0);
    const std::vector<TopoDS_Edge> second = makeSquareEdges(200.0, 0.0, 300.0, 100.0);
    edges.insert(edges.end(), second.cbegin(), second.cend());

    TopoDS_Shape shape;
    ASSERT_EQ(wy3d::ErrorCode::NoError, wy3d::TopoShapeUtil::makePlanarSheetFromEdges(edges, shape));

    // Loops that do not enclose one another end up as separate faces of the same element
    EXPECT_EQ(2, shapeShellCount(shape));
    EXPECT_EQ(2u, shapeFaces(shape).size());
    EXPECT_NEAR(20000.0, totalArea(shape), 1e-6);
}

TEST(TopoShapeUtil, MakePlanarSheetFromEdges_IslandInsideHole)
{
    // Outer 300, hole 200, island 100: the island is material again, so it becomes a face of
    // its own instead of a second hole of the outer face (the point FaceMakerCheese misses)
    std::vector<TopoDS_Edge> edges = makeSquareEdges(0.0, 0.0, 300.0, 300.0);
    const std::vector<TopoDS_Edge> hole = makeSquareEdges(50.0, 50.0, 250.0, 250.0);
    const std::vector<TopoDS_Edge> island = makeSquareEdges(100.0, 100.0, 200.0, 200.0);
    edges.insert(edges.end(), hole.cbegin(), hole.cend());
    edges.insert(edges.end(), island.cbegin(), island.cend());

    TopoDS_Shape shape;
    ASSERT_EQ(wy3d::ErrorCode::NoError, wy3d::TopoShapeUtil::makePlanarSheetFromEdges(edges, shape));

    const std::vector<TopoDS_Face> faces = shapeFaces(shape);
    ASSERT_EQ(2u, faces.size());
    EXPECT_EQ(3, wireCount(shape));
    EXPECT_NEAR(50000.0 + 10000.0, totalArea(shape), 1e-6);
    for (const TopoDS_Face& face : faces)
    {
        EXPECT_TRUE(BRepCheck_Analyzer(face).IsValid());
    }
}

TEST(TopoShapeUtil, MakePlanarSheetFromEdges_FourLevelNesting)
{
    // Nesting alternates all the way down, so the island carries a hole of its own
    std::vector<TopoDS_Edge> edges = makeSquareEdges(0.0, 0.0, 300.0, 300.0);
    const std::vector<TopoDS_Edge> hole = makeSquareEdges(50.0, 50.0, 250.0, 250.0);
    const std::vector<TopoDS_Edge> island = makeSquareEdges(100.0, 100.0, 200.0, 200.0);
    const std::vector<TopoDS_Edge> islandHole = makeSquareEdges(125.0, 125.0, 175.0, 175.0);
    edges.insert(edges.end(), hole.cbegin(), hole.cend());
    edges.insert(edges.end(), island.cbegin(), island.cend());
    edges.insert(edges.end(), islandHole.cbegin(), islandHole.cend());

    TopoDS_Shape shape;
    ASSERT_EQ(wy3d::ErrorCode::NoError, wy3d::TopoShapeUtil::makePlanarSheetFromEdges(edges, shape));

    EXPECT_EQ(2, shapeShellCount(shape));
    EXPECT_EQ(4, wireCount(shape));
    EXPECT_NEAR(50000.0 + (10000.0 - 2500.0), totalArea(shape), 1e-6);
}

TEST(TopoShapeUtil, MakePlanarSheetFromEdges_CrossingLoops)
{
    // The rectangles cross: part of the second one lies inside the first
    std::vector<TopoDS_Edge> edges = makeSquareEdges(0.0, 0.0, 100.0, 100.0);
    const std::vector<TopoDS_Edge> crossing = makeSquareEdges(50.0, -50.0, 150.0, 50.0);
    edges.insert(edges.end(), crossing.cbegin(), crossing.cend());

    TopoDS_Shape shape;
    EXPECT_EQ(wy3d::ErrorCode::PLANARSHEET_LoopsNotNested,
        wy3d::TopoShapeUtil::makePlanarSheetFromEdges(edges, shape));
    EXPECT_TRUE(shape.IsNull());
}

TEST(TopoShapeUtil, MakePlanarSheetFromEdges_TouchingLoops)
{
    // The triangle runs along the lower edge of the square, sharing no vertex with it
    std::vector<TopoDS_Edge> edges = makeSquareEdges(0.0, 0.0, 100.0, 100.0);
    edges.emplace_back(makeEdge(gp_Pnt(25.0, 0.0, 0.0), gp_Pnt(75.0, 0.0, 0.0)));
    edges.emplace_back(makeEdge(gp_Pnt(75.0, 0.0, 0.0), gp_Pnt(50.0, -50.0, 0.0)));
    edges.emplace_back(makeEdge(gp_Pnt(50.0, -50.0, 0.0), gp_Pnt(25.0, 0.0, 0.0)));

    TopoDS_Shape shape;
    EXPECT_EQ(wy3d::ErrorCode::PLANARSHEET_LoopsNotNested,
        wy3d::TopoShapeUtil::makePlanarSheetFromEdges(edges, shape));
    EXPECT_TRUE(shape.IsNull());
}

TEST(TopoShapeUtil, MakePlanarSheetFromEdges_NonCoplanarSecondLoop)
{
    std::vector<TopoDS_Edge> edges = makeSquareEdges(0.0, 0.0, 100.0, 100.0);
    const std::vector<TopoDS_Edge> lifted = makeSquareEdges(20.0, 20.0, 40.0, 40.0, 50.0);
    edges.insert(edges.end(), lifted.cbegin(), lifted.cend());

    TopoDS_Shape shape;
    EXPECT_EQ(wy3d::ErrorCode::PLANARSHEET_EdgesNotCoplanar,
        wy3d::TopoShapeUtil::makePlanarSheetFromEdges(edges, shape));
    EXPECT_TRUE(shape.IsNull());
}

TEST(TopoShapeUtil, MakePlanarSheetFromEdges_CircularHole)
{
    std::vector<TopoDS_Edge> edges = makeSquareEdges(0.0, 0.0, 200.0, 200.0);
    const gp_Circ circle(gp_Ax2(gp_Pnt(100.0, 100.0, 0.0), gp_Dir(0.0, 0.0, 1.0)), 50.0);
    edges.emplace_back(TopoDS::Edge(BRepBuilderAPI_MakeEdge(circle).Edge()));

    TopoDS_Shape shape;
    ASSERT_EQ(wy3d::ErrorCode::NoError, wy3d::TopoShapeUtil::makePlanarSheetFromEdges(edges, shape));

    const std::vector<TopoDS_Face> faces = shapeFaces(shape);
    ASSERT_EQ(1u, faces.size());
    EXPECT_EQ(2, wireCount(faces[0]));
    EXPECT_TRUE(BRepCheck_Analyzer(faces[0]).IsValid());
    EXPECT_NEAR(40000.0 - wy3d::PI * 2500.0, faceArea(faces[0]), 1.0);
}

// Filled sheet from picked edges: a compound holding one shell with one face, whether the loop
// has a plane of its own or not. Several loops only get there by sharing a plane and enclosing
// a single region - a second region is a second face, which is the planar sheet's shape, not
// this one's. Two loops can never go into one BRepFill_Filling anyway (measured: five of six
// arrangements die with an access violation inside Build)

TEST(TopoShapeUtil, MakeFilledSheetFromEdges_EmptyInput)
{
    const std::vector<TopoDS_Edge> edges;

    TopoDS_Shape shape;
    EXPECT_EQ(wy3d::ErrorCode::warnTOPOSHAPE_NullShape,
        wy3d::TopoShapeUtil::makeFilledSheetFromEdges(edges, shape));
    EXPECT_TRUE(shape.IsNull());
}

TEST(TopoShapeUtil, MakeFilledSheetFromEdges_OpenChain)
{
    std::vector<TopoDS_Edge> edges = makeRectangleEdges();
    edges.pop_back();

    TopoDS_Shape shape;
    EXPECT_EQ(wy3d::ErrorCode::PLANARSHEET_EdgesNotClosed,
        wy3d::TopoShapeUtil::makeFilledSheetFromEdges(edges, shape));
    EXPECT_TRUE(shape.IsNull());
}

TEST(TopoShapeUtil, MakeFilledSheetFromEdges_SingleLoopIsCompoundOfOneShell)
{
    // The shape a filled sheet arrives in, on every path: a compound holding one shell per
    // face, never a bare face
    const std::vector<TopoDS_Edge> edges = makeRectangleEdges();

    TopoDS_Shape shape;
    ASSERT_EQ(wy3d::ErrorCode::NoError, wy3d::TopoShapeUtil::makeFilledSheetFromEdges(edges, shape));
    EXPECT_EQ(TopAbs_COMPOUND, shape.ShapeType());
    EXPECT_EQ(1, shapeShellCount(shape));

    const std::vector<TopoDS_Face> faces = shapeFaces(shape);
    ASSERT_EQ(1u, faces.size());
    EXPECT_TRUE(isPlaneSurface(faces[0]));
    EXPECT_TRUE(BRepCheck_Analyzer(faces[0]).IsValid());
    EXPECT_NEAR(10000.0, totalArea(shape), 1e-6);
}

TEST(TopoShapeUtil, MakeFilledSheetFromEdges_SingleClosedEdge)
{
    // A full circle is a closed loop on its own
    const gp_Circ circle(gp_Ax2(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0)), 50.0);
    const std::vector<TopoDS_Edge> edges{ TopoDS::Edge(BRepBuilderAPI_MakeEdge(circle).Edge()) };

    TopoDS_Shape shape;
    ASSERT_EQ(wy3d::ErrorCode::NoError, wy3d::TopoShapeUtil::makeFilledSheetFromEdges(edges, shape));
    EXPECT_EQ(1, shapeShellCount(shape));
    EXPECT_NEAR(wy3d::PI * 2500.0, totalArea(shape), 1e-6);
}

TEST(TopoShapeUtil, MakeFilledSheetFromEdges_SingleNonPlanarLoop)
{
    // Four edges with no common plane: the filler's own case, unchanged by the multi loop work
    const std::vector<TopoDS_Edge> edges = makeNonPlanarQuadEdges();

    TopoDS_Shape shape;
    ASSERT_EQ(wy3d::ErrorCode::NoError, wy3d::TopoShapeUtil::makeFilledSheetFromEdges(edges, shape));
    EXPECT_EQ(1, shapeShellCount(shape));

    const std::vector<TopoDS_Face> faces = shapeFaces(shape);
    ASSERT_EQ(1u, faces.size());
    EXPECT_FALSE(isPlaneSurface(faces[0]));
    EXPECT_GT(faceArea(faces[0]), 0.0);
    for (const gp_Pnt& corner : makeNonPlanarQuadCorners())
    {
        EXPECT_TRUE(hasVertexAt(faces[0], corner, 1e-6));
    }
}

TEST(TopoShapeUtil, MakeFilledSheetFromEdges_CoplanarLoopsUsePlanarHoleRules)
{
    // Both loops lie in z=0, so this is a planar sheet and the inner loop is its hole
    std::vector<TopoDS_Edge> edges = makeSquareEdges(0.0, 0.0, 100.0, 100.0);
    const std::vector<TopoDS_Edge> inner = makeSquareEdges(20.0, 20.0, 40.0, 40.0);
    edges.insert(edges.end(), inner.cbegin(), inner.cend());

    TopoDS_Shape shape;
    ASSERT_EQ(wy3d::ErrorCode::NoError, wy3d::TopoShapeUtil::makeFilledSheetFromEdges(edges, shape));
    EXPECT_EQ(1, shapeShellCount(shape));

    const std::vector<TopoDS_Face> faces = shapeFaces(shape);
    ASSERT_EQ(1u, faces.size());
    EXPECT_EQ(2, wireCount(faces[0]));
    EXPECT_NEAR(10000.0 - 400.0, totalArea(shape), 1e-6);
}

TEST(TopoShapeUtil, MakeFilledSheetFromEdges_CoplanarDisjointRegionsRefused)
{
    // Two squares side by side in z=0: coplanar, so the planar path takes them, but they are two
    // regions - and this entry point answers with one face
    std::vector<TopoDS_Edge> edges = makeSquareEdges(0.0, 0.0, 100.0, 100.0);
    const std::vector<TopoDS_Edge> beside = makeSquareEdges(200.0, 0.0, 300.0, 100.0);
    edges.insert(edges.end(), beside.cbegin(), beside.cend());

    TopoDS_Shape shape;
    EXPECT_EQ(wy3d::ErrorCode::FILLEDSHEET_EdgesNotSingleRegion,
        wy3d::TopoShapeUtil::makeFilledSheetFromEdges(edges, shape));
    EXPECT_TRUE(shape.IsNull());
}

TEST(TopoShapeUtil, MakeFilledSheetFromEdges_CoplanarOuterWithTwoHolesAllowed)
{
    // One outer loop with two holes is still a single region, so it is still a single face
    std::vector<TopoDS_Edge> edges = makeSquareEdges(0.0, 0.0, 100.0, 100.0);
    const std::vector<TopoDS_Edge> firstHole = makeSquareEdges(10.0, 10.0, 30.0, 30.0);
    const std::vector<TopoDS_Edge> secondHole = makeSquareEdges(60.0, 60.0, 90.0, 90.0);
    edges.insert(edges.end(), firstHole.cbegin(), firstHole.cend());
    edges.insert(edges.end(), secondHole.cbegin(), secondHole.cend());

    TopoDS_Shape shape;
    ASSERT_EQ(wy3d::ErrorCode::NoError, wy3d::TopoShapeUtil::makeFilledSheetFromEdges(edges, shape));
    EXPECT_EQ(1, shapeShellCount(shape));

    const std::vector<TopoDS_Face> faces = shapeFaces(shape);
    ASSERT_EQ(1u, faces.size());
    EXPECT_EQ(3, wireCount(faces[0]));
    EXPECT_NEAR(10000.0 - 400.0 - 900.0, totalArea(shape), 1e-6);
}

TEST(TopoShapeUtil, MakeFilledSheetFromEdges_CoplanarIslandInsideHoleRefused)
{
    // An outer loop, a hole in it and a loop inside that hole: the planar rules fill every even
    // nesting depth, so the island is a region of its own - two faces' worth of input
    std::vector<TopoDS_Edge> edges = makeSquareEdges(0.0, 0.0, 100.0, 100.0);
    const std::vector<TopoDS_Edge> hole = makeSquareEdges(20.0, 20.0, 80.0, 80.0);
    const std::vector<TopoDS_Edge> island = makeSquareEdges(40.0, 40.0, 60.0, 60.0);
    edges.insert(edges.end(), hole.cbegin(), hole.cend());
    edges.insert(edges.end(), island.cbegin(), island.cend());

    TopoDS_Shape shape;
    EXPECT_EQ(wy3d::ErrorCode::FILLEDSHEET_EdgesNotSingleRegion,
        wy3d::TopoShapeUtil::makeFilledSheetFromEdges(edges, shape));
    EXPECT_TRUE(shape.IsNull());
}

TEST(TopoShapeUtil, MakeFilledSheetFromEdges_CoplanarCrossingLoopsRefused)
{
    std::vector<TopoDS_Edge> edges = makeSquareEdges(0.0, 0.0, 100.0, 100.0);
    const std::vector<TopoDS_Edge> crossing = makeSquareEdges(50.0, -50.0, 150.0, 50.0);
    edges.insert(edges.end(), crossing.cbegin(), crossing.cend());

    TopoDS_Shape shape;
    EXPECT_EQ(wy3d::ErrorCode::PLANARSHEET_LoopsNotNested,
        wy3d::TopoShapeUtil::makeFilledSheetFromEdges(edges, shape));
    EXPECT_TRUE(shape.IsNull());
}

TEST(TopoShapeUtil, MakeFilledSheetFromEdges_LoopsOnDifferentPlanesRefused)
{
    // A small rectangle raised above a large one: the two loops share no plane, and the planar
    // path answers with its coplanarity verdict before anything is filled. Keeping this input
    // away from the filler is also what keeps the process alive (see the note above)
    std::vector<TopoDS_Edge> edges = makeSquareEdges(0.0, 0.0, 100.0, 100.0);
    const std::vector<TopoDS_Edge> lifted = makeSquareEdges(20.0, 20.0, 40.0, 40.0, 10.0);
    edges.insert(edges.end(), lifted.cbegin(), lifted.cend());

    TopoDS_Shape shape;
    EXPECT_EQ(wy3d::ErrorCode::PLANARSHEET_EdgesNotCoplanar,
        wy3d::TopoShapeUtil::makeFilledSheetFromEdges(edges, shape));
    EXPECT_TRUE(shape.IsNull());
}

TEST(TopoShapeUtil, MakeFilledSheetFromEdges_StackedCoincidentSquaresRefused)
{
    // Two squares of the same size, one above the other: not coplanar either, and how the two sit
    // over each other makes no difference to that
    std::vector<TopoDS_Edge> edges = makeSquareEdges(0.0, 0.0, 100.0, 100.0);
    const std::vector<TopoDS_Edge> lifted = makeSquareEdges(0.0, 0.0, 100.0, 100.0, 10.0);
    edges.insert(edges.end(), lifted.cbegin(), lifted.cend());

    TopoDS_Shape shape;
    EXPECT_EQ(wy3d::ErrorCode::PLANARSHEET_EdgesNotCoplanar,
        wy3d::TopoShapeUtil::makeFilledSheetFromEdges(edges, shape));
    EXPECT_TRUE(shape.IsNull());
}

TEST(TopoShapeUtil, MakeFilledSheetFromEdges_LoopsOnParallelPlanesRefused)
{
    // Parallel planes, apart in xy: one execution would have to make two faces, which is what
    // this entry point does not do - the second loop belongs to an execution of its own
    std::vector<TopoDS_Edge> edges = makeSquareEdges(0.0, 0.0, 100.0, 100.0);
    const std::vector<TopoDS_Edge> lifted = makeSquareEdges(200.0, 0.0, 300.0, 100.0, 50.0);
    edges.insert(edges.end(), lifted.cbegin(), lifted.cend());

    TopoDS_Shape shape;
    EXPECT_EQ(wy3d::ErrorCode::PLANARSHEET_EdgesNotCoplanar,
        wy3d::TopoShapeUtil::makeFilledSheetFromEdges(edges, shape));
    EXPECT_TRUE(shape.IsNull());
}

TEST(TopoShapeUtil, MakeFilledSheetFromEdges_WarpedLoopWithPlanarLoopRefused)
{
    // A warped loop beside a planar one: whichever way the two sit, they share no plane, so the
    // warped loop cannot be a second loop - it is refused over the square and clear of it alike,
    // and refused before anything is filled
    const std::vector<gp_Pnt> overSquare{ gp_Pnt(20.0, 20.0, 10.0), gp_Pnt(40.0, 20.0, 10.0),
        gp_Pnt(40.0, 40.0, 30.0), gp_Pnt(20.0, 40.0, 10.0) };
    const std::vector<gp_Pnt> clear{ gp_Pnt(0.0, 500.0, 0.0), gp_Pnt(100.0, 500.0, 0.0),
        gp_Pnt(100.0, 600.0, 50.0), gp_Pnt(0.0, 600.0, 0.0) };

    for (const std::vector<gp_Pnt>& corners : { overSquare, clear })
    {
        std::vector<TopoDS_Edge> edges = makeSquareEdges(0.0, 0.0, 100.0, 100.0);
        for (size_t i = 0; i < corners.size(); ++i)
        {
            edges.emplace_back(makeEdge(corners[i], corners[(i + 1) % corners.size()]));
        }

        TopoDS_Shape shape;
        EXPECT_EQ(wy3d::ErrorCode::PLANARSHEET_EdgesNotCoplanar,
            wy3d::TopoShapeUtil::makeFilledSheetFromEdges(edges, shape));
        EXPECT_TRUE(shape.IsNull());
    }
}
