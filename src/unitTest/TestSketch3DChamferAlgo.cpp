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

#include <utils/wy3dSketch3DChamferAlgo.h>
#include <utils/wy3dSketch3DCurveParam.h>
#include <wy3dMath.h>
#include <wy3dSketchArc3D.h>
#include <wy3dSketchCircle3D.h>
#include <wy3dSketchCurve3D.h>
#include <wy3dSketchLine3D.h>

#include <cmath>
#include <memory>

namespace
{
using Algo = wy3d::Sketch3DChamferAlgo;
using Result = Algo::Result;

const double kTol = 1e-6;

const wy::Vector3 kAxisZ(0.0, 0.0, 1.0);
const wy::Vector3 kAxisX(1.0, 0.0, 0.0);

// Entities are only ever read here, so the transaction stays open for the whole test.
struct Fixture
{
    std::unique_ptr<wy3d::Database> pDb;
    wydb::Transaction* pTrans = nullptr;

    Fixture()
    {
        pDb = std::make_unique<wy3d::Database>();
        pTrans = pDb->getTransactionManager()->startTransaction();
        EXPECT_NE(pTrans, nullptr);
    }

    wy3d::SketchLine3D* makeLine(const wy::Vector3& startPnt, const wy::Vector3& endPnt)
    {
        wy3d::SketchLine3D* pLine = nullptr;
        EXPECT_EQ(wy3d::SketchLine3D::create(pTrans, startPnt, endPnt, pLine), wy::ErrorStatus::Ok);
        return pLine;
    }
};

void expectPoint(const wy::Vector3& actual, const wy::Vector3& expected)
{
    EXPECT_LE((actual - expected).length(), 1e-9) << "got (" << actual.x() << ", " << actual.y()
        << ", " << actual.z() << ")";
}

void expectRange(double startParam, double endParam, double expectedStart, double expectedEnd)
{
    EXPECT_NEAR(startParam, expectedStart, 1e-9);
    EXPECT_NEAR(endParam, expectedEnd, 1e-9);
}
} // namespace

// An L in the XY plane with the corner at the origin, which is the start of both segments. Neither
// pick is consulted: a corner sitting on an endpoint leaves only one half to keep.
TEST(Sketch3DChamferAlgo, CornerAtTheStartOfBothSegments)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine1st = fixture.makeLine(wy::Vector3::kZero, wy::Vector3(10.0, 0.0, 0.0));
    wy3d::SketchLine3D* pLine2nd = fixture.makeLine(wy::Vector3::kZero, wy::Vector3(0.0, 10.0, 0.0));

    wy3d::Sketch3DChamferData data;
    EXPECT_EQ(Algo::chamferLineLine(3.0, 4.0, kTol, pLine1st, wy::Vector3(5.0, 0.0, 0.0),
        pLine2nd, wy::Vector3(0.0, 5.0, 0.0), data), Result::Ok);

    expectPoint(data.chamferStartPnt, wy::Vector3(3.0, 0.0, 0.0));
    expectPoint(data.chamferEndPnt, wy::Vector3(0.0, 4.0, 0.0));
    // The corner is at the start, so the run from the chamfer back to the far end is what survives.
    expectRange(data.startParam1st, data.endParam1st, 0.3, 1.0);
    expectRange(data.startParam2nd, data.endParam2nd, 0.4, 1.0);
}

// Both corners fall mid-segment. The pick on each is what says which half to keep: here both picks
// sit past the corner, so the far half of each segment survives.
TEST(Sketch3DChamferAlgo, PicksPastTheCornerKeepTheFarHalf)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine1st = fixture.makeLine(wy::Vector3(-10.0, 0.0, 0.0), wy::Vector3(10.0, 0.0, 0.0));
    wy3d::SketchLine3D* pLine2nd = fixture.makeLine(wy::Vector3(0.0, -10.0, 0.0), wy::Vector3(0.0, 10.0, 0.0));

    wy3d::Sketch3DChamferData data;
    EXPECT_EQ(Algo::chamferLineLine(3.0, 4.0, kTol, pLine1st, wy::Vector3(5.0, 0.0, 0.0),
        pLine2nd, wy::Vector3(0.0, 5.0, 0.0), data), Result::Ok);

    expectPoint(data.chamferStartPnt, wy::Vector3(3.0, 0.0, 0.0));
    expectPoint(data.chamferEndPnt, wy::Vector3(0.0, 4.0, 0.0));
    expectRange(data.startParam1st, data.endParam1st, 0.65, 1.0);
    expectRange(data.startParam2nd, data.endParam2nd, 0.7, 1.0);
}

// The same two segments, both picks now before the corner, so the near half survives instead. The
// distances are unchanged: only which side of the corner is kept moves.
TEST(Sketch3DChamferAlgo, PicksBeforeTheCornerKeepTheNearHalf)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine1st = fixture.makeLine(wy::Vector3(-10.0, 0.0, 0.0), wy::Vector3(10.0, 0.0, 0.0));
    wy3d::SketchLine3D* pLine2nd = fixture.makeLine(wy::Vector3(0.0, -10.0, 0.0), wy::Vector3(0.0, 10.0, 0.0));

    wy3d::Sketch3DChamferData data;
    EXPECT_EQ(Algo::chamferLineLine(3.0, 4.0, kTol, pLine1st, wy::Vector3(-5.0, 0.0, 0.0),
        pLine2nd, wy::Vector3(0.0, -5.0, 0.0), data), Result::Ok);

    expectPoint(data.chamferStartPnt, wy::Vector3(-3.0, 0.0, 0.0));
    expectPoint(data.chamferEndPnt, wy::Vector3(0.0, -4.0, 0.0));
    expectRange(data.startParam1st, data.endParam1st, 0.0, 0.35);
    expectRange(data.startParam2nd, data.endParam2nd, 0.0, 0.3);
}

// Each segment is read on its own, so the two picks need not agree.
TEST(Sketch3DChamferAlgo, TheTwoPicksAreReadIndependently)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine1st = fixture.makeLine(wy::Vector3(-10.0, 0.0, 0.0), wy::Vector3(10.0, 0.0, 0.0));
    wy3d::SketchLine3D* pLine2nd = fixture.makeLine(wy::Vector3(0.0, -10.0, 0.0), wy::Vector3(0.0, 10.0, 0.0));

    wy3d::Sketch3DChamferData data;
    EXPECT_EQ(Algo::chamferLineLine(3.0, 4.0, kTol, pLine1st, wy::Vector3(5.0, 0.0, 0.0),
        pLine2nd, wy::Vector3(0.0, -5.0, 0.0), data), Result::Ok);

    expectPoint(data.chamferStartPnt, wy::Vector3(3.0, 0.0, 0.0));
    expectPoint(data.chamferEndPnt, wy::Vector3(0.0, -4.0, 0.0));
    expectRange(data.startParam1st, data.endParam1st, 0.65, 1.0);
    expectRange(data.startParam2nd, data.endParam2nd, 0.0, 0.3);
}

// The corner lies past the end of the first segment, which is legal here for the same reason it is
// in 2D: the chamfer runs from the corner, and the segment is extended back to reach it. A range
// ending above 1 is that extension, and the command layer must place the end, not trim to it.
TEST(Sketch3DChamferAlgo, ACornerPastAnEndExtendsThatCurve)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine1st = fixture.makeLine(wy::Vector3::kZero, wy::Vector3(1.0, 0.0, 0.0));
    wy3d::SketchLine3D* pLine2nd = fixture.makeLine(wy::Vector3(5.0, -1.0, 0.0), wy::Vector3(5.0, 1.0, 0.0));

    wy3d::Sketch3DChamferData data;
    EXPECT_EQ(Algo::chamferLineLine(2.0, 0.5, kTol, pLine1st, wy::Vector3(0.5, 0.0, 0.0),
        pLine2nd, wy::Vector3(5.0, 0.5, 0.0), data), Result::Ok);

    expectPoint(data.chamferStartPnt, wy::Vector3(3.0, 0.0, 0.0));
    expectPoint(data.chamferEndPnt, wy::Vector3(5.0, 0.5, 0.0));
    expectRange(data.startParam1st, data.endParam1st, 0.0, 3.0);
    expectRange(data.startParam2nd, data.endParam2nd, 0.75, 1.0);
}

// A corner that is not in any coordinate plane. Two straight segments sharing a point are always
// coplanar, so nothing here needs a plane: the pair is solved in space directly.
TEST(Sketch3DChamferAlgo, ACornerOutOfTheCoordinatePlanes)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine1st = fixture.makeLine(wy::Vector3::kZero, wy::Vector3(0.0, 0.0, 10.0));
    wy3d::SketchLine3D* pLine2nd = fixture.makeLine(wy::Vector3::kZero, wy::Vector3(0.0, 10.0, 0.0));

    wy3d::Sketch3DChamferData data;
    EXPECT_EQ(Algo::chamferLineLine(2.0, 3.0, kTol, pLine1st, wy::Vector3(0.0, 0.0, 5.0),
        pLine2nd, wy::Vector3(0.0, 5.0, 0.0), data), Result::Ok);

    expectPoint(data.chamferStartPnt, wy::Vector3(0.0, 0.0, 2.0));
    expectPoint(data.chamferEndPnt, wy::Vector3(0.0, 3.0, 0.0));
}

TEST(Sketch3DChamferAlgo, ParallelAndCollinearSupportsHaveNoCorner)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine = fixture.makeLine(wy::Vector3::kZero, wy::Vector3(10.0, 0.0, 0.0));
    wy3d::SketchLine3D* pParallel = fixture.makeLine(wy::Vector3(0.0, 5.0, 0.0), wy::Vector3(10.0, 5.0, 0.0));
    wy3d::SketchLine3D* pCollinear = fixture.makeLine(wy::Vector3(20.0, 0.0, 0.0), wy::Vector3(30.0, 0.0, 0.0));

    wy3d::Sketch3DChamferData data;
    EXPECT_EQ(Algo::chamferLineLine(1.0, 1.0, kTol, pLine, wy::Vector3(5.0, 0.0, 0.0),
        pParallel, wy::Vector3(5.0, 5.0, 0.0), data), Result::Parallel);
    EXPECT_EQ(Algo::chamferLineLine(1.0, 1.0, kTol, pLine, wy::Vector3(5.0, 0.0, 0.0),
        pCollinear, wy::Vector3(25.0, 0.0, 0.0), data), Result::Parallel);
}

// The 3D-only refusal. Two skew lines have no corner to measure from, and a separation wider than
// tol is exactly what intersectInfiniteLines will not paper over - so this is Parallel, not a
// separate reason: from the chamfer's point of view an unreachable corner is a corner that is not
// there. Within tol the pair counts as meeting and the chamfer is built.
TEST(Sketch3DChamferAlgo, SkewSupportsAreRefusedBeyondTolAndTakenWithinIt)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLineA = fixture.makeLine(wy::Vector3::kZero, wy::Vector3(10.0, 0.0, 0.0));
    wy3d::SketchLine3D* pFar = fixture.makeLine(wy::Vector3(5.0, -5.0, 1e-3), wy::Vector3(5.0, 5.0, 1e-3));
    wy3d::SketchLine3D* pNear = fixture.makeLine(wy::Vector3(5.0, -5.0, 1e-9), wy::Vector3(5.0, 5.0, 1e-9));

    wy3d::Sketch3DChamferData data;
    EXPECT_EQ(Algo::chamferLineLine(2.0, 2.0, kTol, pLineA, wy::Vector3(7.0, 0.0, 0.0),
        pFar, wy::Vector3(5.0, 3.0, 1e-3), data), Result::Parallel);

    EXPECT_EQ(Algo::chamferLineLine(2.0, 2.0, kTol, pLineA, wy::Vector3(7.0, 0.0, 0.0),
        pNear, wy::Vector3(5.0, 3.0, 1e-9), data), Result::Ok);
    expectRange(data.startParam1st, data.endParam1st, 0.7, 1.0);
    expectRange(data.startParam2nd, data.endParam2nd, 0.7, 1.0);
}

TEST(Sketch3DChamferAlgo, DistancesTooSmallToUse)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine1st = fixture.makeLine(wy::Vector3::kZero, wy::Vector3(10.0, 0.0, 0.0));
    wy3d::SketchLine3D* pLine2nd = fixture.makeLine(wy::Vector3::kZero, wy::Vector3(0.0, 10.0, 0.0));

    wy3d::Sketch3DChamferData data;
    // Exactly zero would trip the assert, so this is the smallest distance that gets past it.
    EXPECT_EQ(Algo::chamferLineLine(1e-9, 4.0, kTol, pLine1st, wy::Vector3(5.0, 0.0, 0.0),
        pLine2nd, wy::Vector3(0.0, 5.0, 0.0), data), Result::TooShort);
    EXPECT_EQ(Algo::chamferLineLine(3.0, 1e-9, kTol, pLine1st, wy::Vector3(5.0, 0.0, 0.0),
        pLine2nd, wy::Vector3(0.0, 5.0, 0.0), data), Result::TooShort);
}

// The corner sits at the end of the first segment and the start of the second, so a distance longer
// than the first segment would have to eat all of it. The short distance beside it is the same
// geometry with only the length changed, and it comes back fine - which is what pins the refusal on
// the range rather than on any of the other guards that report TooShort.
TEST(Sketch3DChamferAlgo, ADistancePastTheEndOfASegmentIsRefused)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine1st = fixture.makeLine(wy::Vector3::kZero, wy::Vector3(1.0, 0.0, 0.0));
    wy3d::SketchLine3D* pLine2nd = fixture.makeLine(wy::Vector3(1.0, 0.0, 0.0), wy::Vector3(1.0, 5.0, 0.0));

    wy3d::Sketch3DChamferData data;
    EXPECT_EQ(Algo::chamferLineLine(5.0, 1.0, kTol, pLine1st, wy::Vector3(0.5, 0.0, 0.0),
        pLine2nd, wy::Vector3(1.0, 4.0, 0.0), data), Result::TooShort);

    EXPECT_EQ(Algo::chamferLineLine(0.5, 1.0, kTol, pLine1st, wy::Vector3(0.5, 0.0, 0.0),
        pLine2nd, wy::Vector3(1.0, 4.0, 0.0), data), Result::Ok);
    expectPoint(data.chamferStartPnt, wy::Vector3(0.5, 0.0, 0.0));
    expectPoint(data.chamferEndPnt, wy::Vector3(1.0, 1.0, 0.0));
    expectRange(data.startParam1st, data.endParam1st, 0.0, 0.5);
    expectRange(data.startParam2nd, data.endParam2nd, 0.2, 1.0);
}

// Two segments leaving the corner at almost the same angle, so the chamfer points nearly coincide
// and there is no segment to add. Each distance is far above tol and both ranges survive here, so
// the chamfer-segment guard is the only one that can be speaking. The second half of the test is
// the same geometry at a length that does clear it, which is what makes that attribution stick.
// The angle is small enough for the points to nearly coincide, yet wide enough that the supports
// are not called parallel: sin(t) is 5e-5 against the 1e-5 the parallel test uses.
TEST(Sketch3DChamferAlgo, AChamferSegmentTooShortToPlace)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine1st = fixture.makeLine(wy::Vector3::kZero, wy::Vector3(10.0, 0.0, 0.0));
    wy3d::SketchLine3D* pLine2nd = fixture.makeLine(wy::Vector3::kZero, wy::Vector3(10.0, 0.0005, 0.0));

    wy3d::Sketch3DChamferData data;
    EXPECT_EQ(Algo::chamferLineLine(0.005, 0.005, kTol, pLine1st, wy::Vector3(5.0, 0.0, 0.0),
        pLine2nd, wy::Vector3(5.0, 0.00025, 0.0), data), Result::TooShort);

    EXPECT_EQ(Algo::chamferLineLine(1.0, 1.0, kTol, pLine1st, wy::Vector3(5.0, 0.0, 0.0),
        pLine2nd, wy::Vector3(5.0, 0.00025, 0.0), data), Result::Ok);
    expectPoint(data.chamferStartPnt, wy::Vector3(1.0, 0.0, 0.0));
    expectRange(data.startParam1st, data.endParam1st, 0.1, 1.0);
    expectRange(data.startParam2nd, data.endParam2nd, 0.1, 1.0);
}

TEST(Sketch3DChamferAlgo, AnythingButTwoDistinctStraightLinesIsRefused)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine = fixture.makeLine(wy::Vector3::kZero, wy::Vector3(10.0, 0.0, 0.0));
    wy3d::SketchCircle3D* pCircle = nullptr;
    EXPECT_EQ(wy3d::SketchCircle3D::create(fixture.pTrans, wy::Vector3(5.0, 5.0, 0.0), kAxisZ, kAxisX,
        2.0, pCircle), wy::ErrorStatus::Ok);
    wy3d::SketchArc3D* pArc = nullptr;
    EXPECT_EQ(wy3d::SketchArc3D::create(fixture.pTrans, wy::Vector3(5.0, 5.0, 0.0), kAxisZ, kAxisX,
        2.0, 0.0, wy3d::PI, pArc), wy::ErrorStatus::Ok);

    wy3d::Sketch3DChamferData data;
    EXPECT_EQ(Algo::chamferLineLine(1.0, 1.0, kTol, pLine, wy::Vector3(5.0, 0.0, 0.0),
        pCircle, wy::Vector3(7.0, 5.0, 0.0), data), Result::NotStraight);
    EXPECT_EQ(Algo::chamferLineLine(1.0, 1.0, kTol, pArc, wy::Vector3(7.0, 5.0, 0.0),
        pLine, wy::Vector3(5.0, 0.0, 0.0), data), Result::NotStraight);
    EXPECT_EQ(Algo::chamferLineLine(1.0, 1.0, kTol, nullptr, wy::Vector3(5.0, 0.0, 0.0),
        pLine, wy::Vector3(5.0, 0.0, 0.0), data), Result::NotStraight);
    // The same curve in both slots has no corner either.
    EXPECT_EQ(Algo::chamferLineLine(1.0, 1.0, kTol, pLine, wy::Vector3(5.0, 0.0, 0.0),
        pLine, wy::Vector3(5.0, 0.0, 0.0), data), Result::NotStraight);
}

// The pick is read as a position, not as a point that has to lie on the curve: a pick well off the
// line still lands on the correct side of the corner, because the test is a dot product against the
// direction the corner is approached from.
TEST(Sketch3DChamferAlgo, APickOffTheCurveStillPicksASide)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine1st = fixture.makeLine(wy::Vector3(-10.0, 0.0, 0.0), wy::Vector3(10.0, 0.0, 0.0));
    wy3d::SketchLine3D* pLine2nd = fixture.makeLine(wy::Vector3(0.0, -10.0, 0.0), wy::Vector3(0.0, 10.0, 0.0));

    wy3d::Sketch3DChamferData data;
    EXPECT_EQ(Algo::chamferLineLine(3.0, 4.0, kTol, pLine1st, wy::Vector3(5.0, 20.0, 0.0),
        pLine2nd, wy::Vector3(-20.0, -5.0, 0.0), data), Result::Ok);

    expectPoint(data.chamferStartPnt, wy::Vector3(3.0, 0.0, 0.0));
    expectRange(data.startParam1st, data.endParam1st, 0.65, 1.0);
    expectRange(data.startParam2nd, data.endParam2nd, 0.0, 0.3);
}
