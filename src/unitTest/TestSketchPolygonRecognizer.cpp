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
#include <numeric>
#include <utility>
#include <vector>

#include <wy3dPlane.h>
#include <wy3dSketch.h>
#include <wy3dSketchLine.h>
#include <wyMath.h>
#include <wyVector3.h>

#include "utils/SketchPolygonRecognizer.h"

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

    std::vector<int> sequentialOrder(int sides)
    {
        std::vector<int> order(sides);
        std::iota(order.begin(), order.end(), 0);
        return order;
    }

    double polygonSignedArea(const std::vector<Vector2>& corners, const Vector2& center)
    {
        double area2 = 0.0;
        const int sides = static_cast<int>(corners.size());
        for (int i = 0; i < sides; ++i)
        {
            const Vector2 current = corners[i] - center;
            const Vector2 next = corners[(i + 1) % sides] - center;
            area2 += current.cross(next);
        }
        return 0.5 * area2;
    }
}

TEST(SketchPolygonRecognizer, DetectPentagonUnordered)
{
    const Vector2 center(3.0, -2.0);
    const double radius = 10.0;
    const double rotation = 0.2;
    const int sides = 5;
    const std::vector<Vector2> expected = regularPolygonCorners(center, radius, rotation, sides);

    auto pFixture = makeFixture(makeRingSegments(expected, { 2, 4, 0, 3, 1 }));
    ASSERT_NE(pFixture, nullptr);

    const auto pInfo = wy3dApp::tryDetectSketchPolygon(toConstLines(*pFixture));
    ASSERT_NE(pInfo, nullptr);
    const wy3dApp::SketchPolygonInfo& info = *pInfo;

    EXPECT_EQ(info.sides, sides);
    EXPECT_NEAR(info.center.x(), center.x(), 1e-9);
    EXPECT_NEAR(info.center.y(), center.y(), 1e-9);
    EXPECT_NEAR(info.circumRadius, radius, 1e-9);
    EXPECT_NEAR(info.inRadius, radius * std::cos(wy::PI / sides), 1e-9);
    // corners[0] is the vertex closest to +X
    EXPECT_NEAR(info.rotation, rotation, 1e-9);
    // The label edge midpoint sits at rotation + 4.5 * 72deg = -24.5deg, the
    // midpoint closest to +X
    EXPECT_EQ(info.labelEdge, 4);
    EXPECT_GT(polygonSignedArea(info.corners, info.center), 0.0);

    for (int k = 0; k < sides; ++k)
    {
        bool found = false;
        for (const Vector2& corner : expected)
        {
            if ((info.corners[k] - corner).length() < 1e-9) found = true;
        }
        EXPECT_TRUE(found);

        const wy3d::SketchLine* pLine = findLine(*pFixture, info.lineIds[k]);
        ASSERT_NE(pLine, nullptr);
        const Vector2 start = pLine->getStartPoint();
        const Vector2 end = pLine->getEndPoint();
        const Vector2& corner = info.corners[k];
        const Vector2& nextCorner = info.corners[(k + 1) % sides];
        const bool direct = (start - corner).length() < 1e-9
            && (end - nextCorner).length() < 1e-9;
        const bool reversed = (start - nextCorner).length() < 1e-9
            && (end - corner).length() < 1e-9;
        EXPECT_TRUE(direct || reversed);
    }
}

TEST(SketchPolygonRecognizer, CanonicalOrderIsStable)
{
    const Vector2 center(-1.0, 4.0);
    const double rotation = -0.35;
    const int sides = 6;
    const std::vector<Vector2> corners = regularPolygonCorners(center, 9.0, rotation, sides);

    auto pFixture = makeFixture(makeRingSegments(corners, sequentialOrder(sides)));
    ASSERT_NE(pFixture, nullptr);

    const std::vector<const wy3d::SketchLine*> lines = toConstLines(*pFixture);
    const auto pReference = wy3dApp::tryDetectSketchPolygon(lines);
    ASSERT_NE(pReference, nullptr);

    for (int shift = 0; shift < sides; ++shift)
    {
        std::vector<const wy3d::SketchLine*> permuted;
        for (int k = 0; k < sides; ++k) permuted.push_back(lines[(k + shift) % sides]);
        if (shift % 2) std::reverse(permuted.begin(), permuted.end());

        const auto pInfo = wy3dApp::tryDetectSketchPolygon(permuted);
        ASSERT_NE(pInfo, nullptr) << "shift " << shift;
        EXPECT_EQ(pInfo->lineIds, pReference->lineIds);
        EXPECT_EQ(pInfo->labelEdge, pReference->labelEdge);
        EXPECT_NEAR(pInfo->rotation, pReference->rotation, 1e-12);
        for (int k = 0; k < sides; ++k)
        {
            EXPECT_NEAR((pInfo->corners[k] - pReference->corners[k]).length(), 0.0, 1e-12);
        }
    }
}

TEST(SketchPolygonRecognizer, DetectTriangleWithClockwiseInput)
{
    const Vector2 center(1.0, 1.0);
    const double radius = 4.0;
    const int sides = 3;
    const std::vector<Vector2> corners = regularPolygonCorners(center, radius, 0.0, sides);

    // Every line stored backwards -> the ring walk runs clockwise
    std::vector<std::pair<Vector2, Vector2>> segments;
    for (int i = 0; i < sides; ++i)
    {
        segments.push_back(std::make_pair(corners[(i + 1) % sides], corners[i]));
    }
    auto pFixture = makeFixture(segments);
    ASSERT_NE(pFixture, nullptr);

    const auto pInfo = wy3dApp::tryDetectSketchPolygon(toConstLines(*pFixture));
    ASSERT_NE(pInfo, nullptr);
    EXPECT_EQ(pInfo->sides, sides);
    EXPECT_NEAR(pInfo->circumRadius, radius, 1e-9);
    EXPECT_NEAR(pInfo->inRadius, radius * 0.5, 1e-9);
    EXPECT_GT(polygonSignedArea(pInfo->corners, pInfo->center), 0.0);
    EXPECT_NEAR(pInfo->rotation, 0.0, 1e-9);
}

TEST(SketchPolygonRecognizer, RejectsNonPolygons)
{
    // 正方形归矩形 Gizmo
    {
        auto pFixture = makeFixture(makeRingSegments(
            regularPolygonCorners(Vector2(), 5.0, 0.0, 4), sequentialOrder(4)));
        ASSERT_NE(pFixture, nullptr);
        EXPECT_EQ(wy3dApp::tryDetectSketchPolygon(toConstLines(*pFixture)), nullptr);
    }

    // 非正五边形：一个顶点被拉长
    {
        std::vector<Vector2> corners = regularPolygonCorners(Vector2(), 5.0, 0.0, 5);
        corners[2] = 1.2 * corners[2];
        auto pFixture = makeFixture(makeRingSegments(corners, sequentialOrder(5)));
        ASSERT_NE(pFixture, nullptr);
        EXPECT_EQ(wy3dApp::tryDetectSketchPolygon(toConstLines(*pFixture)), nullptr);
    }

    // 未闭合
    {
        auto segments = makeRingSegments(regularPolygonCorners(Vector2(), 5.0, 0.0, 5),
                                         sequentialOrder(5));
        segments[4].second = segments[4].second + Vector2(1e-2, 1e-2);
        auto pFixture = makeFixture(segments);
        ASSERT_NE(pFixture, nullptr);
        EXPECT_EQ(wy3dApp::tryDetectSketchPolygon(toConstLines(*pFixture)), nullptr);
    }

    // 少于 3 条线
    {
        auto pFixture = makeFixture({
            { Vector2(0.0, 0.0), Vector2(5.0, 0.0) },
            { Vector2(5.0, 0.0), Vector2(0.0, 5.0) } });
        ASSERT_NE(pFixture, nullptr);
        EXPECT_EQ(wy3dApp::tryDetectSketchPolygon(toConstLines(*pFixture)), nullptr);
    }

    // 超过 100 条线
    {
        auto pFixture = makeFixture(makeRingSegments(
            regularPolygonCorners(Vector2(), 20.0, 0.0, 101), sequentialOrder(101)));
        ASSERT_NE(pFixture, nullptr);
        EXPECT_EQ(wy3dApp::tryDetectSketchPolygon(toConstLines(*pFixture)), nullptr);
    }

    // 非法容差
    {
        auto pFixture = makeFixture(makeRingSegments(
            regularPolygonCorners(Vector2(), 5.0, 0.0, 5), sequentialOrder(5)));
        ASSERT_NE(pFixture, nullptr);
        EXPECT_EQ(wy3dApp::tryDetectSketchPolygon(toConstLines(*pFixture), 0.0, 1e-6), nullptr);
        EXPECT_EQ(wy3dApp::tryDetectSketchPolygon(toConstLines(*pFixture), 1e-4, 0.0), nullptr);
    }
}

TEST(SketchPolygonRecognizer, DetectAtLargeCoordinates)
{
    const Vector2 center(1e6, -2e6);
    const double radius = 10.0;
    const double rotation = 0.1;
    const int sides = 7;
    const std::vector<Vector2> expected = regularPolygonCorners(center, radius, rotation, sides);

    auto pFixture = makeFixture(makeRingSegments(expected, { 3, 6, 0, 5, 1, 4, 2 }));
    ASSERT_NE(pFixture, nullptr);

    const auto pInfo = wy3dApp::tryDetectSketchPolygon(toConstLines(*pFixture));
    ASSERT_NE(pInfo, nullptr);
    EXPECT_EQ(pInfo->sides, sides);
    EXPECT_NEAR(pInfo->center.x(), center.x(), 1e-6);
    EXPECT_NEAR(pInfo->center.y(), center.y(), 1e-6);
    EXPECT_NEAR(pInfo->circumRadius, radius, 1e-6);
    EXPECT_NEAR(pInfo->rotation, rotation, 1e-6);
    EXPECT_GT(polygonSignedArea(pInfo->corners, pInfo->center), 0.0);
}
