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

#include <array>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include <wy3dPlane.h>
#include <wy3dSketch.h>
#include <wy3dSketchLine.h>
#include <wyVector3.h>

#include "utils/SketchRectangleDragMath.h"
#include "utils/SketchRectangleRecognizer.h"

namespace
{
    using wy::Vector2;

    struct SketchFixture
    {
        std::unique_ptr<wy3d::Database> pDb;
        wy3d::Sketch* pSketch = nullptr;
        std::vector<wy3d::SketchLine*> lines;
    };

    std::unique_ptr<SketchFixture> makeRectangleFixture(
        const std::vector<std::pair<Vector2, Vector2>>& segments)
    {
        auto pFixture = std::make_unique<SketchFixture>();
        pFixture->pDb = std::make_unique<wy3d::Database>();

        wydb::TransactionManager* pMgr = pFixture->pDb->getTransactionManager();
        wydb::Transaction* pTrans = pMgr->startTransaction();
        if (!pTrans) return nullptr;

        const wy3d::Plane plane(wy::Vector3(0.0, 0.0, 0.0),
            wy::Vector3(0.0, 0.0, 1.0), wy::Vector3(1.0, 0.0, 0.0));
        wy3d::Sketch* pSketch = nullptr;
        if (wy3d::Sketch::create(pTrans, plane, pSketch) != wy::ErrorStatus::Ok)
        {
            pMgr->abortTransaction();
            return nullptr;
        }

        for (const auto& segment : segments)
        {
            wy3d::SketchLine* pLine = nullptr;
            if (wy3d::SketchLine::create(pTrans, segment.first, segment.second, pLine)
                != wy::ErrorStatus::Ok)
            {
                pMgr->abortTransaction();
                return nullptr;
            }
            if (pSketch->addEntity(pLine) != wy::ErrorStatus::Ok)
            {
                pMgr->abortTransaction();
                return nullptr;
            }
            pFixture->lines.push_back(pLine);
        }

        if (pMgr->endTransaction() != wy::ErrorStatus::Ok) return nullptr;
        pFixture->pSketch = pSketch;
        return pFixture;
    }

    std::vector<const wy3d::SketchLine*> toConstLines(const SketchFixture& fixture)
    {
        std::vector<const wy3d::SketchLine*> lines;
        lines.reserve(fixture.lines.size());
        for (const wy3d::SketchLine* pLine : fixture.lines) lines.push_back(pLine);
        return lines;
    }

    Vector2 rotate(const Vector2& point, double angle)
    {
        const double c = std::cos(angle);
        const double s = std::sin(angle);
        return Vector2(c * point.x() - s * point.y(), s * point.x() + c * point.y());
    }

    const wy3d::SketchLine* findLine(const SketchFixture& fixture, wydb::ElementId id)
    {
        for (const wy3d::SketchLine* pLine : fixture.lines)
        {
            if (pLine->getId() == id) return pLine;
        }
        return nullptr;
    }
}

TEST(SketchRectangleRecognizer, DetectAxisAlignedUnordered)
{
    // 逆时针矩形 (0,0)-(10,0)-(10,5)-(0,5)，线方向故意混排
    const Vector2 c0(0.0, 0.0);
    const Vector2 c1(10.0, 0.0);
    const Vector2 c2(10.0, 5.0);
    const Vector2 c3(0.0, 5.0);
    auto pFixture = makeRectangleFixture({
        { c1, c0 }, { c2, c1 }, { c3, c2 }, { c0, c3 } });
    ASSERT_NE(pFixture, nullptr);

    const auto pInfo = wy3dApp::tryDetectSketchRectangle(toConstLines(*pFixture));
    ASSERT_NE(pInfo, nullptr);
    const wy3dApp::SketchRectangleInfo& info = *pInfo;

    EXPECT_NEAR(info.halfWidth, 5.0, 1e-9);
    EXPECT_NEAR(info.halfHeight, 2.5, 1e-9);
    EXPECT_NEAR(info.center.x(), 5.0, 1e-9);
    EXPECT_NEAR(info.center.y(), 2.5, 1e-9);
    EXPECT_NEAR(info.u.length(), 1.0, 1e-12);
    EXPECT_NEAR(info.v.length(), 1.0, 1e-12);
    EXPECT_NEAR(info.u.dot(info.v), 0.0, 1e-12);

    // 环序自洽：lineIds[k] 的两个端点分别对应 corners[k] / corners[k+1]
    for (int k = 0; k < 4; ++k)
    {
        const wy3d::SketchLine* pLine = findLine(*pFixture, info.lineIds[k]);
        ASSERT_NE(pLine, nullptr);
        const Vector2 start = pLine->getStartPoint();
        const Vector2 end = pLine->getEndPoint();
        const Vector2 corner = info.corners[k];
        const Vector2 nextCorner = info.corners[(k + 1) % 4];
        const bool direct = (start - corner).length() < 1e-9 && (end - nextCorner).length() < 1e-9;
        const bool reversed = (start - nextCorner).length() < 1e-9 && (end - corner).length() < 1e-9;
        EXPECT_TRUE(direct || reversed);
    }
}

TEST(SketchRectangleRecognizer, DetectRotatedRectangle)
{
    const double angle = 30.0 * 3.14159265358979323846 / 180.0;
    const Vector2 center(20.0, -7.0);
    const double halfWidth = 6.0;
    const double halfHeight = 2.0;

    const Vector2 u(std::cos(angle), std::sin(angle));
    const Vector2 v(-u.y(), u.x());
    const Vector2 c0 = center - halfWidth * u - halfHeight * v;
    const Vector2 c1 = center + halfWidth * u - halfHeight * v;
    const Vector2 c2 = center + halfWidth * u + halfHeight * v;
    const Vector2 c3 = center - halfWidth * u + halfHeight * v;

    auto pFixture = makeRectangleFixture({
        { c2, c1 }, { c0, c1 }, { c3, c2 }, { c0, c3 } });
    ASSERT_NE(pFixture, nullptr);

    const auto pInfo = wy3dApp::tryDetectSketchRectangle(toConstLines(*pFixture));
    ASSERT_NE(pInfo, nullptr);
    const wy3dApp::SketchRectangleInfo& info = *pInfo;

    // u/v 的命名取决于环起点所在边，宽高可能互换；校验尺寸集合与参数化自洽
    EXPECT_NEAR(std::max(info.halfWidth, info.halfHeight), 6.0, 1e-9);
    EXPECT_NEAR(std::min(info.halfWidth, info.halfHeight), 2.0, 1e-9);
    EXPECT_NEAR(info.center.x(), center.x(), 1e-9);
    EXPECT_NEAR(info.center.y(), center.y(), 1e-9);
    EXPECT_NEAR(info.u.length(), 1.0, 1e-12);
    EXPECT_NEAR(info.v.length(), 1.0, 1e-12);
    EXPECT_NEAR(info.u.dot(info.v), 0.0, 1e-12);

    // u 必须平行于矩形的某一组边
    const double alignU = std::fabs(info.u.dot(u));
    const double alignV = std::fabs(info.u.dot(v));
    EXPECT_NEAR(std::max(alignU, alignV), 1.0, 1e-12);

    // 规范参数化能重建 4 个角点
    const std::array<Vector2, 4> expectedCorners = { c0, c1, c2, c3 };
    const std::array<Vector2, 4> reconstructed = {
        info.center - info.halfWidth * info.u - info.halfHeight * info.v,
        info.center + info.halfWidth * info.u - info.halfHeight * info.v,
        info.center + info.halfWidth * info.u + info.halfHeight * info.v,
        info.center - info.halfWidth * info.u + info.halfHeight * info.v };
    for (const Vector2& corner : reconstructed)
    {
        bool found = false;
        for (const Vector2& expected : expectedCorners)
        {
            if ((corner - expected).length() < 1e-9) found = true;
        }
        EXPECT_TRUE(found);
    }
}

TEST(SketchRectangleRecognizer, RejectNonRectangles)
{
    const Vector2 c0(0.0, 0.0);
    const Vector2 c1(10.0, 0.0);
    const Vector2 c2(10.0, 5.0);
    const Vector2 c3(0.0, 5.0);

    // 梯形
    {
        const Vector2 t3(2.0, 5.0);
        auto pFixture = makeRectangleFixture({
            { c0, c1 }, { c1, c2 }, { c2, t3 }, { t3, c0 } });
        ASSERT_NE(pFixture, nullptr);
        EXPECT_EQ(wy3dApp::tryDetectSketchRectangle(toConstLines(*pFixture)), nullptr);
    }

    // 平行四边形（非直角）
    {
        const Vector2 p2(12.0, 5.0);
        const Vector2 p3(2.0, 5.0);
        auto pFixture = makeRectangleFixture({
            { c0, c1 }, { c1, p2 }, { p2, p3 }, { p3, c0 } });
        ASSERT_NE(pFixture, nullptr);
        EXPECT_EQ(wy3dApp::tryDetectSketchRectangle(toConstLines(*pFixture)), nullptr);
    }

    // 未闭合
    {
        auto pFixture = makeRectangleFixture({
            { c0, c1 }, { c1, c2 }, { c2, c3 }, { c3, Vector2(0.5, 0.5) } });
        ASSERT_NE(pFixture, nullptr);
        EXPECT_EQ(wy3dApp::tryDetectSketchRectangle(toConstLines(*pFixture)), nullptr);
    }

    // 端点分叉（4 条线共享一个端点，Debug 下也不能触发断言）
    {
        auto pFixture = makeRectangleFixture({
            { c0, c1 }, { c0, c2 }, { c0, c3 }, { c0, Vector2(-3.0, 2.0) } });
        ASSERT_NE(pFixture, nullptr);
        EXPECT_EQ(wy3dApp::tryDetectSketchRectangle(toConstLines(*pFixture)), nullptr);
    }
}

TEST(SketchRectangleRecognizer, RejectWrongLineCount)
{
    EXPECT_EQ(wy3dApp::tryDetectSketchRectangle({}), nullptr);

    auto pFixture = makeRectangleFixture({
        { Vector2(0.0, 0.0), Vector2(10.0, 0.0) },
        { Vector2(10.0, 0.0), Vector2(10.0, 5.0) },
        { Vector2(10.0, 5.0), Vector2(0.0, 5.0) } });
    ASSERT_NE(pFixture, nullptr);
    EXPECT_EQ(wy3dApp::tryDetectSketchRectangle(toConstLines(*pFixture)), nullptr);
}

TEST(SketchRectangleRecognizer, ReadCornersFromLiveDatabase)
{
    const Vector2 c0(0.0, 0.0);
    const Vector2 c1(10.0, 0.0);
    const Vector2 c2(10.0, 5.0);
    const Vector2 c3(0.0, 5.0);
    auto pFixture = makeRectangleFixture({
        { c1, c0 }, { c2, c1 }, { c3, c2 }, { c0, c3 } });
    ASSERT_NE(pFixture, nullptr);

    const auto pInfo = wy3dApp::tryDetectSketchRectangle(toConstLines(*pFixture));
    ASSERT_NE(pInfo, nullptr);
    const wy3dApp::SketchRectangleInfo& info = *pInfo;

    std::array<Vector2, 4> corners{};
    std::array<bool, 4> startAtCorner{};
    EXPECT_TRUE(wy3dApp::readSketchRectangleCorners(
        pFixture->pDb.get(), info.lineIds, corners, &startAtCorner));

    for (int k = 0; k < 4; ++k)
    {
        // 角点集合应与原始矩形角点一致（顺序取决于环起点）
        bool found = false;
        const Vector2 expected[4] = { c0, c1, c2, c3 };
        for (int j = 0; j < 4; ++j)
        {
            if ((corners[k] - expected[j]).length() < 1e-9) found = true;
        }
        EXPECT_TRUE(found);

        const wy3d::SketchLine* pLine = findLine(*pFixture, info.lineIds[k]);
        ASSERT_NE(pLine, nullptr);
        const Vector2 start = pLine->getStartPoint();
        const double distanceToCurrent = (start - corners[k]).length();
        const double distanceToNext = (start - corners[(k + 1) % 4]).length();
        EXPECT_EQ(startAtCorner[k], distanceToCurrent <= distanceToNext);
    }

    // 端点偏差在 joinTol 内仍可读取
    {
        wydb::TransactionManager* pMgr = pFixture->pDb->getTransactionManager();
        wydb::Transaction* pTrans = pMgr->startTransaction();
        ASSERT_NE(pTrans, nullptr);
        wy3d::SketchLine* pLine = wy3d::SketchLine::cast(
            pTrans->getElementForWrite(pFixture->lines[0]->getId()));
        ASSERT_NE(pLine, nullptr);
        EXPECT_EQ(pLine->setStartPoint(pLine->getStartPoint() + Vector2(1e-7, 1e-7)),
            wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);

        std::array<Vector2, 4> shiftedCorners{};
        EXPECT_TRUE(wy3dApp::readSketchRectangleCorners(
            pFixture->pDb.get(), info.lineIds, shiftedCorners));
    }

    // 端点偏差超过 joinTol 时读取失败
    {
        wydb::TransactionManager* pMgr = pFixture->pDb->getTransactionManager();
        wydb::Transaction* pTrans = pMgr->startTransaction();
        ASSERT_NE(pTrans, nullptr);
        wy3d::SketchLine* pLine = wy3d::SketchLine::cast(
            pTrans->getElementForWrite(pFixture->lines[0]->getId()));
        ASSERT_NE(pLine, nullptr);
        EXPECT_EQ(pLine->setStartPoint(pLine->getStartPoint() + Vector2(1e-2, 1e-2)),
            wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);

        std::array<Vector2, 4> brokenCorners{};
        EXPECT_FALSE(wy3dApp::readSketchRectangleCorners(
            pFixture->pDb.get(), info.lineIds, brokenCorners));
    }
}

TEST(SketchRectangleRecognizer, ReadCornersAfterFlipDrag)
{
    const Vector2 c0(0.0, 0.0);
    const Vector2 c1(10.0, 0.0);
    const Vector2 c2(10.0, 5.0);
    const Vector2 c3(0.0, 5.0);
    auto pFixture = makeRectangleFixture({
        { c0, c1 }, { c1, c2 }, { c2, c3 }, { c3, c0 } });
    ASSERT_NE(pFixture, nullptr);

    const auto pInfo = wy3dApp::tryDetectSketchRectangle(toConstLines(*pFixture));
    ASSERT_NE(pInfo, nullptr);
    const wy3dApp::SketchRectangleInfo& info = *pInfo;

    std::array<Vector2, 4> corners{};
    std::array<bool, 4> lineStartAtCorner{};
    ASSERT_TRUE(wy3dApp::readSketchRectangleCorners(
        pFixture->pDb.get(), info.lineIds, corners, &lineStartAtCorner));

    // 模拟把 corners[1] 拖过对角点：u、v 两个方向同时反向
    constexpr double kMinHalf = 0.0005;
    const std::array<Vector2, 4> newCorners = wy3dApp::computeRectangleDragCorners(
        corners, info.u, info.v, info.halfWidth, info.halfHeight,
        wy3dApp::SketchRectangleHandle::Corner, 1,
        corners[3] + Vector2(-3.0, 3.0), kMinHalf);

    // 与 SketchRectangleGizmo::onDragging 相同的写法写回 4 条线
    {
        wydb::TransactionManager* pMgr = pFixture->pDb->getTransactionManager();
        wydb::Transaction* pTrans = pMgr->startTransaction();
        ASSERT_NE(pTrans, nullptr);
        for (int i = 0; i < 4; ++i)
        {
            wy3d::SketchLine* pLine = wy3d::SketchLine::cast(
                pTrans->getElementForWrite(info.lineIds[i]));
            ASSERT_NE(pLine, nullptr);
            const Vector2& corner = newCorners[i];
            const Vector2& nextCorner = newCorners[(i + 1) % 4];
            EXPECT_EQ(lineStartAtCorner[i]
                ? pLine->setStartPoint(corner) : pLine->setStartPoint(nextCorner),
                wy::ErrorStatus::Ok);
            EXPECT_EQ(lineStartAtCorner[i]
                ? pLine->setEndPoint(nextCorner) : pLine->setEndPoint(corner),
                wy::ErrorStatus::Ok);
        }
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }

    std::array<Vector2, 4> flippedCorners{};
    std::array<bool, 4> flippedLineStartAtCorner{};
    ASSERT_TRUE(wy3dApp::readSketchRectangleCorners(
        pFixture->pDb.get(), info.lineIds, flippedCorners, &flippedLineStartAtCorner));

    for (int i = 0; i < 4; ++i)
    {
        EXPECT_NEAR((flippedCorners[i] - newCorners[i]).length(), 0.0, 1e-9);
        EXPECT_EQ(flippedLineStartAtCorner[i], lineStartAtCorner[i]);
    }

    // 翻转后仍然是 3x3 的矩形
    EXPECT_NEAR((newCorners[1] - newCorners[0]).length(), 3.0, 1e-9);
    EXPECT_NEAR((newCorners[3] - newCorners[0]).length(), 3.0, 1e-9);
}
