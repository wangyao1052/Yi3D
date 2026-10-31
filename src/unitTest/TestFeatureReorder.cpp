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

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dBox.h>
#include <wy3dDefaultChainUpdateFeedback.h>
#include <wy3dExtrudedSheet.h>
#include <wy3dMath.h>
#include <wy3dMove.h>
#include <wy3dPlane.h>
#include <wy3dRotate.h>
#include <wy3dSheet.h>
#include <wy3dSketch.h>
#include <wy3dSketchLine.h>
#include <wy3dSolid.h>

#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <gp_Pnt.hxx>

namespace
{
    // A 100 x 100 x 100 box with two 20 x 20 x 20 members at the same spot:
    // a cut (A) and a fuse (B). [A, B] cuts then fills the slot (1,000,000);
    // [B, A] fuses then cuts it (992,000).
    struct MergeFixture
    {
        std::unique_ptr<wy3d::Database> pDb;
        wydb::ElementId hostId;
        wydb::ElementId cutId;
        wydb::ElementId fillId;
    };

    static MergeFixture createMergeFixture()
    {
        MergeFixture fixture;
        fixture.pDb = std::make_unique<wy3d::Database>();

        wydb::TransactionManager* pMgr = fixture.pDb->getTransactionManager();
        wydb::Transaction* pTrans = pMgr->startTransaction();
        EXPECT_NE(pTrans, nullptr);

        wy3d::Box* pHost(nullptr);
        wy3d::Box* pCut(nullptr);
        wy3d::Box* pFill(nullptr);
        EXPECT_EQ(wy3d::Box::create(pTrans, 100.0, 100.0, 100.0, pHost), wy::ErrorStatus::Ok);
        EXPECT_EQ(wy3d::Box::create(pTrans, 20.0, 20.0, 20.0, pCut), wy::ErrorStatus::Ok);
        EXPECT_EQ(wy3d::Box::create(pTrans, 20.0, 20.0, 20.0, pFill), wy::ErrorStatus::Ok);
        if (!pHost || !pCut || !pFill)
        {
            pMgr->abortTransaction();
            return fixture;
        }

        EXPECT_EQ(pCut->setPosition(wy::Vector3(40.0, 40.0, 0.0)), wy::ErrorStatus::Ok);
        EXPECT_EQ(pCut->setCut(true), wy::ErrorStatus::Ok);
        EXPECT_EQ(pFill->setPosition(wy::Vector3(40.0, 40.0, 0.0)), wy::ErrorStatus::Ok);
        EXPECT_EQ(pHost->addModification(pCut), wy::ErrorStatus::Ok);
        EXPECT_EQ(pHost->addModification(pFill), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);

        fixture.hostId = pHost->getId();
        fixture.cutId = pCut->getId();
        fixture.fillId = pFill->getId();
        return fixture;
    }

    class ModificationOrderReactor : public wydb::DatabaseReactor
    {
    public:
        struct Capture
        {
            wydb::DatabaseChangeCause cause = wydb::DatabaseChangeCause::EndTransaction;
            bool hasOrderPiece = false;
            bool hasShapePiece = false;
        };

        explicit ModificationOrderReactor(const wydb::ElementId& hostId)
            : _hostId(hostId)
        {
        }

        void onDatabaseChanged(
            const wydb::Database* /*pDatabase*/,
            const wydb::Transaction* /*pTransaction*/,
            const wydb::DatabaseChangeInfo& info) override
        {
            Capture capture;
            capture.cause = info.cause;
            capture.hasOrderPiece =
                info.details.isDataPieceDirty(_hostId, wydb::ElementDataPieceType::UserDefinedExt5);
            capture.hasShapePiece =
                info.details.isDataPieceDirty(_hostId, wydb::ElementDataPieceType::Shape);
            _captures.emplace_back(capture);
        }

        const std::vector<Capture>& getCaptures() const
        {
            return _captures;
        }

    private:
        wydb::ElementId _hostId;
        std::vector<Capture> _captures;
    };

    static const ModificationOrderReactor::Capture* findCapture(
        const ModificationOrderReactor& reactor, wydb::DatabaseChangeCause cause)
    {
        const std::vector<ModificationOrderReactor::Capture>& captures = reactor.getCaptures();
        for (auto iter = captures.crbegin(); iter != captures.crend(); ++iter)
        {
            if (cause == iter->cause)
            {
                return &(*iter);
            }
        }
        return nullptr;
    }

    static std::uint32_t getChainErrorCode(wy3d::Database* pDb, const wydb::ElementId& id)
    {
        return wy3d::getErrorCodeFromChainUpdateFeedback(
            pDb->getTransactionManager()->getChainUpdateFeedback(id).get());
    }

    static double getVolume(const TopoDS_Shape& shape)
    {
        GProp_GProps props;
        BRepGProp::VolumeProperties(shape, props);
        return props.Mass();
    }

    static wy::Vector3 getBBoxCenter(const TopoDS_Shape& shape)
    {
        Bnd_Box box;
        BRepBndLib::Add(shape, box);
        const gp_Pnt minCorner = box.CornerMin();
        const gp_Pnt maxCorner = box.CornerMax();
        return wy::Vector3(
            (minCorner.X() + maxCorner.X()) * 0.5,
            (minCorner.Y() + maxCorner.Y()) * 0.5,
            (minCorner.Z() + maxCorner.Z()) * 0.5);
    }

    // A 10 x 10 closed rectangle at the origin on the XY plane
    static wydb::ElementId createSquareSketch(wy3d::Database* pDb)
    {
        wydb::TransactionManager* pMgr = pDb->getTransactionManager();
        wydb::Transaction* pTrans = pMgr->startTransaction();
        EXPECT_NE(pTrans, nullptr);

        wy3d::Plane plane(wy::Vector3::kZero, wy::Vector3::kZAxis, wy::Vector3::kXAxis);
        wy3d::Sketch* pSketch(nullptr);
        EXPECT_EQ(wy3d::Sketch::create(pTrans, plane, pSketch), wy::ErrorStatus::Ok);
        if (!pSketch)
        {
            pMgr->abortTransaction();
            return wydb::ElementId::kNull;
        }

        wy3d::SketchLine* pLines[4] = { nullptr, nullptr, nullptr, nullptr };
        EXPECT_EQ(wy3d::SketchLine::create(pTrans, wy::Vector2(0.0, 0.0), wy::Vector2(10.0, 0.0), pLines[0]), wy::ErrorStatus::Ok);
        EXPECT_EQ(wy3d::SketchLine::create(pTrans, wy::Vector2(10.0, 0.0), wy::Vector2(10.0, 10.0), pLines[1]), wy::ErrorStatus::Ok);
        EXPECT_EQ(wy3d::SketchLine::create(pTrans, wy::Vector2(10.0, 10.0), wy::Vector2(0.0, 10.0), pLines[2]), wy::ErrorStatus::Ok);
        EXPECT_EQ(wy3d::SketchLine::create(pTrans, wy::Vector2(0.0, 10.0), wy::Vector2(0.0, 0.0), pLines[3]), wy::ErrorStatus::Ok);
        for (wy3d::SketchLine* pLine : pLines)
        {
            if (!pLine)
            {
                pMgr->abortTransaction();
                return wydb::ElementId::kNull;
            }
            EXPECT_EQ(pSketch->addEntity(pLine), wy::ErrorStatus::Ok);
        }

        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
        return pSketch->getId();
    }
}

TEST(FeatureReorder, SolidHostOrderChangesGeometry)
{
    MergeFixture fixture = createMergeFixture();
    ASSERT_NE(fixture.hostId, wydb::ElementId::kNull);

    wydb::TransactionManager* pMgr = fixture.pDb->getTransactionManager();

    const wy3d::Solid* pHost = wy3d::Solid::cast(fixture.pDb->getElement(fixture.hostId));
    ASSERT_NE(pHost, nullptr);
    ASSERT_EQ(pHost->getModifications().size(), 2u);
    EXPECT_EQ(pHost->getModifications()[0], fixture.cutId);
    EXPECT_EQ(pHost->getModifications()[1], fixture.fillId);
    EXPECT_NEAR(getVolume(pHost->getShape()), 1000000.0, 1e-3);

    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Solid* pHostWrite = wy3d::Solid::cast(pTrans->getElementForWrite(fixture.hostId));
        ASSERT_NE(pHostWrite, nullptr);
        EXPECT_EQ(pHostWrite->setModifications({ fixture.fillId, fixture.cutId }), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    pHost = wy3d::Solid::cast(fixture.pDb->getElement(fixture.hostId));
    ASSERT_NE(pHost, nullptr);
    EXPECT_EQ(pHost->getModifications()[0], fixture.fillId);
    EXPECT_EQ(pHost->getModifications()[1], fixture.cutId);
    EXPECT_NEAR(getVolume(pHost->getShape()), 992000.0, 1e-3);

    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Solid* pHostWrite = wy3d::Solid::cast(pTrans->getElementForWrite(fixture.hostId));
        ASSERT_NE(pHostWrite, nullptr);
        EXPECT_EQ(pHostWrite->setModifications({ fixture.cutId, fixture.fillId }), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    pHost = wy3d::Solid::cast(fixture.pDb->getElement(fixture.hostId));
    ASSERT_NE(pHost, nullptr);
    EXPECT_EQ(pHost->getModifications()[0], fixture.cutId);
    EXPECT_EQ(pHost->getModifications()[1], fixture.fillId);
    EXPECT_NEAR(getVolume(pHost->getShape()), 1000000.0, 1e-3);
}

TEST(FeatureReorder, SetModificationsValidation)
{
    MergeFixture fixture = createMergeFixture();
    ASSERT_NE(fixture.hostId, wydb::ElementId::kNull);

    wydb::TransactionManager* pMgr = fixture.pDb->getTransactionManager();
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Solid* pHost = wy3d::Solid::cast(pTrans->getElementForWrite(fixture.hostId));
        ASSERT_NE(pHost, nullptr);

        EXPECT_EQ(pHost->setModifications({ fixture.cutId, fixture.fillId }), wy::ErrorStatus::Ok);
        EXPECT_EQ(pHost->setModifications({ fixture.cutId }), wy::ErrorStatus::InvalidInput);
        EXPECT_EQ(pHost->setModifications({ fixture.cutId, fixture.cutId }), wy::ErrorStatus::InvalidInput);
        EXPECT_EQ(pHost->setModifications({ fixture.cutId, fixture.hostId }), wy::ErrorStatus::InvalidInput);
        ASSERT_EQ(pHost->getModifications().size(), 2u);
        EXPECT_EQ(pHost->getModifications()[0], fixture.cutId);
        EXPECT_EQ(pHost->getModifications()[1], fixture.fillId);

        EXPECT_EQ(pHost->setModifications({ fixture.fillId, fixture.cutId }), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    const wy3d::Solid* pHost = wy3d::Solid::cast(fixture.pDb->getElement(fixture.hostId));
    ASSERT_NE(pHost, nullptr);
    ASSERT_EQ(pHost->getModifications().size(), 2u);
    EXPECT_EQ(pHost->getModifications()[0], fixture.fillId);
    EXPECT_EQ(pHost->getModifications()[1], fixture.cutId);
}

TEST(FeatureReorder, SheetHostOrderChangesGeometry)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createSquareSketch(pDb.get());
    ASSERT_NE(sketchId, wydb::ElementId::kNull);

    wydb::ElementId sheetId = wydb::ElementId::kNull;
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
        ASSERT_NE(pSketch, nullptr);
        wy3d::ExtrudedSheet* pSheet(nullptr);
        EXPECT_EQ(wy3d::ExtrudedSheet::create(pTrans, pSketch, 10.0, pSheet), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
        ASSERT_NE(pSheet, nullptr);
        sheetId = pSheet->getId();
    }

    wydb::ElementId moveId = wydb::ElementId::kNull;
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sheet* pSheet = wy3d::Sheet::cast(pTrans->getElementForWrite(sheetId));
        ASSERT_NE(pSheet, nullptr);
        wy3d::Move* pMove(nullptr);
        EXPECT_EQ(wy3d::Move::create(pTrans, pSheet, wy::Vector3(100.0, 0.0, 0.0), pMove), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
        ASSERT_NE(pMove, nullptr);
        moveId = pMove->getId();
    }

    wydb::ElementId rotateId = wydb::ElementId::kNull;
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sheet* pSheet = wy3d::Sheet::cast(pTrans->getElementForWrite(sheetId));
        ASSERT_NE(pSheet, nullptr);
        wy3d::Rotate* pRotate(nullptr);
        EXPECT_EQ(wy3d::Rotate::create(pTrans, pSheet, wy::Vector3::kZero, wy::Vector3::kZAxis,
            wy3d::PI / 2.0, pRotate), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
        ASSERT_NE(pRotate, nullptr);
        rotateId = pRotate->getId();
    }

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    ASSERT_EQ(pSheet->getModifications().size(), 2u);
    EXPECT_EQ(pSheet->getModifications()[0], moveId);
    EXPECT_EQ(pSheet->getModifications()[1], rotateId);

    // Move (100, 0) then rotate +90 deg about the origin: bbox centre (-5, 105)
    const wy::Vector3 centerBefore = getBBoxCenter(pSheet->getShape());
    EXPECT_NEAR(centerBefore.x(), -5.0, 1e-6);
    EXPECT_NEAR(centerBefore.y(), 105.0, 1e-6);

    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sheet* pSheetWrite = wy3d::Sheet::cast(pTrans->getElementForWrite(sheetId));
        ASSERT_NE(pSheetWrite, nullptr);
        EXPECT_EQ(pSheetWrite->setModifications({ rotateId, moveId }), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    // Rotate first, then move: bbox centre (95, 5)
    pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    ASSERT_NE(pSheet, nullptr);
    EXPECT_EQ(pSheet->getModifications()[0], rotateId);
    EXPECT_EQ(pSheet->getModifications()[1], moveId);
    const wy::Vector3 centerAfter = getBBoxCenter(pSheet->getShape());
    EXPECT_NEAR(centerAfter.x(), 95.0, 1e-6);
    EXPECT_NEAR(centerAfter.y(), 5.0, 1e-6);
}

TEST(FeatureReorder, UndoRedo)
{
    MergeFixture fixture = createMergeFixture();
    ASSERT_NE(fixture.hostId, wydb::ElementId::kNull);

    wydb::TransactionManager* pMgr = fixture.pDb->getTransactionManager();
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Solid* pHost = wy3d::Solid::cast(pTrans->getElementForWrite(fixture.hostId));
        ASSERT_NE(pHost, nullptr);
        EXPECT_EQ(pHost->setModifications({ fixture.fillId, fixture.cutId }), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    const wy3d::Solid* pHost = wy3d::Solid::cast(fixture.pDb->getElement(fixture.hostId));
    ASSERT_NE(pHost, nullptr);
    ASSERT_EQ(pHost->getModifications().size(), 2u);
    EXPECT_EQ(pHost->getModifications()[0], fixture.fillId);
    EXPECT_EQ(pHost->getModifications()[1], fixture.cutId);
    EXPECT_NEAR(getVolume(pHost->getShape()), 992000.0, 1e-3);

    EXPECT_EQ(pMgr->undo(), wy::ErrorStatus::Ok);
    pHost = wy3d::Solid::cast(fixture.pDb->getElement(fixture.hostId));
    ASSERT_NE(pHost, nullptr);
    EXPECT_EQ(pHost->getModifications()[0], fixture.cutId);
    EXPECT_EQ(pHost->getModifications()[1], fixture.fillId);
    EXPECT_NEAR(getVolume(pHost->getShape()), 1000000.0, 1e-3);

    EXPECT_EQ(pMgr->redo(), wy::ErrorStatus::Ok);
    pHost = wy3d::Solid::cast(fixture.pDb->getElement(fixture.hostId));
    ASSERT_NE(pHost, nullptr);
    EXPECT_EQ(pHost->getModifications()[0], fixture.fillId);
    EXPECT_EQ(pHost->getModifications()[1], fixture.cutId);
    EXPECT_NEAR(getVolume(pHost->getShape()), 992000.0, 1e-3);
}

TEST(FeatureReorder, IO)
{
    const std::string filePath("./test_feature_reorder.wy3dt");

    MergeFixture fixture = createMergeFixture();
    ASSERT_NE(fixture.hostId, wydb::ElementId::kNull);

    wydb::TransactionManager* pMgr = fixture.pDb->getTransactionManager();
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Solid* pHost = wy3d::Solid::cast(pTrans->getElementForWrite(fixture.hostId));
        ASSERT_NE(pHost, nullptr);
        EXPECT_EQ(pHost->setModifications({ fixture.fillId, fixture.cutId }), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    EXPECT_EQ(fixture.pDb->writeFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);

    {
        std::unique_ptr<wy3d::Database> pReloadedDb = std::make_unique<wy3d::Database>();
        ASSERT_EQ(pReloadedDb->readFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);
        std::remove(filePath.c_str());

        const wy3d::Solid* pHost = wy3d::Solid::cast(pReloadedDb->getElement(fixture.hostId));
        ASSERT_NE(pHost, nullptr);
        ASSERT_EQ(pHost->getModifications().size(), 2u);
        EXPECT_EQ(pHost->getModifications()[0], fixture.fillId);
        EXPECT_EQ(pHost->getModifications()[1], fixture.cutId);
        EXPECT_NEAR(getVolume(pHost->getShape()), 992000.0, 1e-3);
    }
}

// Pins down the closed-source wydb behaviour the feature tree relies on:
// the custom order piece is reported in the change details and is replayed on undo/redo.
TEST(FeatureReorder, OrderPieceSignal)
{
    MergeFixture fixture = createMergeFixture();
    ASSERT_NE(fixture.hostId, wydb::ElementId::kNull);

    ModificationOrderReactor reactor(fixture.hostId);
    ASSERT_EQ(fixture.pDb->addReactor(&reactor), wy::ErrorStatus::Ok);

    wydb::TransactionManager* pMgr = fixture.pDb->getTransactionManager();
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Solid* pHost = wy3d::Solid::cast(pTrans->getElementForWrite(fixture.hostId));
        ASSERT_NE(pHost, nullptr);
        EXPECT_EQ(pHost->setModifications({ fixture.fillId, fixture.cutId }), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    const ModificationOrderReactor::Capture* pCapture =
        findCapture(reactor, wydb::DatabaseChangeCause::EndTransaction);
    ASSERT_NE(pCapture, nullptr);
    EXPECT_TRUE(pCapture->hasOrderPiece);
    EXPECT_TRUE(pCapture->hasShapePiece);

    EXPECT_EQ(pMgr->undo(), wy::ErrorStatus::Ok);
    pCapture = findCapture(reactor, wydb::DatabaseChangeCause::Undo);
    ASSERT_NE(pCapture, nullptr);
    EXPECT_TRUE(pCapture->hasOrderPiece);
    EXPECT_TRUE(pCapture->hasShapePiece);

    EXPECT_EQ(pMgr->redo(), wy::ErrorStatus::Ok);
    pCapture = findCapture(reactor, wydb::DatabaseChangeCause::Redo);
    ASSERT_NE(pCapture, nullptr);
    EXPECT_TRUE(pCapture->hasOrderPiece);
    EXPECT_TRUE(pCapture->hasShapePiece);

    EXPECT_EQ(fixture.pDb->removeReactor(&reactor), wy::ErrorStatus::Ok);
}

// Dependency cleanup rewrites the modification chain through the private setter,
// so it must not report an order change to the feature tree.
TEST(FeatureReorder, OrderPieceNotMarkedOnDependencyErase)
{
    MergeFixture fixture = createMergeFixture();
    ASSERT_NE(fixture.hostId, wydb::ElementId::kNull);

    ModificationOrderReactor reactor(fixture.hostId);
    ASSERT_EQ(fixture.pDb->addReactor(&reactor), wy::ErrorStatus::Ok);

    wydb::TransactionManager* pMgr = fixture.pDb->getTransactionManager();
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wydb::Element* pCut = pTrans->getElementForWrite(fixture.cutId);
        ASSERT_NE(pCut, nullptr);
        EXPECT_EQ(pCut->erase(true), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    const wy3d::Solid* pHost = wy3d::Solid::cast(fixture.pDb->getElement(fixture.hostId));
    ASSERT_NE(pHost, nullptr);
    ASSERT_EQ(pHost->getModifications().size(), 1u);
    EXPECT_EQ(pHost->getModifications()[0], fixture.fillId);

    const ModificationOrderReactor::Capture* pCapture =
        findCapture(reactor, wydb::DatabaseChangeCause::EndTransaction);
    ASSERT_NE(pCapture, nullptr);
    EXPECT_FALSE(pCapture->hasOrderPiece);
    EXPECT_TRUE(pCapture->hasShapePiece);

    EXPECT_EQ(pMgr->undo(), wy::ErrorStatus::Ok);
    pHost = wy3d::Solid::cast(fixture.pDb->getElement(fixture.hostId));
    ASSERT_NE(pHost, nullptr);
    ASSERT_EQ(pHost->getModifications().size(), 2u);
    EXPECT_EQ(pHost->getModifications()[0], fixture.cutId);
    EXPECT_EQ(pHost->getModifications()[1], fixture.fillId);

    pCapture = findCapture(reactor, wydb::DatabaseChangeCause::Undo);
    ASSERT_NE(pCapture, nullptr);
    EXPECT_FALSE(pCapture->hasOrderPiece);
    EXPECT_TRUE(pCapture->hasShapePiece);

    EXPECT_EQ(pMgr->redo(), wy::ErrorStatus::Ok);
    pHost = wy3d::Solid::cast(fixture.pDb->getElement(fixture.hostId));
    ASSERT_NE(pHost, nullptr);
    ASSERT_EQ(pHost->getModifications().size(), 1u);
    EXPECT_EQ(pHost->getModifications()[0], fixture.fillId);

    pCapture = findCapture(reactor, wydb::DatabaseChangeCause::Redo);
    ASSERT_NE(pCapture, nullptr);
    EXPECT_FALSE(pCapture->hasOrderPiece);
    EXPECT_TRUE(pCapture->hasShapePiece);

    EXPECT_EQ(fixture.pDb->removeReactor(&reactor), wy::ErrorStatus::Ok);
}

// A modification is re-evaluated on every host rebuild; finishing healthy must report a
// healthy feedback, otherwise the error from an earlier rebuild outlives the failure.
TEST(FeatureReorder, FailedModificationHealthRecoversAfterReorderBack)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    const std::string filePath = std::string(YI3D_SOURCE_DIR) + "/samples/funnel.wy3dt";
    ASSERT_EQ(pDb->readFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);

    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    const wydb::ElementId hostId(44);
    const wydb::ElementId filletId(45);
    const wydb::ElementId chamferId(46);

    const wy3d::Solid* pHost = wy3d::Solid::cast(pDb->getElement(hostId));
    ASSERT_NE(pHost, nullptr);
    ASSERT_EQ(pHost->getModifications().size(), 2u);
    EXPECT_EQ(pHost->getModifications()[0], filletId);
    EXPECT_EQ(pHost->getModifications()[1], chamferId);
    EXPECT_EQ(0u, getChainErrorCode(pDb.get(), filletId));
    EXPECT_EQ(0u, getChainErrorCode(pDb.get(), chamferId));
    const double volumeBefore = getVolume(pHost->getShape());

    // Chamfer first: it consumes the edges the fillet needs, so the fillet fails.
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        ASSERT_NE(pTrans, nullptr);
        wy3d::Solid* pHostWrite = wy3d::Solid::cast(pTrans->getElementForWrite(hostId));
        ASSERT_NE(pHostWrite, nullptr);
        EXPECT_EQ(pHostWrite->setModifications({ chamferId, filletId }), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    EXPECT_NE(0u, getChainErrorCode(pDb.get(), filletId));

    // Fillet first again: the rebuild succeeds and the fillet's health must recover.
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        ASSERT_NE(pTrans, nullptr);
        wy3d::Solid* pHostWrite = wy3d::Solid::cast(pTrans->getElementForWrite(hostId));
        ASSERT_NE(pHostWrite, nullptr);
        EXPECT_EQ(pHostWrite->setModifications({ filletId, chamferId }), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    EXPECT_EQ(0u, getChainErrorCode(pDb.get(), filletId));
    EXPECT_EQ(0u, getChainErrorCode(pDb.get(), chamferId));

    pHost = wy3d::Solid::cast(pDb->getElement(hostId));
    ASSERT_NE(pHost, nullptr);
    EXPECT_NEAR(getVolume(pHost->getShape()), volumeBefore, 1e-3);
}
