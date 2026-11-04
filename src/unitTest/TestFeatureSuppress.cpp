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
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include <wy3dBodyModification.h>
#include <wy3dChamfer.h>
#include <wy3dDefaultChainUpdateFeedback.h>
#include <wy3dErrorCode.h>
#include <wy3dExtrudedSheet.h>
#include <wy3dFillet.h>
#include <wy3dMath.h>
#include <wy3dMove.h>
#include <wy3dPlane.h>
#include <wy3dSheet.h>
#include <wy3dSketch.h>
#include <wy3dSketchLine.h>
#include <wy3dSolid.h>
#include <wy3dThicken.h>
#include <wy3dUnion.h>

#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>

// A suppressed feature is skipped by the rebuild chain but stays in the
// modification list: downstream features rebuild as usual and fail through the
// existing error paths when they can no longer find what they referenced.

namespace
{
    static double getVolume(const TopoDS_Shape& shape)
    {
        if (shape.IsNull())
        {
            return 0.0;
        }
        GProp_GProps props;
        BRepGProp::VolumeProperties(shape, props);
        return props.Mass();
    }

    static double getMinZ(const TopoDS_Shape& shape)
    {
        Bnd_Box box;
        BRepBndLib::Add(shape, box);
        return box.CornerMin().Z();
    }

    static std::uint32_t getChainErrorCode(wy3d::Database* pDb, const wydb::ElementId& id)
    {
        return wy3d::getErrorCodeFromChainUpdateFeedback(
            pDb->getTransactionManager()->getChainUpdateFeedback(id).get());
    }

    static wy::ErrorStatus applySuppressed(
        wy3d::Database* pDb, const wydb::ElementId& id, bool isSuppressed)
    {
        wydb::TransactionManager* pMgr = pDb->getTransactionManager();
        wydb::Transaction* pTrans = pMgr->startTransaction();
        if (!pTrans)
        {
            return wy::ErrorStatus::InvalidInput;
        }
        wydb::Element* pElem = pTrans->getElementForWrite(id);
        wy::ErrorStatus error = wy::ErrorStatus::InvalidInput;
        if (wy3d::Solid* pSolid = wy3d::Solid::cast(pElem))
        {
            error = pSolid->suppress(isSuppressed);
        }
        else if (wy3d::Sheet* pSheet = wy3d::Sheet::cast(pElem))
        {
            error = pSheet->suppress(isSuppressed);
        }
        else if (wy3d::BodyModification* pModification = wy3d::BodyModification::cast(pElem))
        {
            error = pModification->suppress(isSuppressed);
        }
        if (wy::ErrorStatus::Ok == error)
        {
            return pMgr->endTransaction();
        }
        pMgr->abortTransaction();
        return error;
    }

    // 100 x 100 x 100 host with a 20 x 20 x 20 cut box at (40, 40, 0)
    struct CutFixture
    {
        std::unique_ptr<wy3d::Database> pDb;
        wydb::ElementId hostId;
        wydb::ElementId cutId;
    };

    static CutFixture createCutFixture()
    {
        CutFixture fixture;
        fixture.pDb = std::make_unique<wy3d::Database>();

        wydb::TransactionManager* pMgr = fixture.pDb->getTransactionManager();
        wydb::Transaction* pTrans = pMgr->startTransaction();
        EXPECT_NE(pTrans, nullptr);

        wy3d::Box* pHost(nullptr);
        wy3d::Box* pCut(nullptr);
        EXPECT_EQ(wy3d::Box::create(pTrans, 100.0, 100.0, 100.0, pHost), wy::ErrorStatus::Ok);
        EXPECT_EQ(wy3d::Box::create(pTrans, 20.0, 20.0, 20.0, pCut), wy::ErrorStatus::Ok);
        if (!pHost || !pCut)
        {
            pMgr->abortTransaction();
            return fixture;
        }
        EXPECT_EQ(pCut->setPosition(wy::Vector3(40.0, 40.0, 0.0)), wy::ErrorStatus::Ok);
        EXPECT_EQ(pCut->setCut(true), wy::ErrorStatus::Ok);
        EXPECT_EQ(pHost->addModification(pCut), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);

        fixture.hostId = pHost->getId();
        fixture.cutId = pCut->getId();
        return fixture;
    }

    static wydb::ElementId createRectSketch(wy3d::Database* pDb, double z)
    {
        wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
        wy3d::Plane plane(wy::Vector3(0.0, 0.0, z), wy::Vector3::kZAxis, wy::Vector3::kXAxis);
        wy3d::Sketch* pSketch(nullptr);
        EXPECT_EQ(wy3d::Sketch::create(pTrans, plane, pSketch), wy::ErrorStatus::Ok);
        if (!pSketch)
        {
            pDb->getTransactionManager()->abortTransaction();
            return wydb::ElementId::kNull;
        }
        const wy::Vector2 corners[4] = {
            wy::Vector2(0.0, 0.0), wy::Vector2(100.0, 0.0),
            wy::Vector2(100.0, 50.0), wy::Vector2(0.0, 50.0)
        };
        for (int i = 0; i < 4; ++i)
        {
            wy3d::SketchLine* pLine(nullptr);
            EXPECT_EQ(wy3d::SketchLine::create(pTrans, corners[i], corners[(i + 1) % 4], pLine),
                wy::ErrorStatus::Ok);
            if (!pLine)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }
            EXPECT_EQ(pSketch->addEntity(pLine), wy::ErrorStatus::Ok);
        }
        EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
        return pSketch->getId();
    }
}

// A cut member is skipped: the host keeps its uncut volume, and the member
// stays in the modification list.
TEST(FeatureSuppress, SuppressCutMember)
{
    CutFixture fixture = createCutFixture();
    ASSERT_NE(fixture.hostId, wydb::ElementId::kNull);

    const wy3d::Solid* pHost = wy3d::Solid::cast(fixture.pDb->getElement(fixture.hostId));
    ASSERT_NE(pHost, nullptr);
    EXPECT_NEAR(getVolume(pHost->getShape()), 992000.0, 1e-3);

    EXPECT_EQ(applySuppressed(fixture.pDb.get(), fixture.cutId, true), wy::ErrorStatus::Ok);
    pHost = wy3d::Solid::cast(fixture.pDb->getElement(fixture.hostId));
    ASSERT_NE(pHost, nullptr);
    EXPECT_NEAR(getVolume(pHost->getShape()), 1000000.0, 1e-3);
    ASSERT_EQ(pHost->getModifications().size(), 1u);
    EXPECT_EQ(pHost->getModifications()[0], fixture.cutId);

    EXPECT_EQ(applySuppressed(fixture.pDb.get(), fixture.cutId, false), wy::ErrorStatus::Ok);
    pHost = wy3d::Solid::cast(fixture.pDb->getElement(fixture.hostId));
    ASSERT_NE(pHost, nullptr);
    EXPECT_NEAR(getVolume(pHost->getShape()), 992000.0, 1e-3);
}

// A BodyModification member is skipped by the host's modification loop.
TEST(FeatureSuppress, SuppressModification)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    wydb::ElementId boxId = wydb::ElementId::kNull;
    wydb::ElementId filletId = wydb::ElementId::kNull;
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Box* pBox(nullptr);
        EXPECT_EQ(wy3d::Box::create(pTrans, 30.0, 20.0, 10.0, pBox), wy::ErrorStatus::Ok);
        ASSERT_NE(pBox, nullptr);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
        boxId = pBox->getId();
    }
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Solid* pBox = wy3d::Solid::cast(pTrans->getElementForWrite(boxId));
        ASSERT_NE(pBox, nullptr);
        wy3d::Fillet* pFillet(nullptr);
        EXPECT_EQ(wy3d::Fillet::create(pTrans, pBox, {}, { 0, 1, 2, 3 }, 2.0, pFillet),
            wy::ErrorStatus::Ok);
        ASSERT_NE(pFillet, nullptr);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
        filletId = pFillet->getId();
    }

    const wy3d::Solid* pBox = wy3d::Solid::cast(pDb->getElement(boxId));
    ASSERT_NE(pBox, nullptr);
    const double filletedVolume = getVolume(pBox->getShape());
    EXPECT_LT(filletedVolume, 6000.0);

    EXPECT_EQ(applySuppressed(pDb.get(), filletId, true), wy::ErrorStatus::Ok);
    pBox = wy3d::Solid::cast(pDb->getElement(boxId));
    ASSERT_NE(pBox, nullptr);
    EXPECT_NEAR(getVolume(pBox->getShape()), 6000.0, 1e-3);

    EXPECT_EQ(applySuppressed(pDb.get(), filletId, false), wy::ErrorStatus::Ok);
    pBox = wy3d::Solid::cast(pDb->getElement(boxId));
    ASSERT_NE(pBox, nullptr);
    EXPECT_NEAR(getVolume(pBox->getShape()), filletedVolume, 1e-3);
}

// Suppressing a base body empties its shape without touching the database.
TEST(FeatureSuppress, SuppressBaseSolid)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId boxId = wydb::ElementId::kNull;
    {
        wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
        wy3d::Box* pBox(nullptr);
        EXPECT_EQ(wy3d::Box::create(pTrans, 30.0, 20.0, 10.0, pBox), wy::ErrorStatus::Ok);
        ASSERT_NE(pBox, nullptr);
        EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
        boxId = pBox->getId();
    }

    const wy3d::Solid* pBox = wy3d::Solid::cast(pDb->getElement(boxId));
    ASSERT_NE(pBox, nullptr);
    EXPECT_NEAR(getVolume(pBox->getShape()), 6000.0, 1e-3);

    EXPECT_EQ(applySuppressed(pDb.get(), boxId, true), wy::ErrorStatus::Ok);
    pBox = wy3d::Solid::cast(pDb->getElement(boxId));
    ASSERT_NE(pBox, nullptr);
    EXPECT_TRUE(pBox->isSuppressed());
    EXPECT_TRUE(pBox->getShape().IsNull());

    EXPECT_EQ(applySuppressed(pDb.get(), boxId, false), wy::ErrorStatus::Ok);
    pBox = wy3d::Solid::cast(pDb->getElement(boxId));
    ASSERT_NE(pBox, nullptr);
    EXPECT_FALSE(pBox->isSuppressed());
    EXPECT_NEAR(getVolume(pBox->getShape()), 6000.0, 1e-3);
}

// A sheet and its Move modification are suppressed independently: the sheet
// loses its shape, the move is skipped and leaves the sheet at z = 0.
TEST(FeatureSuppress, SuppressSheetAndModification)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    const wydb::ElementId sketchId = createRectSketch(pDb.get(), 0.0);
    ASSERT_FALSE(sketchId.isNull());

    wydb::ElementId sheetId = wydb::ElementId::kNull;
    wydb::ElementId moveId = wydb::ElementId::kNull;
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
        ASSERT_NE(pSketch, nullptr);
        wy3d::ExtrudedSheet* pSheet(nullptr);
        EXPECT_EQ(wy3d::ExtrudedSheet::create(pTrans, pSketch, 10.0, pSheet), wy::ErrorStatus::Ok);
        ASSERT_NE(pSheet, nullptr);
        wy3d::Move* pMove(nullptr);
        EXPECT_EQ(wy3d::Move::create(pTrans, pSheet, wy::Vector3(0.0, 0.0, 50.0), pMove),
            wy::ErrorStatus::Ok);
        ASSERT_NE(pMove, nullptr);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
        sheetId = pSheet->getId();
        moveId = pMove->getId();
    }

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_NEAR(getMinZ(pSheet->getShape()), 50.0, 1e-6);

    EXPECT_EQ(applySuppressed(pDb.get(), moveId, true), wy::ErrorStatus::Ok);
    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_NEAR(getMinZ(pSheet->getShape()), 0.0, 1e-6);

    EXPECT_EQ(applySuppressed(pDb.get(), moveId, false), wy::ErrorStatus::Ok);
    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_NEAR(getMinZ(pSheet->getShape()), 50.0, 1e-6);

    EXPECT_EQ(applySuppressed(pDb.get(), sheetId, true), wy::ErrorStatus::Ok);
    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_TRUE(pSheet->getShape().IsNull());

    EXPECT_EQ(applySuppressed(pDb.get(), sheetId, false), wy::ErrorStatus::Ok);
    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_NEAR(getMinZ(pSheet->getShape()), 50.0, 1e-6);
}

// A fused member sticks out of the host by 10 x 20 x 20; suppressing it drops
// that volume.
TEST(FeatureSuppress, SuppressFuseMember)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    wydb::ElementId hostId = wydb::ElementId::kNull;
    wydb::ElementId memberId = wydb::ElementId::kNull;
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Box* pHost(nullptr);
        wy3d::Box* pMember(nullptr);
        EXPECT_EQ(wy3d::Box::create(pTrans, 100.0, 100.0, 100.0, pHost), wy::ErrorStatus::Ok);
        EXPECT_EQ(wy3d::Box::create(pTrans, 20.0, 20.0, 20.0, pMember), wy::ErrorStatus::Ok);
        if (!pHost || !pMember)
        {
            pMgr->abortTransaction();
            FAIL();
        }
        EXPECT_EQ(pMember->setPosition(wy::Vector3(90.0, 90.0, 0.0)), wy::ErrorStatus::Ok);
        EXPECT_EQ(pHost->addModification(pMember), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
        hostId = pHost->getId();
        memberId = pMember->getId();
    }

    const wy3d::Solid* pHost = wy3d::Solid::cast(pDb->getElement(hostId));
    ASSERT_NE(pHost, nullptr);
    EXPECT_NEAR(getVolume(pHost->getShape()), 1006000.0, 1e-3);

    EXPECT_EQ(applySuppressed(pDb.get(), memberId, true), wy::ErrorStatus::Ok);
    pHost = wy3d::Solid::cast(pDb->getElement(hostId));
    ASSERT_NE(pHost, nullptr);
    EXPECT_NEAR(getVolume(pHost->getShape()), 1000000.0, 1e-3);

    EXPECT_EQ(applySuppressed(pDb.get(), memberId, false), wy::ErrorStatus::Ok);
    pHost = wy3d::Solid::cast(pDb->getElement(hostId));
    ASSERT_NE(pHost, nullptr);
    EXPECT_NEAR(getVolume(pHost->getShape()), 1006000.0, 1e-3);
}

// A suppressed tool is skipped silently; a suppressed target leaves the boolean
// with an empty shape and reports the existing null-shape warning.
TEST(FeatureSuppress, SuppressBooleanMembers)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    wydb::ElementId targetId = wydb::ElementId::kNull;
    wydb::ElementId tool1Id = wydb::ElementId::kNull;
    wydb::ElementId tool2Id = wydb::ElementId::kNull;
    wydb::ElementId unionId = wydb::ElementId::kNull;
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Box* pTarget(nullptr);
        wy3d::Box* pTool1(nullptr);
        wy3d::Box* pTool2(nullptr);
        EXPECT_EQ(wy3d::Box::create(pTrans, 100.0, 100.0, 100.0, pTarget), wy::ErrorStatus::Ok);
        EXPECT_EQ(wy3d::Box::create(pTrans, 50.0, 50.0, 50.0, pTool1), wy::ErrorStatus::Ok);
        EXPECT_EQ(wy3d::Box::create(pTrans, 60.0, 60.0, 60.0, pTool2), wy::ErrorStatus::Ok);
        if (!pTarget || !pTool1 || !pTool2)
        {
            pMgr->abortTransaction();
            FAIL();
        }
        EXPECT_EQ(pTool1->setPosition(wy::Vector3(100.0, 0.0, 0.0)), wy::ErrorStatus::Ok);
        EXPECT_EQ(pTool2->setPosition(wy::Vector3(0.0, 100.0, 0.0)), wy::ErrorStatus::Ok);
        wy3d::Union* pUnion(nullptr);
        EXPECT_EQ(wy3d::Union::create(pTrans, pTarget, { pTool1, pTool2 }, pUnion),
            wy::ErrorStatus::Ok);
        ASSERT_NE(pUnion, nullptr);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
        targetId = pTarget->getId();
        tool1Id = pTool1->getId();
        tool2Id = pTool2->getId();
        unionId = pUnion->getId();
    }

    const wy3d::Union* pUnion = wy3d::Union::cast(pDb->getElement(unionId));
    ASSERT_NE(pUnion, nullptr);
    EXPECT_NEAR(getVolume(pUnion->getShape()), 1341000.0, 1e-3);

    EXPECT_EQ(applySuppressed(pDb.get(), tool2Id, true), wy::ErrorStatus::Ok);
    pUnion = wy3d::Union::cast(pDb->getElement(unionId));
    ASSERT_NE(pUnion, nullptr);
    EXPECT_NEAR(getVolume(pUnion->getShape()), 1125000.0, 1e-3);
    EXPECT_EQ(0u, getChainErrorCode(pDb.get(), unionId));

    EXPECT_EQ(applySuppressed(pDb.get(), tool2Id, false), wy::ErrorStatus::Ok);
    pUnion = wy3d::Union::cast(pDb->getElement(unionId));
    ASSERT_NE(pUnion, nullptr);
    EXPECT_NEAR(getVolume(pUnion->getShape()), 1341000.0, 1e-3);

    EXPECT_EQ(applySuppressed(pDb.get(), targetId, true), wy::ErrorStatus::Ok);
    pUnion = wy3d::Union::cast(pDb->getElement(unionId));
    ASSERT_NE(pUnion, nullptr);
    EXPECT_TRUE(pUnion->getShape().IsNull());
    EXPECT_EQ(static_cast<std::uint32_t>(wy3d::ErrorCode::warnTOPOSHAPE_NullShape),
        getChainErrorCode(pDb.get(), unionId));

    EXPECT_EQ(applySuppressed(pDb.get(), targetId, false), wy::ErrorStatus::Ok);
    pUnion = wy3d::Union::cast(pDb->getElement(unionId));
    ASSERT_NE(pUnion, nullptr);
    EXPECT_NEAR(getVolume(pUnion->getShape()), 1341000.0, 1e-3);
    EXPECT_EQ(0u, getChainErrorCode(pDb.get(), unionId));

    // The skipped tool keeps its place in the boolean.
    EXPECT_EQ(pUnion->getTools().size(), 2u);
    EXPECT_EQ(pUnion->getTarget(), targetId);
    EXPECT_FALSE(tool1Id.isNull());
}

TEST(FeatureSuppress, UndoRedo)
{
    CutFixture fixture = createCutFixture();
    ASSERT_NE(fixture.hostId, wydb::ElementId::kNull);
    wydb::TransactionManager* pMgr = fixture.pDb->getTransactionManager();

    EXPECT_EQ(applySuppressed(fixture.pDb.get(), fixture.cutId, true), wy::ErrorStatus::Ok);
    const wy3d::Solid* pHost = wy3d::Solid::cast(fixture.pDb->getElement(fixture.hostId));
    ASSERT_NE(pHost, nullptr);
    EXPECT_NEAR(getVolume(pHost->getShape()), 1000000.0, 1e-3);

    EXPECT_EQ(pMgr->undo(), wy::ErrorStatus::Ok);
    const wy3d::Solid* pCut = wy3d::Solid::cast(fixture.pDb->getElement(fixture.cutId));
    ASSERT_NE(pCut, nullptr);
    EXPECT_FALSE(pCut->isSuppressed());
    pHost = wy3d::Solid::cast(fixture.pDb->getElement(fixture.hostId));
    ASSERT_NE(pHost, nullptr);
    EXPECT_NEAR(getVolume(pHost->getShape()), 992000.0, 1e-3);

    EXPECT_EQ(pMgr->redo(), wy::ErrorStatus::Ok);
    pCut = wy3d::Solid::cast(fixture.pDb->getElement(fixture.cutId));
    ASSERT_NE(pCut, nullptr);
    EXPECT_TRUE(pCut->isSuppressed());
    pHost = wy3d::Solid::cast(fixture.pDb->getElement(fixture.hostId));
    ASSERT_NE(pHost, nullptr);
    EXPECT_NEAR(getVolume(pHost->getShape()), 1000000.0, 1e-3);
}

// All three classes persist the flag; a reloaded database rebuilds with the
// suppressed features skipped.
TEST(FeatureSuppress, IO)
{
    const std::string filePath("./test_feature_suppress.wy3dt");

    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    wydb::ElementId boxId = wydb::ElementId::kNull;
    wydb::ElementId filletId = wydb::ElementId::kNull;
    wydb::ElementId sheetId = wydb::ElementId::kNull;
    const wydb::ElementId sketchId = createRectSketch(pDb.get(), 0.0);
    ASSERT_FALSE(sketchId.isNull());
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Box* pBox(nullptr);
        EXPECT_EQ(wy3d::Box::create(pTrans, 30.0, 20.0, 10.0, pBox), wy::ErrorStatus::Ok);
        ASSERT_NE(pBox, nullptr);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
        boxId = pBox->getId();
    }
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Solid* pBox = wy3d::Solid::cast(pTrans->getElementForWrite(boxId));
        ASSERT_NE(pBox, nullptr);
        wy3d::Fillet* pFillet(nullptr);
        EXPECT_EQ(wy3d::Fillet::create(pTrans, pBox, {}, { 0, 1, 2, 3 }, 2.0, pFillet),
            wy::ErrorStatus::Ok);
        ASSERT_NE(pFillet, nullptr);
        wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
        ASSERT_NE(pSketch, nullptr);
        wy3d::ExtrudedSheet* pSheet(nullptr);
        EXPECT_EQ(wy3d::ExtrudedSheet::create(pTrans, pSketch, 10.0, pSheet), wy::ErrorStatus::Ok);
        ASSERT_NE(pSheet, nullptr);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
        filletId = pFillet->getId();
        sheetId = pSheet->getId();
    }

    EXPECT_EQ(applySuppressed(pDb.get(), filletId, true), wy::ErrorStatus::Ok);
    EXPECT_EQ(applySuppressed(pDb.get(), sheetId, true), wy::ErrorStatus::Ok);
    EXPECT_EQ(pDb->writeFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);

    {
        std::unique_ptr<wy3d::Database> pReloadedDb = std::make_unique<wy3d::Database>();
        ASSERT_EQ(pReloadedDb->readFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);
        std::remove(filePath.c_str());

        const wy3d::Fillet* pFillet = wy3d::Fillet::cast(pReloadedDb->getElement(filletId));
        ASSERT_NE(pFillet, nullptr);
        EXPECT_TRUE(pFillet->isSuppressed());

        const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pReloadedDb->getElement(sheetId));
        ASSERT_NE(pSheet, nullptr);
        EXPECT_TRUE(pSheet->isSuppressed());
        EXPECT_TRUE(pSheet->getShape().IsNull());

        const wy3d::Solid* pBox = wy3d::Solid::cast(pReloadedDb->getElement(boxId));
        ASSERT_NE(pBox, nullptr);
        EXPECT_NEAR(getVolume(pBox->getShape()), 6000.0, 1e-3);

        EXPECT_EQ(applySuppressed(pReloadedDb.get(), filletId, false), wy::ErrorStatus::Ok);
        pBox = wy3d::Solid::cast(pReloadedDb->getElement(boxId));
        ASSERT_NE(pBox, nullptr);
        EXPECT_LT(getVolume(pBox->getShape()), 6000.0);
    }
}

// Funnel is a 0.16 file without suppress flags; it must load with every
// feature unsuppressed.
TEST(FeatureSuppress, LegacyFileDefaultsToUnsuppressed)
{
    const std::string filePath = std::string(YI3D_SOURCE_DIR) + "/samples/funnel.wy3dt";
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    ASSERT_EQ(pDb->readFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);

    int numFeatures = 0;
    wy::Iterator<wydb::ElementId> iter = pDb->createIterator();
    for (; !iter.isDone(); iter.moveNext())
    {
        const wydb::Element* pElement = pDb->getElement(iter.current());
        if (const wy3d::Solid* pSolid = wy3d::Solid::cast(pElement))
        {
            EXPECT_FALSE(pSolid->isSuppressed()) << iter.current().value();
            ++numFeatures;
        }
        else if (const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pElement))
        {
            EXPECT_FALSE(pSheet->isSuppressed()) << iter.current().value();
            ++numFeatures;
        }
        else if (const wy3d::BodyModification* pModification = wy3d::BodyModification::cast(pElement))
        {
            EXPECT_FALSE(pModification->isSuppressed()) << iter.current().value();
            ++numFeatures;
        }
    }
    EXPECT_GT(numFeatures, 0);
}

namespace
{
    class OrderPieceReactor : public wydb::DatabaseReactor
    {
    public:
        struct Capture
        {
            bool hasOrderPiece = false;
            bool hasChildShapePiece = false;
        };

        OrderPieceReactor(const wydb::ElementId& hostId, const wydb::ElementId& childId)
            : _hostId(hostId), _childId(childId)
        {
        }

        void onDatabaseChanged(
            const wydb::Database* /*pDatabase*/,
            const wydb::Transaction* /*pTransaction*/,
            const wydb::DatabaseChangeInfo& info) override
        {
            if (wydb::DatabaseChangeCause::EndTransaction != info.cause)
            {
                return;
            }
            Capture capture;
            capture.hasOrderPiece =
                info.details.isDataPieceDirty(_hostId, wydb::ElementDataPieceType::UserDefinedExt5);
            capture.hasChildShapePiece =
                info.details.isDataPieceDirty(_childId, wydb::ElementDataPieceType::Shape);
            _capture = capture;
            _captured = true;
        }

        const Capture& getCapture() const { return _capture; }
        bool wasCaptured() const { return _captured; }

    private:
        wydb::ElementId _hostId;
        wydb::ElementId _childId;
        Capture _capture;
        bool _captured = false;
    };
}

// Toggling suppression must not look like a reorder to the feature tree.
TEST(FeatureSuppress, NoOrderSignal)
{
    CutFixture fixture = createCutFixture();
    ASSERT_NE(fixture.hostId, wydb::ElementId::kNull);

    OrderPieceReactor reactor(fixture.hostId, fixture.cutId);
    ASSERT_EQ(fixture.pDb->addReactor(&reactor), wy::ErrorStatus::Ok);

    EXPECT_EQ(applySuppressed(fixture.pDb.get(), fixture.cutId, true), wy::ErrorStatus::Ok);
    ASSERT_TRUE(reactor.wasCaptured());
    EXPECT_FALSE(reactor.getCapture().hasOrderPiece);
    EXPECT_TRUE(reactor.getCapture().hasChildShapePiece);

    EXPECT_EQ(fixture.pDb->removeReactor(&reactor), wy::ErrorStatus::Ok);
}

// Suppressing a fillet in the funnel sample rebuilds the host without it; the
// downstream chamfer is re-evaluated on the new topology and keeps its place.
TEST(FeatureSuppress, SuppressOnSample)
{
    const std::string filePath = std::string(YI3D_SOURCE_DIR) + "/samples/funnel.wy3dt";
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    ASSERT_EQ(pDb->readFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);

    const wydb::ElementId unionId(44);
    const wydb::ElementId filletId(45);
    const wydb::ElementId chamferId(46);
    const wy3d::Union* pUnion = wy3d::Union::cast(pDb->getElement(unionId));
    ASSERT_NE(pUnion, nullptr);
    const double baselineVolume = getVolume(pUnion->getShape());
    EXPECT_GT(baselineVolume, 0.0);
    EXPECT_EQ(0u, getChainErrorCode(pDb.get(), chamferId));

    EXPECT_EQ(applySuppressed(pDb.get(), filletId, true), wy::ErrorStatus::Ok);
    const wy3d::BodyModification* pFillet = wy3d::BodyModification::cast(pDb->getElement(filletId));
    ASSERT_NE(pFillet, nullptr);
    EXPECT_TRUE(pFillet->isSuppressed());
    pUnion = wy3d::Union::cast(pDb->getElement(unionId));
    ASSERT_NE(pUnion, nullptr);
    EXPECT_GT(std::fabs(getVolume(pUnion->getShape()) - baselineVolume), 1.0);
    ASSERT_EQ(pUnion->getModifications().size(), 2u);

    EXPECT_EQ(applySuppressed(pDb.get(), filletId, false), wy::ErrorStatus::Ok);
    EXPECT_EQ(0u, getChainErrorCode(pDb.get(), chamferId));
    pUnion = wy3d::Union::cast(pDb->getElement(unionId));
    ASSERT_NE(pUnion, nullptr);
    EXPECT_NEAR(getVolume(pUnion->getShape()), baselineVolume, 1e-3);
}

// A downstream feature is not cascaded into suppression: it re-runs, cannot
// find its source and reports through its own existing error path.
TEST(FeatureSuppress, DownstreamThickenOnSuppressedSheet)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    const wydb::ElementId sketchId = createRectSketch(pDb.get(), 0.0);
    ASSERT_FALSE(sketchId.isNull());

    wydb::ElementId sheetId = wydb::ElementId::kNull;
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
        ASSERT_NE(pSketch, nullptr);
        wy3d::ExtrudedSheet* pSheet(nullptr);
        EXPECT_EQ(wy3d::ExtrudedSheet::create(pTrans, pSketch, 10.0, pSheet), wy::ErrorStatus::Ok);
        ASSERT_NE(pSheet, nullptr);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
        sheetId = pSheet->getId();
    }

    wydb::ElementId thickenId = wydb::ElementId::kNull;
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sheet* pSheet = wy3d::Sheet::cast(pTrans->getElementForWrite(sheetId));
        ASSERT_NE(pSheet, nullptr);
        wy3d::Thicken* pThicken(nullptr);
        EXPECT_EQ(wy3d::Thicken::create(pTrans, pSheet, 5.0,
            wy3d::ThickenDirection::OneSide, pThicken), wy::ErrorStatus::Ok);
        ASSERT_NE(pThicken, nullptr);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
        thickenId = pThicken->getId();
    }

    const wy3d::Thicken* pThicken = wy3d::Thicken::cast(pDb->getElement(thickenId));
    ASSERT_NE(pThicken, nullptr);
    const double baselineVolume = getVolume(pThicken->getShape());
    EXPECT_GT(baselineVolume, 0.0);
    EXPECT_EQ(0u, getChainErrorCode(pDb.get(), thickenId));

    EXPECT_EQ(applySuppressed(pDb.get(), sheetId, true), wy::ErrorStatus::Ok);
    pThicken = wy3d::Thicken::cast(pDb->getElement(thickenId));
    ASSERT_NE(pThicken, nullptr);
    EXPECT_TRUE(pThicken->getShape().IsNull());
    EXPECT_EQ(static_cast<std::uint32_t>(wy3d::ErrorCode::THICKEN_InvalidData),
        getChainErrorCode(pDb.get(), thickenId));

    EXPECT_EQ(applySuppressed(pDb.get(), sheetId, false), wy::ErrorStatus::Ok);
    pThicken = wy3d::Thicken::cast(pDb->getElement(thickenId));
    ASSERT_NE(pThicken, nullptr);
    EXPECT_EQ(0u, getChainErrorCode(pDb.get(), thickenId));
    EXPECT_NEAR(getVolume(pThicken->getShape()), baselineVolume, 1e-3);
}
