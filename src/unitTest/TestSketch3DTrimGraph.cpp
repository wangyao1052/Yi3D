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

#include <utils/wy3dSketch3DTrimGraph.h>
#include <wy3dMath.h>
#include <wy3dSketch3D.h>
#include <wy3dSketchArc3D.h>
#include <wy3dSketchCircle3D.h>
#include <wy3dSketchCurve3D.h>
#include <wy3dSketchEllipse3D.h>
#include <wy3dSketchEllipseArc3D.h>
#include <wy3dSketchLine3D.h>
#include <wy3dSketchSpline3D.h>

#include <cmath>
#include <memory>

namespace
{
const double kTol = 1e-6;

const wy::Vector3 kAxisZ(0.0, 0.0, 1.0);
const wy::Vector3 kAxisX(1.0, 0.0, 0.0);

struct Fixture
{
    std::unique_ptr<wy3d::Database> pDb;
    wydb::Transaction* pTrans = nullptr;
    wy3d::Sketch3D* pSketch3D = nullptr;

    Fixture()
    {
        pDb = std::make_unique<wy3d::Database>();
        pTrans = pDb->getTransactionManager()->startTransaction();
        EXPECT_NE(pTrans, nullptr);
        EXPECT_EQ(wy3d::Sketch3D::create(pTrans, pSketch3D), wy::ErrorStatus::Ok);
    }

    wy3d::SketchLine3D* makeLine(const wy::Vector3& startPnt, const wy::Vector3& endPnt)
    {
        wy3d::SketchLine3D* pLine = nullptr;
        EXPECT_EQ(wy3d::SketchLine3D::create(pTrans, startPnt, endPnt, pLine), wy::ErrorStatus::Ok);
        EXPECT_EQ(pSketch3D->addEntity(pLine), wy::ErrorStatus::Ok);
        return pLine;
    }

    wy3d::SketchCircle3D* makeCircle(const wy::Vector3& center, double radius)
    {
        wy3d::SketchCircle3D* pCircle = nullptr;
        EXPECT_EQ(wy3d::SketchCircle3D::create(pTrans, center, kAxisZ, kAxisX, radius, pCircle),
            wy::ErrorStatus::Ok);
        EXPECT_EQ(pSketch3D->addEntity(pCircle), wy::ErrorStatus::Ok);
        return pCircle;
    }

    wy3d::SketchArc3D* makeArc(const wy::Vector3& center, double radius, double startAngle, double endAngle)
    {
        wy3d::SketchArc3D* pArc = nullptr;
        EXPECT_EQ(wy3d::SketchArc3D::create(pTrans, center, kAxisZ, kAxisX, radius, startAngle, endAngle, pArc),
            wy::ErrorStatus::Ok);
        EXPECT_EQ(pSketch3D->addEntity(pArc), wy::ErrorStatus::Ok);
        return pArc;
    }

    wy3d::SketchSpline3D* makeSpline(std::uint32_t degree, const std::vector<wy::Vector3>& controlPoints)
    {
        wy3d::SketchSpline3D* pSpline = nullptr;
        EXPECT_EQ(wy3d::SketchSpline3D::create(pTrans, degree, controlPoints, pSpline), wy::ErrorStatus::Ok);
        EXPECT_EQ(pSketch3D->addEntity(pSpline), wy::ErrorStatus::Ok);
        return pSpline;
    }

    // A ray out of the centre crosses an ellipse centred there exactly once, so a chord through this
    // point cuts it at the polar angle it names and nowhere else.
    wy3d::SketchLine3D* makeRadialChord(double polar)
    {
        const double dx = std::cos(polar);
        const double dy = std::sin(polar);
        // How far the ellipse with semi axes 10 and 5 reaches along that direction.
        const double reach = 1.0 / std::sqrt(dx * dx / 100.0 + dy * dy / 25.0);
        const wy::Vector3 pnt = wy::Vector3(dx, dy, 0.0) * reach;
        return this->makeLine(pnt * 0.5, pnt * 1.5);
    }
};
} // namespace

TEST(Sketch3DTrimGraph, PickingEitherSideOfACrossingGivesTheTwoPieces)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLineA = fixture.makeLine(wy::Vector3::kZero, wy::Vector3(10.0, 0.0, 0.0));
    wy3d::SketchLine3D* pLineB = fixture.makeLine(wy::Vector3(5.0, -5.0, 0.0), wy::Vector3(5.0, 5.0, 0.0));

    wy3d::Sketch3DTrimGraph graph(fixture.pSketch3D, kTol);
    ASSERT_TRUE(graph.isValid());

    // Left of the crossing: the piece from the start to it.
    const wy3d::Sketch3DTrimSegment left = graph.pick(pLineA->getId(), wy::Vector3(2.0, 0.0, 0.0));
    ASSERT_TRUE(left.isValid());
    EXPECT_NEAR(left.startKnot.getParam(), 0.0, 1e-9);
    EXPECT_NEAR(left.endKnot.getParam(), 0.5, 1e-9);
    EXPECT_NEAR((left.endKnot.getPosition() - wy::Vector3(5.0, 0.0, 0.0)).length(), 0.0, 1e-9);

    // Right of it: from the crossing to the end.
    const wy3d::Sketch3DTrimSegment right = graph.pick(pLineA->getId(), wy::Vector3(8.0, 0.0, 0.0));
    ASSERT_TRUE(right.isValid());
    EXPECT_NEAR(right.startKnot.getParam(), 0.5, 1e-9);
    EXPECT_NEAR(right.endKnot.getParam(), 1.0, 1e-9);

    // The same crossing lives on the other line too, so it splits there as well.
    const wy3d::Sketch3DTrimSegment below = graph.pick(pLineB->getId(), wy::Vector3(5.0, -2.0, 0.0));
    ASSERT_TRUE(below.isValid());
    EXPECT_NEAR(below.startKnot.getParam(), 0.0, 1e-9);
    EXPECT_NEAR(below.endKnot.getParam(), 0.5, 1e-9);
}

TEST(Sketch3DTrimGraph, ALineThroughTheMiddleOfABoxIsBracketedByBothEdges)
{
    Fixture fixture;
    fixture.makeLine(wy::Vector3::kZero, wy::Vector3(10.0, 0.0, 0.0));
    fixture.makeLine(wy::Vector3(10.0, 0.0, 0.0), wy::Vector3(10.0, 10.0, 0.0));
    fixture.makeLine(wy::Vector3(10.0, 10.0, 0.0), wy::Vector3(0.0, 10.0, 0.0));
    fixture.makeLine(wy::Vector3(0.0, 10.0, 0.0), wy::Vector3::kZero);
    wy3d::SketchLine3D* pCut = fixture.makeLine(wy::Vector3(5.0, -5.0, 0.0), wy::Vector3(5.0, 15.0, 0.0));

    wy3d::Sketch3DTrimGraph graph(fixture.pSketch3D, kTol);
    ASSERT_TRUE(graph.isValid());

    // The cut runs from y = -5 to y = 15, so the two horizontal edges it crosses at y = 0 and
    // y = 10 sit at 0.25 and 0.75 of its length. The vertical edges are parallel to it.
    const wy3d::Sketch3DTrimSegment first = graph.pick(pCut->getId(), wy::Vector3(5.0, -2.0, 0.0));
    ASSERT_TRUE(first.isValid());
    EXPECT_NEAR(first.startKnot.getParam(), 0.0, 1e-9);
    EXPECT_NEAR(first.endKnot.getParam(), 0.25, 1e-9);

    const wy3d::Sketch3DTrimSegment middle = graph.pick(pCut->getId(), wy::Vector3(5.0, 5.0, 0.0));
    ASSERT_TRUE(middle.isValid());
    EXPECT_NEAR(middle.startKnot.getParam(), 0.25, 1e-9);
    EXPECT_NEAR(middle.endKnot.getParam(), 0.75, 1e-9);
    EXPECT_NEAR((middle.startKnot.getPosition() - wy::Vector3(5.0, 0.0, 0.0)).length(), 0.0, 1e-9);
    EXPECT_NEAR((middle.endKnot.getPosition() - wy::Vector3(5.0, 10.0, 0.0)).length(), 0.0, 1e-9);

    const wy3d::Sketch3DTrimSegment last = graph.pick(pCut->getId(), wy::Vector3(5.0, 12.0, 0.0));
    ASSERT_TRUE(last.isValid());
    EXPECT_NEAR(last.startKnot.getParam(), 0.75, 1e-9);
    EXPECT_NEAR(last.endKnot.getParam(), 1.0, 1e-9);
}

TEST(Sketch3DTrimGraph, ACurveNobodyCrossesReadsAsTheWholeCurve)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLonely = fixture.makeLine(wy::Vector3::kZero, wy::Vector3(10.0, 0.0, 0.0));
    fixture.makeLine(wy::Vector3(0.0, 20.0, 0.0), wy::Vector3(10.0, 20.0, 0.0));

    wy3d::Sketch3DTrimGraph graph(fixture.pSketch3D, kTol);
    ASSERT_TRUE(graph.isValid());

    const wy3d::Sketch3DTrimSegment segment = graph.pick(pLonely->getId(), wy::Vector3(3.0, 0.0, 0.0));
    ASSERT_TRUE(segment.isValid());
    EXPECT_NEAR(segment.startKnot.getParam(), 0.0, 1e-9);
    EXPECT_NEAR(segment.endKnot.getParam(), 1.0, 1e-9);
    EXPECT_NEAR((segment.startKnot.getPosition() - wy::Vector3::kZero).length(), 0.0, 1e-9);
    EXPECT_NEAR((segment.endKnot.getPosition() - wy::Vector3(10.0, 0.0, 0.0)).length(), 0.0, 1e-9);
}

// A circle carries no end knots, so a single crossing is all it would take to split it in two - but
// one crossing still leaves a single arc, and trimming that would consume the whole curve. The
// graph says "whole curve" so the command can refuse instead.
TEST(Sketch3DTrimGraph, ACircleCrossedOnceIsStillTheWholeCircle)
{
    Fixture fixture;
    wy3d::SketchCircle3D* pCircle = fixture.makeCircle(wy::Vector3::kZero, 5.0);
    fixture.makeLine(wy::Vector3(-10.0, 0.0, 0.0), wy::Vector3(3.0, 0.0, 0.0));

    wy3d::Sketch3DTrimGraph graph(fixture.pSketch3D, kTol);
    ASSERT_TRUE(graph.isValid());

    const wy3d::Sketch3DTrimSegment segment = graph.pick(pCircle->getId(), wy::Vector3(0.0, 5.0, 0.0));
    ASSERT_TRUE(segment.isValid());
    EXPECT_NEAR(segment.startKnot.getParam(), 0.0, 1e-9);
    EXPECT_NEAR(segment.endKnot.getParam(), 1.0, 1e-9);
}

TEST(Sketch3DTrimGraph, AnArcSplitsAtACrossingAndKeepsItsOwnSweep)
{
    Fixture fixture;
    wy3d::SketchArc3D* pArc = fixture.makeArc(wy::Vector3::kZero, 5.0, 0.0, wy3d::PI_2);
    // Cuts the arc at 45 degrees, which is half its sweep.
    fixture.makeLine(wy::Vector3(3.5355339, -5.0, 0.0), wy::Vector3(3.5355339, 5.0, 0.0));

    wy3d::Sketch3DTrimGraph graph(fixture.pSketch3D, kTol);
    ASSERT_TRUE(graph.isValid());

    const wy3d::Sketch3DTrimSegment head = graph.pick(pArc->getId(), wy::Vector3(4.9, 1.0, 0.0));
    ASSERT_TRUE(head.isValid());
    EXPECT_NEAR(head.startKnot.getParam(), 0.0, 1e-6);
    EXPECT_NEAR(head.endKnot.getParam(), 0.5, 1e-6);

    const wy3d::Sketch3DTrimSegment tail = graph.pick(pArc->getId(), wy::Vector3(1.0, 4.9, 0.0));
    ASSERT_TRUE(tail.isValid());
    EXPECT_NEAR(tail.startKnot.getParam(), 0.5, 1e-6);
    EXPECT_NEAR(tail.endKnot.getParam(), 1.0, 1e-6);
}

// Two collinear overlapping segments share infinitely many points, so there is no crossing to cut
// at. The graph must survive it rather than filling the knot list with garbage.
TEST(Sketch3DTrimGraph, CollinearOverlapRecordsNoKnots)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLineA = fixture.makeLine(wy::Vector3::kZero, wy::Vector3(10.0, 0.0, 0.0));
    wy3d::SketchLine3D* pLineB = fixture.makeLine(wy::Vector3(5.0, 0.0, 0.0), wy::Vector3(15.0, 0.0, 0.0));

    wy3d::Sketch3DTrimGraph graph(fixture.pSketch3D, kTol);
    ASSERT_TRUE(graph.isValid());

    const wy3d::Sketch3DTrimSegment segment = graph.pick(pLineA->getId(), wy::Vector3(2.0, 0.0, 0.0));
    ASSERT_TRUE(segment.isValid());
    EXPECT_NEAR(segment.startKnot.getParam(), 0.0, 1e-9);
    EXPECT_NEAR(segment.endKnot.getParam(), 1.0, 1e-9);

    const wy3d::Sketch3DTrimSegment other = graph.pick(pLineB->getId(), wy::Vector3(6.0, 0.0, 0.0));
    ASSERT_TRUE(other.isValid());
    EXPECT_NEAR(other.startKnot.getParam(), 0.0, 1e-9);
    EXPECT_NEAR(other.endKnot.getParam(), 1.0, 1e-9);
}

// A pick on a curve the graph does not know about is answered with an invalid segment rather than
// a crash, which is what lets the command fall back to doing nothing.
TEST(Sketch3DTrimGraph, PickingAnUnknownCurveIsRefused)
{
    Fixture fixture;
    fixture.makeLine(wy::Vector3::kZero, wy::Vector3(10.0, 0.0, 0.0));

    wy3d::Sketch3DTrimGraph graph(fixture.pSketch3D, kTol);
    ASSERT_TRUE(graph.isValid());

    const wy3d::Sketch3DTrimSegment segment = graph.pick(wydb::ElementId::kNull, wy::Vector3::kZero);
    EXPECT_FALSE(segment.isValid());
}

TEST(Sketch3DTrimGraph, AnEmptySketchIsStillAValidGraph)
{
    Fixture fixture;

    wy3d::Sketch3DTrimGraph graph(fixture.pSketch3D, kTol);
    EXPECT_TRUE(graph.isValid());
}

// Two crossings split a curve into three pieces, and the middle one is bracketed by both.
TEST(Sketch3DTrimGraph, TwoCrossingsSplitACurveIntoThreePieces)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine = fixture.makeLine(wy::Vector3::kZero, wy::Vector3(30.0, 0.0, 0.0));
    fixture.makeLine(wy::Vector3(10.0, -5.0, 0.0), wy::Vector3(10.0, 5.0, 0.0));
    fixture.makeLine(wy::Vector3(20.0, -5.0, 0.0), wy::Vector3(20.0, 5.0, 0.0));

    wy3d::Sketch3DTrimGraph graph(fixture.pSketch3D, kTol);
    ASSERT_TRUE(graph.isValid());

    const wy3d::Sketch3DTrimSegment head = graph.pick(pLine->getId(), wy::Vector3(4.0, 0.0, 0.0));
    ASSERT_TRUE(head.isValid());
    EXPECT_NEAR(head.startKnot.getParam(), 0.0, 1e-9);
    EXPECT_NEAR(head.endKnot.getParam(), 1.0 / 3.0, 1e-9);

    const wy3d::Sketch3DTrimSegment middle = graph.pick(pLine->getId(), wy::Vector3(15.0, 0.0, 0.0));
    ASSERT_TRUE(middle.isValid());
    EXPECT_NEAR(middle.startKnot.getParam(), 1.0 / 3.0, 1e-9);
    EXPECT_NEAR(middle.endKnot.getParam(), 2.0 / 3.0, 1e-9);

    const wy3d::Sketch3DTrimSegment tail = graph.pick(pLine->getId(), wy::Vector3(26.0, 0.0, 0.0));
    ASSERT_TRUE(tail.isValid());
    EXPECT_NEAR(tail.startKnot.getParam(), 2.0 / 3.0, 1e-9);
    EXPECT_NEAR(tail.endKnot.getParam(), 1.0, 1e-9);
}

// A chord cutting a circle on both sides splits it into two arcs, and the piece behind the seam
// comes back as a parameter past 1 rather than as a negative one.
TEST(Sketch3DTrimGraph, ACircleCrossedTwiceSplitsIntoTwoArcs)
{
    Fixture fixture;
    wy3d::SketchCircle3D* pCircle = fixture.makeCircle(wy::Vector3::kZero, 5.0);
    // A horizontal chord across the circle, well clear of the seam at 0 degrees.
    fixture.makeLine(wy::Vector3(-10.0, 4.0, 0.0), wy::Vector3(10.0, 4.0, 0.0));

    wy3d::Sketch3DTrimGraph graph(fixture.pSketch3D, kTol);
    ASSERT_TRUE(graph.isValid());

    // The chord meets the circle where the sine is 4/5, so the two crossings straddle the top of the
    // circle at 53.13 and 126.87 degrees, and a circle's parameter is the polar angle over a turn.
    // Picking at the top brackets the short piece between them; picking at the bottom brackets the
    // long one, which is the same pair read the other way round.
    const double side = std::sqrt(25.0 - 16.0);
    const double lowParam = std::atan2(4.0, side) / wy3d::TWO_PI;
    const double highParam = std::atan2(4.0, -side) / wy3d::TWO_PI;

    // The piece through the bottom of the circle runs off the end of the parameter range and back in
    // at the start, so its end knot comes back below its start knot rather than wrapped past 1. The
    // wrap belongs to whoever reads the pair: a circle's parameter is its polar angle, and the
    // command turns the pair into the arc that is left over.
    const wy3d::Sketch3DTrimSegment low = graph.pick(pCircle->getId(), wy::Vector3(0.0, -5.0, 0.0));
    ASSERT_TRUE(low.isValid());
    EXPECT_NEAR(low.startKnot.getParam(), highParam, 1e-6);
    EXPECT_NEAR(low.endKnot.getParam(), lowParam, 1e-6);
    EXPECT_LT(low.endKnot.getParam(), low.startKnot.getParam());
    EXPECT_NEAR((low.startKnot.getPosition() - wy::Vector3(-side, 4.0, 0.0)).length(), 0.0, 1e-6);
    EXPECT_NEAR((low.endKnot.getPosition() - wy::Vector3(side, 4.0, 0.0)).length(), 0.0, 1e-6);

    const wy3d::Sketch3DTrimSegment high = graph.pick(pCircle->getId(), wy::Vector3(0.0, 5.0, 0.0));
    ASSERT_TRUE(high.isValid());
    EXPECT_NEAR(high.startKnot.getParam(), lowParam, 1e-6);
    EXPECT_NEAR(high.endKnot.getParam(), highParam, 1e-6);
    EXPECT_NEAR((high.endKnot.getPosition() - wy::Vector3(-side, 4.0, 0.0)).length(), 0.0, 1e-6);
}

// An ellipse and an ellipse arc are trimmed the same way, and the crossings come back on the
// entity's own angle rather than on the parametric one its curve is drawn with.
TEST(Sketch3DTrimGraph, AnEllipseIsTrimmedAtItsParametricAngles)
{
    Fixture fixture;
    wy3d::SketchEllipse3D* pEllipse = nullptr;
    ASSERT_EQ(wy3d::SketchEllipse3D::create(fixture.pTrans, wy::Vector3::kZero, kAxisZ, kAxisX,
        10.0, 0.5, pEllipse), wy::ErrorStatus::Ok);
    ASSERT_EQ(fixture.pSketch3D->addEntity(pEllipse), wy::ErrorStatus::Ok);

    // A chord across the ellipse at the height of its 60 degree point: the minor axis reaches
    // 5*sin(60) there and the major axis is 5 out, so the crossings sit at (5, 4.33) and (-5, 4.33).
    const double yCut = 5.0 * std::sin(60.0 * wy3d::PI / 180.0);
    fixture.makeLine(wy::Vector3(-8.0, yCut, 0.0), wy::Vector3(8.0, yCut, 0.0));

    wy3d::Sketch3DTrimGraph graph(fixture.pSketch3D, kTol);
    ASSERT_TRUE(graph.isValid());

    // An ellipse's own angle is the polar one, which is what getPointAt turns into the parametric
    // angle only at the very last step. So the parameters come back at 40.9 and 139.1 degrees over a
    // turn, not at the 60 and 120 degrees the parametric reading of the same two points would give.
    const double firstParam = std::atan2(yCut, 5.0) / wy3d::TWO_PI;
    const double secondParam = std::atan2(yCut, -5.0) / wy3d::TWO_PI;

    // Picking the far end of the major axis takes the piece that runs from the second crossing round
    // through the seam to the first, so the pair comes back in that order.
    const wy3d::Sketch3DTrimSegment farEnd = graph.pick(pEllipse->getId(), wy::Vector3(-10.0, 0.0, 0.0));
    ASSERT_TRUE(farEnd.isValid());
    EXPECT_NEAR(farEnd.startKnot.getParam(), secondParam, 1e-6);
    EXPECT_NEAR(farEnd.endKnot.getParam(), firstParam, 1e-6);
    EXPECT_NEAR((farEnd.startKnot.getPosition() - wy::Vector3(-5.0, yCut, 0.0)).length(), 0.0, 1e-6);
    EXPECT_NEAR((farEnd.endKnot.getPosition() - wy::Vector3(5.0, yCut, 0.0)).length(), 0.0, 1e-6);
}

// A spline splits at its crossings like any other curve, and the parameter kept for each knot is
// recovered by projecting back onto the spline.
// An ellipse arc's parameter is measured from its own start rather than from the major axis, so the
// two crossings come back as fractions of its sweep.
TEST(Sketch3DTrimGraph, AnEllipseArcSplitsAtItsOwnAngles)
{
    Fixture fixture;
    const double deg = wy3d::PI / 180.0;
    wy3d::SketchEllipseArc3D* pEllipseArc = nullptr;
    ASSERT_EQ(wy3d::SketchEllipseArc3D::create(fixture.pTrans, wy::Vector3::kZero, kAxisZ, kAxisX,
        10.0, 0.5, 30.0 * deg, 120.0 * deg, pEllipseArc), wy::ErrorStatus::Ok);
    ASSERT_EQ(fixture.pSketch3D->addEntity(pEllipseArc), wy::ErrorStatus::Ok);

    // The arc starts at 30 degrees and runs 90, so crossings at 50 and 110 land at two ninths and
    // eight ninths of the way along it.
    fixture.makeRadialChord(50.0 * deg);
    fixture.makeRadialChord(110.0 * deg);

    wy3d::Sketch3DTrimGraph graph(fixture.pSketch3D, kTol);
    ASSERT_TRUE(graph.isValid());

    const wy3d::Sketch3DTrimSegment middle = graph.pick(pEllipseArc->getId(),
        pEllipseArc->getPointAt(5.0 / 9.0));
    ASSERT_TRUE(middle.isValid());
    EXPECT_NEAR(middle.startKnot.getParam(), 2.0 / 9.0, 1e-6);
    EXPECT_NEAR(middle.endKnot.getParam(), 8.0 / 9.0, 1e-6);
    EXPECT_NEAR((middle.startKnot.getPosition() - pEllipseArc->getPointAt(2.0 / 9.0)).length(), 0.0, 1e-6);
    EXPECT_NEAR((middle.endKnot.getPosition() - pEllipseArc->getPointAt(8.0 / 9.0)).length(), 0.0, 1e-6);
}

// A crossing that lies behind an arc's start is not on the arc at all, so it is no place to cut. An
// extend keeps such a knot - that is what it grows towards - and a trim must not.
TEST(Sketch3DTrimGraph, ACrossingBehindAnArcStartIsDropped)
{
    Fixture fixture;
    const double deg = wy3d::PI / 180.0;
    wy3d::SketchEllipseArc3D* pEllipseArc = nullptr;
    ASSERT_EQ(wy3d::SketchEllipseArc3D::create(fixture.pTrans, wy::Vector3::kZero, kAxisZ, kAxisX,
        10.0, 0.5, 30.0 * deg, 120.0 * deg, pEllipseArc), wy::ErrorStatus::Ok);
    ASSERT_EQ(fixture.pSketch3D->addEntity(pEllipseArc), wy::ErrorStatus::Ok);

    // The ray at 10 degrees meets the ellipse behind the arc's start, so it is no crossing of the
    // arc's own curve.
    fixture.makeRadialChord(10.0 * deg);

    wy3d::Sketch3DTrimGraph graph(fixture.pSketch3D, kTol);
    ASSERT_TRUE(graph.isValid());

    // One crossing cannot cut anything, so the arc reads as a whole - and its end knot is its own end
    // rather than the point behind its start.
    const wy3d::Sketch3DTrimSegment segment = graph.pick(pEllipseArc->getId(),
        pEllipseArc->getPointAt(0.5));
    ASSERT_TRUE(segment.isValid());
    EXPECT_NEAR(segment.startKnot.getParam(), 0.0, 1e-6);
    EXPECT_NEAR(segment.endKnot.getParam(), 1.0, 1e-6);
    EXPECT_NEAR((segment.endKnot.getPosition() - pEllipseArc->getEndPoint()).length(), 0.0, 1e-6);
}

TEST(Sketch3DTrimGraph, ASplineSplitsAtItsCrossings)
{
    Fixture fixture;
    wy3d::SketchSpline3D* pSpline = fixture.makeSpline(3,
        {wy::Vector3(-10.0, 0.0, 0.0), wy::Vector3(-3.0, -5.0, 0.0),
            wy::Vector3(3.0, -5.0, 0.0), wy::Vector3(10.0, 0.0, 0.0)});

    // A chord under the crown, crossing both flanks: the arch dips to y = -3.75 at its lowest, so a
    // line at y = -2 cuts it twice.
    fixture.makeLine(wy::Vector3(-8.0, -2.0, 0.0), wy::Vector3(8.0, -2.0, 0.0));

    wy3d::Sketch3DTrimGraph graph(fixture.pSketch3D, kTol);
    ASSERT_TRUE(graph.isValid());

    // Picking the crown of the arch brackets the piece between the two crossings, which is
    // symmetric about the midpoint.
    const wy3d::Sketch3DTrimSegment crown = graph.pick(pSpline->getId(), pSpline->getPointAt(0.5));
    ASSERT_TRUE(crown.isValid());
    EXPECT_NEAR(crown.startKnot.getParam() + crown.endKnot.getParam(), 1.0, 1e-6);
    EXPECT_LT(crown.startKnot.getParam(), 0.5);
    EXPECT_GT(crown.endKnot.getParam(), 0.5);
}

// A crossing sitting on a curve's own end is not a place to cut: the piece beyond it has nothing
// left to keep.
TEST(Sketch3DTrimGraph, ACrossingAtAnEndLeavesTheCurveWhole)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine = fixture.makeLine(wy::Vector3::kZero, wy::Vector3(10.0, 0.0, 0.0));
    fixture.makeLine(wy::Vector3::kZero, wy::Vector3(0.0, 8.0, 0.0));

    wy3d::Sketch3DTrimGraph graph(fixture.pSketch3D, kTol);
    ASSERT_TRUE(graph.isValid());

    const wy3d::Sketch3DTrimSegment segment = graph.pick(pLine->getId(), wy::Vector3(5.0, 0.0, 0.0));
    ASSERT_TRUE(segment.isValid());
    EXPECT_NEAR(segment.startKnot.getParam(), 0.0, 1e-9);
    EXPECT_NEAR(segment.endKnot.getParam(), 1.0, 1e-9);
}

// The cascade a trim leaves behind: the piece cut off a curve becomes a child of its node, and a
// knot that has fallen off the parent still counts if the child covers it. Without that, cutting a
// curve would silently kill every crossing the other curves had recorded against it - which is
// what makes a trim survive an undo.
TEST(Sketch3DTrimGraph, APieceCutOffKeepsItsCrossingsAliveForTheOtherCurve)
{
    Fixture fixture;
    // The crossing sits on B's own start, so shortening B from the front is enough to drop it.
    wy3d::SketchLine3D* pB = fixture.makeLine(wy::Vector3(0.0, 5.0, 0.0), wy::Vector3(10.0, 5.0, 0.0));
    wy3d::SketchLine3D* pA = fixture.makeLine(wy::Vector3(0.0, 0.0, 0.0), wy::Vector3(0.0, 10.0, 0.0));

    wy3d::Sketch3DTrimGraph graph(fixture.pSketch3D, kTol);
    ASSERT_TRUE(graph.isValid());

    // Halfway up A is the near side of the crossing, so the piece from its start to that crossing.
    const wy3d::Sketch3DTrimSegment before = graph.pick(pA->getId(), wy::Vector3(0.0, 2.0, 0.0));
    ASSERT_TRUE(before.isValid());
    EXPECT_NEAR(before.startKnot.getParam(), 0.0, 1e-9);
    EXPECT_NEAR(before.endKnot.getParam(), 0.5, 1e-9);

    // B loses its head, the way the command leaves it after a trim. The crossing is now off B, so
    // as far as A is concerned there is nothing to cut at any more.
    ASSERT_EQ(pB->setStartPoint(wy::Vector3(3.0, 5.0, 0.0)), wy::ErrorStatus::Ok);
    graph.getNode(pA->getId())->refresh(fixture.pDb.get());
    graph.getNode(pB->getId())->refresh(fixture.pDb.get());

    const wy3d::Sketch3DTrimSegment orphaned = graph.pick(pA->getId(), wy::Vector3(0.0, 2.0, 0.0));
    ASSERT_TRUE(orphaned.isValid());
    EXPECT_NEAR(orphaned.startKnot.getParam(), 0.0, 1e-9);
    EXPECT_NEAR(orphaned.endKnot.getParam(), 1.0, 1e-9);

    // Hang the piece that was cut off onto B's node, the way the command does, and the crossing is
    // answered for again - by the child rather than by B itself.
    wy3d::SketchLine3D* pHead = fixture.makeLine(wy::Vector3(0.0, 5.0, 0.0), wy::Vector3(3.0, 5.0, 0.0));
    wy3d::Sketch3DTrimNodeSPtr pNodeB = graph.getNode(pB->getId());
    ASSERT_TRUE(pNodeB);
    wy3d::Sketch3DTrimNodeSPtr pChild = pNodeB->clone(pHead->getId());
    ASSERT_TRUE(pChild);
    pNodeB->appendChild(pChild);
    graph.getNode(pA->getId())->refresh(fixture.pDb.get());
    pNodeB->refresh(fixture.pDb.get());
    pChild->refresh(fixture.pDb.get());

    const wy3d::Sketch3DTrimSegment restored = graph.pick(pA->getId(), wy::Vector3(0.0, 2.0, 0.0));
    ASSERT_TRUE(restored.isValid());
    EXPECT_NEAR(restored.startKnot.getParam(), 0.0, 1e-9);
    EXPECT_NEAR(restored.endKnot.getParam(), 0.5, 1e-9);
    EXPECT_NEAR((restored.endKnot.getPosition() - wy::Vector3(0.0, 5.0, 0.0)).length(), 0.0, 1e-9);
}

// An arc whose crossings sit either side of the seam is the case where reading a knot's parameter
// as a plain distance along the curve would put the pieces in the wrong order.
TEST(Sketch3DTrimGraph, AnArcAcrossTheSeamSplitsInTheRightOrder)
{
    Fixture fixture;
    const double deg = wy3d::PI / 180.0;
    wy3d::SketchArc3D* pArc = fixture.makeArc(wy::Vector3::kZero, 5.0, 350.0 * deg, 30.0 * deg);

    // Two radial chords: one just before the circle's seam at 355 degrees, one just after it at 5.
    const auto makeRadial = [&](double polarDeg) {
        const double polar = polarDeg * deg;
        fixture.makeLine(wy::Vector3(3.0 * std::cos(polar), 3.0 * std::sin(polar), 0.0),
            wy::Vector3(7.0 * std::cos(polar), 7.0 * std::sin(polar), 0.0));
    };
    makeRadial(355.0);
    makeRadial(5.0);

    wy3d::Sketch3DTrimGraph graph(fixture.pSketch3D, kTol);
    ASSERT_TRUE(graph.isValid());

    // The arc runs a tenth of a turn from 350 degrees, so its parameter advances across the seam
    // rather than restarting at it: 355 is an eighth of the way in, 5 degrees three eighths. A pick
    // on the seam itself, at 0 degrees, is bracketed by the two of them in that order.
    const wy3d::Sketch3DTrimSegment middle = graph.pick(pArc->getId(), wy::Vector3(5.0, 0.0, 0.0));
    ASSERT_TRUE(middle.isValid());
    EXPECT_NEAR(middle.startKnot.getParam(), 1.0 / 8.0, 1e-6);
    EXPECT_NEAR(middle.endKnot.getParam(), 3.0 / 8.0, 1e-6);
    EXPECT_NEAR((middle.startKnot.getPosition() - wy::Vector3(5.0 * std::cos(355.0 * deg),
        5.0 * std::sin(355.0 * deg), 0.0)).length(), 0.0, 1e-6);
    EXPECT_NEAR((middle.endKnot.getPosition() - wy::Vector3(5.0 * std::cos(5.0 * deg),
        5.0 * std::sin(5.0 * deg), 0.0)).length(), 0.0, 1e-6);
}
