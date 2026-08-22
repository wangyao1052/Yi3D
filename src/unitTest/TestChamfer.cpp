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
#include <wy3dChamfer.h>
#include <wy3dParamNames.h>
#include <wy3dParamEnumDef.h>
#include <wy3dMath.h>
#include <wydbParameter.h>

// --- helpers ---

// 创建基体 Box (第一步事务, 保证拓扑已生成) 并返回其 id
static wydb::ElementId createBaseBox(wy3d::Database* pDb)
{
    wydb::ElementId boxId = wydb::ElementId::kNull;
    {
        wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
        Box* pBox(nullptr);
        Box::create(pTrans, 30.0, 20.0, 10.0, pBox);
        pDb->getTransactionManager()->endTransaction();
        boxId = pBox->getId();
    }
    return boxId;
}

// --- EqualDistance (旧 5 参签名, 向后兼容) ---

TEST(Chamfer, EqualDistanceViaLegacyCreate)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId boxId = createBaseBox(pDb.get());

    Chamfer* pChamfer(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Solid* pSolid = wy3d::Solid::cast(pTrans->getElementForWrite(boxId));
        ASSERT_NE(pSolid, nullptr);
        std::vector<std::uint32_t> edgeIndices = { 0, 1, 2, 3 };
        wy::ErrorStatus error = Chamfer::create(pTrans, pSolid, {}, edgeIndices, 2.0, pChamfer);
        EXPECT_EQ(error, wy::ErrorStatus::Ok);
        pMgr->endTransaction();
    }
    ASSERT_NE(pChamfer, nullptr);
    EXPECT_EQ(pChamfer->getChamferType(), ChamferType::EqualDistance);
    EXPECT_EQ(pChamfer->getDistance1(), 2.0);
    EXPECT_EQ(pChamfer->getDistance2(), 2.0);
    EXPECT_EQ(pChamfer->getAngle(), wy3d::PI_4);
    EXPECT_FALSE(pChamfer->isFlipped());
    const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(boxId));
    ASSERT_NE(pSolid, nullptr);
    EXPECT_FALSE(pSolid->getShape().IsNull());
    EXPECT_EQ(pChamfer->getNewFaceIndices().size(), 4u);
}

// --- DistanceDistance (两个距离) ---

TEST(Chamfer, DistanceDistance)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId boxId = createBaseBox(pDb.get());

    Chamfer* pChamfer(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Solid* pSolid = wy3d::Solid::cast(pTrans->getElementForWrite(boxId));
        ASSERT_NE(pSolid, nullptr);
        std::vector<std::uint32_t> edgeIndices = { 0, 1, 2, 3 };
        wy::ErrorStatus error = Chamfer::create(pTrans, pSolid, {}, edgeIndices,
            ChamferType::DistanceDistance, 2.0, 4.0, wy3d::PI_4, false, pChamfer);
        EXPECT_EQ(error, wy::ErrorStatus::Ok);
        pMgr->endTransaction();
    }
    ASSERT_NE(pChamfer, nullptr);
    EXPECT_EQ(pChamfer->getChamferType(), ChamferType::DistanceDistance);
    EXPECT_EQ(pChamfer->getDistance1(), 2.0);
    EXPECT_EQ(pChamfer->getDistance2(), 4.0);
    EXPECT_EQ(pChamfer->getAngle(), wy3d::PI_4);
    EXPECT_FALSE(pChamfer->isFlipped());
    const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(boxId));
    ASSERT_NE(pSolid, nullptr);
    EXPECT_FALSE(pSolid->getShape().IsNull());
    EXPECT_EQ(pChamfer->getNewFaceIndices().size(), 4u);
}

// --- DistanceAngle (距离+角度) + 翻转 ---

TEST(Chamfer, DistanceAngleWithFlip)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId boxId = createBaseBox(pDb.get());

    Chamfer* pChamfer(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Solid* pSolid = wy3d::Solid::cast(pTrans->getElementForWrite(boxId));
        ASSERT_NE(pSolid, nullptr);
        std::vector<std::uint32_t> edgeIndices = { 0, 1, 2, 3 };
        wy::ErrorStatus error = Chamfer::create(pTrans, pSolid, {}, edgeIndices,
            ChamferType::DistanceAngle, 3.0, 1.0, wy3d::degreesToRadians(30.0), true, pChamfer);
        EXPECT_EQ(error, wy::ErrorStatus::Ok);
        pMgr->endTransaction();
    }
    ASSERT_NE(pChamfer, nullptr);
    EXPECT_EQ(pChamfer->getChamferType(), ChamferType::DistanceAngle);
    EXPECT_EQ(pChamfer->getDistance1(), 3.0);
    EXPECT_EQ(pChamfer->getAngle(), wy3d::degreesToRadians(30.0));
    EXPECT_TRUE(pChamfer->isFlipped());
    const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(boxId));
    ASSERT_NE(pSolid, nullptr);
    EXPECT_FALSE(pSolid->getShape().IsNull());
    EXPECT_EQ(pChamfer->getNewFaceIndices().size(), 4u);
}

// --- 非法角度 ---

TEST(Chamfer, InvalidAngleRejected)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId boxId = createBaseBox(pDb.get());

    Chamfer* pChamfer(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Solid* pSolid = wy3d::Solid::cast(pTrans->getElementForWrite(boxId));
        ASSERT_NE(pSolid, nullptr);
        std::vector<std::uint32_t> edgeIndices = { 0, 1, 2, 3 };
        // angle = 0.0 (0度)
        EXPECT_EQ(Chamfer::create(pTrans, pSolid, {}, edgeIndices,
            ChamferType::DistanceAngle, 2.0, 2.0, 0.0, false, pChamfer), wy::ErrorStatus::InvalidInput);
        EXPECT_EQ(pChamfer, nullptr);
        // angle = PI (180度)
        EXPECT_EQ(Chamfer::create(pTrans, pSolid, {}, edgeIndices,
            ChamferType::DistanceAngle, 2.0, 2.0, wy3d::PI, false, pChamfer), wy::ErrorStatus::InvalidInput);
        EXPECT_EQ(pChamfer, nullptr);
        pMgr->endTransaction();
    }
}

TEST(Chamfer, SetAngleInvalidRejected)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId boxId = createBaseBox(pDb.get());

    Chamfer* pChamfer(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Solid* pSolid = wy3d::Solid::cast(pTrans->getElementForWrite(boxId));
        ASSERT_NE(pSolid, nullptr);
        std::vector<std::uint32_t> edgeIndices = { 0, 1, 2, 3 };
        EXPECT_EQ(Chamfer::create(pTrans, pSolid, {}, edgeIndices,
            ChamferType::EqualDistance, 2.0, 2.0, wy3d::PI_4, false, pChamfer), wy::ErrorStatus::Ok);
        pMgr->endTransaction();
    }
    ASSERT_NE(pChamfer, nullptr);

    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        Chamfer* pWrite = Chamfer::cast(pTrans->getElementForWrite(pChamfer->getId()));
        ASSERT_NE(pWrite, nullptr);
        EXPECT_EQ(pWrite->setAngle(wy3d::PI), wy::ErrorStatus::InvalidInput);
        EXPECT_EQ(pWrite->setAngle(0.0), wy::ErrorStatus::InvalidInput);
        pMgr->endTransaction();
    }
    EXPECT_EQ(pChamfer->getAngle(), wy3d::PI_4);
}

// --- 非法倒角类型 ---

TEST(Chamfer, InvalidTypeRejected)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId boxId = createBaseBox(pDb.get());

    Chamfer* pChamfer(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Solid* pSolid = wy3d::Solid::cast(pTrans->getElementForWrite(boxId));
        ASSERT_NE(pSolid, nullptr);
        std::vector<std::uint32_t> edgeIndices = { 0, 1, 2, 3 };
        EXPECT_EQ(Chamfer::create(pTrans, pSolid, {}, edgeIndices,
            static_cast<ChamferType>(99), 2.0, 2.0, wy3d::PI_4, false, pChamfer), wy::ErrorStatus::InvalidInput);
        EXPECT_EQ(pChamfer, nullptr);
        EXPECT_EQ(Chamfer::create(pTrans, pSolid, {}, edgeIndices,
            ChamferType::EqualDistance, 2.0, 2.0, wy3d::PI_4, false, pChamfer), wy::ErrorStatus::Ok);
        pMgr->endTransaction();
    }
    ASSERT_NE(pChamfer, nullptr);

    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        Chamfer* pWrite = Chamfer::cast(pTrans->getElementForWrite(pChamfer->getId()));
        ASSERT_NE(pWrite, nullptr);
        EXPECT_EQ(pWrite->setChamferType(static_cast<ChamferType>(99)), wy::ErrorStatus::InvalidInput);
        pMgr->endTransaction();
    }
    EXPECT_EQ(pChamfer->getChamferType(), ChamferType::EqualDistance);
}

// --- 非法第二距离 (小于 kMinValue) ---

TEST(Chamfer, InvalidDistance2Rejected)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId boxId = createBaseBox(pDb.get());

    Chamfer* pChamfer(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Solid* pSolid = wy3d::Solid::cast(pTrans->getElementForWrite(boxId));
        ASSERT_NE(pSolid, nullptr);
        std::vector<std::uint32_t> edgeIndices = { 0, 1, 2, 3 };
        EXPECT_EQ(Chamfer::create(pTrans, pSolid, {}, edgeIndices,
            ChamferType::DistanceDistance, 2.0, 0.0, wy3d::PI_4, false, pChamfer), wy::ErrorStatus::InvalidInput);
        EXPECT_EQ(pChamfer, nullptr);
        pMgr->endTransaction();
    }
}

// --- 参数接口往返 (枚举 + 角度) ---

TEST(Chamfer, ParamRoundTrip)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    wydb::ElementId boxId = createBaseBox(pDb.get());

    Chamfer* pChamfer(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Solid* pSolid = wy3d::Solid::cast(pTrans->getElementForWrite(boxId));
        ASSERT_NE(pSolid, nullptr);
        std::vector<std::uint32_t> edgeIndices = { 0, 1, 2, 3 };
        EXPECT_EQ(Chamfer::create(pTrans, pSolid, {}, edgeIndices,
            ChamferType::EqualDistance, 2.0, 2.0, wy3d::PI_4, false, pChamfer), wy::ErrorStatus::Ok);
        pMgr->endTransaction();
    }
    ASSERT_NE(pChamfer, nullptr);

    const std::string className = wy3d::Chamfer::classInfo()->className();
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        Chamfer* pWrite = Chamfer::cast(pTrans->getElementForWrite(pChamfer->getId()));
        ASSERT_NE(pWrite, nullptr);
        // 枚举参数: Any<ParamEnumDef>
        wy3d::ParamEnumDef enumDef(
            {{static_cast<int>(ChamferType::EqualDistance), "Equal distance"},
             {static_cast<int>(ChamferType::DistanceDistance), "Distance-Distance"},
             {static_cast<int>(ChamferType::DistanceAngle), "Distance-Angle"}},
            static_cast<int>(ChamferType::DistanceDistance));
        EXPECT_EQ(pWrite->setParameterValue(className, wy3d::ParamNames::CHAMFER_TYPE,
            *wydb::ParameterValue::createAny(enumDef)), wy::ErrorStatus::Ok);
        // 角度参数: 按度传入, 内部存弧度
        EXPECT_EQ(pWrite->setParameterValue(className, wy3d::ParamNames::CHAMFER_ANGLE,
            *wydb::ParameterValue::createDouble(30.0)), wy::ErrorStatus::Ok);
        pMgr->endTransaction();
    }
    EXPECT_EQ(pChamfer->getChamferType(), ChamferType::DistanceDistance);
    EXPECT_EQ(pChamfer->getAngle(), wy3d::degreesToRadians(30.0));

    // 读取参数值: 面板按度返回
    wydb::ParameterValueUPtr pAngleVal = pChamfer->getParameterValue(className, wy3d::ParamNames::CHAMFER_ANGLE);
    ASSERT_NE(pAngleVal, nullptr);
    EXPECT_DOUBLE_EQ(pAngleVal->asDouble(), 30.0);

    wydb::ParameterValueUPtr pTypeVal = pChamfer->getParameterValue(className, wy3d::ParamNames::CHAMFER_TYPE);
    ASSERT_NE(pTypeVal, nullptr);
    ASSERT_TRUE(pTypeVal->isAny());
    const auto* pAnyVal = dynamic_cast<const wydb::AnyParameterValue*>(pTypeVal.get());
    ASSERT_NE(pAnyVal, nullptr);
    const wy3d::ParamEnumDef* pDef = pAnyVal->tryGet<wy3d::ParamEnumDef>();
    ASSERT_NE(pDef, nullptr);
    EXPECT_EQ(pDef->currentValue, static_cast<int>(ChamferType::DistanceDistance));
}
