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

#include <cmath>
#include <string>
#include <memory>
#include <vector>

#include <wy3dSplitFace.h>
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

// A 30 x 20 x 10 box with a corner at the origin: 6 faces before any split, 2200 of area.
// The curves live in a 3D sketch of their own, the way the Intersection Curve command leaves
// them, and every curve the sketch holds is a tool of the split.

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

    static double bodyArea(const TopoDS_Shape& shape)
    {
        GProp_GProps gprops;
        BRepGProp::SurfaceProperties(shape, gprops);
        return gprops.Mass();
    }

    // Index, as getShape() returns the faces, of the planar face parallel to XY sitting at z
    static std::uint32_t findFaceIndexAtZ(const TopoDS_Shape& shape, double z)
    {
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
            if (std::fabs(surface.Plane().Axis().Direction().Z()) < 1.0 - 1e-9)
            {
                continue;
            }

            GProp_GProps props;
            BRepGProp::SurfaceProperties(face, props);
            if (std::fabs(props.CentreOfMass().Z() - z) > 1e-6)
            {
                continue;
            }

            return static_cast<std::uint32_t>(i - 1);
        }
        return UINT_MAX;
    }

    // Every face/edge of the built shape must resolve a topo name, or picking cannot find it
    static void expectAllTopoNamed(const wy3d::Solid* pSolid)
    {
        const wy3d::TopoNaming* pTopoNaming = pSolid->getTopoNaming();
        ASSERT_NE(pTopoNaming, nullptr);

        std::vector<std::string> info;
        EXPECT_TRUE(pTopoNaming->check(pSolid->getShape(), info)) << (info.empty() ? "" : info[0]);

        const TopoDS_Shape& shape = pSolid->getShape();
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

    static wydb::ElementId createBaseBox(wy3d::Database* pDb)
    {
        wydb::ElementId boxId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            Box* pBox(nullptr);
            EXPECT_EQ(Box::create(pTrans, 30.0, 20.0, 10.0, pBox), wy::ErrorStatus::Ok);
            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
            boxId = pBox->getId();
        }
        return boxId;
    }

    static wydb::ElementId createEmptySketch3D(wy3d::Database* pDb)
    {
        wydb::ElementId sketchId = wydb::ElementId::kNull;
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Sketch3D* pSketch3D(nullptr);
            EXPECT_EQ(wy3d::Sketch3D::create(pTrans, pSketch3D), wy::ErrorStatus::Ok);
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
            EXPECT_NE(pSketch3D, nullptr);

            wy3d::SketchLine3D* pLine(nullptr);
            EXPECT_EQ(wy3d::SketchLine3D::create(pTrans, startPnt, endPnt, pLine), wy::ErrorStatus::Ok);
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
            EXPECT_NE(pSketch3D, nullptr);

            wy3d::SketchCircle3D* pCircle(nullptr);
            EXPECT_EQ(wy3d::SketchCircle3D::create(pTrans, center, wy::Vector3::kZAxis, wy::Vector3::kXAxis,
                radius, pCircle), wy::ErrorStatus::Ok);
            EXPECT_EQ(pSketch3D->addEntity(pCircle), wy::ErrorStatus::Ok);
            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
            circleId = pCircle->getId();
        }
        return circleId;
    }

    // Split the top face with the whole sketch and hand back the feature
    static wy3d::SplitFace* splitTopFace(wy3d::Database* pDb, const wydb::ElementId& boxId,
        const wydb::ElementId& sketchId)
    {
        wy3d::SplitFace* pSplitFace(nullptr);
        {
            wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
            wy3d::Solid* pSolid = wy3d::Solid::cast(pTrans->getElementForWrite(boxId));
            EXPECT_NE(pSolid, nullptr);

            const std::uint32_t topFaceIndex = findFaceIndexAtZ(pSolid->getShape(), 10.0);
            EXPECT_NE(topFaceIndex, UINT_MAX);
            wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(sketchId));
            EXPECT_NE(pSketch3D, nullptr);
            EXPECT_EQ(wy3d::SplitFace::create(pTrans, pSolid, { topFaceIndex }, pSketch3D, pSplitFace),
                wy::ErrorStatus::Ok);
            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
        }
        return pSplitFace;
    }
}

// --- An open curve whose two ends land on the face boundary divides the face ---

TEST(SplitFace, SplitTopFaceWithOpenLine)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId boxId = createBaseBox(pDb.get());
    wydb::ElementId sketchId = createEmptySketch3D(pDb.get());
    addLine(pDb.get(), sketchId, wy::Vector3(0.0, 5.0, 10.0), wy::Vector3(30.0, 5.0, 10.0));

    wy3d::SplitFace* pSplitFace = splitTopFace(pDb.get(), boxId, sketchId);
    ASSERT_NE(pSplitFace, nullptr);
    EXPECT_EQ(pSplitFace->getSketch(), sketchId);
    EXPECT_FALSE(pSplitFace->getFaces().empty());

    const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(boxId));
    ASSERT_NE(pSolid, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), boxId), 0u);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pSplitFace->getId()), 0u);
    EXPECT_EQ(countFaces(pSolid->getShape()), 7);
    EXPECT_NEAR(bodyArea(pSolid->getShape()), 2200.0, 1e-6);
    EXPECT_EQ(pSplitFace->getNewFaceIndices().size(), 2u);
    expectAllTopoNamed(pSolid);
}

// --- A closed curve inside the face divides it too ---

TEST(SplitFace, SplitTopFaceWithClosedCircle)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId boxId = createBaseBox(pDb.get());
    wydb::ElementId sketchId = createEmptySketch3D(pDb.get());
    addCircle(pDb.get(), sketchId, wy::Vector3(15.0, 10.0, 10.0), 3.0);

    wy3d::SplitFace* pSplitFace = splitTopFace(pDb.get(), boxId, sketchId);
    ASSERT_NE(pSplitFace, nullptr);

    const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(boxId));
    ASSERT_NE(pSolid, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), boxId), 0u);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pSplitFace->getId()), 0u);
    EXPECT_EQ(countFaces(pSolid->getShape()), 7);
    EXPECT_NEAR(bodyArea(pSolid->getShape()), 2200.0, 1e-6);
    EXPECT_EQ(pSplitFace->getNewFaceIndices().size(), 2u);
    expectAllTopoNamed(pSolid);
}

// --- Every curve of the sketch is a tool, without anyone listing them ---

TEST(SplitFace, AllCurvesOfSketchAreUsed)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId boxId = createBaseBox(pDb.get());
    wydb::ElementId sketchId = createEmptySketch3D(pDb.get());
    addLine(pDb.get(), sketchId, wy::Vector3(0.0, 5.0, 10.0), wy::Vector3(30.0, 5.0, 10.0));
    addCircle(pDb.get(), sketchId, wy::Vector3(15.0, 15.0, 10.0), 3.0);

    wy3d::SplitFace* pSplitFace = splitTopFace(pDb.get(), boxId, sketchId);
    ASSERT_NE(pSplitFace, nullptr);

    const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(boxId));
    ASSERT_NE(pSolid, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pSplitFace->getId()), 0u);
    // The line cuts the face in two; the circle inside one of the halves cuts that one too
    EXPECT_EQ(countFaces(pSolid->getShape()), 8);
    EXPECT_NEAR(bodyArea(pSolid->getShape()), 2200.0, 1e-6);
    expectAllTopoNamed(pSolid);
}

// --- A curve that does not touch the body at all ---

TEST(SplitFace, CurveOffBodyFails)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId boxId = createBaseBox(pDb.get());
    wydb::ElementId sketchId = createEmptySketch3D(pDb.get());
    addLine(pDb.get(), sketchId, wy::Vector3(0.0, 5.0, 50.0), wy::Vector3(30.0, 5.0, 50.0));

    wy3d::SplitFace* pSplitFace = splitTopFace(pDb.get(), boxId, sketchId);
    ASSERT_NE(pSplitFace, nullptr);

    const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(boxId));
    ASSERT_NE(pSolid, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pSplitFace->getId()),
        static_cast<std::uint32_t>(wy3d::ErrorCode::SPLITFACE_NoFaceSplit));
    EXPECT_EQ(countFaces(pSolid->getShape()), 6);
    EXPECT_NEAR(bodyArea(pSolid->getShape()), 2200.0, 1e-6);
}

// --- A sketch holding no curve leaves the feature nothing to cut with ---

TEST(SplitFace, EmptySketchFails)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId boxId = createBaseBox(pDb.get());
    wydb::ElementId sketchId = createEmptySketch3D(pDb.get());

    wy3d::SplitFace* pSplitFace = splitTopFace(pDb.get(), boxId, sketchId);
    ASSERT_NE(pSplitFace, nullptr);

    const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(boxId));
    ASSERT_NE(pSolid, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pSplitFace->getId()),
        static_cast<std::uint32_t>(wy3d::ErrorCode::SPLITFACE_CurveNotExists));
    EXPECT_EQ(countFaces(pSolid->getShape()), 6);
    EXPECT_NEAR(bodyArea(pSolid->getShape()), 2200.0, 1e-6);
}

// --- An open curve that dead-ends inside the face splits nothing ---

TEST(SplitFace, CurveDeadEndsInFaceInterior)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId boxId = createBaseBox(pDb.get());
    wydb::ElementId sketchId = createEmptySketch3D(pDb.get());
    addLine(pDb.get(), sketchId, wy::Vector3(10.0, 10.0, 10.0), wy::Vector3(20.0, 10.0, 10.0));

    wy3d::SplitFace* pSplitFace = splitTopFace(pDb.get(), boxId, sketchId);
    ASSERT_NE(pSplitFace, nullptr);

    const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(boxId));
    ASSERT_NE(pSolid, nullptr);
    EXPECT_EQ(getChainErrorCode(pDb.get(), pSplitFace->getId()),
        static_cast<std::uint32_t>(wy3d::ErrorCode::SPLITFACE_NoFaceSplit));
    EXPECT_EQ(countFaces(pSolid->getShape()), 6);
}

// --- Null and empty inputs ---

TEST(SplitFace, NullArgsAndEmptyInput)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId boxId = createBaseBox(pDb.get());
    wydb::ElementId sketchId = createEmptySketch3D(pDb.get());
    addLine(pDb.get(), sketchId, wy::Vector3(0.0, 5.0, 10.0), wy::Vector3(30.0, 5.0, 10.0));

    const std::size_t numElements = countElements(pDb.get());

    wy3d::SplitFace* pSplitFace(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Solid* pSolid = wy3d::Solid::cast(pTrans->getElementForWrite(boxId));
        ASSERT_NE(pSolid, nullptr);
        const std::uint32_t topFaceIndex = findFaceIndexAtZ(pSolid->getShape(), 10.0);
        ASSERT_NE(topFaceIndex, UINT_MAX);
        wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(sketchId));
        ASSERT_NE(pSketch3D, nullptr);

        EXPECT_EQ(wy3d::SplitFace::create(nullptr, pSolid, { topFaceIndex }, pSketch3D, pSplitFace),
            wy::ErrorStatus::NullTransactionPointer);
        EXPECT_EQ(wy3d::SplitFace::create(pTrans, static_cast<wy3d::Solid*>(nullptr),
            { topFaceIndex }, pSketch3D, pSplitFace), wy::ErrorStatus::NullElementPointer);
        EXPECT_EQ(wy3d::SplitFace::create(pTrans, pSolid, {}, pSketch3D, pSplitFace),
            wy::ErrorStatus::InvalidInput);
        EXPECT_EQ(wy3d::SplitFace::create(pTrans, pSolid, { topFaceIndex },
            static_cast<wy3d::Sketch3D*>(nullptr), pSplitFace), wy::ErrorStatus::NullElementPointer);
        EXPECT_EQ(pSplitFace, nullptr);

        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    EXPECT_EQ(countElements(pDb.get()), numElements);
}

// --- Erasure ---

TEST(SplitFace, EraseCurveAndSketch)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId boxId = createBaseBox(pDb.get());
    wydb::ElementId sketchId = createEmptySketch3D(pDb.get());
    addLine(pDb.get(), sketchId, wy::Vector3(0.0, 5.0, 10.0), wy::Vector3(30.0, 5.0, 10.0));
    wydb::ElementId line2Id = addLine(pDb.get(), sketchId, wy::Vector3(0.0, 15.0, 10.0), wy::Vector3(30.0, 15.0, 10.0));

    wy3d::SplitFace* pSplitFace = splitTopFace(pDb.get(), boxId, sketchId);
    ASSERT_NE(pSplitFace, nullptr);

    const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(boxId));
    ASSERT_NE(pSolid, nullptr);
    EXPECT_EQ(countFaces(pSolid->getShape()), 8);

    // Erasing one curve leaves the feature alive with one imprint fewer - the other line still
    // splits, so there is still something to do
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::SketchLine3D* pLineWrite = wy3d::SketchLine3D::cast(pTrans->getElementForWrite(line2Id));
        ASSERT_NE(pLineWrite, nullptr);
        EXPECT_EQ(pLineWrite->erase(true), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    EXPECT_TRUE(pSplitFace->isErased() == false);
    EXPECT_EQ(countFaces(pSolid->getShape()), 7);

    // Erasing the sketch takes the feature with it
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pSketchWrite = wy3d::Sketch3D::cast(pTrans->getElementForWrite(sketchId));
        ASSERT_NE(pSketchWrite, nullptr);
        EXPECT_EQ(pSketchWrite->erase(true), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    EXPECT_TRUE(pSplitFace->isErased());
}

// --- The feature follows edits of the sketch it reads ---

TEST(SplitFace, ChainUpdateOnSketchEdit)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId boxId = createBaseBox(pDb.get());
    wydb::ElementId sketchId = createEmptySketch3D(pDb.get());
    addLine(pDb.get(), sketchId, wy::Vector3(0.0, 5.0, 10.0), wy::Vector3(30.0, 5.0, 10.0));

    wy3d::SplitFace* pSplitFace = splitTopFace(pDb.get(), boxId, sketchId);
    ASSERT_NE(pSplitFace, nullptr);
    const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(boxId));
    ASSERT_NE(pSolid, nullptr);
    EXPECT_EQ(countFaces(pSolid->getShape()), 7);

    // Draw one more curve across the face and stop there. Nothing writes to the feature: it reads
    // the sketch as a whole, so the sketch going dirty has to be enough to bring it up again.
    addLine(pDb.get(), sketchId, wy::Vector3(0.0, 15.0, 10.0), wy::Vector3(30.0, 15.0, 10.0));
    EXPECT_EQ(getChainErrorCode(pDb.get(), pSplitFace->getId()), 0u);

    EXPECT_EQ(countFaces(pSolid->getShape()), 8);
    EXPECT_NEAR(bodyArea(pSolid->getShape()), 2200.0, 1e-6);
    expectAllTopoNamed(pSolid);
}

// --- IO ---

TEST(SplitFace, IO)
{
    std::string filePath("./test_split_face.wy3dt");
    wydb::ElementId boxId = wydb::ElementId::kNull;
    wydb::ElementId sketchId = wydb::ElementId::kNull;
    wydb::ElementId splitFaceId = wydb::ElementId::kNull;

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        boxId = createBaseBox(pDb.get());
        sketchId = createEmptySketch3D(pDb.get());
        addLine(pDb.get(), sketchId, wy::Vector3(0.0, 5.0, 10.0), wy::Vector3(30.0, 5.0, 10.0));

        wy3d::SplitFace* pSplitFace = splitTopFace(pDb.get(), boxId, sketchId);
        ASSERT_NE(pSplitFace, nullptr);
        splitFaceId = pSplitFace->getId();

        EXPECT_EQ(pDb->writeFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);
    }

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        EXPECT_EQ(pDb->readFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);

        const wy3d::SplitFace* pSplitFace = wy3d::SplitFace::cast(pDb->getElement(splitFaceId));
        ASSERT_NE(pSplitFace, nullptr);
        EXPECT_EQ(pSplitFace->getSketch(), sketchId);
        EXPECT_FALSE(pSplitFace->getFaces().empty());

        const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(boxId));
        ASSERT_NE(pSolid, nullptr);
        EXPECT_EQ(countFaces(pSolid->getShape()), 7);
        EXPECT_NEAR(bodyArea(pSolid->getShape()), 2200.0, 1e-6);
        expectAllTopoNamed(pSolid);
    }
}
