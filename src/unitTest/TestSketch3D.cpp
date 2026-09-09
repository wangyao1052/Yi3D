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

#include <wy3dSketch3D.h>
#include <wy3dSketchLine3D.h>
#include <wy3dSketchCircle3D.h>
#include <wy3dSketch3DParamNames.h>
#include <wy3dMath.h>
#include <wydbParameter.h>
#include <wyIterator.h>

#include <cmath>
#include <set>

namespace
{
    std::size_t countElements(wy3d::Database* pDb)
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
}

// --- Create ---

TEST(Sketch3D, CreateContainer)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    wy3d::Sketch3D* pSketch3D(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        EXPECT_EQ(wy3d::Sketch3D::create(pTrans, pSketch3D), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSketch3D, nullptr);
    EXPECT_TRUE(pSketch3D->getChildren().empty());
    EXPECT_TRUE(pSketch3D->getParent().isNull());
}

TEST(Sketch3D, CreateLine)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    wy3d::SketchLine3D* pLine(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        EXPECT_EQ(wy3d::SketchLine3D::create(pTrans, wy::Vector3(1.0, 2.0, 3.0), wy::Vector3(4.0, 6.0, 8.0), pLine), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pLine, nullptr);
    EXPECT_DOUBLE_EQ(pLine->getStartPoint().x(), 1.0);
    EXPECT_DOUBLE_EQ(pLine->getStartPoint().y(), 2.0);
    EXPECT_DOUBLE_EQ(pLine->getStartPoint().z(), 3.0);
    EXPECT_DOUBLE_EQ(pLine->getEndPoint().x(), 4.0);
    EXPECT_DOUBLE_EQ(pLine->getEndPoint().y(), 6.0);
    EXPECT_DOUBLE_EQ(pLine->getEndPoint().z(), 8.0);
    EXPECT_NEAR(pLine->getLength(), std::sqrt(50.0), 1e-9);
}

TEST(Sketch3D, CreateCircle)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    wy3d::SketchCircle3D* pCircle(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        // xDir (2,2,0) is not perpendicular to normal (0,0,1) — create orthogonalizes it
        EXPECT_EQ(wy3d::SketchCircle3D::create(pTrans, wy::Vector3(10.0, 0.0, 5.0), wy::Vector3(0.0, 0.0, 1.0), wy::Vector3(2.0, 2.0, 0.0), 25.0, pCircle), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pCircle, nullptr);
    EXPECT_DOUBLE_EQ(pCircle->getCenter().x(), 10.0);
    EXPECT_DOUBLE_EQ(pCircle->getCenter().y(), 0.0);
    EXPECT_DOUBLE_EQ(pCircle->getCenter().z(), 5.0);
    EXPECT_NEAR(pCircle->getNormal().length(), 1.0, 1e-9);
    EXPECT_DOUBLE_EQ(pCircle->getNormal().z(), 1.0);
    EXPECT_NEAR(pCircle->getXDir().length(), 1.0, 1e-9);
    EXPECT_NEAR(pCircle->getXDir().x(), std::sqrt(2.0) / 2.0, 1e-9);
    EXPECT_NEAR(pCircle->getXDir().y(), std::sqrt(2.0) / 2.0, 1e-9);
    EXPECT_NEAR(pCircle->getXDir().z(), 0.0, 1e-9);
    EXPECT_DOUBLE_EQ(pCircle->getRadius(), 25.0);

    // normal/xDir are immutable via parameters
    const std::string className = wy3d::SketchCircle3D::classInfo()->className();
    EXPECT_EQ(pCircle->setParameterValue(className, wy3d::Sketch3DParamNames::SKETCH_CIRCLE3D_PARAM_NORMAL_X,
        *wydb::ParameterValue::createDouble(0.0)), wy::ErrorStatus::ParameterReadonly);
    EXPECT_EQ(pCircle->setParameterValue(className, wy3d::Sketch3DParamNames::SKETCH_CIRCLE3D_PARAM_XDIR_X,
        *wydb::ParameterValue::createDouble(0.0)), wy::ErrorStatus::ParameterReadonly);

    // Parametric start point = center + radius * xDir
    const wy::Vector3 pStart = pCircle->getStartPoint();
    EXPECT_NEAR(pStart.x(), 10.0 + 25.0 * pCircle->getXDir().x(), 1e-9);
    EXPECT_NEAR(pStart.y(), 25.0 * pCircle->getXDir().y(), 1e-9);
    EXPECT_NEAR(pStart.z(), 5.0, 1e-9);

    // zero xDir: derived from world axes (X, then Y, then Z)
    wy3d::SketchCircle3D* pAutoXDir(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        // normal along the X axis: X is parallel, so Y is used
        EXPECT_EQ(wy3d::SketchCircle3D::create(pTrans, wy::Vector3::kZero, wy::Vector3::kXAxis, wy::Vector3::kZero, 10.0, pAutoXDir), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pAutoXDir, nullptr);
    EXPECT_DOUBLE_EQ(pAutoXDir->getXDir().x(), 0.0);
    EXPECT_DOUBLE_EQ(pAutoXDir->getXDir().y(), 1.0);
    EXPECT_DOUBLE_EQ(pAutoXDir->getXDir().z(), 0.0);
}

// --- Container / entity relationship ---

TEST(Sketch3D, AddEntityAndChildren)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    wy3d::Sketch3D* pSketch3D(nullptr);
    wy3d::SketchLine3D* pLine(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        EXPECT_EQ(wy3d::Sketch3D::create(pTrans, pSketch3D), wy::ErrorStatus::Ok);
        EXPECT_EQ(wy3d::SketchLine3D::create(pTrans, wy::Vector3::kZero, wy::Vector3(1.0, 0.0, 0.0), pLine), wy::ErrorStatus::Ok);
        EXPECT_EQ(pSketch3D->addEntity(pLine), wy::ErrorStatus::Ok);
        // adding the same entity twice is idempotent
        EXPECT_EQ(pSketch3D->addEntity(pLine), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSketch3D, nullptr);
    ASSERT_NE(pLine, nullptr);

    const std::vector<wydb::ElementId> children = pSketch3D->getChildren();
    ASSERT_EQ(children.size(), 1u);
    EXPECT_EQ(children[0], pLine->getId());
    EXPECT_EQ(pLine->getParent(), pSketch3D->getId());

    wy::Iterator<wydb::ElementId> iter = pSketch3D->createIterator();
    ASSERT_FALSE(iter.isDone());
    EXPECT_EQ(iter.current(), pLine->getId());
    iter.moveNext();
    EXPECT_TRUE(iter.isDone());

    // an entity owned by another sketch is rejected
    wy3d::Sketch3D* pOther(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        EXPECT_EQ(wy3d::Sketch3D::create(pTrans, pOther), wy::ErrorStatus::Ok);
        EXPECT_EQ(pOther->addEntity(pLine), wy::ErrorStatus::InvalidInput);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pOther, nullptr);
    EXPECT_TRUE(pOther->getChildren().empty());
}

// --- Parameters ---

TEST(Sketch3D, LineParamRoundTrip)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    wy3d::SketchLine3D* pLine(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        EXPECT_EQ(wy3d::SketchLine3D::create(pTrans, wy::Vector3(1.0, 2.0, 3.0), wy::Vector3(4.0, 6.0, 8.0), pLine), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pLine, nullptr);

    const std::string className = wy3d::SketchLine3D::classInfo()->className();

    // read parameters
    {
        wydb::ParameterValueUPtr pVal = pLine->getParameterValue(className, wy3d::Sketch3DParamNames::SKETCH_LINE3D_PARAM_START_X);
        ASSERT_NE(pVal, nullptr);
        EXPECT_TRUE(pVal->isDouble());
        EXPECT_DOUBLE_EQ(pVal->asDouble(), 1.0);
    }
    {
        wydb::ParameterValueUPtr pVal = pLine->getParameterValue(className, wy3d::Sketch3DParamNames::SKETCH_LINE3D_PARAM_END_Z);
        ASSERT_NE(pVal, nullptr);
        EXPECT_DOUBLE_EQ(pVal->asDouble(), 8.0);
    }
    {
        wydb::ParameterValueUPtr pVal = pLine->getParameterValue(className, wy3d::Sketch3DParamNames::SKETCH_LINE3D_PARAM_LENGTH);
        ASSERT_NE(pVal, nullptr);
        EXPECT_NEAR(pVal->asDouble(), std::sqrt(50.0), 1e-9);
    }

    // set START_X via parameter
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::SketchLine3D* pWrite = wy3d::SketchLine3D::cast(pTrans->getElementForWrite(pLine->getId()));
        ASSERT_NE(pWrite, nullptr);
        EXPECT_EQ(pWrite->setParameterValue(className, wy3d::Sketch3DParamNames::SKETCH_LINE3D_PARAM_START_X,
            *wydb::ParameterValue::createDouble(0.0)), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    EXPECT_DOUBLE_EQ(pLine->getStartPoint().x(), 0.0);

    // set LENGTH moves the end point along the current direction
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::SketchLine3D* pWrite = wy3d::SketchLine3D::cast(pTrans->getElementForWrite(pLine->getId()));
        ASSERT_NE(pWrite, nullptr);
        EXPECT_EQ(pWrite->setParameterValue(className, wy3d::Sketch3DParamNames::SKETCH_LINE3D_PARAM_LENGTH,
            *wydb::ParameterValue::createDouble(10.0)), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    EXPECT_NEAR(pLine->getLength(), 10.0, 1e-9);

    // non-positive length is rejected
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::SketchLine3D* pWrite = wy3d::SketchLine3D::cast(pTrans->getElementForWrite(pLine->getId()));
        ASSERT_NE(pWrite, nullptr);
        EXPECT_EQ(pWrite->setParameterValue(className, wy3d::Sketch3DParamNames::SKETCH_LINE3D_PARAM_LENGTH,
            *wydb::ParameterValue::createDouble(-5.0)), wy::ErrorStatus::InvalidInput);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    EXPECT_NEAR(pLine->getLength(), 10.0, 1e-9);

    // unknown parameter is rejected
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::SketchLine3D* pWrite = wy3d::SketchLine3D::cast(pTrans->getElementForWrite(pLine->getId()));
        ASSERT_NE(pWrite, nullptr);
        EXPECT_EQ(pWrite->setParameterValue(className, "Unknown",
            *wydb::ParameterValue::createDouble(1.0)), wy::ErrorStatus::ParameterNotFound);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
}

TEST(Sketch3D, CircleParamRoundTrip)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    wy3d::SketchCircle3D* pCircle(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        EXPECT_EQ(wy3d::SketchCircle3D::create(pTrans, wy::Vector3(10.0, 0.0, 5.0), wy::Vector3(0.0, 0.0, 1.0), wy::Vector3(1.0, 0.0, 0.0), 25.0, pCircle), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pCircle, nullptr);

    const std::string className = wy3d::SketchCircle3D::classInfo()->className();

    // derived parameters
    {
        wydb::ParameterValueUPtr pVal = pCircle->getParameterValue(className, wy3d::Sketch3DParamNames::SKETCH_CIRCLE3D_PARAM_DIAMETER);
        ASSERT_NE(pVal, nullptr);
        EXPECT_NEAR(pVal->asDouble(), 50.0, 1e-9);
    }
    {
        wydb::ParameterValueUPtr pVal = pCircle->getParameterValue(className, wy3d::Sketch3DParamNames::SKETCH_CIRCLE3D_PARAM_PERIMETER);
        ASSERT_NE(pVal, nullptr);
        EXPECT_NEAR(pVal->asDouble(), 2.0 * wy3d::PI * 25.0, 1e-9);
    }
    {
        wydb::ParameterValueUPtr pVal = pCircle->getParameterValue(className, wy3d::Sketch3DParamNames::SKETCH_CIRCLE3D_PARAM_AREA);
        ASSERT_NE(pVal, nullptr);
        EXPECT_NEAR(pVal->asDouble(), wy3d::PI * 25.0 * 25.0, 1e-9);
    }

    // set DIAMETER updates the radius
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::SketchCircle3D* pWrite = wy3d::SketchCircle3D::cast(pTrans->getElementForWrite(pCircle->getId()));
        ASSERT_NE(pWrite, nullptr);
        EXPECT_EQ(pWrite->setParameterValue(className, wy3d::Sketch3DParamNames::SKETCH_CIRCLE3D_PARAM_DIAMETER,
            *wydb::ParameterValue::createDouble(30.0)), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    EXPECT_DOUBLE_EQ(pCircle->getRadius(), 15.0);

    // set PERIMETER updates the radius
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::SketchCircle3D* pWrite = wy3d::SketchCircle3D::cast(pTrans->getElementForWrite(pCircle->getId()));
        ASSERT_NE(pWrite, nullptr);
        EXPECT_EQ(pWrite->setParameterValue(className, wy3d::Sketch3DParamNames::SKETCH_CIRCLE3D_PARAM_PERIMETER,
            *wydb::ParameterValue::createDouble(2.0 * wy3d::PI * 10.0)), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    EXPECT_NEAR(pCircle->getRadius(), 10.0, 1e-9);

    // too small radius is rejected
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::SketchCircle3D* pWrite = wy3d::SketchCircle3D::cast(pTrans->getElementForWrite(pCircle->getId()));
        ASSERT_NE(pWrite, nullptr);
        EXPECT_EQ(pWrite->setParameterValue(className, wy3d::Sketch3DParamNames::SKETCH_CIRCLE3D_PARAM_RADIUS,
            *wydb::ParameterValue::createDouble(0.0005)), wy::ErrorStatus::InvalidInput);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    EXPECT_NEAR(pCircle->getRadius(), 10.0, 1e-9);
}

// --- Invalid input ---

TEST(Sketch3D, InvalidInput)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    wy3d::SketchCircle3D* pCircle(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        EXPECT_EQ(wy3d::SketchCircle3D::create(pTrans, wy::Vector3(10.0, 0.0, 5.0), wy::Vector3(0.0, 0.0, 1.0), wy::Vector3(1.0, 0.0, 0.0), 25.0, pCircle), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pCircle, nullptr);

    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::SketchCircle3D* pWrite = wy3d::SketchCircle3D::cast(pTrans->getElementForWrite(pCircle->getId()));
        ASSERT_NE(pWrite, nullptr);
        EXPECT_EQ(pWrite->setRadius(-1.0), wy::ErrorStatus::InvalidInput);
        EXPECT_EQ(pWrite->setRadius(0.0), wy::ErrorStatus::InvalidInput);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    EXPECT_DOUBLE_EQ(pCircle->getRadius(), 25.0);

    // create with an invalid radius fails and returns no element
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::SketchCircle3D* pBad(nullptr);
        EXPECT_EQ(wy3d::SketchCircle3D::create(pTrans, wy::Vector3::kZero, wy::Vector3::kZAxis, wy::Vector3::kXAxis, -1.0, pBad), wy::ErrorStatus::InvalidInput);
        EXPECT_EQ(pBad, nullptr);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    // create with xDir parallel to normal fails and returns no element
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::SketchCircle3D* pBad(nullptr);
        EXPECT_EQ(wy3d::SketchCircle3D::create(pTrans, wy::Vector3::kZero, wy::Vector3::kZAxis, wy::Vector3::kZAxis, 10.0, pBad), wy::ErrorStatus::InvalidInput);
        EXPECT_EQ(pBad, nullptr);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    EXPECT_EQ(countElements(pDb.get()), 1u);
}

// --- IO ---

TEST(Sketch3D, IOLineCircleContainer)
{
    std::string filePath("./test_sketch3d.wy3dt");
    wydb::ElementId sketch3DId = wydb::ElementId::kNull;
    wydb::ElementId lineId = wydb::ElementId::kNull;
    wydb::ElementId circleId = wydb::ElementId::kNull;

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        wydb::TransactionManager* pMgr = pDb->getTransactionManager();

        {
            wydb::Transaction* pTrans = pMgr->startTransaction();
            wy3d::Sketch3D* pSketch3D(nullptr);
            wy3d::SketchLine3D* pLine(nullptr);
            wy3d::SketchCircle3D* pCircle(nullptr);
            EXPECT_EQ(wy3d::Sketch3D::create(pTrans, pSketch3D), wy::ErrorStatus::Ok);
            EXPECT_EQ(wy3d::SketchLine3D::create(pTrans, wy::Vector3(1.0, 2.0, 3.0), wy::Vector3(4.0, 6.0, 8.0), pLine), wy::ErrorStatus::Ok);
            EXPECT_EQ(wy3d::SketchCircle3D::create(pTrans, wy::Vector3(10.0, 0.0, 5.0), wy::Vector3(0.0, 0.0, 1.0), wy::Vector3(1.0, 0.0, 0.0), 25.0, pCircle), wy::ErrorStatus::Ok);
            EXPECT_EQ(pSketch3D->addEntity(pLine), wy::ErrorStatus::Ok);
            EXPECT_EQ(pSketch3D->addEntity(pCircle), wy::ErrorStatus::Ok);
            EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);

            sketch3DId = pSketch3D->getId();
            lineId = pLine->getId();
            circleId = pCircle->getId();
        }

        EXPECT_EQ(pDb->writeFile(filePath, {wydb::FileType::Text}), wy::ErrorStatus::Ok);
    }

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        EXPECT_EQ(pDb->readFile(filePath, {wydb::FileType::Text}), wy::ErrorStatus::Ok);

        const wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pDb->getElement(sketch3DId));
        ASSERT_NE(pSketch3D, nullptr);
        const std::vector<wydb::ElementId> children = pSketch3D->getChildren();
        ASSERT_EQ(children.size(), 2u);

        const wy3d::SketchLine3D* pLine = wy3d::SketchLine3D::cast(pDb->getElement(lineId));
        ASSERT_NE(pLine, nullptr);
        EXPECT_DOUBLE_EQ(pLine->getStartPoint().x(), 1.0);
        EXPECT_DOUBLE_EQ(pLine->getEndPoint().z(), 8.0);
        EXPECT_EQ(pLine->getParent(), sketch3DId);

        const wy3d::SketchCircle3D* pCircle = wy3d::SketchCircle3D::cast(pDb->getElement(circleId));
        ASSERT_NE(pCircle, nullptr);
        EXPECT_DOUBLE_EQ(pCircle->getCenter().x(), 10.0);
        EXPECT_DOUBLE_EQ(pCircle->getNormal().z(), 1.0);
        EXPECT_DOUBLE_EQ(pCircle->getRadius(), 25.0);
    }
}

// --- Erasure ---

TEST(Sketch3D, EraseEntityDropsFromContainer)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    wy3d::Sketch3D* pSketch3D(nullptr);
    wy3d::SketchLine3D* pLine(nullptr);
    wy3d::SketchCircle3D* pCircle(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        EXPECT_EQ(wy3d::Sketch3D::create(pTrans, pSketch3D), wy::ErrorStatus::Ok);
        EXPECT_EQ(wy3d::SketchLine3D::create(pTrans, wy::Vector3(1.0, 2.0, 3.0), wy::Vector3(4.0, 6.0, 8.0), pLine), wy::ErrorStatus::Ok);
        EXPECT_EQ(wy3d::SketchCircle3D::create(pTrans, wy::Vector3(10.0, 0.0, 5.0), wy::Vector3(0.0, 0.0, 1.0), wy::Vector3(1.0, 0.0, 0.0), 25.0, pCircle), wy::ErrorStatus::Ok);
        EXPECT_EQ(pSketch3D->addEntity(pLine), wy::ErrorStatus::Ok);
        EXPECT_EQ(pSketch3D->addEntity(pCircle), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSketch3D, nullptr);
    ASSERT_EQ(pSketch3D->getChildren().size(), 2u);

    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::SketchLine3D* pLineWrite = wy3d::SketchLine3D::cast(pTrans->getElementForWrite(pLine->getId()));
        ASSERT_NE(pLineWrite, nullptr);
        EXPECT_EQ(pLineWrite->erase(true), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    EXPECT_TRUE(pLine->isErased());
    const std::vector<wydb::ElementId> children = pSketch3D->getChildren();
    ASSERT_EQ(children.size(), 1u);
    EXPECT_EQ(children[0], pCircle->getId());
    EXPECT_EQ(countElements(pDb.get()), 2u); // sketch3D + circle
}

TEST(Sketch3D, EraseContainerCascades)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    wy3d::Sketch3D* pSketch3D(nullptr);
    wy3d::SketchLine3D* pLine(nullptr);
    wy3d::SketchCircle3D* pCircle(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        EXPECT_EQ(wy3d::Sketch3D::create(pTrans, pSketch3D), wy::ErrorStatus::Ok);
        EXPECT_EQ(wy3d::SketchLine3D::create(pTrans, wy::Vector3(1.0, 2.0, 3.0), wy::Vector3(4.0, 6.0, 8.0), pLine), wy::ErrorStatus::Ok);
        EXPECT_EQ(wy3d::SketchCircle3D::create(pTrans, wy::Vector3(10.0, 0.0, 5.0), wy::Vector3(0.0, 0.0, 1.0), wy::Vector3(1.0, 0.0, 0.0), 25.0, pCircle), wy::ErrorStatus::Ok);
        EXPECT_EQ(pSketch3D->addEntity(pLine), wy::ErrorStatus::Ok);
        EXPECT_EQ(pSketch3D->addEntity(pCircle), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSketch3D, nullptr);

    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pWrite = wy3d::Sketch3D::cast(pTrans->getElementForWrite(pSketch3D->getId()));
        ASSERT_NE(pWrite, nullptr);
        EXPECT_EQ(pWrite->erase(true), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    EXPECT_TRUE(pSketch3D->isErased());
    EXPECT_TRUE(pLine->isErased());
    EXPECT_TRUE(pCircle->isErased());
    EXPECT_EQ(countElements(pDb.get()), 0u);
}

// --- Parameter schema (property-panel contract) ---

TEST(Sketch3D, ParameterSchemasExposed)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    wy3d::Sketch3D* pSketch3D(nullptr);
    wy3d::SketchLine3D* pLine(nullptr);
    wy3d::SketchCircle3D* pCircle(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        EXPECT_EQ(wy3d::Sketch3D::create(pTrans, pSketch3D), wy::ErrorStatus::Ok);
        EXPECT_EQ(wy3d::SketchLine3D::create(pTrans, wy::Vector3(1.0, 2.0, 3.0), wy::Vector3(4.0, 6.0, 8.0), pLine), wy::ErrorStatus::Ok);
        EXPECT_EQ(wy3d::SketchCircle3D::create(pTrans, wy::Vector3(10.0, 0.0, 5.0), wy::Vector3(0.0, 0.0, 1.0), wy::Vector3(1.0, 0.0, 0.0), 25.0, pCircle), wy::ErrorStatus::Ok);
        EXPECT_EQ(pSketch3D->addEntity(pLine), wy::ErrorStatus::Ok);
        EXPECT_EQ(pSketch3D->addEntity(pCircle), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSketch3D, nullptr);
    ASSERT_NE(pLine, nullptr);
    ASSERT_NE(pCircle, nullptr);

    // The container exposes no parameters (empty panel, like a 2D sketch)
    EXPECT_TRUE(pSketch3D->listParameters().empty());

    // Line schema: 7 own defs + 1 inherited readonly ID from SketchCurve3D
    const std::string lineClassName = wy3d::SketchLine3D::classInfo()->className();
    const std::string curve3dClassName = wy3d::SketchCurve3D::classInfo()->className();
    std::vector<const wydb::ParameterDefinition*> lineDefs = pLine->listParameters();
    ASSERT_EQ(lineDefs.size(), 8u);
    std::set<std::string> lineNames;
    bool foundReadonlyId(false);
    for (const wydb::ParameterDefinition* pDef : lineDefs)
    {
        ASSERT_NE(pDef, nullptr);
        EXPECT_TRUE(pDef->getClassName() == lineClassName || pDef->getClassName() == curve3dClassName);
        if (pDef->getName() == wy3d::Sketch3DParamNames::SKETCH_CURVE3D_ID)
        {
            EXPECT_EQ(pDef->getClassName(), curve3dClassName);
            EXPECT_TRUE(pDef->isReadonly());
            foundReadonlyId = true;
        }
        lineNames.insert(pDef->getName());
    }
    EXPECT_TRUE(foundReadonlyId);
    EXPECT_EQ(lineNames.count(wy3d::Sketch3DParamNames::SKETCH_LINE3D_PARAM_START_X), 1u);
    EXPECT_EQ(lineNames.count(wy3d::Sketch3DParamNames::SKETCH_LINE3D_PARAM_START_Y), 1u);
    EXPECT_EQ(lineNames.count(wy3d::Sketch3DParamNames::SKETCH_LINE3D_PARAM_START_Z), 1u);
    EXPECT_EQ(lineNames.count(wy3d::Sketch3DParamNames::SKETCH_LINE3D_PARAM_END_X), 1u);
    EXPECT_EQ(lineNames.count(wy3d::Sketch3DParamNames::SKETCH_LINE3D_PARAM_END_Y), 1u);
    EXPECT_EQ(lineNames.count(wy3d::Sketch3DParamNames::SKETCH_LINE3D_PARAM_END_Z), 1u);
    EXPECT_EQ(lineNames.count(wy3d::Sketch3DParamNames::SKETCH_LINE3D_PARAM_LENGTH), 1u);

    // Circle schema: 13 own defs + 1 inherited readonly ID from SketchCurve3D
    const std::string circleClassName = wy3d::SketchCircle3D::classInfo()->className();
    std::vector<const wydb::ParameterDefinition*> circleDefs = pCircle->listParameters();
    ASSERT_EQ(circleDefs.size(), 14u);
    std::set<std::string> circleNames;
    for (const wydb::ParameterDefinition* pDef : circleDefs)
    {
        ASSERT_NE(pDef, nullptr);
        EXPECT_TRUE(pDef->getClassName() == circleClassName || pDef->getClassName() == curve3dClassName);
        circleNames.insert(pDef->getName());
        // NORMAL/XDIR are creation-time values: readonly in the panel, like the inherited ID
        const std::string& name = pDef->getName();
        bool expectReadonly = false;
        if (name == wy3d::Sketch3DParamNames::SKETCH_CIRCLE3D_PARAM_NORMAL_X ||
            name == wy3d::Sketch3DParamNames::SKETCH_CIRCLE3D_PARAM_NORMAL_Y ||
            name == wy3d::Sketch3DParamNames::SKETCH_CIRCLE3D_PARAM_NORMAL_Z ||
            name == wy3d::Sketch3DParamNames::SKETCH_CIRCLE3D_PARAM_XDIR_X ||
            name == wy3d::Sketch3DParamNames::SKETCH_CIRCLE3D_PARAM_XDIR_Y ||
            name == wy3d::Sketch3DParamNames::SKETCH_CIRCLE3D_PARAM_XDIR_Z ||
            name == wy3d::Sketch3DParamNames::SKETCH_CURVE3D_ID)
        {
            expectReadonly = true;
        }
        EXPECT_EQ(pDef->isReadonly(), expectReadonly);
    }
    EXPECT_EQ(circleNames.count(wy3d::Sketch3DParamNames::SKETCH_CURVE3D_ID), 1u);
    EXPECT_EQ(circleNames.count(wy3d::Sketch3DParamNames::SKETCH_CIRCLE3D_PARAM_CENTER_X), 1u);
    EXPECT_EQ(circleNames.count(wy3d::Sketch3DParamNames::SKETCH_CIRCLE3D_PARAM_CENTER_Y), 1u);
    EXPECT_EQ(circleNames.count(wy3d::Sketch3DParamNames::SKETCH_CIRCLE3D_PARAM_CENTER_Z), 1u);
    EXPECT_EQ(circleNames.count(wy3d::Sketch3DParamNames::SKETCH_CIRCLE3D_PARAM_NORMAL_X), 1u);
    EXPECT_EQ(circleNames.count(wy3d::Sketch3DParamNames::SKETCH_CIRCLE3D_PARAM_NORMAL_Y), 1u);
    EXPECT_EQ(circleNames.count(wy3d::Sketch3DParamNames::SKETCH_CIRCLE3D_PARAM_NORMAL_Z), 1u);
    EXPECT_EQ(circleNames.count(wy3d::Sketch3DParamNames::SKETCH_CIRCLE3D_PARAM_XDIR_X), 1u);
    EXPECT_EQ(circleNames.count(wy3d::Sketch3DParamNames::SKETCH_CIRCLE3D_PARAM_XDIR_Y), 1u);
    EXPECT_EQ(circleNames.count(wy3d::Sketch3DParamNames::SKETCH_CIRCLE3D_PARAM_XDIR_Z), 1u);
    EXPECT_EQ(circleNames.count(wy3d::Sketch3DParamNames::SKETCH_CIRCLE3D_PARAM_RADIUS), 1u);
    EXPECT_EQ(circleNames.count(wy3d::Sketch3DParamNames::SKETCH_CIRCLE3D_PARAM_DIAMETER), 1u);
    EXPECT_EQ(circleNames.count(wy3d::Sketch3DParamNames::SKETCH_CIRCLE3D_PARAM_PERIMETER), 1u);
    EXPECT_EQ(circleNames.count(wy3d::Sketch3DParamNames::SKETCH_CIRCLE3D_PARAM_AREA), 1u);
}
