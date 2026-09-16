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
#include <wy3dFillet.h>
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

// A fillet may sit on a sheet as well as on a solid: the host is data, not type, and the kernel
// says so itself - BRepFilletAPI_MakeFillet builds "on the broken edges of a shell or solid". What
// a sheet adds to the story is the open boundary - an edge with a single adjacent face has no
// dihedral to round, and OCCT neither rejects nor reports such an edge once another usable one is
// in the builder, so Fillet::modifyOwnerShape checks every edge before the algorithm sees the shape.
//
// A 100 x 50 rectangle extruded 10 deep gives the four walls, no caps, 3000 of area (300 perimeter
// * 10). Rounding one vertical edge at radius 2 replaces the corner with a quarter cylinder of 10
// height: the two walls lose their straight run up to the tangents (2 * 2 * 10) and the arc adds
// pi * 2 / 2 * 10, so the surface comes out at 3000 - 40 + 10 * pi = 2991.416, one face more than
// before.
//
// The sheets go through expectSheetTopoNamed rather than expectAllTopoNamed, which pins the one
// naming invariant a filleted sheet breaks. See that helper for what and why.

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
    // faces. An edge with two of them is the only kind a fillet can take.
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

    // The first howMany of them, for the cases that fillet more than one edge at a time
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

    // A sheet element holding a shape of any kind, so a bare shell can host a fillet without
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

    // Fillet the given edges of the sheet at one radius and hand back the feature
    static wy3d::Fillet* filletSheetEdges(wy3d::Database* pDb, const wydb::ElementId& sheetId,
        const std::vector<std::uint32_t>& edgeIndices, double radius = 2.0)
    {
        wy3d::Fillet* pFillet(nullptr);
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Sheet* pSheet = wy3d::Sheet::cast(pTrans->getElementForWrite(sheetId));
            if (!pSheet)
            {
                pDb->getTransactionManager()->abortTransaction();
                return nullptr;
            }
            EXPECT_EQ(wy3d::Fillet::create(pTrans, pSheet, {}, edgeIndices, radius, pFillet),
                wy::ErrorStatus::Ok);
            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
        }
        return pFillet;
    }
}

// --- A wall edge of an extruded sheet takes a fillet, and the sheet stays a sheet ---

TEST(FilletSheet, ExtrudedSheetWallEdge)
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
    wy3d::Fillet* pFillet = filletSheetEdges(pDb.get(), sheetId, { edgeIndex });
    ASSERT_NE(pFillet, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pFillet->getId()), 0u);

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 5);
    EXPECT_EQ(countShells(pSheet->getShape()), 1);
    EXPECT_EQ(countSolids(pSheet->getShape()), 0); // still a sheet, never a solid
    EXPECT_NEAR(bodyArea(pSheet->getShape()), 3000.0 - 40.0 + 10.0 * wy3d::PI, 1e-6);
    expectAllTopoNamed(pSheet);
}

// --- The free ends of a blend are named after the blend itself: they both come from the same old
// --- edge, so the blend face is what tells them apart, and the number appears only when there is
// --- more than one of them

TEST(FilletSheet, FreeEdgesNamedAfterTheBlend)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    const std::uint32_t edgeIndex = findEdgeIndexWithFaces(pSheet->getShape(), 2);
    ASSERT_NE(edgeIndex, UINT_MAX);
    wy3d::Fillet* pFillet = filletSheetEdges(pDb.get(), sheetId, { edgeIndex });
    ASSERT_NE(pFillet, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pFillet->getId()), 0u);

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    const wy3d::TopoNaming* pTopoNaming = pSheet->getTopoNaming();
    ASSERT_NE(pTopoNaming, nullptr);

    const std::vector<std::uint32_t> blendIndices = pFillet->getNewFaceIndices();
    ASSERT_EQ(blendIndices.size(), 1u);
    TopTools_IndexedMapOfShape faceMap;
    TopExp::MapShapes(pSheet->getShape(), TopAbs_ShapeEnum::TopAbs_FACE, faceMap);
    const TopoDS_Face blend = TopoDS::Face(faceMap(blendIndices.front() + 1));
    const std::string blendName = pTopoNaming->getTopoName(blend);
    ASSERT_FALSE(blendName.empty());

    // The blend's free ends: its edges that have no adjacent face but the blend itself
    TopTools_IndexedDataMapOfShapeListOfShape edgeFaceMap;
    TopExp::MapShapesAndAncestors(pSheet->getShape(), TopAbs_ShapeEnum::TopAbs_EDGE,
        TopAbs_ShapeEnum::TopAbs_FACE, edgeFaceMap);
    TopTools_IndexedMapOfShape blendEdges;
    TopExp::MapShapes(blend, TopAbs_ShapeEnum::TopAbs_EDGE, blendEdges);
    std::vector<std::string> freeNames;
    for (int i = 1; i <= blendEdges.Extent(); ++i)
    {
        const TopoDS_Shape& edge = blendEdges(i);
        const TopTools_ListOfShape& faces = edgeFaceMap.FindFromKey(edge);
        bool hasOtherFace(false);
        for (TopTools_ListIteratorOfListOfShape it(faces); it.More(); it.Next())
        {
            if (!it.Value().IsSame(blend))
            {
                hasOtherFace = true;
                break;
            }
        }
        if (!hasOtherFace)
        {
            freeNames.emplace_back(pTopoNaming->getTopoName(edge));
        }
    }

    ASSERT_EQ(freeNames.size(), 2u);
    const std::string prefix = blendName + "+@" + std::to_string(pFillet->getId().value());
    EXPECT_EQ(freeNames[0], prefix + "#1");
    EXPECT_EQ(freeNames[1], prefix + "#2");
}

// --- Two edges at once: each one lays down its own blend, nothing at all is special about the
// --- second, and the host stays one sheet

TEST(FilletSheet, ExtrudedSheetEdgeChain)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    std::vector<std::uint32_t> edgeIndices = findEdgeIndicesWithFaces(pSheet->getShape(), 2, 2);
    ASSERT_EQ(edgeIndices.size(), 2u);

    wy3d::Fillet* pFillet = filletSheetEdges(pDb.get(), sheetId, edgeIndices);
    ASSERT_NE(pFillet, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pFillet->getId()), 0u);

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 6);
    EXPECT_EQ(countShells(pSheet->getShape()), 1);
    EXPECT_EQ(countSolids(pSheet->getShape()), 0);
    expectAllTopoNamed(pSheet);
}

// --- A bare shell, no sheet builder involved, takes one too ---

TEST(FilletSheet, BareShellInteriorEdge)
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
    wy3d::Fillet* pFillet = filletSheetEdges(pDb.get(), sheetId, { edgeIndex });
    ASSERT_NE(pFillet, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pFillet->getId()), 0u);

    // OCCT hands the result back in a compound whatever went in, which is the container the
    // sheet builders already use
    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(pSheet->getShape().ShapeType(), TopAbs_ShapeEnum::TopAbs_COMPOUND);
    EXPECT_EQ(countFaces(pSheet->getShape()), 5);
    EXPECT_EQ(countSolids(pSheet->getShape()), 0);
    EXPECT_NEAR(bodyArea(pSheet->getShape()), 3000.0 - 40.0 + 10.0 * wy3d::PI, 1e-6);
    expectAllTopoNamed(pSheet);
}

// --- An edge with a single adjacent face has no dihedral: refused, host untouched ---

TEST(FilletSheet, FreeEdgeIsRefused)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createFilledSheet(pDb.get(), sketchId);

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    ASSERT_EQ(countFaces(pSheet->getShape()), 1);
    const std::uint32_t edgeIndex = findEdgeIndexWithFaces(pSheet->getShape(), 1);
    ASSERT_NE(edgeIndex, UINT_MAX);

    wy3d::Fillet* pFillet = filletSheetEdges(pDb.get(), sheetId, { edgeIndex });
    ASSERT_NE(pFillet, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pFillet->getId()),
        static_cast<std::uint32_t>(wy3d::ErrorCode::FILLET_EdgeNotTwoFaces));
    EXPECT_FALSE(pFillet->isErased());

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 1);
    EXPECT_NEAR(bodyArea(pSheet->getShape()), 5000.0, 1e-6);
}

// --- OCCT would silently drop the free edge and fillet the rest; here nothing is filleted ---

TEST(FilletSheet, FreeEdgeAmongInteriorEdgesIsRefused)
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

    wy3d::Fillet* pFillet = filletSheetEdges(pDb.get(), sheetId, { interiorIndex, freeIndex });
    ASSERT_NE(pFillet, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pFillet->getId()),
        static_cast<std::uint32_t>(wy3d::ErrorCode::FILLET_EdgeNotTwoFaces));

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 4);
    EXPECT_NEAR(bodyArea(pSheet->getShape()), 3000.0, 1e-6);
}

// --- Two faces sewn only geometrically, each in a shell of its own, have no interior edge: the
// --- seam is two boundary edges. The guard sees each of them as a lone edge and refuses, where
// --- OCCT would have raised "no suitable edges" from deep inside the builder.

TEST(FilletSheet, SingleFaceShellCompoundIsRefused)
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

    wy3d::Fillet* pFillet = filletSheetEdges(pDb.get(), sheetId, { edgeIndex });
    ASSERT_NE(pFillet, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pFillet->getId()),
        static_cast<std::uint32_t>(wy3d::ErrorCode::FILLET_EdgeNotTwoFaces));

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 2);
}

// --- The host regenerates: the fillet is put back on the new shape ---

TEST(FilletSheet, ChainUpdateOnHostRegenerate)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    const std::uint32_t edgeIndex = findEdgeIndexWithFaces(pSheet->getShape(), 2);
    ASSERT_NE(edgeIndex, UINT_MAX);
    wy3d::Fillet* pFillet = filletSheetEdges(pDb.get(), sheetId, { edgeIndex });
    ASSERT_NE(pFillet, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), sheetId), 0u);

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    ASSERT_EQ(countFaces(pSheet->getShape()), 5);

    // Deepen the extrusion: the walls regenerate, and the edge has to be rounded again
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::ExtrudedSheet* pWrite = wy3d::ExtrudedSheet::cast(pTrans->getElementForWrite(sheetId));
        ASSERT_NE(pWrite, nullptr);
        EXPECT_EQ(pWrite->setDepth(20.0), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    EXPECT_EQ(getChainErrorCode(pDb.get(), pFillet->getId()), 0u);
    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 5);
    EXPECT_NEAR(bodyArea(pSheet->getShape()), 6000.0 - 80.0 + 20.0 * wy3d::PI, 1e-6);
    expectAllTopoNamed(pSheet);
}

// --- Out and back in ---

TEST(FilletSheet, IO)
{
    std::string filePath("./test_fillet_sheet.wy3dt");
    wydb::ElementId sketchId = wydb::ElementId::kNull;
    wydb::ElementId sheetId = wydb::ElementId::kNull;
    wydb::ElementId filletId = wydb::ElementId::kNull;

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        sketchId = createRectSketch(pDb.get());
        sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

        const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
        ASSERT_NE(pSheet, nullptr);
        const std::uint32_t edgeIndex = findEdgeIndexWithFaces(pSheet->getShape(), 2);
        ASSERT_NE(edgeIndex, UINT_MAX);
        wy3d::Fillet* pFillet = filletSheetEdges(pDb.get(), sheetId, { edgeIndex });
        ASSERT_NE(pFillet, nullptr);
        filletId = pFillet->getId();

        EXPECT_EQ(pDb->writeFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);
    }

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        EXPECT_EQ(pDb->readFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);

        const wy3d::Fillet* pFillet = wy3d::Fillet::cast(pDb->getElement(filletId));
        ASSERT_NE(pFillet, nullptr);
        EXPECT_EQ(pFillet->getParent(), sheetId);
        EXPECT_FALSE(pFillet->getEdges().empty());
        EXPECT_NEAR(pFillet->getRadius(), 2.0, 1e-6);

        const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
        ASSERT_NE(pSheet, nullptr);
        std::vector<wydb::ElementId> children = pSheet->getChildren();
        EXPECT_NE(std::find(children.cbegin(), children.cend(), filletId), children.cend());

        EXPECT_EQ(countFaces(pSheet->getShape()), 5);
        EXPECT_EQ(countSolids(pSheet->getShape()), 0);
        EXPECT_NEAR(bodyArea(pSheet->getShape()), 3000.0 - 40.0 + 10.0 * wy3d::PI, 1e-6);
        expectAllTopoNamed(pSheet);
    }
}

// --- One undo step in and out ---

TEST(FilletSheet, UndoRedo)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    const std::uint32_t edgeIndex = findEdgeIndexWithFaces(pSheet->getShape(), 2);
    ASSERT_NE(edgeIndex, UINT_MAX);
    ASSERT_NE(filletSheetEdges(pDb.get(), sheetId, { edgeIndex }), nullptr);

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

TEST(FilletSheet, SolidHostStillWorks)
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

    wy3d::Fillet* pFillet(nullptr);
    {
        wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
        wy3d::Solid* pSolid = wy3d::Solid::cast(pTrans->getElementForWrite(boxId));
        ASSERT_NE(pSolid, nullptr);
        EXPECT_EQ(wy3d::Fillet::create(pTrans, pSolid, {}, { 0, 1, 2, 3 }, 2.0, pFillet),
            wy::ErrorStatus::Ok);
        EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pFillet, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pFillet->getId()), 0u);

    const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(boxId));
    ASSERT_NE(pSolid, nullptr);
    // OCCT wraps the filleted box in a compound too, so the container says nothing about what is
    // inside it: the solid is still the one and only solid, which is what this pins
    EXPECT_EQ(pSolid->getShape().ShapeType(), TopAbs_ShapeEnum::TopAbs_COMPOUND);
    EXPECT_EQ(countSolids(pSolid->getShape()), 1);
    EXPECT_EQ(countFaces(pSolid->getShape()), 10);
    expectAllTopoNamed(pSolid);
}

// --- Nothing to work with ---

TEST(FilletSheet, NullArgs)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

    wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
    wy3d::Sheet* pSheet = wy3d::Sheet::cast(pTrans->getElementForWrite(sheetId));
    ASSERT_NE(pSheet, nullptr);
    const std::uint32_t edgeIndex = findEdgeIndexWithFaces(pSheet->getShape(), 2);
    ASSERT_NE(edgeIndex, UINT_MAX);

    wy3d::Fillet* pFillet(nullptr);
    EXPECT_EQ(wy3d::Fillet::create(nullptr, pSheet, {}, { edgeIndex }, 2.0, pFillet),
        wy::ErrorStatus::NullDatabasePointer);
    EXPECT_EQ(wy3d::Fillet::create(pTrans, static_cast<wy3d::Sheet*>(nullptr), {}, { edgeIndex }, 2.0, pFillet),
        wy::ErrorStatus::NullElementPointer);
    EXPECT_EQ(wy3d::Fillet::create(pTrans, pSheet, {}, {}, 2.0, pFillet),
        wy::ErrorStatus::InvalidInput);
    EXPECT_EQ(pFillet, nullptr);

    EXPECT_EQ(pDb->getTransactionManager()->abortTransaction(), wy::ErrorStatus::Ok);
}
