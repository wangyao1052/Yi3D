///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2024-2026 Wang Yao <wangyao1052@163.com>
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

#include <algorithm>
#include <climits>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include <wy3dBox.h>
#include <wy3dChamfer.h>
#include <wy3dExtrudedSheet.h>
#include <wy3dFilledSheet.h>
#include <wy3dNonParametricSheet.h>
#include <wy3dSheet.h>
#include <wy3dSketch.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchPlane.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>
#include <wy3dMath.h>

#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Shell.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <BRepGProp.hxx>
#include <BRep_Builder.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <GProp_GProps.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

// A chamfer may sit on a sheet as well as on a solid: the host is data, not type. What a sheet
// adds to the story is the open boundary - an edge with a single adjacent face has no dihedral to
// chamfer, and OCCT neither rejects nor reports such an edge once another usable one is in the
// builder, so Chamfer::modifyOwnerShape checks every edge before the algorithm sees the shape.
//
// A 100 x 50 rectangle extruded 10 deep gives the four walls, no caps, 3000 of area (300
// perimeter * 10). Chamfering one vertical edge at distance 2 takes 2 * 2 * 10 off the two walls
// and lays a strip of 2 * sqrt(2) * 10 between them: 2988.28, one face more than before.
//
// The sheets go through expectSheetTopoNamed rather than expectAllTopoNamed, which pins the one
// naming invariant a chamfered sheet breaks. See that helper for what and why.

namespace
{
    static std::uint32_t getChainErrorCode(wy3d::Database* pDb, const wydb::ElementId& id)
    {
        return wy3d::getErrorCodeFromChainUpdateFeedback(
            pDb->getTransactionManager()->getChainUpdateFeedback(id).get());
    }

    static int countFaces(const TopoDS_Shape& shape)
    {
        int n(0);
        for (TopExp_Explorer exp(shape, TopAbs_FACE); exp.More(); exp.Next())
        {
            ++n;
        }
        return n;
    }

    static int countShells(const TopoDS_Shape& shape)
    {
        int n(0);
        for (TopExp_Explorer exp(shape, TopAbs_SHELL); exp.More(); exp.Next())
        {
            ++n;
        }
        return n;
    }

    static int countSolids(const TopoDS_Shape& shape)
    {
        int n(0);
        for (TopExp_Explorer exp(shape, TopAbs_SOLID); exp.More(); exp.Next())
        {
            ++n;
        }
        return n;
    }

    static double bodyArea(const TopoDS_Shape& shape)
    {
        GProp_GProps gprops;
        BRepGProp::SurfaceProperties(shape, gprops);
        return gprops.Mass();
    }

    // Every face/edge of the built shape must resolve a topo name, or picking cannot find it
    template<typename Body>
    static void expectEverySubShapeNamed(const Body* pBody)
    {
        const wy3d::TopoNaming* pTopoNaming = pBody->getTopoNaming();
        ASSERT_NE(pTopoNaming, nullptr);
        const TopoDS_Shape& shape = pBody->getShape();

        TopTools_IndexedMapOfShape faces;
        TopExp::MapShapes(shape, TopAbs_ShapeEnum::TopAbs_FACE, faces);
        EXPECT_GT(faces.Extent(), 0);
        for (int i = 1; i <= faces.Extent(); ++i)
        {
            EXPECT_FALSE(pTopoNaming->getTopoName(faces(i)).empty()) << "face " << i;
        }

        TopTools_IndexedMapOfShape edges;
        TopExp::MapShapes(shape, TopAbs_ShapeEnum::TopAbs_EDGE, edges);
        EXPECT_GT(edges.Extent(), 0);
        for (int i = 1; i <= edges.Extent(); ++i)
        {
            EXPECT_FALSE(pTopoNaming->getTopoName(edges(i)).empty()) << "edge " << i;
        }
    }

    // Names that are complete and unique - what TopoNaming::check() covers
    template<typename Body>
    static void expectAllTopoNamed(const Body* pBody)
    {
        expectEverySubShapeNamed(pBody);

        const wy3d::TopoNaming* pTopoNaming = pBody->getTopoNaming();
        ASSERT_NE(pTopoNaming, nullptr);
        std::vector<std::string> info;
        EXPECT_TRUE(pTopoNaming->check(pBody->getShape(), info))
            << (info.empty() ? "" : info[0]);
    }

    // Index, as the body's shape returns them, of a face/edge with the given number of adjacent
    // faces. An edge with two of them is the only kind a chamfer can take.
    static std::uint32_t findEdgeIndexWithFaces(const TopoDS_Shape& shape, int adjacentFaces)
    {
        TopTools_IndexedDataMapOfShapeListOfShape edgeFaceMap;
        TopExp::MapShapesAndAncestors(shape, TopAbs_EDGE, TopAbs_FACE, edgeFaceMap);
        for (int i = 1; i <= edgeFaceMap.Extent(); ++i)
        {
            if (adjacentFaces == edgeFaceMap.FindFromIndex(i).Extent())
            {
                return static_cast<std::uint32_t>(i - 1);
            }
        }
        return UINT_MAX;
    }

    // The first howMany of them, for the cases that chamfer more than one edge at a time
    static std::vector<std::uint32_t> findEdgeIndicesWithFaces(
        const TopoDS_Shape& shape, int adjacentFaces, unsigned int howMany)
    {
        std::vector<std::uint32_t> indices;
        TopTools_IndexedDataMapOfShapeListOfShape edgeFaceMap;
        TopExp::MapShapesAndAncestors(shape, TopAbs_EDGE, TopAbs_FACE, edgeFaceMap);
        for (int i = 1; i <= edgeFaceMap.Extent() && indices.size() < howMany; ++i)
        {
            if (adjacentFaces == edgeFaceMap.FindFromIndex(i).Extent())
            {
                indices.emplace_back(static_cast<std::uint32_t>(i - 1));
            }
        }
        return indices;
    }

    // Create a 2D sketch on the plane z = 0 with a 100x50 closed rectangle
    static wydb::ElementId createRectSketch(wy3d::Database* pDb)
    {
        wydb::ElementId sketchId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::SketchPlane plane(wy::Vector3(0.0, 0.0, 0.0), wy::Vector3::kZAxis, wy::Vector3::kXAxis);
            wy3d::Sketch* pSketch(nullptr);
            EXPECT_EQ(wy3d::Sketch::create(pTrans, plane, pSketch), wy::ErrorStatus::Ok);
            if (!pSketch)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }

            wy3d::SketchLine* pLines[4] = { nullptr, nullptr, nullptr, nullptr };
            EXPECT_EQ(wy3d::SketchLine::create(pTrans, wy::Vector2(0.0, 0.0), wy::Vector2(100.0, 0.0), pLines[0]), wy::ErrorStatus::Ok);
            EXPECT_EQ(wy3d::SketchLine::create(pTrans, wy::Vector2(100.0, 0.0), wy::Vector2(100.0, 50.0), pLines[1]), wy::ErrorStatus::Ok);
            EXPECT_EQ(wy3d::SketchLine::create(pTrans, wy::Vector2(100.0, 50.0), wy::Vector2(0.0, 50.0), pLines[2]), wy::ErrorStatus::Ok);
            EXPECT_EQ(wy3d::SketchLine::create(pTrans, wy::Vector2(0.0, 50.0), wy::Vector2(0.0, 0.0), pLines[3]), wy::ErrorStatus::Ok);
            for (wy3d::SketchLine* pLine : pLines)
            {
                if (!pLine)
                {
                    pDb->getTransactionManager()->abortTransaction();
                    return wydb::ElementId::kNull;
                }
                EXPECT_EQ(pSketch->addEntity(pLine), wy::ErrorStatus::Ok);
            }

            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
            sketchId = pSketch->getId();
        }
        return sketchId;
    }

    // The rectangle extruded depth deep along +Z: four walls, no caps
    static wydb::ElementId createExtrudedSheet(wy3d::Database* pDb, const wydb::ElementId& sketchId, double depth)
    {
        wydb::ElementId sheetId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
            if (!pSketch)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }

            wy3d::ExtrudedSheet* pSheet(nullptr);
            EXPECT_EQ(wy3d::ExtrudedSheet::create(pTrans, pSketch, depth, pSheet), wy::ErrorStatus::Ok);
            if (!pSheet)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }
            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
            sheetId = pSheet->getId();
        }
        return sheetId;
    }

    // A planar sheet of one face at z = 0 covering 100 x 50: every one of its edges is a boundary
    static wydb::ElementId createFilledSheet(wy3d::Database* pDb, const wydb::ElementId& sketchId)
    {
        wydb::ElementId sheetId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
            if (!pSketch)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }

            wy3d::FilledSheet* pSheet(nullptr);
            EXPECT_EQ(wy3d::FilledSheet::create(pTrans, pSketch, pSheet), wy::ErrorStatus::Ok);
            if (!pSheet)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }
            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
            sheetId = pSheet->getId();
        }
        return sheetId;
    }

    // A sheet element holding a shape of any kind, so a bare shell can host a chamfer without
    // going through one of the sheet builders
    static wydb::ElementId createNonParametricSheet(wy3d::Database* pDb, const TopoDS_Shape& shape)
    {
        wydb::ElementId sheetId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::NonParametricSheet* pSheet(nullptr);
            EXPECT_EQ(wy3d::NonParametricSheet::create(pTrans, shape, pSheet), wy::ErrorStatus::Ok);
            if (!pSheet)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }
            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
            sheetId = pSheet->getId();
        }
        return sheetId;
    }

    // The four walls of the rectangle, as a bare open shell: what ExtrudedSheet wraps in a compound
    static TopoDS_Shape openShell()
    {
        BRepBuilderAPI_MakePolygon polygon;
        polygon.Add(gp_Pnt(0.0, 0.0, 0.0));
        polygon.Add(gp_Pnt(100.0, 0.0, 0.0));
        polygon.Add(gp_Pnt(100.0, 50.0, 0.0));
        polygon.Add(gp_Pnt(0.0, 50.0, 0.0));
        polygon.Close();
        return BRepPrimAPI_MakePrism(polygon.Wire(), gp_Vec(0.0, 0.0, 10.0)).Shape();
    }

    // Two faces meeting along one geometric edge, each closed in a shell of its own: the form a
    // sheet built face by face arrives in, and the one that has no interior edge at all, the two
    // edges along the seam each belonging to a single face
    static TopoDS_Shape singleFaceShellCompound()
    {
        BRepBuilderAPI_MakePolygon polygonA;
        polygonA.Add(gp_Pnt(0.0, 0.0, 0.0));
        polygonA.Add(gp_Pnt(10.0, 0.0, 0.0));
        polygonA.Add(gp_Pnt(10.0, 10.0, 0.0));
        polygonA.Add(gp_Pnt(0.0, 10.0, 0.0));
        polygonA.Close();

        BRepBuilderAPI_MakePolygon polygonB;
        polygonB.Add(gp_Pnt(0.0, 0.0, 0.0));
        polygonB.Add(gp_Pnt(10.0, 0.0, 0.0));
        polygonB.Add(gp_Pnt(10.0, 0.0, 10.0));
        polygonB.Add(gp_Pnt(0.0, 0.0, 10.0));
        polygonB.Close();

        BRep_Builder builder;
        TopoDS_Shell shellA, shellB;
        builder.MakeShell(shellA);
        builder.Add(shellA, BRepBuilderAPI_MakeFace(polygonA.Wire()).Face());
        builder.MakeShell(shellB);
        builder.Add(shellB, BRepBuilderAPI_MakeFace(polygonB.Wire()).Face());

        TopoDS_Compound compound;
        builder.MakeCompound(compound);
        builder.Add(compound, shellA);
        builder.Add(compound, shellB);
        return compound;
    }

    // Chamfer the given edges of the sheet at one equal distance and hand back the feature
    static wy3d::Chamfer* chamferSheetEdges(wy3d::Database* pDb, const wydb::ElementId& sheetId,
        const std::vector<std::uint32_t>& edgeIndices, double distance = 2.0)
    {
        wy3d::Chamfer* pChamfer(nullptr);
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Sheet* pSheet = wy3d::Sheet::cast(pTrans->getElementForWrite(sheetId));
            if (!pSheet)
            {
                pDb->getTransactionManager()->abortTransaction();
                return nullptr;
            }
            EXPECT_EQ(wy3d::Chamfer::create(pTrans, pSheet, {}, edgeIndices,
                wy3d::ChamferType::EqualDistance, distance, distance, wy3d::PI_4, false, pChamfer),
                wy::ErrorStatus::Ok);
            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
        }
        return pChamfer;
    }
}

// --- A wall edge of an extruded sheet takes a chamfer, and the sheet stays a sheet ---

TEST(ChamferSheet, ExtrudedSheetWallEdge)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    ASSERT_EQ(countFaces(pSheet->getShape()), 4);
    ASSERT_NEAR(bodyArea(pSheet->getShape()), 3000.0, 1e-6);

    const std::uint32_t edgeIndex = findEdgeIndexWithFaces(pSheet->getShape(), 2);
    ASSERT_NE(edgeIndex, UINT_MAX);
    wy3d::Chamfer* pChamfer = chamferSheetEdges(pDb.get(), sheetId, { edgeIndex });
    ASSERT_NE(pChamfer, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pChamfer->getId()), 0u);

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 5);
    EXPECT_EQ(countShells(pSheet->getShape()), 1);
    EXPECT_EQ(countSolids(pSheet->getShape()), 0); // still a sheet, never a solid
    EXPECT_NEAR(bodyArea(pSheet->getShape()), 3000.0 - 40.0 + 2.0 * std::sqrt(2.0) * 10.0, 1e-6);
    expectAllTopoNamed(pSheet);
}

// --- Two edges at once: each one lays down its own strip, nothing at all is special about the
// --- second, and the host stays one sheet

TEST(ChamferSheet, ExtrudedSheetEdgeChain)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    std::vector<std::uint32_t> edgeIndices = findEdgeIndicesWithFaces(pSheet->getShape(), 2, 2);
    ASSERT_EQ(edgeIndices.size(), 2u);

    wy3d::Chamfer* pChamfer = chamferSheetEdges(pDb.get(), sheetId, edgeIndices);
    ASSERT_NE(pChamfer, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pChamfer->getId()), 0u);

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 6);
    EXPECT_EQ(countShells(pSheet->getShape()), 1);
    EXPECT_EQ(countSolids(pSheet->getShape()), 0);
    expectAllTopoNamed(pSheet);
}

// --- A bare shell, no sheet builder involved, takes one too ---

TEST(ChamferSheet, BareShellInteriorEdge)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sheetId = createNonParametricSheet(pDb.get(), openShell());
    ASSERT_FALSE(sheetId.isNull());

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    ASSERT_EQ(pSheet->getShape().ShapeType(), TopAbs_ShapeEnum::TopAbs_SHELL);
    ASSERT_EQ(countFaces(pSheet->getShape()), 4);

    const std::uint32_t edgeIndex = findEdgeIndexWithFaces(pSheet->getShape(), 2);
    ASSERT_NE(edgeIndex, UINT_MAX);
    wy3d::Chamfer* pChamfer = chamferSheetEdges(pDb.get(), sheetId, { edgeIndex });
    ASSERT_NE(pChamfer, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pChamfer->getId()), 0u);

    // OCCT hands the result back in a compound whatever went in, which is the container the
    // sheet builders already use
    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(pSheet->getShape().ShapeType(), TopAbs_ShapeEnum::TopAbs_COMPOUND);
    EXPECT_EQ(countFaces(pSheet->getShape()), 5);
    EXPECT_EQ(countSolids(pSheet->getShape()), 0);
    EXPECT_NEAR(bodyArea(pSheet->getShape()), 3000.0 - 40.0 + 2.0 * std::sqrt(2.0) * 10.0, 1e-6);
    expectAllTopoNamed(pSheet);
}

// --- An edge with a single adjacent face has no dihedral: refused, host untouched ---

TEST(ChamferSheet, FreeEdgeIsRefused)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createFilledSheet(pDb.get(), sketchId);

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    ASSERT_EQ(countFaces(pSheet->getShape()), 1);
    const std::uint32_t edgeIndex = findEdgeIndexWithFaces(pSheet->getShape(), 1);
    ASSERT_NE(edgeIndex, UINT_MAX);

    wy3d::Chamfer* pChamfer = chamferSheetEdges(pDb.get(), sheetId, { edgeIndex });
    ASSERT_NE(pChamfer, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pChamfer->getId()),
        static_cast<std::uint32_t>(wy3d::ErrorCode::CHAMFER_EdgeNotTwoFaces));
    EXPECT_FALSE(pChamfer->isErased());

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 1);
    EXPECT_NEAR(bodyArea(pSheet->getShape()), 5000.0, 1e-6);
}

// --- OCCT would silently drop the free edge and chamfer the rest; here nothing is chamfered ---

TEST(ChamferSheet, FreeEdgeAmongInteriorEdgesIsRefused)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    const std::uint32_t interiorIndex = findEdgeIndexWithFaces(pSheet->getShape(), 2);
    const std::uint32_t freeIndex = findEdgeIndexWithFaces(pSheet->getShape(), 1);
    ASSERT_NE(interiorIndex, UINT_MAX);
    ASSERT_NE(freeIndex, UINT_MAX);

    wy3d::Chamfer* pChamfer = chamferSheetEdges(pDb.get(), sheetId, { interiorIndex, freeIndex });
    ASSERT_NE(pChamfer, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pChamfer->getId()),
        static_cast<std::uint32_t>(wy3d::ErrorCode::CHAMFER_EdgeNotTwoFaces));

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 4);
    EXPECT_NEAR(bodyArea(pSheet->getShape()), 3000.0, 1e-6);
}

// --- Two faces sewn only geometrically, each in a shell of its own, have no interior edge: the
// --- seam is two boundary edges. The guard sees each of them as a lone edge and refuses, where
// --- OCCT would have raised "no suitable edges" from deep inside the builder.

TEST(ChamferSheet, SingleFaceShellCompoundIsRefused)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sheetId = createNonParametricSheet(pDb.get(), singleFaceShellCompound());
    ASSERT_FALSE(sheetId.isNull());

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    ASSERT_EQ(countFaces(pSheet->getShape()), 2);
    ASSERT_EQ(countShells(pSheet->getShape()), 2);
    const std::uint32_t edgeIndex = findEdgeIndexWithFaces(pSheet->getShape(), 1);
    ASSERT_NE(edgeIndex, UINT_MAX);

    wy3d::Chamfer* pChamfer = chamferSheetEdges(pDb.get(), sheetId, { edgeIndex });
    ASSERT_NE(pChamfer, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pChamfer->getId()),
        static_cast<std::uint32_t>(wy3d::ErrorCode::CHAMFER_EdgeNotTwoFaces));

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 2);
}

// --- The two-distance mode needs a reference face, so it wants the same two-face edge ---

TEST(ChamferSheet, DistanceDistanceOnWallEdge)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    const std::uint32_t edgeIndex = findEdgeIndexWithFaces(pSheet->getShape(), 2);
    ASSERT_NE(edgeIndex, UINT_MAX);

    wy3d::Chamfer* pChamfer(nullptr);
    {
        wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
        wy3d::Sheet* pSheetWrite = wy3d::Sheet::cast(pTrans->getElementForWrite(sheetId));
        ASSERT_NE(pSheetWrite, nullptr);
        EXPECT_EQ(wy3d::Chamfer::create(pTrans, pSheetWrite, {}, { edgeIndex },
            wy3d::ChamferType::DistanceDistance, 2.0, 3.0, wy3d::PI_4, false, pChamfer),
            wy::ErrorStatus::Ok);
        EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pChamfer, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pChamfer->getId()), 0u);

    // 2 * 10 off one wall and 3 * 10 off the other, the strip between them is sqrt(13) wide
    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 5);
    EXPECT_NEAR(bodyArea(pSheet->getShape()), 3000.0 - 20.0 - 30.0 + std::sqrt(13.0) * 10.0, 1e-6);
    expectAllTopoNamed(pSheet);
}

// --- The host regenerates: the chamfer is put back on the new shape ---

TEST(ChamferSheet, ChainUpdateOnHostRegenerate)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    const std::uint32_t edgeIndex = findEdgeIndexWithFaces(pSheet->getShape(), 2);
    ASSERT_NE(edgeIndex, UINT_MAX);
    wy3d::Chamfer* pChamfer = chamferSheetEdges(pDb.get(), sheetId, { edgeIndex });
    ASSERT_NE(pChamfer, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), sheetId), 0u);

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    ASSERT_EQ(countFaces(pSheet->getShape()), 5);

    // Deepen the extrusion: the walls regenerate, and the edge has to be chamfered again
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::ExtrudedSheet* pWrite = wy3d::ExtrudedSheet::cast(pTrans->getElementForWrite(sheetId));
        ASSERT_NE(pWrite, nullptr);
        EXPECT_EQ(pWrite->setDepth(20.0), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    EXPECT_EQ(getChainErrorCode(pDb.get(), pChamfer->getId()), 0u);
    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 5);
    EXPECT_NEAR(bodyArea(pSheet->getShape()), 6000.0 - 80.0 + 2.0 * std::sqrt(2.0) * 20.0, 1e-6);
    expectAllTopoNamed(pSheet);
}

// --- Out and back in ---

TEST(ChamferSheet, IO)
{
    std::string filePath("./test_chamfer_sheet.wy3dt");
    wydb::ElementId sketchId = wydb::ElementId::kNull;
    wydb::ElementId sheetId = wydb::ElementId::kNull;
    wydb::ElementId chamferId = wydb::ElementId::kNull;

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        sketchId = createRectSketch(pDb.get());
        sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

        const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
        ASSERT_NE(pSheet, nullptr);
        const std::uint32_t edgeIndex = findEdgeIndexWithFaces(pSheet->getShape(), 2);
        ASSERT_NE(edgeIndex, UINT_MAX);
        wy3d::Chamfer* pChamfer = chamferSheetEdges(pDb.get(), sheetId, { edgeIndex });
        ASSERT_NE(pChamfer, nullptr);
        chamferId = pChamfer->getId();

        EXPECT_EQ(pDb->writeFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);
    }

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        EXPECT_EQ(pDb->readFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);

        const wy3d::Chamfer* pChamfer = wy3d::Chamfer::cast(pDb->getElement(chamferId));
        ASSERT_NE(pChamfer, nullptr);
        EXPECT_EQ(pChamfer->getParent(), sheetId);
        EXPECT_FALSE(pChamfer->getEdges().empty());

        const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
        ASSERT_NE(pSheet, nullptr);
        std::vector<wydb::ElementId> children = pSheet->getChildren();
        EXPECT_NE(std::find(children.cbegin(), children.cend(), chamferId), children.cend());

        EXPECT_EQ(countFaces(pSheet->getShape()), 5);
        EXPECT_EQ(countSolids(pSheet->getShape()), 0);
        EXPECT_NEAR(bodyArea(pSheet->getShape()), 3000.0 - 40.0 + 2.0 * std::sqrt(2.0) * 10.0, 1e-6);
        expectAllTopoNamed(pSheet);
    }
}

// --- One undo step in and out ---

TEST(ChamferSheet, UndoRedo)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    const std::uint32_t edgeIndex = findEdgeIndexWithFaces(pSheet->getShape(), 2);
    ASSERT_NE(edgeIndex, UINT_MAX);
    ASSERT_NE(chamferSheetEdges(pDb.get(), sheetId, { edgeIndex }), nullptr);

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    ASSERT_EQ(countFaces(pSheet->getShape()), 5);

    EXPECT_EQ(pMgr->undo(), wy::ErrorStatus::Ok);
    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 4);
    EXPECT_NEAR(bodyArea(pSheet->getShape()), 3000.0, 1e-6);

    EXPECT_EQ(pMgr->redo(), wy::ErrorStatus::Ok);
    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 5);
    expectAllTopoNamed(pSheet);
}

// --- The solid overload is the same one it always was ---

TEST(ChamferSheet, SolidHostStillWorks)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId boxId = wydb::ElementId::kNull;
    {
        wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
        Box* pBox(nullptr);
        EXPECT_EQ(Box::create(pTrans, 30.0, 20.0, 10.0, pBox), wy::ErrorStatus::Ok);
        ASSERT_NE(pBox, nullptr);
        EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
        boxId = pBox->getId();
    }

    wy3d::Chamfer* pChamfer(nullptr);
    {
        wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
        wy3d::Solid* pSolid = wy3d::Solid::cast(pTrans->getElementForWrite(boxId));
        ASSERT_NE(pSolid, nullptr);
        EXPECT_EQ(wy3d::Chamfer::create(pTrans, pSolid, {}, { 0, 1, 2, 3 },
            wy3d::ChamferType::EqualDistance, 2.0, 2.0, wy3d::PI_4, false, pChamfer),
            wy::ErrorStatus::Ok);
        EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pChamfer, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pChamfer->getId()), 0u);

    const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(boxId));
    ASSERT_NE(pSolid, nullptr);
    // OCCT wraps the chamfered box in a compound too, so the container says nothing about what is
    // inside it: the solid is still the one and only solid, which is what this pins
    EXPECT_EQ(pSolid->getShape().ShapeType(), TopAbs_ShapeEnum::TopAbs_COMPOUND);
    EXPECT_EQ(countSolids(pSolid->getShape()), 1);
    EXPECT_EQ(countFaces(pSolid->getShape()), 10);
    expectAllTopoNamed(pSolid);
}

// --- Nothing to work with ---

TEST(ChamferSheet, NullArgs)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

    wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
    wy3d::Sheet* pSheet = wy3d::Sheet::cast(pTrans->getElementForWrite(sheetId));
    ASSERT_NE(pSheet, nullptr);
    const std::uint32_t edgeIndex = findEdgeIndexWithFaces(pSheet->getShape(), 2);
    ASSERT_NE(edgeIndex, UINT_MAX);

    wy3d::Chamfer* pChamfer(nullptr);
    EXPECT_EQ(wy3d::Chamfer::create(nullptr, pSheet, {}, { edgeIndex },
        wy3d::ChamferType::EqualDistance, 2.0, 2.0, wy3d::PI_4, false, pChamfer),
        wy::ErrorStatus::NullDatabasePointer);
    EXPECT_EQ(wy3d::Chamfer::create(pTrans, static_cast<wy3d::Sheet*>(nullptr), {}, { edgeIndex },
        wy3d::ChamferType::EqualDistance, 2.0, 2.0, wy3d::PI_4, false, pChamfer),
        wy::ErrorStatus::NullElementPointer);
    EXPECT_EQ(wy3d::Chamfer::create(pTrans, pSheet, {}, {},
        wy3d::ChamferType::EqualDistance, 2.0, 2.0, wy3d::PI_4, false, pChamfer),
        wy::ErrorStatus::InvalidInput);
    EXPECT_EQ(pChamfer, nullptr);

    EXPECT_EQ(pDb->getTransactionManager()->abortTransaction(), wy::ErrorStatus::Ok);
}
