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
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include <wy3dMove.h>
#include <wy3dRotate.h>
#include <wy3dMirror.h>
#include <wy3dSheet.h>
#include <wy3dSolid.h>
#include <wy3dSketch.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchPlane.h>
#include <wy3dFilledSheet.h>
#include <wy3dExtrudedSheet.h>
#include <wy3dNonParametricSheet.h>
#include <wy3dSplitFace.h>
#include <wy3dSketch3D.h>
#include <wy3dSketchLine3D.h>
#include <wy3dTopoName.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>

#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <TopoDS.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>

// Move / Rotate / Mirror all ask only for a TopoDS_Shape to work on, so the body they are handed
// can be a solid or a sheet - the host is data, not a type. These tests hold them to that on
// sheets: translate and rotate replace the sheet's shape, while the mirror appends a mirrored copy
// of it (COMPOUND[original, copy]) and leaves every original face where it was.
//
// A 100 x 50 rectangle sketch fills to a sheet of one face and 5000 of area; extruding it 10 deep
// gives the four walls without caps, 3000 of area.

namespace
{
    static const double kRectArea = 5000.0;

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

    static wy::Vector3 centreOfMass(const TopoDS_Shape& shape)
    {
        GProp_GProps gprops;
        BRepGProp::SurfaceProperties(shape, gprops);
        const gp_Pnt& centre = gprops.CentreOfMass();
        return wy::Vector3(centre.X(), centre.Y(), centre.Z());
    }

    static double minCoord(const TopoDS_Shape& shape, int axis)
    {
        assert(0 <= axis && axis <= 2);
        Bnd_Box box;
        BRepBndLib::Add(shape, box);
        const gp_Pnt& corner = box.CornerMin();
        const double corners[3] = { corner.X(), corner.Y(), corner.Z() };
        return corners[axis];
    }

    static double maxCoord(const TopoDS_Shape& shape, int axis)
    {
        assert(0 <= axis && axis <= 2);
        Bnd_Box box;
        BRepBndLib::Add(shape, box);
        const gp_Pnt& corner = box.CornerMax();
        const double corners[3] = { corner.X(), corner.Y(), corner.Z() };
        return corners[axis];
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

    static wy3d::TopoNameList faceNames(const wy3d::Sheet* pSheet)
    {
        const wy3d::TopoNaming* pTopoNaming = pSheet->getTopoNaming();
        wy3d::TopoNameList names;
        TopTools_IndexedMapOfShape faces;
        TopExp::MapShapes(pSheet->getShape(), TopAbs_ShapeEnum::TopAbs_FACE, faces);
        for (int i = 1; i <= faces.Extent(); ++i)
        {
            names.emplace_back(pTopoNaming->getTopoName(faces(i)));
        }
        return names;
    }

    template<typename Body>
    static void expectFaceNamesUnchanged(const Body* pBody, const wy3d::TopoNameList& expected)
    {
        TopTools_IndexedMapOfShape faces;
        TopExp::MapShapes(pBody->getShape(), TopAbs_ShapeEnum::TopAbs_FACE, faces);
        ASSERT_EQ(static_cast<int>(expected.size()), faces.Extent());
        for (int i = 1; i <= faces.Extent(); ++i)
        {
            EXPECT_EQ(pBody->getTopoNaming()->getTopoName(faces(i)), expected[i - 1]) << "face " << i;
        }
    }

    // A 100 x 50 closed rectangle on the plane z = z
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

    // The host has to be opened for write before a modification can hang under it
    static wy3d::Move* moveSheet(wy3d::Database* pDb, const wydb::ElementId& sheetId, const wy::Vector3& moveVec)
    {
        wy3d::Move* pMove(nullptr);
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Sheet* pSheet = wy3d::Sheet::cast(pTrans->getElementForWrite(sheetId));
            if (!pSheet)
            {
                pDb->getTransactionManager()->abortTransaction();
                return nullptr;
            }
            EXPECT_EQ(wy3d::Move::create(pTrans, pSheet, moveVec, pMove), wy::ErrorStatus::Ok);
            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
        }
        return pMove;
    }

    static wy3d::Rotate* rotateSheet(wy3d::Database* pDb, const wydb::ElementId& sheetId,
        const wy::Vector3& center, const wy::Vector3& axis, double angle)
    {
        wy3d::Rotate* pRotate(nullptr);
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Sheet* pSheet = wy3d::Sheet::cast(pTrans->getElementForWrite(sheetId));
            if (!pSheet)
            {
                pDb->getTransactionManager()->abortTransaction();
                return nullptr;
            }
            EXPECT_EQ(wy3d::Rotate::create(pTrans, pSheet, center, axis, angle, pRotate), wy::ErrorStatus::Ok);
            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
        }
        return pRotate;
    }

    static wy3d::Mirror* mirrorSheet(wy3d::Database* pDb, const wydb::ElementId& sheetId,
        const wy3d::SketchPlane& plane)
    {
        wy3d::Mirror* pMirror(nullptr);
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Sheet* pSheet = wy3d::Sheet::cast(pTrans->getElementForWrite(sheetId));
            if (!pSheet)
            {
                pDb->getTransactionManager()->abortTransaction();
                return nullptr;
            }
            EXPECT_EQ(wy3d::Mirror::create(pTrans, pSheet, plane, pMirror), wy::ErrorStatus::Ok);
            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
        }
        return pMirror;
    }

    // The plane z = 0, so a mirror flips z
    static wy3d::SketchPlane planeXY()
    {
        return wy3d::SketchPlane(wy::Vector3(0.0, 0.0, 0.0), wy::Vector3::kZAxis, wy::Vector3::kXAxis);
    }

    // The plane x = 0, so a mirror flips x
    static wy3d::SketchPlane planeYZ()
    {
        return wy3d::SketchPlane(wy::Vector3(0.0, 0.0, 0.0), wy::Vector3::kXAxis, wy::Vector3::kYAxis);
    }
}

// --- Translate on a sheet host ---

TEST(MoveSheet, TranslateExtrudedSheet)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    ASSERT_EQ(countFaces(pSheet->getShape()), 4);
    const wy3d::TopoNameList namesBefore = faceNames(pSheet);

    wy3d::Move* pMove = moveSheet(pDb.get(), sheetId, wy::Vector3(30.0, -20.0, 5.0));
    ASSERT_NE(pMove, nullptr);

    EXPECT_EQ(getChainErrorCode(pDb.get(), sheetId), 0u);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pMove->getId()), 0u);
    EXPECT_EQ(pMove->getParent(), sheetId);

    // The host keeps its own children view: the feature hangs under the sheet
    std::vector<wydb::ElementId> children = pSheet->getChildren();
    EXPECT_NE(std::find(children.cbegin(), children.cend(), pMove->getId()), children.cend());

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 4);
    EXPECT_EQ(countShells(pSheet->getShape()), 1);
    EXPECT_NEAR(bodyArea(pSheet->getShape()), 3000.0, 1e-6);
    EXPECT_NEAR(minCoord(pSheet->getShape(), 0), 30.0, 1e-6);
    EXPECT_NEAR(minCoord(pSheet->getShape(), 1), -20.0, 1e-6);
    EXPECT_NEAR(minCoord(pSheet->getShape(), 2), 5.0, 1e-6);
    expectFaceNamesUnchanged(pSheet, namesBefore);
    expectAllTopoNamed(pSheet);
}

// A parametric host rebuilds its shape from its parameters, so a second vector moves the sheet
// from where it was built, not from where the first vector left it
TEST(MoveSheet, ChangeVectorDoesNotAccumulate)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

    wy3d::Move* pMove = moveSheet(pDb.get(), sheetId, wy::Vector3(100.0, 0.0, 0.0));
    ASSERT_NE(pMove, nullptr);

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_NEAR(minCoord(pSheet->getShape(), 0), 100.0, 1e-6);

    for (double x : { 200.0, 50.0 })
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Move* pMoveWrite = wy3d::Move::cast(pTrans->getElementForWrite(pMove->getId()));
        ASSERT_NE(pMoveWrite, nullptr);
        EXPECT_EQ(pMoveWrite->setVector(wy::Vector3(x, 0.0, 0.0)), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);

        pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
        ASSERT_NE(pSheet, nullptr);
        EXPECT_NEAR(minCoord(pSheet->getShape(), 0), x, 1e-6) << "move x = " << x;
        EXPECT_EQ(countFaces(pSheet->getShape()), 4);
    }
}

// A non-parametric sheet re-runs from the shape it was created with, and that shape is what a
// reload has to start from
TEST(MoveSheet, NonParametricHostRerunsFromSource)
{
    const std::string filePath("./test_move_sheet_nonparam.wy3dt");

    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sheetId = wydb::ElementId::kNull;
    wydb::ElementId moveId = wydb::ElementId::kNull;
    {
        BRepBuilderAPI_MakeFace face(
            gp_Pln(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0)), 0.0, 100.0, 0.0, 50.0);
        sheetId = createNonParametricSheet(pDb.get(), face.Shape());
        ASSERT_NE(sheetId, wydb::ElementId::kNull);

        wy3d::Move* pMove = moveSheet(pDb.get(), sheetId, wy::Vector3(100.0, 0.0, 0.0));
        ASSERT_NE(pMove, nullptr);
        moveId = pMove->getId();

        const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
        ASSERT_NE(pSheet, nullptr);
        EXPECT_EQ(countFaces(pSheet->getShape()), 1);
        EXPECT_NEAR(minCoord(pSheet->getShape(), 0), 100.0, 1e-6);
        EXPECT_EQ(getChainErrorCode(pDb.get(), sheetId), 0u);

        EXPECT_EQ(pDb->writeFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);
    }

    {
        std::unique_ptr<wy3d::Database> pReloadedDb = std::make_unique<wy3d::Database>();
        ASSERT_EQ(pReloadedDb->readFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);
        std::remove(filePath.c_str());
        wydb::TransactionManager* pMgr = pReloadedDb->getTransactionManager();

        const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pReloadedDb->getElement(sheetId));
        ASSERT_NE(pSheet, nullptr);
        EXPECT_NEAR(minCoord(pSheet->getShape(), 0), 100.0, 1e-6);

        // Each new vector moves the sheet from where it was built, not from where the last one
        // left it
        for (double x : { 300.0, 20.0 })
        {
            wydb::Transaction* pTrans = pMgr->startTransaction();
            wy3d::Move* pMoveWrite = wy3d::Move::cast(pTrans->getElementForWrite(moveId));
            ASSERT_NE(pMoveWrite, nullptr);
            EXPECT_EQ(pMoveWrite->setVector(wy::Vector3(x, 0.0, 0.0)), wy::ErrorStatus::Ok);
            EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);

            pSheet = wy3d::Sheet::cast(pReloadedDb->getElement(sheetId));
            ASSERT_NE(pSheet, nullptr);
            EXPECT_NEAR(minCoord(pSheet->getShape(), 0), x, 1e-6) << "move x = " << x;
            EXPECT_EQ(countFaces(pSheet->getShape()), 1);
        }
    }
}

TEST(MoveSheet, UndoRedo)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

    ASSERT_NE(moveSheet(pDb.get(), sheetId, wy::Vector3(100.0, 0.0, 0.0)), nullptr);

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    ASSERT_NEAR(minCoord(pSheet->getShape(), 0), 100.0, 1e-6);

    EXPECT_EQ(pMgr->undo(), wy::ErrorStatus::Ok);
    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_NEAR(minCoord(pSheet->getShape(), 0), 0.0, 1e-6);
    EXPECT_EQ(countFaces(pSheet->getShape()), 4);

    EXPECT_EQ(pMgr->redo(), wy::ErrorStatus::Ok);
    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_NEAR(minCoord(pSheet->getShape(), 0), 100.0, 1e-6);
    expectAllTopoNamed(pSheet);
}

TEST(MoveSheet, IO)
{
    const std::string filePath("./test_move_sheet.wy3dt");
    wydb::ElementId sheetId = wydb::ElementId::kNull;
    wydb::ElementId moveId = wydb::ElementId::kNull;
    wy3d::TopoNameList namesBefore;

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        wydb::ElementId sketchId = createRectSketch(pDb.get());
        sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

        const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
        ASSERT_NE(pSheet, nullptr);
        namesBefore = faceNames(pSheet);

        wy3d::Move* pMove = moveSheet(pDb.get(), sheetId, wy::Vector3(0.0, 40.0, 0.0));
        ASSERT_NE(pMove, nullptr);
        moveId = pMove->getId();

        EXPECT_EQ(pDb->writeFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);
    }

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        ASSERT_EQ(pDb->readFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);
        std::remove(filePath.c_str());

        const wy3d::Move* pMove = wy3d::Move::cast(pDb->getElement(moveId));
        ASSERT_NE(pMove, nullptr);
        EXPECT_EQ(pMove->getParent(), sheetId);
        EXPECT_DOUBLE_EQ(pMove->getVector().y(), 40.0);

        const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
        ASSERT_NE(pSheet, nullptr);
        EXPECT_EQ(countFaces(pSheet->getShape()), 4);
        EXPECT_NEAR(minCoord(pSheet->getShape(), 1), 40.0, 1e-6);
        expectFaceNamesUnchanged(pSheet, namesBefore);
        expectAllTopoNamed(pSheet);
    }
}

// --- Rotate on a sheet host ---

TEST(RotateSheet, RotateFilledSheet)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createFilledSheet(pDb.get(), sketchId);

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    ASSERT_EQ(countFaces(pSheet->getShape()), 1);
    const wy3d::TopoNameList namesBefore = faceNames(pSheet);
    EXPECT_NEAR(centreOfMass(pSheet->getShape()).x(), 50.0, 1e-6);
    EXPECT_NEAR(centreOfMass(pSheet->getShape()).y(), 25.0, 1e-6);

    // A quarter turn about the z axis through the origin
    wy3d::Rotate* pRotate = rotateSheet(pDb.get(), sheetId,
        wy::Vector3(0.0, 0.0, 0.0), wy::Vector3::kZAxis, wy3d::PI / 2.0);
    ASSERT_NE(pRotate, nullptr);

    EXPECT_EQ(getChainErrorCode(pDb.get(), sheetId), 0u);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pRotate->getId()), 0u);
    EXPECT_EQ(pRotate->getParent(), sheetId);

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 1);
    EXPECT_NEAR(bodyArea(pSheet->getShape()), kRectArea, 1e-6);
    EXPECT_NEAR(centreOfMass(pSheet->getShape()).x(), -25.0, 1e-6);
    EXPECT_NEAR(centreOfMass(pSheet->getShape()).y(), 50.0, 1e-6);
    expectFaceNamesUnchanged(pSheet, namesBefore);
    expectAllTopoNamed(pSheet);

    // The angle is what the sheet re-runs from, not a stack of turns
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Rotate* pRotateWrite = wy3d::Rotate::cast(pTrans->getElementForWrite(pRotate->getId()));
        ASSERT_NE(pRotateWrite, nullptr);
        EXPECT_EQ(pRotateWrite->setAngle(wy3d::PI / 4.0), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    const double cosA = std::cos(wy3d::PI / 4.0);
    const double sinA = std::sin(wy3d::PI / 4.0);
    EXPECT_NEAR(centreOfMass(pSheet->getShape()).x(), 50.0 * cosA - 25.0 * sinA, 1e-6);
    EXPECT_NEAR(centreOfMass(pSheet->getShape()).y(), 50.0 * sinA + 25.0 * cosA, 1e-6);

    EXPECT_EQ(pMgr->undo(), wy::ErrorStatus::Ok);
    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_NEAR(centreOfMass(pSheet->getShape()).x(), -25.0, 1e-6);

    EXPECT_EQ(pMgr->redo(), wy::ErrorStatus::Ok);
    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_NEAR(centreOfMass(pSheet->getShape()).x(), 50.0 * cosA - 25.0 * sinA, 1e-6);
    expectAllTopoNamed(pSheet);
}

TEST(RotateSheet, IO)
{
    const std::string filePath("./test_rotate_sheet.wy3dt");
    wydb::ElementId sheetId = wydb::ElementId::kNull;
    wydb::ElementId rotateId = wydb::ElementId::kNull;
    wy3d::TopoNameList namesBefore;

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        wydb::ElementId sketchId = createRectSketch(pDb.get());
        sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

        const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
        ASSERT_NE(pSheet, nullptr);
        namesBefore = faceNames(pSheet);

        wy3d::Rotate* pRotate = rotateSheet(pDb.get(), sheetId,
            wy::Vector3(0.0, 0.0, 0.0), wy::Vector3::kZAxis, wy3d::PI / 2.0);
        ASSERT_NE(pRotate, nullptr);
        rotateId = pRotate->getId();

        EXPECT_EQ(pDb->writeFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);
    }

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        ASSERT_EQ(pDb->readFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);
        std::remove(filePath.c_str());

        const wy3d::Rotate* pRotate = wy3d::Rotate::cast(pDb->getElement(rotateId));
        ASSERT_NE(pRotate, nullptr);
        EXPECT_EQ(pRotate->getParent(), sheetId);
        EXPECT_NEAR(pRotate->getAngle(), wy3d::PI / 2.0, 1e-9);

        const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
        ASSERT_NE(pSheet, nullptr);
        EXPECT_EQ(countFaces(pSheet->getShape()), 4);
        EXPECT_NEAR(bodyArea(pSheet->getShape()), 3000.0, 1e-6);
        EXPECT_NEAR(minCoord(pSheet->getShape(), 1), 0.0, 1e-6);
        EXPECT_NEAR(maxCoord(pSheet->getShape(), 0), 0.0, 1e-6);
        EXPECT_NEAR(maxCoord(pSheet->getShape(), 1), 100.0, 1e-6);
        expectFaceNamesUnchanged(pSheet, namesBefore);
        expectAllTopoNamed(pSheet);
    }
}

// --- Mirror on a sheet host appends a copy and leaves the original alone ---

TEST(MirrorSheet, AppendsOneCopyOnFilledSheet)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createFilledSheet(pDb.get(), sketchId);

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    ASSERT_EQ(countFaces(pSheet->getShape()), 1);
    ASSERT_EQ(countShells(pSheet->getShape()), 1);
    const wy3d::TopoNameList namesBefore = faceNames(pSheet);
    ASSERT_EQ(namesBefore.size(), 1u);
    const wy3d::TopoName& sourceName = namesBefore.front();

    wy3d::Mirror* pMirror = mirrorSheet(pDb.get(), sheetId, planeYZ());
    ASSERT_NE(pMirror, nullptr);

    EXPECT_EQ(getChainErrorCode(pDb.get(), sheetId), 0u);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pMirror->getId()), 0u);
    EXPECT_EQ(pMirror->getParent(), sheetId);
    EXPECT_TRUE(pMirror->getSource().isNull());
    EXPECT_EQ(pMirror->getNewFaceIndices().size(), 1u);

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    const TopoDS_Shape& shape = pSheet->getShape();

    // The original face and the mirrored copy, the copy on the other side of the plane
    EXPECT_EQ(pSheet->getShape().ShapeType(), TopAbs_ShapeEnum::TopAbs_COMPOUND);
    EXPECT_EQ(countFaces(shape), 2);
    EXPECT_EQ(countShells(shape), 2);
    EXPECT_NEAR(bodyArea(shape), 2.0 * kRectArea, 1e-6);
    EXPECT_NEAR(minCoord(shape, 0), -100.0, 1e-6);
    EXPECT_NEAR(maxCoord(shape, 0), 100.0, 1e-6);

    // The original face keeps its name, the copy is named after it and carries the mirror's id
    // exactly once (a name that got the suffix twice would come back here with two of them)
    const wy3d::TopoNaming* pTopoNaming = pSheet->getTopoNaming();
    ASSERT_NE(pTopoNaming, nullptr);
    TopTools_IndexedMapOfShape faces;
    TopExp::MapShapes(shape, TopAbs_ShapeEnum::TopAbs_FACE, faces);
    ASSERT_EQ(faces.Extent(), 2);
    const wy3d::TopoName name1 = pTopoNaming->getTopoName(faces(1));
    const wy3d::TopoName name2 = pTopoNaming->getTopoName(faces(2));
    ASSERT_TRUE((name1 == sourceName) != (name2 == sourceName)) << "one of the two keeps the name";
    const wy3d::TopoName& copyName = (name1 == sourceName) ? name2 : name1;

    EXPECT_EQ(copyName.compare(0, sourceName.size(), sourceName), 0);
    std::vector<std::uint32_t> ids;
    ASSERT_TRUE(wy3d::TopoNameCodec::extractIds(copyName, ids));
    EXPECT_EQ(std::count(ids.cbegin(), ids.cend(), pMirror->getId().value()), 1);
    expectAllTopoNamed(pSheet);
}

TEST(MirrorSheet, AppendsOneCopyOnExtrudedSheet)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    ASSERT_EQ(countFaces(pSheet->getShape()), 4);
    const wy3d::TopoNameList namesBefore = faceNames(pSheet);
    ASSERT_EQ(namesBefore.size(), 4u);

    wy3d::Mirror* pMirror = mirrorSheet(pDb.get(), sheetId, planeYZ());
    ASSERT_NE(pMirror, nullptr);
    EXPECT_EQ(pMirror->getNewFaceIndices().size(), 4u);

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 8);
    EXPECT_EQ(countShells(pSheet->getShape()), 2);
    EXPECT_NEAR(bodyArea(pSheet->getShape()), 6000.0, 1e-6);
    EXPECT_NEAR(minCoord(pSheet->getShape(), 0), -100.0, 1e-6);
    EXPECT_NEAR(maxCoord(pSheet->getShape(), 0), 100.0, 1e-6);

    // The four walls of the original are all still there, under their own names
    const wy3d::TopoNaming* pTopoNaming = pSheet->getTopoNaming();
    ASSERT_NE(pTopoNaming, nullptr);
    for (const wy3d::TopoName& name : namesBefore)
    {
        const TopoDS_Shape& face = pTopoNaming->find(TopAbs_ShapeEnum::TopAbs_FACE, name);
        EXPECT_FALSE(face.IsNull()) << "original face name got lost: " << name;
    }
    expectAllTopoNamed(pSheet);
}

// What the mirror re-runs from is the host's own shape, so a second plane gives a second copy of
// the original - not a copy of the last result, and not a leftover from it
TEST(MirrorSheet, ChangePlaneRerunsFromTheHostShape)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createFilledSheet(pDb.get(), sketchId);

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    const wy3d::TopoNameList namesBefore = faceNames(pSheet);

    wy3d::Mirror* pMirror = mirrorSheet(pDb.get(), sheetId, planeXY());
    ASSERT_NE(pMirror, nullptr);

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    ASSERT_EQ(countFaces(pSheet->getShape()), 2);
    ASSERT_NEAR(maxCoord(pSheet->getShape(), 2), 0.0, 1e-6);

    // Another plane: the copy goes up to z = 60, the original stays at z = 0
    {
        const wy3d::SketchPlane plane(wy::Vector3(0.0, 0.0, 30.0), wy::Vector3::kZAxis, wy::Vector3::kXAxis);
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Mirror* pMirrorWrite = wy3d::Mirror::cast(pTrans->getElementForWrite(pMirror->getId()));
        ASSERT_NE(pMirrorWrite, nullptr);
        EXPECT_EQ(pMirrorWrite->setPlane(plane), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), sheetId), 0u);
    EXPECT_EQ(countFaces(pSheet->getShape()), 2);
    EXPECT_EQ(countShells(pSheet->getShape()), 2);
    EXPECT_NEAR(bodyArea(pSheet->getShape()), 2.0 * kRectArea, 1e-6);
    EXPECT_NEAR(minCoord(pSheet->getShape(), 2), 0.0, 1e-6);
    EXPECT_NEAR(maxCoord(pSheet->getShape(), 2), 60.0, 1e-6);

    // The copy follows, the original is still the only face at z = 0
    const wy3d::TopoNaming* pTopoNaming = pSheet->getTopoNaming();
    ASSERT_NE(pTopoNaming, nullptr);
    EXPECT_FALSE(pTopoNaming->find(TopAbs_ShapeEnum::TopAbs_FACE, namesBefore.front()).IsNull());
    expectAllTopoNamed(pSheet);
}

TEST(MirrorSheet, EraseFeatureRestoresHost)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    const wy3d::TopoNameList namesBefore = faceNames(pSheet);

    wy3d::Mirror* pMirror = mirrorSheet(pDb.get(), sheetId, planeXY());
    ASSERT_NE(pMirror, nullptr);
    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    ASSERT_EQ(countFaces(pSheet->getShape()), 8);

    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wydb::Element* pMirrorWrite = pTrans->getElementForWrite(pMirror->getId());
        ASSERT_NE(pMirrorWrite, nullptr);
        EXPECT_EQ(pMirrorWrite->erase(true), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    EXPECT_TRUE(pMirror->isErased());

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 4);
    EXPECT_EQ(countShells(pSheet->getShape()), 1);
    EXPECT_NEAR(bodyArea(pSheet->getShape()), 3000.0, 1e-6);
    expectFaceNamesUnchanged(pSheet, namesBefore);
    expectAllTopoNamed(pSheet);
}

// The copy's faces carry names like any other, so a feature hung under the sheet can find them
TEST(MirrorSheet, DownstreamSplitFaceOnTheCopy)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    // The sheet sits at z = 10, its mirror lands at z = -10
    wydb::ElementId sketchId = createRectSketchAtZ(pDb.get(), 10.0);
    wydb::ElementId sheetId = createFilledSheet(pDb.get(), sketchId);

    wy3d::Mirror* pMirror = mirrorSheet(pDb.get(), sheetId, planeXY());
    ASSERT_NE(pMirror, nullptr);

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    ASSERT_EQ(countFaces(pSheet->getShape()), 2);
    const std::uint32_t copyFaceIndex = findFaceIndexAt(pSheet->getShape(), 2, -10.0);
    ASSERT_NE(copyFaceIndex, UINT_MAX);

    // A line across the copy, in its own plane
    wydb::ElementId curveSketchId = createEmptySketch3D(pDb.get());
    addLine(pDb.get(), curveSketchId,
        wy::Vector3(0.0, 25.0, -10.0), wy::Vector3(100.0, 25.0, -10.0));

    wy3d::SplitFace* pSplitFace(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sheet* pSheetWrite = wy3d::Sheet::cast(pTrans->getElementForWrite(sheetId));
        ASSERT_NE(pSheetWrite, nullptr);
        wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(curveSketchId));
        ASSERT_NE(pSketch3D, nullptr);
        EXPECT_EQ(wy3d::SplitFace::create(pTrans, pSheetWrite, { copyFaceIndex }, pSketch3D, pSplitFace),
            wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSplitFace, nullptr);

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), sheetId), 0u);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pSplitFace->getId()), 0u);
    EXPECT_EQ(countFaces(pSheet->getShape()), 3);
    EXPECT_EQ(countShells(pSheet->getShape()), 2);
    expectAllTopoNamed(pSheet);
}

TEST(MirrorSheet, IO)
{
    const std::string filePath("./test_mirror_sheet.wy3dt");
    wydb::ElementId sheetId = wydb::ElementId::kNull;
    wydb::ElementId mirrorId = wydb::ElementId::kNull;
    wy3d::TopoNameList namesBefore;

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        wydb::ElementId sketchId = createRectSketch(pDb.get());
        sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

        const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
        ASSERT_NE(pSheet, nullptr);
        namesBefore = faceNames(pSheet);

        wy3d::Mirror* pMirror = mirrorSheet(pDb.get(), sheetId, planeYZ());
        ASSERT_NE(pMirror, nullptr);
        mirrorId = pMirror->getId();

        EXPECT_EQ(pDb->writeFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);
    }

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        ASSERT_EQ(pDb->readFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);
        std::remove(filePath.c_str());

        const wy3d::Mirror* pMirror = wy3d::Mirror::cast(pDb->getElement(mirrorId));
        ASSERT_NE(pMirror, nullptr);
        EXPECT_EQ(pMirror->getParent(), sheetId);
        EXPECT_TRUE(pMirror->getSource().isNull());

        const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
        ASSERT_NE(pSheet, nullptr);
        EXPECT_EQ(countFaces(pSheet->getShape()), 8);
        EXPECT_EQ(countShells(pSheet->getShape()), 2);
        EXPECT_NEAR(bodyArea(pSheet->getShape()), 6000.0, 1e-6);

        std::vector<wydb::ElementId> children = pSheet->getChildren();
        EXPECT_NE(std::find(children.cbegin(), children.cend(), mirrorId), children.cend());
        for (const wy3d::TopoName& name : namesBefore)
        {
            EXPECT_FALSE(pSheet->getTopoNaming()->find(TopAbs_ShapeEnum::TopAbs_FACE, name).IsNull());
        }
        expectAllTopoNamed(pSheet);
    }
}

// --- Null and empty inputs ---

TEST(MirrorSheet, NullArgs)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);

    const std::size_t numElements = countElements(pDb.get());

    wy3d::Mirror* pMirror(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sheet* pSheet = wy3d::Sheet::cast(pTrans->getElementForWrite(sheetId));
        ASSERT_NE(pSheet, nullptr);

        EXPECT_EQ(wy3d::Mirror::create(nullptr, pSheet, planeYZ(), pMirror),
            wy::ErrorStatus::NullTransactionPointer);
        EXPECT_EQ(wy3d::Mirror::create(pTrans, static_cast<wy3d::Sheet*>(nullptr), planeYZ(), pMirror),
            wy::ErrorStatus::NullElementPointer);
        EXPECT_EQ(pMirror, nullptr);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    EXPECT_EQ(countElements(pDb.get()), numElements);
}

// --- The solid host of the same feature still goes through the boolean path ---

TEST(MirrorSheet, SolidHostStillUsesTheBooleanPath)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    // A box standing clear of the plane z = 0, so the mirror cannot merge with it
    wydb::ElementId boxId = wydb::ElementId::kNull;
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        Box* pBox(nullptr);
        EXPECT_EQ(Box::create(pTrans, 30.0, 20.0, 10.0, pBox), wy::ErrorStatus::Ok);
        ASSERT_NE(pBox, nullptr);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
        boxId = pBox->getId();
        EXPECT_NEAR(minCoord(pBox->getShape(), 2), 0.0, 1e-6);
    }

    const wy3d::Solid* pBox = wy3d::Solid::cast(pDb->getElement(boxId));
    ASSERT_NE(pBox, nullptr);
    const wy3d::TopoNameList namesBefore = [&pBox]()
    {
        wy3d::TopoNameList names;
        TopTools_IndexedMapOfShape faces;
        TopExp::MapShapes(pBox->getShape(), TopAbs_ShapeEnum::TopAbs_FACE, faces);
        for (int i = 1; i <= faces.Extent(); ++i)
        {
            names.emplace_back(pBox->getTopoNaming()->getTopoName(faces(i)));
        }
        return names;
    }();

    wy3d::Mirror* pMirror(nullptr);
    {
        const wy3d::SketchPlane plane(wy::Vector3(0.0, 0.0, 35.0), wy::Vector3::kZAxis, wy::Vector3::kXAxis);
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Solid* pBoxWrite = wy3d::Solid::cast(pTrans->getElementForWrite(boxId));
        ASSERT_NE(pBoxWrite, nullptr);
        EXPECT_EQ(wy3d::Mirror::create(pTrans, pBoxWrite, pBoxWrite, plane, pMirror), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pMirror, nullptr);

    pBox = wy3d::Solid::cast(pDb->getElement(boxId));
    ASSERT_NE(pBox, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), boxId), 0u);
    EXPECT_EQ(countFaces(pBox->getShape()), 12);
    EXPECT_EQ(countSolids(pBox->getShape()), 2);
    EXPECT_NEAR(bodyArea(pBox->getShape()), 2.0 * 2200.0, 1e-6);
    EXPECT_NEAR(minCoord(pBox->getShape(), 2), 0.0, 1e-6);
    EXPECT_NEAR(maxCoord(pBox->getShape(), 2), 70.0, 1e-6);
    for (const wy3d::TopoName& name : namesBefore)
    {
        EXPECT_FALSE(pBox->getTopoNaming()->find(TopAbs_ShapeEnum::TopAbs_FACE, name).IsNull());
    }
    expectAllTopoNamed(pBox);
}
