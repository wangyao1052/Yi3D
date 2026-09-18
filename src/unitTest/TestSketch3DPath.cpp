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
#include <wy3dSketch3DProfile.h>
#include <wy3dSketch3DPath.h>
#include <wy3dSketchLine3D.h>
#include <wy3dSketchCircle3D.h>
#include <wy3dSketchArc3D.h>
#include <wy3dSketchSpline3D.h>
#include <wy3dErrorCode.h>
#include <wy3dMath.h>

#include <set>

namespace
{
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

    // A degenerate line (start == end) is accepted by the element itself
    static wydb::ElementId createDegenerateLineSketch3D(wy3d::Database* pDb, wydb::ElementId& lineId)
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
            wy3d::SketchLine3D* pLine(nullptr);
            EXPECT_EQ(wy3d::SketchLine3D::create(pTrans,
                wy::Vector3(1.0, 0.0, 0.0), wy::Vector3(1.0, 0.0, 0.0), pLine), wy::ErrorStatus::Ok);
            if (!pLine)
            {
                pDb->getTransactionManager()->abortTransaction();
                return wydb::ElementId::kNull;
            }
            lineId = pLine->getId();
            EXPECT_EQ(pSketch3D->addEntity(pLine), wy::ErrorStatus::Ok);
            EXPECT_EQ(pDb->getTransactionManager()->endTransaction(), wy::ErrorStatus::Ok);
            sketchId = pSketch3D->getId();
        }
        return sketchId;
    }

    static bool addLines3D(wy3d::Database* pDb, const wydb::ElementId& sketchId,
        const std::vector<std::pair<wy::Vector3, wy::Vector3>>& segments)
    {
        wydb::TransactionManager* pMgr = pDb->getTransactionManager();
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(sketchId));
        if (!pSketch3D)
        {
            pMgr->abortTransaction();
            return false;
        }
        for (const std::pair<wy::Vector3, wy::Vector3>& segment : segments)
        {
            wy3d::SketchLine3D* pLine(nullptr);
            if (wy::ErrorStatus::Ok != wy3d::SketchLine3D::create(pTrans, segment.first, segment.second, pLine) || !pLine)
            {
                pMgr->abortTransaction();
                return false;
            }
            if (wy::ErrorStatus::Ok != pSketch3D->addEntity(pLine))
            {
                pMgr->abortTransaction();
                return false;
            }
        }
        return wy::ErrorStatus::Ok == pMgr->endTransaction();
    }

    static bool addCircle3D(wy3d::Database* pDb, const wydb::ElementId& sketchId,
        const wy::Vector3& center, const wy::Vector3& normal, const wy::Vector3& xDir, double radius)
    {
        wydb::TransactionManager* pMgr = pDb->getTransactionManager();
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(sketchId));
        if (!pSketch3D)
        {
            pMgr->abortTransaction();
            return false;
        }
        wy3d::SketchCircle3D* pCircle(nullptr);
        if (wy::ErrorStatus::Ok != wy3d::SketchCircle3D::create(pTrans, center, normal, xDir, radius, pCircle) || !pCircle)
        {
            pMgr->abortTransaction();
            return false;
        }
        if (wy::ErrorStatus::Ok != pSketch3D->addEntity(pCircle))
        {
            pMgr->abortTransaction();
            return false;
        }
        return wy::ErrorStatus::Ok == pMgr->endTransaction();
    }

    static bool addArc3D(wy3d::Database* pDb, const wydb::ElementId& sketchId,
        const wy::Vector3& center, const wy::Vector3& normal, const wy::Vector3& xDir,
        double radius, double startAngle, double endAngle)
    {
        wydb::TransactionManager* pMgr = pDb->getTransactionManager();
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(sketchId));
        if (!pSketch3D)
        {
            pMgr->abortTransaction();
            return false;
        }
        wy3d::SketchArc3D* pArc(nullptr);
        if (wy::ErrorStatus::Ok != wy3d::SketchArc3D::create(pTrans, center, normal, xDir,
            radius, startAngle, endAngle, pArc) || !pArc)
        {
            pMgr->abortTransaction();
            return false;
        }
        if (wy::ErrorStatus::Ok != pSketch3D->addEntity(pArc))
        {
            pMgr->abortTransaction();
            return false;
        }
        return wy::ErrorStatus::Ok == pMgr->endTransaction();
    }

    static bool addSpline3D(wy3d::Database* pDb, const wydb::ElementId& sketchId,
        const std::vector<wy::Vector3>& fitPoints)
    {
        wydb::TransactionManager* pMgr = pDb->getTransactionManager();
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(sketchId));
        if (!pSketch3D)
        {
            pMgr->abortTransaction();
            return false;
        }
        wy3d::SketchSpline3D* pSpline(nullptr);
        if (wy::ErrorStatus::Ok != wy3d::SketchSpline3D::create(pTrans, fitPoints, pSpline) || !pSpline)
        {
            pMgr->abortTransaction();
            return false;
        }
        if (wy::ErrorStatus::Ok != pSketch3D->addEntity(pSpline))
        {
            pMgr->abortTransaction();
            return false;
        }
        return wy::ErrorStatus::Ok == pMgr->endTransaction();
    }

    static bool erase3DEntity(wy3d::Database* pDb, const wydb::ElementId& entityId)
    {
        wydb::TransactionManager* pMgr = pDb->getTransactionManager();
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wydb::Element* pElem = pTrans->getElementForWrite(entityId);
        if (!pElem || wy::ErrorStatus::Ok != pElem->erase(true))
        {
            pMgr->abortTransaction();
            return false;
        }
        return wy::ErrorStatus::Ok == pMgr->endTransaction();
    }

    static const wy3d::Sketch3D* getSketch3D(wy3d::Database* pDb, const wydb::ElementId& sketchId)
    {
        return wy3d::Sketch3D::cast(pDb->getElement(sketchId));
    }

    static std::vector<wydb::ElementId> getCurveIds(const wy3d::Sketch3D* pSketch3D)
    {
        std::vector<wydb::ElementId> ids;
        for (wy::Iterator<wydb::ElementId> iter = pSketch3D->createIterator(); !iter.isDone(); iter.moveNext())
        {
            ids.emplace_back(iter.current());
        }
        return ids;
    }

    static std::set<wydb::ElementId> errorIds(const std::shared_ptr<wy3d::SketchError>& pError)
    {
        if (!pError) return std::set<wydb::ElementId>();
        return std::set<wydb::ElementId>(pError->ids.cbegin(), pError->ids.cend());
    }

    static std::vector<wydb::ElementId> pathIds(const wy3d::Sketch3DPath& path)
    {
        std::vector<wydb::ElementId> ids;
        for (const wy3d::BiCurve3D& biCurve : path.getPath())
        {
            ids.emplace_back(biCurve.curve->getId());
        }
        return ids;
    }

    static std::vector<bool> pathOrients(const wy3d::Sketch3DPath& path)
    {
        std::vector<bool> orients;
        for (const wy3d::BiCurve3D& biCurve : path.getPath())
        {
            orients.emplace_back(biCurve.orient);
        }
        return orients;
    }
}

// A -> B -> C -> D, curves created in path order: the forward search finds the
// whole chain in one go
TEST(Sketch3DPath, OpenChainOfThreeLines)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createEmptySketch3D(pDb.get());
    ASSERT_FALSE(sketchId.isNull());

    const wy::Vector3 a(0.0, 0.0, 0.0);
    const wy::Vector3 b(100.0, 0.0, 0.0);
    const wy::Vector3 c(100.0, 100.0, 0.0);
    const wy::Vector3 d(0.0, 100.0, 20.0);
    ASSERT_TRUE(addLines3D(pDb.get(), sketchId, { { a, b }, { b, c }, { c, d } }));

    const wy3d::Sketch3D* pSketch3D = getSketch3D(pDb.get(), sketchId);
    ASSERT_NE(pSketch3D, nullptr);
    const std::vector<wydb::ElementId> curveIds = getCurveIds(pSketch3D);
    ASSERT_EQ(curveIds.size(), 3u);

    wy3d::Sketch3DPath path(pSketch3D);
    EXPECT_TRUE(path.check());
    EXPECT_EQ(pathIds(path), curveIds);
    EXPECT_EQ(pathOrients(path), std::vector<bool>({ true, true, true }));
}

// Curve 0 is created in the middle of the A->B->C->D chain, so the forward search
// stops early and the path has to be assembled from the reverse search prefix plus
// the forward one
TEST(Sketch3DPath, Curve0InTheMiddleOfTheChain)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createEmptySketch3D(pDb.get());
    ASSERT_FALSE(sketchId.isNull());

    const wy::Vector3 a(0.0, 0.0, 0.0);
    const wy::Vector3 b(100.0, 0.0, 0.0);
    const wy::Vector3 c(100.0, 100.0, 0.0);
    const wy::Vector3 d(0.0, 100.0, 0.0);
    // curve 0 = B->C (the middle segment), curve 1 = C->D, curve 2 = A->B
    ASSERT_TRUE(addLines3D(pDb.get(), sketchId, { { b, c }, { c, d }, { a, b } }));

    const wy3d::Sketch3D* pSketch3D = getSketch3D(pDb.get(), sketchId);
    ASSERT_NE(pSketch3D, nullptr);
    const std::vector<wydb::ElementId> curveIds = getCurveIds(pSketch3D);
    ASSERT_EQ(curveIds.size(), 3u);

    wy3d::Sketch3DPath path(pSketch3D);
    EXPECT_TRUE(path.check());
    // Walking from A: curve 2 (forward), then curve 0, then curve 1
    EXPECT_EQ(pathIds(path), std::vector<wydb::ElementId>({ curveIds[2], curveIds[0], curveIds[1] }));
    EXPECT_EQ(pathOrients(path), std::vector<bool>({ true, true, true }));
}

// Each curve is stored against the direction the path runs, yet the path comes out
// of the walk with every curve in its own stored direction and simply starts at the
// far end: consistency is required at each vertex, not across the whole chain
TEST(Sketch3DPath, ReversedChainWalksForward)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createEmptySketch3D(pDb.get());
    ASSERT_FALSE(sketchId.isNull());

    const wy::Vector3 a(0.0, 0.0, 0.0);
    const wy::Vector3 b(100.0, 0.0, 0.0);
    const wy::Vector3 c(100.0, 100.0, 0.0);
    const wy::Vector3 d(0.0, 100.0, 0.0);
    // curve 0 = D->C, curve 1 = C->B, curve 2 = B->A: the path runs D -> C -> B -> A
    ASSERT_TRUE(addLines3D(pDb.get(), sketchId, { { d, c }, { c, b }, { b, a } }));

    const wy3d::Sketch3D* pSketch3D = getSketch3D(pDb.get(), sketchId);
    ASSERT_NE(pSketch3D, nullptr);
    const std::vector<wydb::ElementId> curveIds = getCurveIds(pSketch3D);
    ASSERT_EQ(curveIds.size(), 3u);

    wy3d::Sketch3DPath path(pSketch3D);
    EXPECT_TRUE(path.check());
    EXPECT_EQ(pathIds(path), curveIds);
    EXPECT_EQ(pathOrients(path), std::vector<bool>({ true, true, true }));
}

// Two segments meeting tail-to-tail at their shared endpoint. Same story as the
// head-to-head case below: the rule is about stored direction, not about geometry
TEST(Sketch3DPath, TailToTailJunctionRejected)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createEmptySketch3D(pDb.get());
    ASSERT_FALSE(sketchId.isNull());

    const wy::Vector3 a(0.0, 0.0, 0.0);
    const wy::Vector3 b(100.0, 0.0, 0.0);
    const wy::Vector3 c(100.0, 100.0, 0.0);
    // curve 0 = A->B, curve 1 = C->B: both end at B
    ASSERT_TRUE(addLines3D(pDb.get(), sketchId, { { a, b }, { c, b } }));

    const wy3d::Sketch3D* pSketch3D = getSketch3D(pDb.get(), sketchId);
    ASSERT_NE(pSketch3D, nullptr);

    wy3d::Sketch3DPath path(pSketch3D);
    EXPECT_FALSE(path.check());
    std::shared_ptr<wy3d::SketchError> pError = path.getError();
    ASSERT_NE(pError, nullptr);
    EXPECT_EQ(pError->type, wy3d::ErrorCode::SKETCH_MoreThanTwoCurvesAtOneEndPoint);
}

// Two segments meeting head-to-head at their shared endpoint. The geometry is a
// valid polyline but the 2D rule is direction sensitive ("at most one curve may
// start at a point and at most one may end there"), and the 3D path mirrors it
TEST(Sketch3DPath, HeadToHeadJunctionRejected)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createEmptySketch3D(pDb.get());
    ASSERT_FALSE(sketchId.isNull());

    const wy::Vector3 a(0.0, 0.0, 0.0);
    const wy::Vector3 b(100.0, 0.0, 0.0);
    const wy::Vector3 c(100.0, 100.0, 0.0);
    // curve 0 = B->A, curve 1 = B->C: both start at B
    ASSERT_TRUE(addLines3D(pDb.get(), sketchId, { { b, a }, { b, c } }));

    const wy3d::Sketch3D* pSketch3D = getSketch3D(pDb.get(), sketchId);
    ASSERT_NE(pSketch3D, nullptr);

    wy3d::Sketch3DPath path(pSketch3D);
    EXPECT_FALSE(path.check());
    std::shared_ptr<wy3d::SketchError> pError = path.getError();
    ASSERT_NE(pError, nullptr);
    EXPECT_EQ(pError->type, wy3d::ErrorCode::SKETCH_MoreThanTwoCurvesAtOneEndPoint);
}

TEST(Sketch3DPath, SingleCircle)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createEmptySketch3D(pDb.get());
    ASSERT_FALSE(sketchId.isNull());
    ASSERT_TRUE(addCircle3D(pDb.get(), sketchId, wy::Vector3::kZero, wy::Vector3::kZAxis,
        wy::Vector3::kXAxis, 25.0));

    const wy3d::Sketch3D* pSketch3D = getSketch3D(pDb.get(), sketchId);
    ASSERT_NE(pSketch3D, nullptr);

    wy3d::Sketch3DPath path(pSketch3D);
    EXPECT_TRUE(path.check());
    ASSERT_EQ(path.getPath().size(), 1u);
    EXPECT_TRUE(path.getPath().front().orient);
}

// A closed chain of lines is accepted (the 2D SketchPath does the same)
TEST(Sketch3DPath, ClosedTriangleOfLines)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createEmptySketch3D(pDb.get());
    ASSERT_FALSE(sketchId.isNull());

    const wy::Vector3 a(0.0, 0.0, 0.0);
    const wy::Vector3 b(100.0, 0.0, 0.0);
    const wy::Vector3 c(50.0, 80.0, 0.0);
    ASSERT_TRUE(addLines3D(pDb.get(), sketchId, { { a, b }, { b, c }, { c, a } }));

    const wy3d::Sketch3D* pSketch3D = getSketch3D(pDb.get(), sketchId);
    ASSERT_NE(pSketch3D, nullptr);

    wy3d::Sketch3DPath path(pSketch3D);
    EXPECT_TRUE(path.check());
    EXPECT_EQ(path.getPath().size(), 3u);
    EXPECT_EQ(pathOrients(path), std::vector<bool>({ true, true, true }));
}

TEST(Sketch3DPath, ArcSplineLineChain)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createEmptySketch3D(pDb.get());
    ASSERT_FALSE(sketchId.isNull());

    const wy::Vector3 a(0.0, 0.0, 0.0);
    const wy::Vector3 b(10.0, 0.0, 0.0);
    const wy::Vector3 c(20.0, 10.0, 0.0);
    const wy::Vector3 d(30.0, 10.0, 0.0);
    // line A->B, arc B->C (centre (10,10,0), radius 10, 270deg -> 360deg), spline C->D
    ASSERT_TRUE(addLines3D(pDb.get(), sketchId, { { a, b } }));
    ASSERT_TRUE(addArc3D(pDb.get(), sketchId, wy::Vector3(10.0, 10.0, 0.0), wy::Vector3::kZAxis,
        wy::Vector3::kXAxis, 10.0, 1.5 * wy3d::PI, wy3d::TWO_PI));
    ASSERT_TRUE(addSpline3D(pDb.get(), sketchId, { c, wy::Vector3(25.0, 20.0, 5.0), d }));

    const wy3d::Sketch3D* pSketch3D = getSketch3D(pDb.get(), sketchId);
    ASSERT_NE(pSketch3D, nullptr);
    const std::vector<wydb::ElementId> curveIds = getCurveIds(pSketch3D);
    ASSERT_EQ(curveIds.size(), 3u);

    wy3d::Sketch3DPath path(pSketch3D);
    EXPECT_TRUE(path.check());
    EXPECT_EQ(pathIds(path), curveIds);
    EXPECT_EQ(pathOrients(path), std::vector<bool>({ true, true, true }));
}

TEST(Sketch3DPath, TwoCirclesRejected)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createEmptySketch3D(pDb.get());
    ASSERT_FALSE(sketchId.isNull());
    ASSERT_TRUE(addCircle3D(pDb.get(), sketchId, wy::Vector3::kZero, wy::Vector3::kZAxis,
        wy::Vector3::kXAxis, 25.0));
    ASSERT_TRUE(addCircle3D(pDb.get(), sketchId, wy::Vector3(100.0, 0.0, 0.0), wy::Vector3::kZAxis,
        wy::Vector3::kXAxis, 25.0));

    const wy3d::Sketch3D* pSketch3D = getSketch3D(pDb.get(), sketchId);
    ASSERT_NE(pSketch3D, nullptr);

    wy3d::Sketch3DPath path(pSketch3D);
    EXPECT_FALSE(path.check());
    std::shared_ptr<wy3d::SketchError> pError = path.getError();
    ASSERT_NE(pError, nullptr);
    EXPECT_EQ(pError->type, wy3d::ErrorCode::PATH_MoreThanOneLoopIsNotAllowed);
    EXPECT_TRUE(pError->ids.empty());
}

TEST(Sketch3DPath, ChainPlusDisjointLoopRejected)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createEmptySketch3D(pDb.get());
    ASSERT_FALSE(sketchId.isNull());

    const wy::Vector3 a(0.0, 0.0, 0.0);
    const wy::Vector3 b(100.0, 0.0, 0.0);
    const wy::Vector3 c(100.0, 100.0, 0.0);
    ASSERT_TRUE(addLines3D(pDb.get(), sketchId, { { a, b }, { b, c } }));
    ASSERT_TRUE(addCircle3D(pDb.get(), sketchId, wy::Vector3(500.0, 0.0, 0.0), wy::Vector3::kZAxis,
        wy::Vector3::kXAxis, 25.0));

    const wy3d::Sketch3D* pSketch3D = getSketch3D(pDb.get(), sketchId);
    ASSERT_NE(pSketch3D, nullptr);

    wy3d::Sketch3DPath path(pSketch3D);
    EXPECT_FALSE(path.check());
    std::shared_ptr<wy3d::SketchError> pError = path.getError();
    ASSERT_NE(pError, nullptr);
    EXPECT_EQ(pError->type, wy3d::ErrorCode::PATH_MoreThanOneLoopIsNotAllowed);
}

TEST(Sketch3DPath, TwoDisjointChainsRejected)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createEmptySketch3D(pDb.get());
    ASSERT_FALSE(sketchId.isNull());

    const wy::Vector3 a(0.0, 0.0, 0.0);
    const wy::Vector3 b(100.0, 0.0, 0.0);
    const wy::Vector3 c(100.0, 100.0, 0.0);
    const wy::Vector3 d(0.0, 100.0, 0.0);
    const wy::Vector3 e(0.0, 200.0, 0.0);
    const wy::Vector3 f(100.0, 200.0, 0.0);
    ASSERT_TRUE(addLines3D(pDb.get(), sketchId, { { a, b }, { b, c }, { d, e }, { e, f } }));

    const wy3d::Sketch3D* pSketch3D = getSketch3D(pDb.get(), sketchId);
    ASSERT_NE(pSketch3D, nullptr);

    wy3d::Sketch3DPath path(pSketch3D);
    EXPECT_FALSE(path.check());
    std::shared_ptr<wy3d::SketchError> pError = path.getError();
    ASSERT_NE(pError, nullptr);
    EXPECT_EQ(pError->type, wy3d::ErrorCode::PATH_MoreThanOneLoopIsNotAllowed);
}

// Three lines sharing their start point (the 2D rule is "at most one curve may
// start at a point and at most one may end at it")
TEST(Sketch3DPath, BranchRejected)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createEmptySketch3D(pDb.get());
    ASSERT_FALSE(sketchId.isNull());

    const wy::Vector3 startPnt(0.0, 0.0, 0.0);
    ASSERT_TRUE(addLines3D(pDb.get(), sketchId, {
        { startPnt, wy::Vector3(100.0, 0.0, 0.0) },
        { startPnt, wy::Vector3(0.0, 100.0, 0.0) },
        { startPnt, wy::Vector3(0.0, 0.0, 100.0) } }));

    const wy3d::Sketch3D* pSketch3D = getSketch3D(pDb.get(), sketchId);
    ASSERT_NE(pSketch3D, nullptr);
    const std::vector<wydb::ElementId> curveIds = getCurveIds(pSketch3D);
    ASSERT_EQ(curveIds.size(), 3u);

    wy3d::Sketch3DPath path(pSketch3D);
    EXPECT_FALSE(path.check());
    std::shared_ptr<wy3d::SketchError> pError = path.getError();
    ASSERT_NE(pError, nullptr);
    EXPECT_EQ(pError->type, wy3d::ErrorCode::SKETCH_MoreThanTwoCurvesAtOneEndPoint);
    EXPECT_EQ(errorIds(pError), std::set<wydb::ElementId>(curveIds.cbegin(), curveIds.cend()));
}

// Two coincident lines drawn in the same direction. The profile accepts this
// (every vertex has degree 2) but the path must not: both of them start at the
// same point, which the 2D rule rejects
TEST(Sketch3DPath, SameDirectionDuplicateLinesRejected)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createEmptySketch3D(pDb.get());
    ASSERT_FALSE(sketchId.isNull());

    const wy::Vector3 startPnt(0.0, 0.0, 0.0);
    const wy::Vector3 endPnt(100.0, 0.0, 0.0);
    ASSERT_TRUE(addLines3D(pDb.get(), sketchId, { { startPnt, endPnt }, { startPnt, endPnt } }));

    const wy3d::Sketch3D* pSketch3D = getSketch3D(pDb.get(), sketchId);
    ASSERT_NE(pSketch3D, nullptr);

    // The profile's "degree must be 2" rule accepts it ...
    wy3d::Sketch3DProfile profile(pSketch3D);
    EXPECT_TRUE(profile.check());

    // ... while the path's rule does not
    wy3d::Sketch3DPath path(pSketch3D);
    EXPECT_FALSE(path.check());
    std::shared_ptr<wy3d::SketchError> pError = path.getError();
    ASSERT_NE(pError, nullptr);
    EXPECT_EQ(pError->type, wy3d::ErrorCode::SKETCH_MoreThanTwoCurvesAtOneEndPoint);
}

// Two arcs P->Q and Q->P: at most one curve starts and one ends at each vertex,
// so the pair is walked as a degenerate closed loop (the 2D path behaves the
// same); only the reversed pair is rejected
TEST(Sketch3DPath, OppositeDirectionDuplicateLinesAccepted)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createEmptySketch3D(pDb.get());
    ASSERT_FALSE(sketchId.isNull());

    const wy::Vector3 startPnt(0.0, 0.0, 0.0);
    const wy::Vector3 endPnt(100.0, 0.0, 0.0);
    ASSERT_TRUE(addLines3D(pDb.get(), sketchId, { { startPnt, endPnt }, { endPnt, startPnt } }));

    const wy3d::Sketch3D* pSketch3D = getSketch3D(pDb.get(), sketchId);
    ASSERT_NE(pSketch3D, nullptr);

    wy3d::Sketch3DPath path(pSketch3D);
    EXPECT_TRUE(path.check());
    EXPECT_EQ(path.getPath().size(), 2u);
}

TEST(Sketch3DPath, EmptySketchReportsNoCurves)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createEmptySketch3D(pDb.get());
    ASSERT_FALSE(sketchId.isNull());

    const wy3d::Sketch3D* pSketch3D = getSketch3D(pDb.get(), sketchId);
    ASSERT_NE(pSketch3D, nullptr);

    wy3d::Sketch3DPath path(pSketch3D);
    EXPECT_FALSE(path.check());
    std::shared_ptr<wy3d::SketchError> pError = path.getError();
    ASSERT_NE(pError, nullptr);
    EXPECT_EQ(pError->type, wy3d::ErrorCode::PATH_NoCurves);
}

TEST(Sketch3DPath, DegenerateLineRejected)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId lineId = wydb::ElementId::kNull;
    wydb::ElementId sketchId = createDegenerateLineSketch3D(pDb.get(), lineId);
    ASSERT_FALSE(sketchId.isNull());
    ASSERT_FALSE(lineId.isNull());

    const wy3d::Sketch3D* pSketch3D = getSketch3D(pDb.get(), sketchId);
    ASSERT_NE(pSketch3D, nullptr);

    wy3d::Sketch3DPath path(pSketch3D);
    EXPECT_FALSE(path.check());
    std::shared_ptr<wy3d::SketchError> pError = path.getError();
    ASSERT_NE(pError, nullptr);
    EXPECT_EQ(pError->type, wy3d::ErrorCode::PATH_InvalidPath);
    EXPECT_EQ(errorIds(pError), std::set<wydb::ElementId>({ lineId }));
}

// A square whose two meeting endpoints straddle a fusion bin centre: the very
// same tolerance wart the 3D profile has, documented here so that a change in
// the shared quantization shows up in both suites
TEST(Sketch3DPath, NearCoincidentGapAccepted)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createEmptySketch3D(pDb.get());
    ASSERT_FALSE(sketchId.isNull());

    // A square whose two meeting endpoints straddle a fusion bin centre (-4e-6
    // and +4e-6 both quantize to bin 0 at tol 1e-5)
    ASSERT_TRUE(addLines3D(pDb.get(), sketchId, {
        { wy::Vector3(0.0, -4e-6, 0.0), wy::Vector3(10.0, 0.0, 0.0) },
        { wy::Vector3(10.0, 0.0, 0.0), wy::Vector3(10.0, 10.0, 0.0) },
        { wy::Vector3(10.0, 10.0, 0.0), wy::Vector3(0.0, 10.0, 0.0) },
        { wy::Vector3(0.0, 10.0, 0.0), wy::Vector3(0.0, 4e-6, 0.0) } }));

    const wy3d::Sketch3D* pSketch3D = getSketch3D(pDb.get(), sketchId);
    ASSERT_NE(pSketch3D, nullptr);

    wy3d::Sketch3DPath path(pSketch3D);
    EXPECT_TRUE(path.check());
}

// The path holds non-owning curve pointers: a rerun must not hand back the ones
// collected before the sketch was edited
TEST(Sketch3DPath, ReusedAfterSketchEdit)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createEmptySketch3D(pDb.get());
    ASSERT_FALSE(sketchId.isNull());

    const wy::Vector3 a(0.0, 0.0, 0.0);
    const wy::Vector3 b(100.0, 0.0, 0.0);
    const wy::Vector3 c(100.0, 100.0, 0.0);
    const wy::Vector3 d(0.0, 100.0, 0.0);
    ASSERT_TRUE(addLines3D(pDb.get(), sketchId, { { a, b }, { b, c }, { c, d } }));

    const wy3d::Sketch3D* pSketch3D = getSketch3D(pDb.get(), sketchId);
    ASSERT_NE(pSketch3D, nullptr);
    const std::vector<wydb::ElementId> curveIds = getCurveIds(pSketch3D);
    ASSERT_EQ(curveIds.size(), 3u);

    wy3d::Sketch3DPath path(pSketch3D);
    EXPECT_TRUE(path.check());
    ASSERT_EQ(path.getPath().size(), 3u);

    // Dropping the last segment turns the chain into a shorter and still valid
    // one; the stale result must not survive
    ASSERT_TRUE(erase3DEntity(pDb.get(), curveIds[2]));
    const wy3d::Sketch3D* pEditedSketch3D = getSketch3D(pDb.get(), sketchId);
    ASSERT_NE(pEditedSketch3D, nullptr);

    EXPECT_TRUE(path.check());
    EXPECT_EQ(path.getPath().size(), 2u);
}

// Two arcs sharing both endpoints with the same stored direction: a "lens". The
// profile rule only asks for degree 2 at every vertex, so it is a legal closed
// loop there, while the path rule rejects it because both arcs start at A and
// both end at B. The two rules are deliberately different
TEST(Sketch3DPath, LensIsLegalForProfileButNotForPath)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId sketchId = createEmptySketch3D(pDb.get());
    ASSERT_FALSE(sketchId.isNull());

    // Both run from (10,0,0) to (-10,0,0), bulging to +Y and -Y respectively
    ASSERT_TRUE(addArc3D(pDb.get(), sketchId, wy::Vector3::kZero, wy::Vector3::kZAxis,
        wy::Vector3::kXAxis, 10.0, 0.0, wy3d::PI));
    ASSERT_TRUE(addArc3D(pDb.get(), sketchId, wy::Vector3::kZero, wy::Vector3(0.0, 0.0, -1.0),
        wy::Vector3::kXAxis, 10.0, 0.0, wy3d::PI));

    const wy3d::Sketch3D* pSketch3D = getSketch3D(pDb.get(), sketchId);
    ASSERT_NE(pSketch3D, nullptr);
    const std::vector<wydb::ElementId> curveIds = getCurveIds(pSketch3D);
    ASSERT_EQ(curveIds.size(), 2u);

    wy3d::Sketch3DProfile profile(pSketch3D);
    EXPECT_TRUE(profile.check());

    wy3d::Sketch3DPath path(pSketch3D);
    EXPECT_FALSE(path.check());
    std::shared_ptr<wy3d::SketchError> pError = path.getError();
    ASSERT_NE(pError, nullptr);
    EXPECT_EQ(pError->type, wy3d::ErrorCode::SKETCH_MoreThanTwoCurvesAtOneEndPoint);
    EXPECT_EQ(errorIds(pError), std::set<wydb::ElementId>({ curveIds[0], curveIds[1] }));
}
