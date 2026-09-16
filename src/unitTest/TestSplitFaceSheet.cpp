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

#include <wy3dSplitFace.h>
#include <wy3dFilledSheet.h>
#include <wy3dExtrudedSheet.h>
#include <wy3dThicken.h>
#include <wy3dOffsetSheet.h>
#include <wy3dSolidify.h>
#include <wy3dSewnSheet.h>
#include <wy3dSketch.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchPlane.h>
#include <wy3dSketch3D.h>
#include <wy3dSketchLine3D.h>
#include <wy3dSketchCircle3D.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>

#include <TopoDS.hxx>
#include <TopoDS_Shape.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <gp_Pnt.hxx>

// The same imprint SplitFace makes on a solid's faces (TestSplitFace.cpp), but with a sheet as
// the host body. A sheet renders its faces the same way a solid does, so the feature has no
// reason to care which one it was handed - the tests below hold it to that.
//
// A 100 x 50 rectangle sketch fills to a sheet of one face and 5000 of area; extruding it 10
// deep gives the four walls without caps, 3000 of area (300 perimeter * 10). The curves live in
// a 3D sketch of their own, the way the Intersection Curve command leaves them.

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

    // Create a 2D sketch on the plane z = 0 with a 100x50 closed rectangle
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

    static wydb::ElementId addCircle(wy3d::Database* pDb, const wydb::ElementId& sketchId,
        const wy::Vector3& center, double radius)
    {
        wydb::ElementId circleId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(sketchId));
            if (!pSketch3D)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }

            wy3d::SketchCircle3D* pCircle(nullptr);
            EXPECT_EQ(wy3d::SketchCircle3D::create(pTrans, center, wy::Vector3::kZAxis, wy::Vector3::kXAxis,
                radius, pCircle), wy::ErrorStatus::Ok);
            if (!pCircle)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }
            EXPECT_EQ(pSketch3D->addEntity(pCircle), wy::ErrorStatus::Ok);
            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
            circleId = pCircle->getId();
        }
        return circleId;
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

    // The rectangle extruded depth deep along +Z: six faces, four of them walls
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

    // Sew the walls of an extruded rectangle to a filled cap on each end, which closes the
    // shell. The caps need sketches of their own: a sketch belongs to the one feature that
    // consumes it, and the walls already took theirs.
    static wydb::ElementId createClosedSheet(wy3d::Database* pDb, double depth,
        const wydb::ElementId& wallSheetId, wydb::ElementId& bottomCapId, wydb::ElementId& topCapId)
    {
        wydb::ElementId bottomSketchId = createRectSketchAtZ(pDb, 0.0);
        wydb::ElementId topSketchId = createRectSketchAtZ(pDb, depth);
        if (bottomSketchId.isNull() || topSketchId.isNull()) return wydb::ElementId::kNull;
        bottomCapId = createFilledSheet(pDb, bottomSketchId);
        topCapId = createFilledSheet(pDb, topSketchId);
        if (bottomCapId.isNull() || topCapId.isNull()) return wydb::ElementId::kNull;

        wydb::ElementId sewnId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Sheet* pWall = wy3d::Sheet::cast(pTrans->getElementForWrite(wallSheetId));
            wy3d::Sheet* pBottomCap = wy3d::Sheet::cast(pTrans->getElementForWrite(bottomCapId));
            wy3d::Sheet* pTopCap = wy3d::Sheet::cast(pTrans->getElementForWrite(topCapId));
            if (!pWall || !pBottomCap || !pTopCap)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }

            wy3d::SewnSheet* pSewn(nullptr);
            EXPECT_EQ(wy3d::SewnSheet::create(pTrans, { pWall, pBottomCap, pTopCap }, 1e-5, pSewn),
                wy::ErrorStatus::Ok);
            if (!pSewn)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }
            sewnId = pSewn->getId();
            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
        }
        return sewnId;
    }
}

// --- A filled sheet (a compound of one shell) takes the imprint like a solid does ---

TEST(SplitFaceSheet, SplitFilledSheetFaceWithOpenLine)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createFilledSheet(pDb.get(), sketchId);
    wydb::ElementId curveSketchId = createEmptySketch3D(pDb.get());
    wydb::ElementId lineId = addLine(pDb.get(), curveSketchId,
        wy::Vector3(0.0, 25.0, 0.0), wy::Vector3(100.0, 25.0, 0.0));

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 1);
    EXPECT_NEAR(bodyArea(pSheet->getShape()), 5000.0, 1e-6);

    wy3d::SplitFace* pSplitFace = splitFace(pDb.get(), sheetId,
        findFaceIndexAt(pSheet->getShape(), 2, 0.0), curveSketchId);
    ASSERT_NE(pSplitFace, nullptr);

    EXPECT_EQ(getChainErrorCode(pDb.get(), sheetId), 0u);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pSplitFace->getId()), 0u);
    EXPECT_EQ(pSplitFace->getParent(), sheetId);
    EXPECT_EQ(pSplitFace->getSketch(), curveSketchId);
    EXPECT_EQ(pSplitFace->getNewFaceIndices().size(), 2u);

    // The tool sketch hangs under the feature, the way a profile sketch does
    const wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pDb->getElement(curveSketchId));
    ASSERT_NE(pSketch3D, nullptr);
    EXPECT_EQ(pSketch3D->getParent(), pSplitFace->getId());

    // The host keeps its own children view: the feature hangs under the sheet
    std::vector<wydb::ElementId> children = pSheet->getChildren();
    EXPECT_NE(std::find(children.cbegin(), children.cend(), pSplitFace->getId()), children.cend());

    // The sheet stays a compound of one shell - a per-face wrapping would break the features
    // downstream that count shells (Solidify wants exactly one)
    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 2);
    EXPECT_NEAR(bodyArea(pSheet->getShape()), 5000.0, 1e-6);
    EXPECT_EQ(pSheet->getShape().ShapeType(), TopAbs_ShapeEnum::TopAbs_COMPOUND);
    EXPECT_EQ(countShells(pSheet->getShape()), 1);
    expectAllTopoNamed(pSheet);
}

// --- An extruded sheet is the same compound-of-one-shell host ---

TEST(SplitFaceSheet, SplitExtrudedSheetFaceWithOpenLine)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);
    wydb::ElementId curveSketchId = createEmptySketch3D(pDb.get());

    // Across the x = 0 wall, from its bottom edge to its top one
    wydb::ElementId lineId = addLine(pDb.get(), curveSketchId,
        wy::Vector3(0.0, 25.0, 0.0), wy::Vector3(0.0, 25.0, 10.0));

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 4);

    wy3d::SplitFace* pSplitFace = splitFace(pDb.get(), sheetId,
        findFaceIndexAt(pSheet->getShape(), 0, 0.0), curveSketchId);
    ASSERT_NE(pSplitFace, nullptr);

    EXPECT_EQ(getChainErrorCode(pDb.get(), sheetId), 0u);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pSplitFace->getId()), 0u);

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 5);
    EXPECT_NEAR(bodyArea(pSheet->getShape()), 3000.0, 1e-6);
    EXPECT_EQ(countShells(pSheet->getShape()), 1);
    expectAllTopoNamed(pSheet);
}

// --- A closed curve inside the face divides it too ---

TEST(SplitFaceSheet, SplitSheetFaceWithClosedCircle)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createFilledSheet(pDb.get(), sketchId);
    wydb::ElementId curveSketchId = createEmptySketch3D(pDb.get());
    wydb::ElementId circleId = addCircle(pDb.get(), curveSketchId, wy::Vector3(50.0, 25.0, 0.0), 10.0);

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);

    wy3d::SplitFace* pSplitFace = splitFace(pDb.get(), sheetId,
        findFaceIndexAt(pSheet->getShape(), 2, 0.0), curveSketchId);
    ASSERT_NE(pSplitFace, nullptr);

    EXPECT_EQ(getChainErrorCode(pDb.get(), sheetId), 0u);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pSplitFace->getId()), 0u);
    EXPECT_EQ(pSplitFace->getNewFaceIndices().size(), 2u);

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 2);
    EXPECT_NEAR(bodyArea(pSheet->getShape()), 5000.0, 1e-6);
    expectAllTopoNamed(pSheet);
}

// --- Erasure ---

TEST(SplitFaceSheet, EraseCurveAndSketch)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createFilledSheet(pDb.get(), sketchId);
    wydb::ElementId curveSketchId = createEmptySketch3D(pDb.get());
    wydb::ElementId line1Id = addLine(pDb.get(), curveSketchId,
        wy::Vector3(0.0, 15.0, 0.0), wy::Vector3(100.0, 15.0, 0.0));
    wydb::ElementId line2Id = addLine(pDb.get(), curveSketchId,
        wy::Vector3(0.0, 35.0, 0.0), wy::Vector3(100.0, 35.0, 0.0));

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);

    wy3d::SplitFace* pSplitFace = splitFace(pDb.get(), sheetId,
        findFaceIndexAt(pSheet->getShape(), 2, 0.0), curveSketchId);
    ASSERT_NE(pSplitFace, nullptr);

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 3);

    // Erasing one curve leaves the feature alive with one imprint fewer - the other line still
    // splits, so there is still something to do
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::SketchLine3D* pLineWrite = wy3d::SketchLine3D::cast(pTrans->getElementForWrite(line2Id));
        ASSERT_NE(pLineWrite, nullptr);
        EXPECT_EQ(pLineWrite->erase(true), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    EXPECT_FALSE(pSplitFace->isErased());

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 2);

    // Erasing the sketch takes the feature with it, and the sheet with it
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pSketchWrite = wy3d::Sketch3D::cast(pTrans->getElementForWrite(curveSketchId));
        ASSERT_NE(pSketchWrite, nullptr);
        EXPECT_EQ(pSketchWrite->erase(true), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    EXPECT_TRUE(pSplitFace->isErased());

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 1);
}

// --- Who owns the tool sketch: one feature at a time ---

TEST(SplitFaceSheet, SketchOwnership)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    // Two sheets of their own, and one 3D sketch of curves both would split with
    wydb::ElementId sketchAId = createRectSketch(pDb.get());
    wydb::ElementId sheetAId = createFilledSheet(pDb.get(), sketchAId);
    wydb::ElementId sketchBId = createRectSketch(pDb.get());
    wydb::ElementId sheetBId = createFilledSheet(pDb.get(), sketchBId);

    wydb::ElementId curveSketchId = createEmptySketch3D(pDb.get());
    wydb::ElementId lineId = addLine(pDb.get(), curveSketchId,
        wy::Vector3(0.0, 25.0, 0.0), wy::Vector3(100.0, 25.0, 0.0));

    const wy3d::Sheet* pSheetA = wy3d::Sheet::cast(pDb->getElement(sheetAId));
    ASSERT_NE(pSheetA, nullptr);
    wy3d::SplitFace* pSplitFaceA = splitFace(pDb.get(), sheetAId,
        findFaceIndexAt(pSheetA->getShape(), 2, 0.0), curveSketchId);
    ASSERT_NE(pSplitFaceA, nullptr);

    const wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pDb->getElement(curveSketchId));
    ASSERT_NE(pSketch3D, nullptr);
    EXPECT_EQ(pSketch3D->getParent(), pSplitFaceA->getId());

    // A second feature cannot take the same sketch: a sketch belongs to the one feature that
    // splits with it, so one that already has an owner is refused (the rule FilledSheet keeps).
    const wy3d::Sheet* pSheetB = wy3d::Sheet::cast(pDb->getElement(sheetBId));
    ASSERT_NE(pSheetB, nullptr);
    const std::uint32_t faceBIndex = findFaceIndexAt(pSheetB->getShape(), 2, 0.0);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sheet* pSheetBWrite = wy3d::Sheet::cast(pTrans->getElementForWrite(sheetBId));
        wy3d::Sketch3D* pSketch3DWrite = wy3d::Sketch3D::cast(pTrans->getElementForWrite(curveSketchId));
        ASSERT_NE(pSheetBWrite, nullptr);
        ASSERT_NE(pSketch3DWrite, nullptr);

        wy3d::SplitFace* pSplitFaceB(nullptr);
        EXPECT_EQ(wy3d::SplitFace::create(pTrans, pSheetBWrite, { faceBIndex }, pSketch3DWrite, pSplitFaceB),
            wy::ErrorStatus::InvalidInput);
        EXPECT_EQ(pSplitFaceB, nullptr);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    // The refused feature neither moved the sketch nor dirtied the sheet it was aimed at
    pSketch3D = wy3d::Sketch3D::cast(pDb->getElement(curveSketchId));
    ASSERT_NE(pSketch3D, nullptr);
    EXPECT_EQ(pSketch3D->getParent(), pSplitFaceA->getId());
    EXPECT_EQ(getChainErrorCode(pDb.get(), sheetBId), 0u);

    // Erasing the owner releases the sketch instead of taking it along
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::SplitFace* pWrite = wy3d::SplitFace::cast(pTrans->getElementForWrite(pSplitFaceA->getId()));
        ASSERT_NE(pWrite, nullptr);
        EXPECT_EQ(pWrite->erase(true), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    pSketch3D = wy3d::Sketch3D::cast(pDb->getElement(curveSketchId));
    ASSERT_NE(pSketch3D, nullptr);
    EXPECT_FALSE(pSketch3D->isErased());
    EXPECT_TRUE(pSketch3D->getParent().isNull());

    pSheetA = wy3d::Sheet::cast(pDb->getElement(sheetAId));
    ASSERT_NE(pSheetA, nullptr);
    EXPECT_EQ(countFaces(pSheetA->getShape()), 1);

    // The released sketch is free to be consumed again
    wy3d::SplitFace* pSplitFaceB = splitFace(pDb.get(), sheetBId, faceBIndex, curveSketchId);
    ASSERT_NE(pSplitFaceB, nullptr);

    pSketch3D = wy3d::Sketch3D::cast(pDb->getElement(curveSketchId));
    ASSERT_NE(pSketch3D, nullptr);
    EXPECT_EQ(pSketch3D->getParent(), pSplitFaceB->getId());
}

// --- The feature follows edits of the sketch it reads ---

TEST(SplitFaceSheet, ChainUpdateOnSketchEdit)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createFilledSheet(pDb.get(), sketchId);
    wydb::ElementId curveSketchId = createEmptySketch3D(pDb.get());
    addLine(pDb.get(), curveSketchId,
        wy::Vector3(0.0, 25.0, 0.0), wy::Vector3(100.0, 25.0, 0.0));

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    wy3d::SplitFace* pSplitFace = splitFace(pDb.get(), sheetId,
        findFaceIndexAt(pSheet->getShape(), 2, 0.0), curveSketchId);
    ASSERT_NE(pSplitFace, nullptr);

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 2);

    // Draw one more curve across the face and stop there. Nothing writes to the feature: it reads
    // the sketch as a whole, so the sketch going dirty has to be enough to bring it up again.
    addLine(pDb.get(), curveSketchId,
        wy::Vector3(0.0, 40.0, 0.0), wy::Vector3(100.0, 40.0, 0.0));
    EXPECT_EQ(getChainErrorCode(pDb.get(), pSplitFace->getId()), 0u);

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 3);
    EXPECT_NEAR(bodyArea(pSheet->getShape()), 5000.0, 1e-6);
    expectAllTopoNamed(pSheet);
}

// --- The feature follows the host regenerating under it ---

TEST(SplitFaceSheet, ChainUpdateOnHostRegenerate)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);
    wydb::ElementId curveSketchId = createEmptySketch3D(pDb.get());

    // On the x = 0 wall, so that a change of depth leaves the curve lying on it still
    wydb::ElementId lineId = addLine(pDb.get(), curveSketchId,
        wy::Vector3(0.0, 0.0, 5.0), wy::Vector3(0.0, 50.0, 5.0));

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    wy3d::SplitFace* pSplitFace = splitFace(pDb.get(), sheetId,
        findFaceIndexAt(pSheet->getShape(), 0, 0.0), curveSketchId);
    ASSERT_NE(pSplitFace, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), sheetId), 0u);

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 5);
    EXPECT_NEAR(bodyArea(pSheet->getShape()), 3000.0, 1e-6);

    // Deepen the extrusion: the sheet regenerates, and the imprint has to be put back on it
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
    EXPECT_EQ(getChainErrorCode(pDb.get(), pSplitFace->getId()), 0u);
    EXPECT_EQ(countFaces(pSheet->getShape()), 5);
    EXPECT_NEAR(bodyArea(pSheet->getShape()), 6000.0, 1e-6);
    expectAllTopoNamed(pSheet);
}

// --- Thicken downstream of a split sheet ---

TEST(SplitFaceSheet, DownstreamThicken)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    // Two identical sheets, one of them split
    wydb::ElementId sketchAId = createRectSketch(pDb.get());
    wydb::ElementId sheetAId = createExtrudedSheet(pDb.get(), sketchAId, 10.0);
    wydb::ElementId sketchBId = createRectSketch(pDb.get());
    wydb::ElementId sheetBId = createExtrudedSheet(pDb.get(), sketchBId, 10.0);

    wydb::ElementId curveSketchId = createEmptySketch3D(pDb.get());
    wydb::ElementId lineId = addLine(pDb.get(), curveSketchId,
        wy::Vector3(0.0, 25.0, 0.0), wy::Vector3(0.0, 25.0, 10.0));

    const wy3d::Sheet* pSheetBSplit = wy3d::Sheet::cast(pDb->getElement(sheetBId));
    ASSERT_NE(pSheetBSplit, nullptr);
    ASSERT_NE(splitFace(pDb.get(), sheetBId,
        findFaceIndexAt(pSheetBSplit->getShape(), 0, 0.0), curveSketchId), nullptr);
    EXPECT_EQ(countFaces(pSheetBSplit->getShape()), 5);

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

        wy3d::Sheet* pSheetB = wy3d::Sheet::cast(pTrans->getElementForWrite(sheetBId));
        ASSERT_NE(pSheetB, nullptr);
        wy3d::Thicken* pSolidB(nullptr);
        EXPECT_EQ(wy3d::Thicken::create(pTrans, pSheetB, 5.0, wy3d::ThickenDirection::OneSide, pSolidB),
            wy::ErrorStatus::Ok);
        ASSERT_NE(pSolidB, nullptr);
        solidBId = pSolidB->getId();

        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    const wy3d::Solid* pSolidA = wy3d::Solid::cast(pDb->getElement(solidAId));
    ASSERT_NE(pSolidA, nullptr);
    const wy3d::Solid* pSolidB = wy3d::Solid::cast(pDb->getElement(solidBId));
    ASSERT_NE(pSolidB, nullptr);

    EXPECT_EQ(getChainErrorCode(pDb.get(), solidAId), 0u);
    EXPECT_EQ(getChainErrorCode(pDb.get(), solidBId), 0u);
    EXPECT_GT(countFaces(pSolidB->getShape()), countFaces(pSolidA->getShape()));
    EXPECT_EQ(countSolids(pSolidB->getShape()), 1);
    expectAllTopoNamed(pSolidA);
    expectAllTopoNamed(pSolidB);
}

// --- OffsetSheet downstream of a split sheet ---

TEST(SplitFaceSheet, DownstreamOffsetSheet)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createExtrudedSheet(pDb.get(), sketchId, 10.0);
    wydb::ElementId curveSketchId = createEmptySketch3D(pDb.get());
    wydb::ElementId lineId = addLine(pDb.get(), curveSketchId,
        wy::Vector3(0.0, 25.0, 0.0), wy::Vector3(0.0, 25.0, 10.0));

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    ASSERT_NE(splitFace(pDb.get(), sheetId,
        findFaceIndexAt(pSheet->getShape(), 0, 0.0), curveSketchId), nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 5);

    wydb::ElementId offsetId = wydb::ElementId::kNull;
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sheet* pSheetWrite = wy3d::Sheet::cast(pTrans->getElementForWrite(sheetId));
        ASSERT_NE(pSheetWrite, nullptr);
        wy3d::OffsetSheet* pOffset(nullptr);
        EXPECT_EQ(wy3d::OffsetSheet::create(pTrans, pSheetWrite, 3.0, pOffset), wy::ErrorStatus::Ok);
        ASSERT_NE(pOffset, nullptr);
        offsetId = pOffset->getId();
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    const wy3d::OffsetSheet* pOffset = wy3d::OffsetSheet::cast(pDb->getElement(offsetId));
    ASSERT_NE(pOffset, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), offsetId), 0u);
    EXPECT_EQ(countFaces(pOffset->getShape()), 5);
    expectAllTopoNamed(pOffset);
}

// --- Solidify downstream of a split sheet ---

TEST(SplitFaceSheet, DownstreamSolidify)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    // Solidify wants a closed shell, so the walls are capped off first
    wydb::ElementId rectSketchId = createRectSketch(pDb.get());
    wydb::ElementId wallSheetId = createExtrudedSheet(pDb.get(), rectSketchId, 10.0);
    wydb::ElementId bottomCapId = wydb::ElementId::kNull;
    wydb::ElementId topCapId = wydb::ElementId::kNull;
    wydb::ElementId sheetId = createClosedSheet(pDb.get(), 10.0, wallSheetId, bottomCapId, topCapId);
    ASSERT_FALSE(sheetId.isNull());

    wydb::ElementId curveSketchId = createEmptySketch3D(pDb.get());
    wydb::ElementId lineId = addLine(pDb.get(), curveSketchId,
        wy::Vector3(0.0, 25.0, 10.0), wy::Vector3(100.0, 25.0, 10.0));

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 6);
    ASSERT_NE(splitFace(pDb.get(), sheetId,
        findFaceIndexAt(pSheet->getShape(), 2, 10.0), curveSketchId), nullptr);

    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(countFaces(pSheet->getShape()), 7);

    wydb::ElementId solidId = wydb::ElementId::kNull;
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sheet* pSheetWrite = wy3d::Sheet::cast(pTrans->getElementForWrite(sheetId));
        ASSERT_NE(pSheetWrite, nullptr);
        wy3d::Solidify* pSolidify(nullptr);
        EXPECT_EQ(wy3d::Solidify::create(pTrans, pSheetWrite, pSolidify), wy::ErrorStatus::Ok);
        ASSERT_NE(pSolidify, nullptr);
        solidId = pSolidify->getId();
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(solidId));
    ASSERT_NE(pSolid, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), solidId), 0u);
    EXPECT_EQ(countSolids(pSolid->getShape()), 1);
    EXPECT_EQ(countFaces(pSolid->getShape()), 7);
    expectAllTopoNamed(pSolid);
}

// --- SewnSheet downstream of a split sheet ---

TEST(SplitFaceSheet, DownstreamSewnSheet)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();

    // Capping off a split sheet: the imprint is a real edge now, so the walls sew back onto the
    // caps and the shell closes with one face more than an unsplit one would have
    wydb::ElementId rectSketchId = createRectSketch(pDb.get());
    wydb::ElementId wallSheetId = createExtrudedSheet(pDb.get(), rectSketchId, 10.0);
    wydb::ElementId curveSketchId = createEmptySketch3D(pDb.get());
    wydb::ElementId lineId = addLine(pDb.get(), curveSketchId,
        wy::Vector3(0.0, 25.0, 0.0), wy::Vector3(0.0, 25.0, 10.0));

    const wy3d::Sheet* pWall = wy3d::Sheet::cast(pDb->getElement(wallSheetId));
    ASSERT_NE(pWall, nullptr);
    ASSERT_NE(splitFace(pDb.get(), wallSheetId,
        findFaceIndexAt(pWall->getShape(), 0, 0.0), curveSketchId), nullptr);
    EXPECT_EQ(countFaces(pWall->getShape()), 5);

    wydb::ElementId bottomCapId = wydb::ElementId::kNull;
    wydb::ElementId topCapId = wydb::ElementId::kNull;
    wydb::ElementId sewnId = createClosedSheet(pDb.get(), 10.0, wallSheetId, bottomCapId, topCapId);
    ASSERT_FALSE(sewnId.isNull());

    const wy3d::Sheet* pSewn = wy3d::Sheet::cast(pDb->getElement(sewnId));
    ASSERT_NE(pSewn, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), sewnId), 0u);
    EXPECT_EQ(countFaces(pSewn->getShape()), 7);
    // SewnSheet hands back a bare shell, not a compound
    EXPECT_EQ(pSewn->getShape().ShapeType(), TopAbs_ShapeEnum::TopAbs_SHELL);
    expectAllTopoNamed(pSewn);
}

// --- IO: the chain has to survive a round trip ---

TEST(SplitFaceSheet, IO)
{
    std::string filePath("./test_split_face_sheet.wy3dt");
    wydb::ElementId sketchId = wydb::ElementId::kNull;
    wydb::ElementId sheetId = wydb::ElementId::kNull;
    wydb::ElementId curveSketchId = wydb::ElementId::kNull;
    wydb::ElementId splitFaceId = wydb::ElementId::kNull;

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        sketchId = createRectSketch(pDb.get());
        sheetId = createFilledSheet(pDb.get(), sketchId);
        curveSketchId = createEmptySketch3D(pDb.get());
        addLine(pDb.get(), curveSketchId,
            wy::Vector3(0.0, 25.0, 0.0), wy::Vector3(100.0, 25.0, 0.0));

        const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
        ASSERT_NE(pSheet, nullptr);
        wy3d::SplitFace* pSplitFace = splitFace(pDb.get(), sheetId,
            findFaceIndexAt(pSheet->getShape(), 2, 0.0), curveSketchId);
        ASSERT_NE(pSplitFace, nullptr);
        splitFaceId = pSplitFace->getId();

        EXPECT_EQ(pDb->writeFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);
    }

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        EXPECT_EQ(pDb->readFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);

        const wy3d::SplitFace* pSplitFace = wy3d::SplitFace::cast(pDb->getElement(splitFaceId));
        ASSERT_NE(pSplitFace, nullptr);
        EXPECT_EQ(pSplitFace->getParent(), sheetId);
        EXPECT_EQ(pSplitFace->getSketch(), curveSketchId);
        EXPECT_FALSE(pSplitFace->getFaces().empty());

        const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
        ASSERT_NE(pSheet, nullptr);
        EXPECT_EQ(countFaces(pSheet->getShape()), 2);
        EXPECT_NEAR(bodyArea(pSheet->getShape()), 5000.0, 1e-6);
        std::vector<wydb::ElementId> children = pSheet->getChildren();
        EXPECT_NE(std::find(children.cbegin(), children.cend(), splitFaceId), children.cend());
        expectAllTopoNamed(pSheet);
    }
}

// --- Null and empty inputs ---

TEST(SplitFaceSheet, NullArgs)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createRectSketch(pDb.get());
    wydb::ElementId sheetId = createFilledSheet(pDb.get(), sketchId);
    wydb::ElementId curveSketchId = createEmptySketch3D(pDb.get());
    addLine(pDb.get(), curveSketchId,
        wy::Vector3(0.0, 25.0, 0.0), wy::Vector3(100.0, 25.0, 0.0));

    const std::size_t numElements = countElements(pDb.get());

    wy3d::SplitFace* pSplitFace(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sheet* pSheet = wy3d::Sheet::cast(pTrans->getElementForWrite(sheetId));
        ASSERT_NE(pSheet, nullptr);
        const wy3d::Sheet* pSheetRead = wy3d::Sheet::cast(pDb->getElement(sheetId));
        ASSERT_NE(pSheetRead, nullptr);
        const std::uint32_t faceIndex = findFaceIndexAt(pSheetRead->getShape(), 2, 0.0);
        ASSERT_NE(faceIndex, UINT_MAX);
        wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(curveSketchId));
        ASSERT_NE(pSketch3D, nullptr);

        EXPECT_EQ(wy3d::SplitFace::create(nullptr, pSheet, { faceIndex }, pSketch3D, pSplitFace),
            wy::ErrorStatus::NullTransactionPointer);
        EXPECT_EQ(wy3d::SplitFace::create(pTrans, static_cast<wy3d::Sheet*>(nullptr),
            { faceIndex }, pSketch3D, pSplitFace), wy::ErrorStatus::NullElementPointer);
        EXPECT_EQ(wy3d::SplitFace::create(pTrans, pSheet, {}, pSketch3D, pSplitFace),
            wy::ErrorStatus::InvalidInput);
        EXPECT_EQ(wy3d::SplitFace::create(pTrans, pSheet, { faceIndex },
            static_cast<wy3d::Sketch3D*>(nullptr), pSplitFace), wy::ErrorStatus::NullElementPointer);

        EXPECT_EQ(pSplitFace, nullptr);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    EXPECT_EQ(countElements(pDb.get()), numElements);
}

// --- The solid overload of the same feature ---

TEST(SplitFaceSheet, SolidHostUnaffected)
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

    wydb::ElementId curveSketchId = createEmptySketch3D(pDb.get());
    addLine(pDb.get(), curveSketchId,
        wy::Vector3(0.0, 5.0, 10.0), wy::Vector3(30.0, 5.0, 10.0));

    const wy3d::Solid* pBox = wy3d::Solid::cast(pDb->getElement(boxId));
    ASSERT_NE(pBox, nullptr);
    EXPECT_EQ(countFaces(pBox->getShape()), 6);
    const std::uint32_t topFaceIndex = findFaceIndexAt(pBox->getShape(), 2, 10.0);
    ASSERT_NE(topFaceIndex, UINT_MAX);

    // 这条路走的是实体那个重载(片体的那个由上面的 helper 覆盖)
    wy3d::SplitFace* pSplitFace(nullptr);
    {
        wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
        wy3d::Solid* pBoxWrite = wy3d::Solid::cast(pTrans->getElementForWrite(boxId));
        ASSERT_NE(pBoxWrite, nullptr);
        wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(curveSketchId));
        ASSERT_NE(pSketch3D, nullptr);
        EXPECT_EQ(wy3d::SplitFace::create(pTrans, pBoxWrite, { topFaceIndex }, pSketch3D, pSplitFace),
            wy::ErrorStatus::Ok);
        EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSplitFace, nullptr);

    pBox = wy3d::Solid::cast(pDb->getElement(boxId));
    ASSERT_NE(pBox, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), boxId), 0u);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pSplitFace->getId()), 0u);
    EXPECT_EQ(countFaces(pBox->getShape()), 7);
    EXPECT_NEAR(bodyArea(pBox->getShape()), 2200.0, 1e-6);
    expectAllTopoNamed(pBox);
}
