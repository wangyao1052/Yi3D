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
#include <memory>
#include <utility>
#include <vector>

#include <wy3dPlane.h>
#include <wy3dSketch.h>
#include <wy3dSketchLine.h>
#include <wyMath.h>
#include <wyVector3.h>

#include "utils/SketchClosedLoop.h"

namespace
{
    using wy::Vector2;

    struct SketchFixture
    {
        std::unique_ptr<wy3d::Database> pDb;
        std::vector<wy3d::SketchLine*> lines;
    };

    std::unique_ptr<SketchFixture> makeFixture(
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
        return pFixture;
    }

    std::vector<const wy3d::SketchLine*> toConstLines(const SketchFixture& fixture)
    {
        std::vector<const wy3d::SketchLine*> lines;
        lines.reserve(fixture.lines.size());
        for (const wy3d::SketchLine* pLine : fixture.lines) lines.push_back(pLine);
        return lines;
    }

    const wy3d::SketchLine* findLine(const SketchFixture& fixture, wydb::ElementId id)
    {
        for (const wy3d::SketchLine* pLine : fixture.lines)
        {
            if (pLine->getId() == id) return pLine;
        }
        return nullptr;
    }

    std::vector<Vector2> regularPolygonCorners(const Vector2& center,
                                               double radius,
                                               double rotation,
                                               int sides)
    {
        const double step = wy::TWO_PI / sides;
        std::vector<Vector2> corners(sides);
        for (int i = 0; i < sides; ++i)
        {
            const double angle = rotation + i * step;
            corners[i] = center + radius * Vector2(std::cos(angle), std::sin(angle));
        }
        return corners;
    }

    // Ring segments with mixed storage direction, in the given order
    std::vector<std::pair<Vector2, Vector2>> makeRingSegments(
        const std::vector<Vector2>& corners,
        const std::vector<int>& order)
    {
        const int sides = static_cast<int>(corners.size());
        std::vector<std::pair<Vector2, Vector2>> segments;
        for (int k = 0; k < sides; ++k)
        {
            const int i = order[k];
            const Vector2& start = corners[i];
            const Vector2& end = corners[(i + 1) % sides];
            segments.push_back((k % 2) ? std::make_pair(end, start) : std::make_pair(start, end));
        }
        return segments;
    }

    bool sameLoop(const wy3dApp::SketchClosedLoop& a, const wy3dApp::SketchClosedLoop& b)
    {
        if (a.sketchId != b.sketchId) return false;
        if (a.lineIds != b.lineIds || a.corners.size() != b.corners.size()) return false;
        if (a.lineStartAtCorner != b.lineStartAtCorner) return false;
        for (int i = 0; i < static_cast<int>(a.corners.size()); ++i)
        {
            if ((a.corners[i] - b.corners[i]).length() > 0.0) return false;
        }
        return true;
    }
}

TEST(SketchClosedLoop, BuildsRingFromUnorderedMixedDirectionLines)
{
    const Vector2 center(3.0, -2.0);
    const double radius = 10.0;
    const double rotation = 0.2;
    const int sides = 5;
    const std::vector<Vector2> expected = regularPolygonCorners(center, radius, rotation, sides);

    auto pFixture = makeFixture(makeRingSegments(expected, { 2, 4, 0, 3, 1 }));
    ASSERT_NE(pFixture, nullptr);

    wy3dApp::SketchClosedLoop loop;
    ASSERT_TRUE(wy3dApp::buildSketchClosedLoop(toConstLines(*pFixture), 1e-6, loop));
    ASSERT_EQ(static_cast<int>(loop.corners.size()), sides);
    ASSERT_EQ(static_cast<int>(loop.lineIds.size()), sides);
    ASSERT_EQ(static_cast<int>(loop.lineStartAtCorner.size()), sides);
    EXPECT_NE(loop.sketchId, wydb::ElementId());

    for (int k = 0; k < sides; ++k)
    {
        bool found = false;
        for (const Vector2& corner : expected)
        {
            if ((loop.corners[k] - corner).length() < 1e-9) found = true;
        }
        EXPECT_TRUE(found);

        const wy3d::SketchLine* pLine = findLine(*pFixture, loop.lineIds[k]);
        ASSERT_NE(pLine, nullptr);
        const Vector2 start = pLine->getStartPoint();
        const Vector2 end = pLine->getEndPoint();
        const Vector2& corner = loop.corners[k];
        const Vector2& nextCorner = loop.corners[(k + 1) % sides];
        const bool direct = (start - corner).length() < 1e-9
            && (end - nextCorner).length() < 1e-9;
        const bool reversed = (start - nextCorner).length() < 1e-9
            && (end - corner).length() < 1e-9;
        EXPECT_TRUE(direct || reversed);
        EXPECT_EQ(loop.lineStartAtCorner[k],
            (start - corner).length2() <= (start - nextCorner).length2());
    }
}

TEST(SketchClosedLoop, RejectsOpenBranchAndDuplicateLines)
{
    const std::vector<Vector2> corners = regularPolygonCorners(Vector2(), 5.0, 0.0, 4);

    // 少于 3 条线
    {
        auto pFixture = makeFixture({ { corners[0], corners[1] }, { corners[1], corners[2] } });
        ASSERT_NE(pFixture, nullptr);
        wy3dApp::SketchClosedLoop loop;
        EXPECT_FALSE(wy3dApp::buildSketchClosedLoop(toConstLines(*pFixture), 1e-6, loop));
    }

    // 未闭合
    {
        auto segments = makeRingSegments(corners, { 0, 1, 2, 3 });
        segments[3].second = segments[3].second + Vector2(1e-2, 1e-2);
        auto pFixture = makeFixture(segments);
        ASSERT_NE(pFixture, nullptr);
        wy3dApp::SketchClosedLoop loop;
        EXPECT_FALSE(wy3dApp::buildSketchClosedLoop(toConstLines(*pFixture), 1e-6, loop));
    }

    // 分叉：三条线共用一个端点
    {
        auto pFixture = makeFixture({
            { Vector2(0.0, 0.0), Vector2(5.0, 0.0) },
            { Vector2(0.0, 0.0), Vector2(0.0, 5.0) },
            { Vector2(0.0, 0.0), Vector2(-5.0, 0.0) } });
        ASSERT_NE(pFixture, nullptr);
        wy3dApp::SketchClosedLoop loop;
        EXPECT_FALSE(wy3dApp::buildSketchClosedLoop(toConstLines(*pFixture), 1e-6, loop));
    }

    // 重复线：同一元素出现两次
    {
        auto pFixture = makeFixture(makeRingSegments(corners, { 0, 1, 2, 3 }));
        ASSERT_NE(pFixture, nullptr);
        std::vector<const wy3d::SketchLine*> lines = toConstLines(*pFixture);
        lines.push_back(lines[0]);
        wy3dApp::SketchClosedLoop loop;
        EXPECT_FALSE(wy3dApp::buildSketchClosedLoop(lines, 1e-6, loop));
    }
}

TEST(SketchClosedLoop, ReverseTwiceIsIdentity)
{
    const std::vector<Vector2> corners =
        regularPolygonCorners(Vector2(1.0, 2.0), 7.0, 0.3, 6);
    auto pFixture = makeFixture(makeRingSegments(corners, { 0, 1, 2, 3, 4, 5 }));
    ASSERT_NE(pFixture, nullptr);

    wy3dApp::SketchClosedLoop loop;
    ASSERT_TRUE(wy3dApp::buildSketchClosedLoop(toConstLines(*pFixture), 1e-6, loop));

    wy3dApp::SketchClosedLoop reversed = loop;
    wy3dApp::reverseClosedLoopRing(reversed);
    EXPECT_EQ(reversed.lineIds[0], loop.lineIds.back());
    EXPECT_NEAR((reversed.corners[0] - loop.corners[0]).length(), 0.0, 1e-12);
    EXPECT_NEAR((reversed.corners[1] - loop.corners[loop.corners.size() - 1]).length(), 0.0, 1e-12);

    // 反转后仍然是同一个环：lineIds[i] 连接 corners[i] -> corners[i+1]
    for (int i = 0; i < static_cast<int>(reversed.corners.size()); ++i)
    {
        const wy3d::SketchLine* pLine = findLine(*pFixture, reversed.lineIds[i]);
        ASSERT_NE(pLine, nullptr);
        const Vector2 start = pLine->getStartPoint();
        const Vector2 end = pLine->getEndPoint();
        const Vector2& corner = reversed.corners[i];
        const Vector2& nextCorner = reversed.corners[(i + 1) % reversed.corners.size()];
        const bool direct = (start - corner).length() < 1e-9
            && (end - nextCorner).length() < 1e-9;
        const bool reversedStorage = (start - nextCorner).length() < 1e-9
            && (end - corner).length() < 1e-9;
        EXPECT_TRUE(direct || reversedStorage);
    }

    wy3dApp::reverseClosedLoopRing(reversed);
    EXPECT_TRUE(sameLoop(loop, reversed));
}

TEST(SketchClosedLoop, RotateStartKeepsRingConsistent)
{
    const std::vector<Vector2> corners =
        regularPolygonCorners(Vector2(-4.0, 1.0), 6.0, -0.4, 7);
    auto pFixture = makeFixture(makeRingSegments(corners, { 0, 1, 2, 3, 4, 5, 6 }));
    ASSERT_NE(pFixture, nullptr);

    wy3dApp::SketchClosedLoop loop;
    ASSERT_TRUE(wy3dApp::buildSketchClosedLoop(toConstLines(*pFixture), 1e-6, loop));

    wy3dApp::SketchClosedLoop rotated = loop;
    wy3dApp::rotateClosedLoopStart(rotated, 3);
    for (int i = 0; i < 7; ++i)
    {
        EXPECT_EQ(rotated.lineIds[i], loop.lineIds[(i + 3) % 7]);
        EXPECT_NEAR((rotated.corners[i] - loop.corners[(i + 3) % 7]).length(), 0.0, 1e-12);
        EXPECT_EQ(rotated.lineStartAtCorner[i], loop.lineStartAtCorner[(i + 3) % 7]);
    }

    // 负索引与整圈回绕
    wy3dApp::SketchClosedLoop wrapped = loop;
    wy3dApp::rotateClosedLoopStart(wrapped, 7);
    EXPECT_TRUE(sameLoop(loop, wrapped));

    wy3dApp::SketchClosedLoop negative = loop;
    wy3dApp::rotateClosedLoopStart(negative, -1);
    EXPECT_EQ(negative.lineIds[0], loop.lineIds[6]);
}

TEST(SketchClosedLoop, SignedAreaUsesCenterRelativeCoordinates)
{
    const std::vector<Vector2> ccw = {
        Vector2(0.0, 0.0), Vector2(10.0, 0.0), Vector2(10.0, 10.0), Vector2(0.0, 10.0) };
    const Vector2 center(5.0, 5.0);
    EXPECT_NEAR(wy3dApp::computeClosedLoopSignedArea(ccw, center), 100.0, 1e-12);

    std::vector<Vector2> cw(ccw.rbegin(), ccw.rend());
    EXPECT_NEAR(wy3dApp::computeClosedLoopSignedArea(cw, center), -100.0, 1e-12);

    // 大坐标下仍按相对中心的坐标计算，不会被相消误差带偏
    const Vector2 bigCenter(1e7, -2e7);
    std::vector<Vector2> big;
    for (const Vector2& corner : ccw) big.push_back(corner + bigCenter);
    EXPECT_NEAR(wy3dApp::computeClosedLoopSignedArea(big, bigCenter), 100.0, 1e-9);
}

TEST(SketchClosedLoop, ReadAndWriteRoundTrip)
{
    const Vector2 center(2.0, -3.0);
    const double rotation = 0.15;
    const int sides = 5;
    const std::vector<Vector2> expected =
        regularPolygonCorners(center, 8.0, rotation, sides);
    auto pFixture = makeFixture(makeRingSegments(expected, { 4, 2, 0, 1, 3 }));
    ASSERT_NE(pFixture, nullptr);

    wy3dApp::SketchClosedLoop loop;
    ASSERT_TRUE(wy3dApp::buildSketchClosedLoop(toConstLines(*pFixture), 1e-6, loop));

    std::vector<Vector2> liveCorners;
    std::vector<bool> liveStartAtCorner;
    ASSERT_TRUE(wy3dApp::readSketchClosedLoopCorners(
        pFixture->pDb.get(), loop.lineIds, liveCorners, &liveStartAtCorner));
    ASSERT_EQ(static_cast<int>(liveCorners.size()), sides);
    for (int i = 0; i < sides; ++i)
    {
        EXPECT_NEAR((liveCorners[i] - loop.corners[i]).length(), 0.0, 1e-9);
        EXPECT_EQ(liveStartAtCorner[i], loop.lineStartAtCorner[i]);
    }

    // 整体平移后写回，再读回应与新角点一致
    const Vector2 shift(1.5, -2.5);
    std::vector<Vector2> movedCorners(sides);
    for (int i = 0; i < sides; ++i) movedCorners[i] = liveCorners[i] + shift;

    wydb::TransactionManager* pMgr = pFixture->pDb->getTransactionManager();
    wydb::Transaction* pTrans = pMgr->startTransaction();
    ASSERT_NE(pTrans, nullptr);
    EXPECT_TRUE(wy3dApp::writeSketchClosedLoopCorners(
        pTrans, loop.lineIds, liveStartAtCorner, movedCorners));
    EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);

    std::vector<Vector2> rereadCorners;
    std::vector<bool> rereadStartAtCorner;
    ASSERT_TRUE(wy3dApp::readSketchClosedLoopCorners(
        pFixture->pDb.get(), loop.lineIds, rereadCorners, &rereadStartAtCorner));
    for (int i = 0; i < sides; ++i)
    {
        EXPECT_NEAR((rereadCorners[i] - movedCorners[i]).length(), 0.0, 1e-9);
        EXPECT_EQ(rereadStartAtCorner[i], liveStartAtCorner[i]);
    }

    // 写回后仍然是一个闭合环
    wy3dApp::SketchClosedLoop rebuilt;
    ASSERT_TRUE(wy3dApp::buildSketchClosedLoop(toConstLines(*pFixture), 1e-6, rebuilt));
    EXPECT_EQ(static_cast<int>(rebuilt.corners.size()), sides);
}

TEST(SketchClosedLoop, ReadRejectsBrokenJoin)
{
    const std::vector<Vector2> corners =
        regularPolygonCorners(Vector2(), 5.0, 0.0, 5);
    auto pFixture = makeFixture(makeRingSegments(corners, { 0, 1, 2, 3, 4 }));
    ASSERT_NE(pFixture, nullptr);

    wy3dApp::SketchClosedLoop loop;
    ASSERT_TRUE(wy3dApp::buildSketchClosedLoop(toConstLines(*pFixture), 1e-6, loop));

    wydb::TransactionManager* pMgr = pFixture->pDb->getTransactionManager();
    wydb::Transaction* pTrans = pMgr->startTransaction();
    ASSERT_NE(pTrans, nullptr);
    wy3d::SketchLine* pLine = wy3d::SketchLine::cast(
        pTrans->getElementForWrite(pFixture->lines[0]->getId()));
    ASSERT_NE(pLine, nullptr);
    EXPECT_EQ(pLine->setStartPoint(pLine->getStartPoint() + Vector2(1e-2, 0.0)),
        wy::ErrorStatus::Ok);
    EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);

    std::vector<Vector2> readCorners;
    EXPECT_FALSE(wy3dApp::readSketchClosedLoopCorners(
        pFixture->pDb.get(), loop.lineIds, readCorners));
}
