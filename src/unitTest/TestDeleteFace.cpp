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

#include <algorithm>
#include <cmath>
#include <string>
#include <memory>
#include <vector>

#include <wy3dDeleteFace.h>
#include <wy3dSplitFace.h>
#include <wy3dFilledSheet.h>
#include <wy3dNonParametricSheet.h>
#include <wy3dExtrudedSheet.h>
#include <wy3dThicken.h>
#include <wy3dSheet.h>
#include <wy3dSketch.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchPlane.h>
#include <wy3dSketch3D.h>
#include <wy3dSketchLine3D.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>

#include <TopoDS.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Compound.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <BRep_Builder.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <GProp_GProps.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <gp_Pnt.hxx>

// DeleteFace takes faces off a sheet and leaves an opening where they were. Only sheets host it,
// so the type is what the argument says; the geometry that matters is that what stays is the
// very same geometry, down to the topo names the rest of the chain hangs off.
//
// A 100 x 50 rectangle sketch fills to a sheet of one face and 5000 of area; extruding it 10
// deep gives the four walls without caps, 3000 of area (300 perimeter * 10): two of 1000 and two
// of 500. Losing a wall takes its area with it and nothing else.

namespace
{
    static std::size_t countElements(wy3d::Database* pDb)
    {
        std::size_t n(0);
        wy::Iterator<wydb::ElementId> iter = pDb->createIterator();
        while (!iter.isDone())
        {
            const wydb::Element* pElem = pDb->getElement(iter.current());
            if (pElem && !pElem->isErased())
            {
                ++n;
            }
            iter.moveNext();
        }
        return n;
    }

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

    static double bodyArea(const TopoDS_Shape& shape)
    {
        GProp_GProps gprops;
        BRepGProp::SurfaceProperties(shape, gprops);
        return gprops.Mass();
    }

    // Index, as getShape() returns the faces, of the planar face perpendicular to the given
    // axis (0 = X, 1 = Y, 2 = Z) whose centre of mass sits at coord along that same axis
    static std::uint32_t findFaceIndexAt(const TopoDS_Shape& shape, int axis, double coord)
    {
        assert(0 <= axis && axis <= 2);

        TopTools_IndexedMapOfShape faces;
        TopExp::MapShapes(shape, TopAbs_ShapeEnum::TopAbs_FACE, faces);
        for (int i = 1; i <= faces.Extent(); ++i)
        {
            const TopoDS_Face& face = TopoDS::Face(faces(i));
            BRepAdaptor_Surface surface(face);
            if (GeomAbs_Plane != surface.GetType())
            {
                continue;
            }

            const gp_Dir& dir = surface.Plane().Axis().Direction();
            const double dirs[3] = { dir.X(), dir.Y(), dir.Z() };
            if (std::fabs(dirs[axis]) < 1.0 - 1e-9)
            {
                continue;
            }

            GProp_GProps props;
            BRepGProp::SurfaceProperties(face, props);
            const gp_Pnt& centre = props.CentreOfMass();
            const double centres[3] = { centre.X(), centre.Y(), centre.Z() };
            if (std::fabs(centres[axis] - coord) > 1e-6)
            {
                continue;
            }

            return static_cast<std::uint32_t>(i - 1);
        }
        return UINT_MAX;
    }

    // Every face/edge of the built shape must resolve a topo name, or picking cannot find it
    template<typename Body>
    static void expectAllTopoNamed(const Body* pBody)
    {
        const wy3d::TopoNaming* pTopoNaming = pBody->getTopoNaming();
        ASSERT_NE(pTopoNaming, nullptr);
        const TopoDS_Shape& shape = pBody->getShape();

        // The name table must hold exactly the sub-shapes the body has now: a name left over
        // from a deleted face would be a face the user can pick and the body does not have
        std::vector<std::string> info;
        EXPECT_TRUE(pTopoNaming->check(shape, info)) << (info.empty() ? "" : info[0]);

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

    // Create a 2D sketch on the plane z = 0 with a 100x50 closed rectangle
    static wydb::ElementId createRectSketchAtZ(wy3d::Database* pDb, double z)
    {
        wydb::ElementId sketchId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::SketchPlane plane(wy::Vector3(0.0, 0.0, z), wy::Vector3::kZAxis, wy::Vector3::kXAxis);
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

    static wydb::ElementId createRectSketch(wy3d::Database* pDb)
    {
        return createRectSketchAtZ(pDb, 0.0);
    }

    static wydb::ElementId createEmptySketch3D(wy3d::Database* pDb)
    {
        wydb::ElementId sketchId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Sketch3D* pSketch3D(nullptr);
            EXPECT_EQ(wy3d::Sketch3D::create(pTrans, pSketch3D), wy::ErrorStatus::Ok);
            if (!pSketch3D)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }
            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
            sketchId = pSketch3D->getId();
        }
        return sketchId;
    }

    static wydb::ElementId addLine(wy3d::Database* pDb, const wydb::ElementId& sketchId,
        const wy::Vector3& startPnt, const wy::Vector3& endPnt)
    {
        wydb::ElementId lineId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(sketchId));
            if (!pSketch3D)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }

            wy3d::SketchLine3D* pLine(nullptr);
            EXPECT_EQ(wy3d::SketchLine3D::create(pTrans, startPnt, endPnt, pLine), wy::ErrorStatus::Ok);
            if (!pLine)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }
            EXPECT_EQ(pSketch3D->addEntity(pLine), wy::ErrorStatus::Ok);
            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
            lineId = pLine->getId();
        }
        return lineId;
    }

    // A planar sheet of one face at z = 0 covering 100 x 50
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

    // Split the given face of the sheet with the whole sketch and hand back the feature
    static wy3d::SplitFace* splitFace(wy3d::Database* pDb, const wydb::ElementId& sheetId,
        std::uint32_t faceIndex, const wydb::ElementId& sketchId)
    {
        wy3d::SplitFace* pSplitFace(nullptr);
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Sheet* pSheet = wy3d::Sheet::cast(pTrans->getElementForWrite(sheetId));
            wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(sketchId));
            if (!pSheet || !pSketch3D)
            {
                pDb->getTransactionManager()->abortTransaction();
                return nullptr;
            }
            EXPECT_NE(faceIndex, UINT_MAX);
            EXPECT_EQ(wy3d::SplitFace::create(pTrans, pSheet, { faceIndex }, pSketch3D, pSplitFace),
                wy::ErrorStatus::Ok);
            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
        }
        return pSplitFace;
    }

    // Delete the given faces of the sheet and hand back the feature
    static wy3d::DeleteFace* deleteFace(wy3d::Database* pDb, const wydb::ElementId& sheetId,
        const std::vector<std::uint32_t>& faceIndices)
    {
        wy3d::DeleteFace* pDeleteFace(nullptr);
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Sheet* pSheet = wy3d::Sheet::cast(pTrans->getElementForWrite(sheetId));
            if (!pSheet)
            {
                pDb->getTransactionManager()->abortTransaction();
                return nullptr;
            }
            EXPECT_EQ(wy3d::DeleteFace::create(pTrans, pSheet, faceIndices, pDeleteFace), wy::ErrorStatus::Ok);
            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
        }
        return pDeleteFace;
    }

    // The outer shell of a box, so a fixture can hold bare shells: the solid it came from would
    // be turned away by DeleteFace before it ever reached the rebuild.
    static TopoDS_Shape outerShell(const TopoDS_Shape& shape)
    {
        for (TopExp_Explorer exp(shape, TopAbs_SHELL); exp.More(); exp.Next())
        {
            return exp.Current();
        }
        return TopoDS_Shape();
    }

    // Two 10 x 10 x 10 box shells in one compound, far enough apart that every face of the first
    // sits at a coordinate of its own. One shell of 600 of area, two of 1200.
    static TopoDS_Shape twoBoxShells()
    {
        BRep_Builder builder;
        TopoDS_Compound compound;
        builder.MakeCompound(compound);
        builder.Add(compound, outerShell(BRepPrimAPI_MakeBox(gp_Pnt(0.0, 0.0, 0.0), 10.0, 10.0, 10.0).Shape()));
        builder.Add(compound, outerShell(BRepPrimAPI_MakeBox(gp_Pnt(100.0, 100.0, 100.0), 10.0, 10.0, 10.0).Shape()));
        return compound;
    }

    // A sheet element holding a shape of any kind. NonParametricSheet takes whatever it is given,
    // so this is how a shape reaches DeleteFace without anyone passing in a Sheet subtype that
    // would have rejected it.
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
}

// --- A wall off an extruded rectangle goes away, the rest of the sheet stays as it was ---

TEST(DeleteFace, DeleteOneFaceOfExtrudedSheet)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 4);
    EXPECT_EQ(countShells(pSheet->getShape()), 1);
    EXPECT_NEAR(bodyArea(pSheet->getShape()), 3000.0, 1e-6);

    // The wall at x = 0, which is one of the two 50 x 10 ones
    const std::uint32_t wallIndex = findFaceIndexAt(pSheet->getShape(), 0, 0.0);
    ASSERT_NE(wallIndex, UINT_MAX);
    wy3d::DeleteFace* pDeleteFace = deleteFace(pDb.get(), sheetId, { wallIndex });
    ASSERT_NE(pDeleteFace, nullptr);

    EXPECT_EQ(getChainErrorCode(pDb.get(), sheetId), 0u);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pDeleteFace->getId()), 0u);
    EXPECT_EQ(pDeleteFace->getParent(), sheetId);
    EXPECT_FALSE(pDeleteFace->getFaces().empty());

    // The host keeps its own children view: the feature hangs under the sheet
    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    std::vector<wydb::ElementId> children = pSheet->getChildren();
    EXPECT_NE(std::find(children.cbegin(), children.cend(), pDeleteFace->getId()), children.cend());

    // One wall short, and the sheet is still a compound of the one shell - a body that fell
    // apart into pieces would break the features downstream that count shells
    EXPECT_EQ(countFaces(pSheet->getShape()), 3);
    EXPECT_EQ(countShells(pSheet->getShape()), 1);
    EXPECT_NEAR(bodyArea(pSheet->getShape()), 2500.0, 1e-6);
    EXPECT_EQ(pSheet->getShape().ShapeType(), TopAbs_ShapeEnum::TopAbs_COMPOUND);
    expectAllTopoNamed(pSheet);
}

// --- Several faces in one go ---

TEST(DeleteFace, DeleteTwoFacesAtOnce)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    const std::uint32_t leftWall = findFaceIndexAt(pSheet->getShape(), 0, 0.0);
    const std::uint32_t rightWall = findFaceIndexAt(pSheet->getShape(), 0, 100.0);
    ASSERT_NE(leftWall, UINT_MAX);
    ASSERT_NE(rightWall, UINT_MAX);

    wy3d::DeleteFace* pDeleteFace = deleteFace(pDb.get(), sheetId, { leftWall, rightWall });
    ASSERT_NE(pDeleteFace, nullptr);
    EXPECT_EQ(pDeleteFace->getFaces().size(), 2u);
    EXPECT_EQ(getChainErrorCode(pDb.get(), sheetId), 0u);

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 2);
    EXPECT_EQ(countShells(pSheet->getShape()), 1);
    EXPECT_NEAR(bodyArea(pSheet->getShape()), 2000.0, 1e-6);
    expectAllTopoNamed(pSheet);
}

// --- Split a face, then delete the piece that is not wanted ---

TEST(DeleteFace, SplitThenDelete)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createFilledSheet(pDb.get(), sketchId);
    wydb::ElementId curveSketchId = createEmptySketch3D(pDb.get());
    addLine(pDb.get(), curveSketchId,
        wy::Vector3(0.0, 25.0, 0.0), wy::Vector3(100.0, 25.0, 0.0));

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    ASSERT_NE(splitFace(pDb.get(), sheetId, findFaceIndexAt(pSheet->getShape(), 2, 0.0),
        curveSketchId), nullptr);

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    ASSERT_EQ(countFaces(pSheet->getShape()), 2);
    EXPECT_NEAR(bodyArea(pSheet->getShape()), 5000.0, 1e-6);

    // Both halves are the same size, so either one leaves the other 2500
    wy3d::DeleteFace* pDeleteFace = deleteFace(pDb.get(), sheetId,
        { findFaceIndexAt(pSheet->getShape(), 2, 0.0) });
    ASSERT_NE(pDeleteFace, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), sheetId), 0u);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pDeleteFace->getId()), 0u);

    // Two modifications in a row on one host: the split runs first and puts the pieces back,
    // then the delete takes one of them away
    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 1);
    EXPECT_EQ(countShells(pSheet->getShape()), 1);
    EXPECT_NEAR(bodyArea(pSheet->getShape()), 2500.0, 1e-6);
    expectAllTopoNamed(pSheet);
}

// --- Taking the last face away is refused, not obeyed ---

TEST(DeleteFace, DeleteAllFacesFails)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createFilledSheet(pDb.get(), sketchId);

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    ASSERT_EQ(countFaces(pSheet->getShape()), 1);

    wy3d::DeleteFace* pDeleteFace = deleteFace(pDb.get(), sheetId,
        { findFaceIndexAt(pSheet->getShape(), 2, 0.0) });
    ASSERT_NE(pDeleteFace, nullptr);

    // The feature stays on the tree carrying the error; the sheet is untouched
    EXPECT_EQ(getChainErrorCode(pDb.get(), pDeleteFace->getId()),
        static_cast<std::uint32_t>(wy3d::ErrorCode::DELETEFACE_NoFaceLeft));
    EXPECT_FALSE(pDeleteFace->isErased());

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 1);
    EXPECT_NEAR(bodyArea(pSheet->getShape()), 5000.0, 1e-6);
    expectAllTopoNamed(pSheet);
}

// --- One shell of two loses all of its faces: it goes away with them, rather than staying on as
// a shell with nothing in it ---

TEST(DeleteFace, EmptiedShellIsDropped)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sheetId = createNonParametricSheet(pDb.get(), twoBoxShells());
    ASSERT_FALSE(sheetId.isNull());

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    ASSERT_EQ(countFaces(pSheet->getShape()), 12);
    ASSERT_EQ(countShells(pSheet->getShape()), 2);
    ASSERT_NEAR(bodyArea(pSheet->getShape()), 1200.0, 1e-6);

    // Every face of the box at the origin; the far one keeps all six of its own
    const std::vector<std::uint32_t> firstBoxFaces = {
        findFaceIndexAt(pSheet->getShape(), 0, 0.0), findFaceIndexAt(pSheet->getShape(), 0, 10.0),
        findFaceIndexAt(pSheet->getShape(), 1, 0.0), findFaceIndexAt(pSheet->getShape(), 1, 10.0),
        findFaceIndexAt(pSheet->getShape(), 2, 0.0), findFaceIndexAt(pSheet->getShape(), 2, 10.0)
    };
    for (std::uint32_t faceIndex : firstBoxFaces)
    {
        ASSERT_NE(faceIndex, UINT_MAX);
    }

    ASSERT_NE(deleteFace(pDb.get(), sheetId, firstBoxFaces), nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), sheetId), 0u);

    // One shell left, not two: the emptied one is gone from the shape rather than standing there
    // empty, and the names of its faces and edges are gone with it
    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 6);
    EXPECT_EQ(countShells(pSheet->getShape()), 1);
    EXPECT_NEAR(bodyArea(pSheet->getShape()), 600.0, 1e-6);
    expectAllTopoNamed(pSheet);
}

// --- A solid wearing a sheet's clothes is turned away rather than hollowed out ---

TEST(DeleteFace, SolidHostIsRefused)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sheetId = createNonParametricSheet(pDb.get(), BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape());
    ASSERT_FALSE(sheetId.isNull());

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    ASSERT_EQ(pSheet->getShape().ShapeType(), TopAbs_ShapeEnum::TopAbs_SOLID);
    ASSERT_EQ(countFaces(pSheet->getShape()), 6);

    wy3d::DeleteFace* pDeleteFace = deleteFace(pDb.get(), sheetId, { 0 });
    ASSERT_NE(pDeleteFace, nullptr);

    // Refused, and the feature stays on the tree carrying the error like any other failed update
    EXPECT_EQ(getChainErrorCode(pDb.get(), pDeleteFace->getId()),
        static_cast<std::uint32_t>(wy3d::ErrorCode::TOPOSHAPE_GenerateShapeError));
    EXPECT_FALSE(pDeleteFace->isErased());

    // The host comes back untouched: still a closed solid of six faces, not an open one of five
    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(pSheet->getShape().ShapeType(), TopAbs_ShapeEnum::TopAbs_SOLID);
    EXPECT_EQ(countFaces(pSheet->getShape()), 6);
}

// --- The host regenerates: the deletion is put back on the new shape ---

TEST(DeleteFace, ChainUpdateOnHostRegenerate)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    wy3d::DeleteFace* pDeleteFace = deleteFace(pDb.get(), sheetId,
        { findFaceIndexAt(pSheet->getShape(), 0, 0.0) });
    ASSERT_NE(pDeleteFace, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), sheetId), 0u);

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 3);
    EXPECT_NEAR(bodyArea(pSheet->getShape()), 2500.0, 1e-6);

    // Deepen the extrusion: the sheet regenerates, and the wall has to go away again
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::ExtrudedSheet* pWrite = wy3d::ExtrudedSheet::cast(pTrans->getElementForWrite(sheetId));
        ASSERT_NE(pWrite, nullptr);
        EXPECT_EQ(pWrite->setDepth(20.0), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), sheetId), 0u);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pDeleteFace->getId()), 0u);
    EXPECT_EQ(countFaces(pSheet->getShape()), 3);
    EXPECT_NEAR(bodyArea(pSheet->getShape()), 5000.0, 1e-6);
    expectAllTopoNamed(pSheet);
}

// --- Erasing the sheet takes the feature with it, and the other way round ---

TEST(DeleteFace, EraseSheetErasesFeature)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    wy3d::DeleteFace* pDeleteFace = deleteFace(pDb.get(), sheetId,
        { findFaceIndexAt(pSheet->getShape(), 0, 0.0) });
    ASSERT_NE(pDeleteFace, nullptr);
    EXPECT_FALSE(pDeleteFace->isErased());

    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sheet* pSheetWrite = wy3d::Sheet::cast(pTrans->getElementForWrite(sheetId));
        ASSERT_NE(pSheetWrite, nullptr);
        EXPECT_EQ(pSheetWrite->erase(true), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    EXPECT_TRUE(pDeleteFace->isErased());
}

// --- What is left of the sheet still feeds the features downstream ---

TEST(DeleteFace, DownstreamThicken)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    // Two identical sheets, one of them missing a wall
    wydb::ElementId sketchAId = createRectSketch(pDb.get());
    wydb::ElementId sheetAId = createExtrudedSheet(pDb.get(), sketchAId, 10.0);
    wydb::ElementId sketchBId = createRectSketch(pDb.get());
    wydb::ElementId sheetBId = createExtrudedSheet(pDb.get(), sketchBId, 10.0);

    const wy3d::Sheet* pSheetB = wy3d::Sheet::cast(pDb->getElement(sheetBId));
    ASSERT_NE(pSheetB, nullptr);
    ASSERT_NE(deleteFace(pDb.get(), sheetBId, { findFaceIndexAt(pSheetB->getShape(), 0, 0.0) }), nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), sheetBId), 0u);

    wydb::ElementId solidAId = wydb::ElementId::kNull;
    wydb::ElementId solidBId = wydb::ElementId::kNull;
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();

        wy3d::Sheet* pSheetA = wy3d::Sheet::cast(pTrans->getElementForWrite(sheetAId));
        ASSERT_NE(pSheetA, nullptr);
        wy3d::Thicken* pSolidA(nullptr);
        EXPECT_EQ(wy3d::Thicken::create(pTrans, pSheetA, 5.0, wy3d::ThickenDirection::OneSide, pSolidA),
            wy::ErrorStatus::Ok);
        ASSERT_NE(pSolidA, nullptr);
        solidAId = pSolidA->getId();

        wy3d::Sheet* pSheetBWrite = wy3d::Sheet::cast(pTrans->getElementForWrite(sheetBId));
        ASSERT_NE(pSheetBWrite, nullptr);
        wy3d::Thicken* pSolidB(nullptr);
        EXPECT_EQ(wy3d::Thicken::create(pTrans, pSheetBWrite, 5.0, wy3d::ThickenDirection::OneSide, pSolidB),
            wy::ErrorStatus::Ok);
        ASSERT_NE(pSolidB, nullptr);
        solidBId = pSolidB->getId();

        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    EXPECT_EQ(getChainErrorCode(pDb.get(), solidAId), 0u);
    EXPECT_EQ(getChainErrorCode(pDb.get(), solidBId), 0u);

    const wy3d::Solid* pSolidA = wy3d::Solid::cast(pDb->getElement(solidAId));
    const wy3d::Solid* pSolidB = wy3d::Solid::cast(pDb->getElement(solidBId));
    ASSERT_NE(pSolidA, nullptr);
    ASSERT_NE(pSolidB, nullptr);
    EXPECT_LT(bodyArea(pSolidB->getShape()), bodyArea(pSolidA->getShape()));
    expectAllTopoNamed(pSolidA);
    expectAllTopoNamed(pSolidB);
}

// --- Out and back in ---

TEST(DeleteFace, IO)
{
    std::string filePath("./test_delete_face.wy3dt");
    wydb::ElementId sketchId = wydb::ElementId::kNull;
    wydb::ElementId sheetId = wydb::ElementId::kNull;
    wydb::ElementId deleteFaceId = wydb::ElementId::kNull;

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        sketchId = createRectSketch(pDb.get());
        sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

        const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
        ASSERT_NE(pSheet, nullptr);
        wy3d::DeleteFace* pDeleteFace = deleteFace(pDb.get(), sheetId,
            { findFaceIndexAt(pSheet->getShape(), 0, 0.0) });
        ASSERT_NE(pDeleteFace, nullptr);
        deleteFaceId = pDeleteFace->getId();

        EXPECT_EQ(pDb->writeFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);
    }

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        EXPECT_EQ(pDb->readFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);

        const wy3d::DeleteFace* pDeleteFace = wy3d::DeleteFace::cast(pDb->getElement(deleteFaceId));
        ASSERT_NE(pDeleteFace, nullptr);
        EXPECT_EQ(pDeleteFace->getParent(), sheetId);
        EXPECT_FALSE(pDeleteFace->getFaces().empty());

        const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
        ASSERT_NE(pSheet, nullptr);
        std::vector<wydb::ElementId> children = pSheet->getChildren();
        EXPECT_NE(std::find(children.cbegin(), children.cend(), deleteFaceId), children.cend());

        EXPECT_EQ(countFaces(pSheet->getShape()), 3);
        EXPECT_EQ(countShells(pSheet->getShape()), 1);
        EXPECT_NEAR(bodyArea(pSheet->getShape()), 2500.0, 1e-6);
        expectAllTopoNamed(pSheet);
    }
}

// --- One undo step in and out ---

TEST(DeleteFace, UndoRedo)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    ASSERT_NE(deleteFace(pDb.get(), sheetId, { findFaceIndexAt(pSheet->getShape(), 0, 0.0) }), nullptr);

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    ASSERT_EQ(countFaces(pSheet->getShape()), 3);

    EXPECT_EQ(pMgr->undo(), wy::ErrorStatus::Ok);
    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 4);
    EXPECT_NEAR(bodyArea(pSheet->getShape()), 3000.0, 1e-6);

    EXPECT_EQ(pMgr->redo(), wy::ErrorStatus::Ok);
    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 3);
    EXPECT_NEAR(bodyArea(pSheet->getShape()), 2500.0, 1e-6);
    expectAllTopoNamed(pSheet);
}

// --- Bad arguments ---

TEST(DeleteFace, NullArgsAndEmptyInput)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

    const std::size_t numElements = countElements(pDb.get());

    wy3d::DeleteFace* pDeleteFace(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sheet* pSheet = wy3d::Sheet::cast(pTrans->getElementForWrite(sheetId));
        ASSERT_NE(pSheet, nullptr);
        const std::uint32_t wallIndex = findFaceIndexAt(pSheet->getShape(), 0, 0.0);
        ASSERT_NE(wallIndex, UINT_MAX);

        EXPECT_EQ(wy3d::DeleteFace::create(nullptr, pSheet, { wallIndex }, pDeleteFace),
            wy::ErrorStatus::NullTransactionPointer);
        EXPECT_EQ(wy3d::DeleteFace::create(pTrans, nullptr, { wallIndex }, pDeleteFace),
            wy::ErrorStatus::NullElementPointer);
        EXPECT_EQ(wy3d::DeleteFace::create(pTrans, pSheet, {}, pDeleteFace),
            wy::ErrorStatus::InvalidInput);
        EXPECT_EQ(pDeleteFace, nullptr);

        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    EXPECT_EQ(countElements(pDb.get()), numElements);
}
