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

#include <wy3dExtrusion.h>
#include <wy3dSketch.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchPlane.h>
#include <wy3dParamNames.h>
#include <wy3dParamEnumDef.h>
#include <wydbParameter.h>

#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <fstream>

// --- helpers ---

// Create a sketch on the XY plane with a 100x50 closed rectangle and return its id
static wydb::ElementId createRectSketch(wy3d::Database* pDb)
{
    wydb::ElementId sketchId = wydb::ElementId::kNull;
    {
        wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
        wy3d::SketchPlane plane(wy::Vector3::kZero, wy::Vector3::kZAxis, wy::Vector3::kXAxis);
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

static void getShapeBounds(const TopoDS_Shape& shape, double& zmin, double& zmax)
{
    Bnd_Box bndBox;
    BRepBndLib::Add(shape, bndBox);
    zmin = bndBox.CornerMin().Z();
    zmax = bndBox.CornerMax().Z();
}

// --- Create ---

TEST(Extrusion, CreateOneSideDefault)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createRectSketch(pDb.get());

    Extrusion* pExtrusion(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
        ASSERT_NE(pSketch, nullptr);
        EXPECT_EQ(Extrusion::create(pTrans, pSketch, 20.0, pExtrusion), wy::ErrorStatus::Ok);
        pMgr->endTransaction();
    }
    ASSERT_NE(pExtrusion, nullptr);
    EXPECT_EQ(pExtrusion->getDirection(), ExtrusionDirection::OneSide);
    EXPECT_DOUBLE_EQ(pExtrusion->getDepth(), 20.0);
}

TEST(Extrusion, CreateWithDirection)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createRectSketch(pDb.get());

    Extrusion* pExtrusion(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
        ASSERT_NE(pSketch, nullptr);
        EXPECT_EQ(Extrusion::create(pTrans, pSketch, ExtrusionDirection::Symmetric, 20.0, pExtrusion), wy::ErrorStatus::Ok);
        pMgr->endTransaction();
    }
    ASSERT_NE(pExtrusion, nullptr);
    EXPECT_EQ(pExtrusion->getDirection(), ExtrusionDirection::Symmetric);

    // createCut with direction (the cut needs its own sketch)
    wydb::ElementId cutSketchId = createRectSketch(pDb.get());
    Extrusion* pCut(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(cutSketchId));
        ASSERT_NE(pSketch, nullptr);
        wy3d::Solid* pSolid = wy3d::Solid::cast(pTrans->getElementForWrite(pExtrusion->getId()));
        ASSERT_NE(pSolid, nullptr);
        EXPECT_EQ(Extrusion::createCut(pTrans, pSketch, ExtrusionDirection::Symmetric, 10.0, pSolid, pCut), wy::ErrorStatus::Ok);
        pMgr->endTransaction();
    }
    ASSERT_NE(pCut, nullptr);
    EXPECT_TRUE(pCut->isCut());
    EXPECT_EQ(pCut->getDirection(), ExtrusionDirection::Symmetric);
}

TEST(Extrusion, LegacyCreateCompatible)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createRectSketch(pDb.get());

    Extrusion* pExtrusion(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
        ASSERT_NE(pSketch, nullptr);
        EXPECT_EQ(Extrusion::create(pTrans, pSketch, 20.0, pExtrusion), wy::ErrorStatus::Ok);
        pMgr->endTransaction();
    }
    ASSERT_NE(pExtrusion, nullptr);

    // setDirection via field change
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        Extrusion* pWrite = Extrusion::cast(pTrans->getElementForWrite(pExtrusion->getId()));
        ASSERT_NE(pWrite, nullptr);
        EXPECT_EQ(pWrite->setDirection(ExtrusionDirection::Symmetric), wy::ErrorStatus::Ok);
        pMgr->endTransaction();
    }
    EXPECT_EQ(pExtrusion->getDirection(), ExtrusionDirection::Symmetric);

    // invalid enum values are rejected
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        Extrusion* pWrite = Extrusion::cast(pTrans->getElementForWrite(pExtrusion->getId()));
        ASSERT_NE(pWrite, nullptr);
        EXPECT_EQ(pWrite->setDirection(static_cast<ExtrusionDirection>(42)), wy::ErrorStatus::InvalidInput);
        EXPECT_EQ(pWrite->setDirection(static_cast<ExtrusionDirection>(-1)), wy::ErrorStatus::InvalidInput);
        pMgr->endTransaction();
    }
    EXPECT_EQ(pExtrusion->getDirection(), ExtrusionDirection::Symmetric);
}

// --- Geometry ---

TEST(Extrusion, SymmetricGeometry)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    const wydb::ElementId sketchId1 = createRectSketch(pDb.get());
    const wydb::ElementId sketchId2 = createRectSketch(pDb.get());
    const wydb::ElementId sketchId3 = createRectSketch(pDb.get());

    Extrusion* pOneSide(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId1));
        ASSERT_NE(pSketch, nullptr);
        EXPECT_EQ(Extrusion::create(pTrans, pSketch, ExtrusionDirection::OneSide, 20.0, pOneSide), wy::ErrorStatus::Ok);
        pMgr->endTransaction();
    }
    ASSERT_NE(pOneSide, nullptr);

    Extrusion* pSymmetric(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId2));
        ASSERT_NE(pSketch, nullptr);
        EXPECT_EQ(Extrusion::create(pTrans, pSketch, ExtrusionDirection::Symmetric, 20.0, pSymmetric), wy::ErrorStatus::Ok);
        pMgr->endTransaction();
    }
    ASSERT_NE(pSymmetric, nullptr);

    double zmin(0.0), zmax(0.0);
    getShapeBounds(pOneSide->getShape(), zmin, zmax);
    EXPECT_NEAR(zmin, 0.0, 1e-6);
    EXPECT_NEAR(zmax, 20.0, 1e-6);

    getShapeBounds(pSymmetric->getShape(), zmin, zmax);
    EXPECT_NEAR(zmin, -10.0, 1e-6);
    EXPECT_NEAR(zmax, 10.0, 1e-6);

    // negative depth has no direction meaning in symmetric mode
    Extrusion* pSymmetricNegative(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId3));
        ASSERT_NE(pSketch, nullptr);
        EXPECT_EQ(Extrusion::create(pTrans, pSketch, ExtrusionDirection::Symmetric, -20.0, pSymmetricNegative), wy::ErrorStatus::Ok);
        pMgr->endTransaction();
    }
    ASSERT_NE(pSymmetricNegative, nullptr);
    getShapeBounds(pSymmetricNegative->getShape(), zmin, zmax);
    EXPECT_NEAR(zmin, -10.0, 1e-6);
    EXPECT_NEAR(zmax, 10.0, 1e-6);
}

TEST(Extrusion, SymmetricWithStartOffset)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createRectSketch(pDb.get());

    Extrusion* pExtrusion(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
        ASSERT_NE(pSketch, nullptr);
        EXPECT_EQ(Extrusion::create(pTrans, pSketch, ExtrusionDirection::Symmetric, 20.0, pExtrusion), wy::ErrorStatus::Ok);
        EXPECT_EQ(pExtrusion->setStartOffset(5.0), wy::ErrorStatus::Ok);
        pMgr->endTransaction();
    }
    ASSERT_NE(pExtrusion, nullptr);

    // The body is centered on the offset sketch plane: [-5, 15]
    double zmin(0.0), zmax(0.0);
    getShapeBounds(pExtrusion->getShape(), zmin, zmax);
    EXPECT_NEAR(zmin, -5.0, 1e-6);
    EXPECT_NEAR(zmax, 15.0, 1e-6);
}

// --- Parameters ---

TEST(Extrusion, ParamRoundTrip)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId sketchId = createRectSketch(pDb.get());

    Extrusion* pExtrusion(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
        ASSERT_NE(pSketch, nullptr);
        EXPECT_EQ(Extrusion::create(pTrans, pSketch, 20.0, pExtrusion), wy::ErrorStatus::Ok);
        pMgr->endTransaction();
    }
    ASSERT_NE(pExtrusion, nullptr);

    const std::string className = wy3d::Extrusion::classInfo()->className();
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        Extrusion* pWrite = Extrusion::cast(pTrans->getElementForWrite(pExtrusion->getId()));
        ASSERT_NE(pWrite, nullptr);
        // enum parameter via Any<ParamEnumDef>
        wy3d::ParamEnumDef enumDef(
            {{static_cast<int>(ExtrusionDirection::OneSide), "One Side"},
             {static_cast<int>(ExtrusionDirection::Symmetric), "Symmetric"}},
            static_cast<int>(ExtrusionDirection::Symmetric));
        EXPECT_EQ(pWrite->setParameterValue(className, wy3d::ParamNames::EXTRUSION_PARAM_DIRECTION,
            *wydb::ParameterValue::createAny(enumDef)), wy::ErrorStatus::Ok);
        // enum parameter via plain integer
        EXPECT_EQ(pWrite->setParameterValue(className, wy3d::ParamNames::EXTRUSION_PARAM_DIRECTION,
            *wydb::ParameterValue::createInteger(0)), wy::ErrorStatus::Ok);
        // double input is rejected
        EXPECT_EQ(pWrite->setParameterValue(className, wy3d::ParamNames::EXTRUSION_PARAM_DIRECTION,
            *wydb::ParameterValue::createDouble(1.0)), wy::ErrorStatus::InvalidInput);
        pMgr->endTransaction();
    }
    EXPECT_EQ(pExtrusion->getDirection(), ExtrusionDirection::OneSide);

    wydb::ParameterValueUPtr pDirVal = pExtrusion->getParameterValue(className, wy3d::ParamNames::EXTRUSION_PARAM_DIRECTION);
    ASSERT_NE(pDirVal, nullptr);
    ASSERT_TRUE(pDirVal->isAny());
    const auto* pAnyVal = dynamic_cast<const wydb::AnyParameterValue*>(pDirVal.get());
    ASSERT_NE(pAnyVal, nullptr);
    const wy3d::ParamEnumDef* pDef = pAnyVal->tryGet<wy3d::ParamEnumDef>();
    ASSERT_NE(pDef, nullptr);
    EXPECT_EQ(pDef->currentValue, static_cast<int>(ExtrusionDirection::OneSide));
}

// --- IO ---

TEST(Extrusion, IO)
{
    std::string filePath("./test_extrusion.wy3dt");
    wydb::ElementId extrusionId = wydb::ElementId::kNull;

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        wydb::TransactionManager* pMgr = pDb->getTransactionManager();
        wydb::ElementId sketchId = createRectSketch(pDb.get());

        Extrusion* pExtrusion(nullptr);
        {
            wydb::Transaction* pTrans = pMgr->startTransaction();
            wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
            ASSERT_NE(pSketch, nullptr);
            EXPECT_EQ(Extrusion::create(pTrans, pSketch, ExtrusionDirection::Symmetric, 20.0, pExtrusion), wy::ErrorStatus::Ok);
            pMgr->endTransaction();
        }
        ASSERT_NE(pExtrusion, nullptr);
        extrusionId = pExtrusion->getId();

        EXPECT_EQ(pDb->writeFile(filePath, {wydb::FileType::Text}), wy::ErrorStatus::Ok);
    }

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        EXPECT_EQ(pDb->readFile(filePath, {wydb::FileType::Text}), wy::ErrorStatus::Ok);

        const wydb::Element* pElem = pDb->getElement(extrusionId);
        ASSERT_NE(pElem, nullptr);
        const Extrusion* pExtrusion = Extrusion::cast(pElem);
        ASSERT_NE(pExtrusion, nullptr);
        EXPECT_EQ(pExtrusion->getDirection(), ExtrusionDirection::Symmetric);
        EXPECT_DOUBLE_EQ(pExtrusion->getDepth(), 20.0);
        double zmin(0.0), zmax(0.0);
        getShapeBounds(pExtrusion->getShape(), zmin, zmax);
        EXPECT_NEAR(zmin, -10.0, 1e-6);
        EXPECT_NEAR(zmax, 10.0, 1e-6);
    }
}

TEST(Extrusion, IOWithDirection)
{
    std::string filePath("./test_extrusion_dir.wy3dt");

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        wydb::TransactionManager* pMgr = pDb->getTransactionManager();
        wydb::ElementId sketchId = createRectSketch(pDb.get());

        Extrusion* pExtrusion(nullptr);
        {
            wydb::Transaction* pTrans = pMgr->startTransaction();
            wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
            ASSERT_NE(pSketch, nullptr);
            EXPECT_EQ(Extrusion::create(pTrans, pSketch, ExtrusionDirection::OneSide, 20.0, pExtrusion), wy::ErrorStatus::Ok);
            pMgr->endTransaction();
        }
        ASSERT_NE(pExtrusion, nullptr);
        EXPECT_EQ(pDb->writeFile(filePath, {wydb::FileType::Text}), wy::ErrorStatus::Ok);
    }

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        EXPECT_EQ(pDb->readFile(filePath, {wydb::FileType::Text}), wy::ErrorStatus::Ok);

        // The default OneSide direction round-trips through the file
        wy::Iterator<wydb::ElementId> iter = pDb->createIterator();
        bool foundExtrusion(false);
        while (!iter.isDone())
        {
            const wydb::Element* pElem = pDb->getElement(iter.current());
            if (pElem && !pElem->isErased())
            {
                if (const Extrusion* pExtrusion = Extrusion::cast(pElem))
                {
                    foundExtrusion = true;
                    EXPECT_EQ(pExtrusion->getDirection(), ExtrusionDirection::OneSide);
                }
            }
            iter.moveNext();
        }
        EXPECT_TRUE(foundExtrusion);
    }
}
