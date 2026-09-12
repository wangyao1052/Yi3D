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

#include "wy3d/topo/TopoShapeUtil.h"

#include <vector>

namespace
{
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

TEST(TopoShapeUtil, MakePlanarFaceFromEdges_ExactPlane)
{
    std::vector<TopoDS_Edge> edges = makeRectangleEdges();

    TopoDS_Face face;
    EXPECT_EQ(wy3d::ErrorCode::NoError, wy3d::TopoShapeUtil::makePlanarFaceFromEdges(edges, face));
    ASSERT_FALSE(face.IsNull());
    EXPECT_TRUE(isPlaneSurface(face));

    wy3d::SketchPlane plane;
    ASSERT_TRUE(wy3d::TopoShapeUtil::getFacePlane(face, plane));
    EXPECT_NEAR(1.0, std::abs(plane.getNormal().z()), 1e-9);
    EXPECT_NEAR(0.0, plane.getOrigin().z(), 1e-9);
    EXPECT_NEAR(10000.0, faceArea(face), 1e-6);
}

TEST(TopoShapeUtil, MakePlanarFaceFromEdges_OpenChain)
{
    const gp_Pnt p0(0, 0, 0), p1(100, 0, 0), p2(100, 100, 0), p3(0, 100, 0);
    const std::vector<TopoDS_Edge> openChain{ makeEdge(p0, p1), makeEdge(p1, p2), makeEdge(p2, p3) };

    TopoDS_Face face;
    EXPECT_EQ(wy3d::ErrorCode::PLANARSHEET_EdgesNotClosed,
        wy3d::TopoShapeUtil::makePlanarFaceFromEdges(openChain, face));
    EXPECT_TRUE(face.IsNull());
}

TEST(TopoShapeUtil, MakePlanarFaceFromEdges_ClosedLoopPlusStrayEdge)
{
    const gp_Pnt p0(0, 0, 0), p1(100, 0, 0), p2(0, 100, 0), s0(200, 200, 0), s1(300, 200, 0);
    const std::vector<TopoDS_Edge> edges{
        makeEdge(p0, p1), makeEdge(p1, p2), makeEdge(p2, p0), makeEdge(s0, s1)
    };

    TopoDS_Face face;
    EXPECT_EQ(wy3d::ErrorCode::PLANARSHEET_EdgesNotClosed,
        wy3d::TopoShapeUtil::makePlanarFaceFromEdges(edges, face));
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

TEST(TopoShapeUtil, MakePlanarFaceFromEdges_EmptyInput)
{
    const std::vector<TopoDS_Edge> edges;

    TopoDS_Face face;
    EXPECT_EQ(wy3d::ErrorCode::warnTOPOSHAPE_NullShape,
        wy3d::TopoShapeUtil::makePlanarFaceFromEdges(edges, face));
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

    // Measured on the bundled OCCT 7.7: the plane shortcut does not throw for this
    // wire, it yields a null face and the coplanarity code. Other geometries may throw
    // instead (then PLANARSHEET_InvalidData surfaces), so callers must treat every
    // non-NoError as "the loop is not planar"
    TopoDS_Face planarFace;
    EXPECT_EQ(wy3d::ErrorCode::PLANARSHEET_EdgesNotCoplanar,
        wy3d::TopoShapeUtil::makePlanarFaceFromEdges(edges, planarFace));
    EXPECT_TRUE(planarFace.IsNull());

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
