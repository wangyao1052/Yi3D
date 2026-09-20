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

#include <utils/wy3dSketch3DExtendGraph.h>
#include <wy3dMath.h>
#include <wy3dSketch3D.h>
#include <wy3dSketchArc3D.h>
#include <wy3dSketchCircle3D.h>
#include <wy3dSketchCurve3D.h>
#include <wy3dSketchEllipseArc3D.h>
#include <wy3dSketchLine3D.h>
#include <wy3dSketchSpline3D.h>

#include <cmath>
#include <memory>
#include <vector>

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
};

// A degree 3 arch, y(t) = -15 t (1 - t). Its end tangent points along (7, 5, 0).
std::vector<wy::Vector3> archPoles()
{
    return {wy::Vector3(-10.0, 0.0, 0.0), wy::Vector3(-3.0, -5.0, 0.0),
        wy::Vector3(3.0, -5.0, 0.0), wy::Vector3(10.0, 0.0, 0.0)};
}
} // namespace

// A line that stops short of another one is the whole point of the command: the piece that would
// close the gap is recorded as a knot past the end of the first line.
TEST(Sketch3DExtendGraph, ALineStopsShortAndReachesTheOther)
{
    Fixture fixture;
    wy3d::SketchLine3D* pShort = fixture.makeLine(wy::Vector3::kZero, wy::Vector3(10.0, 0.0, 0.0));
    fixture.makeLine(wy::Vector3(15.0, -5.0, 0.0), wy::Vector3(15.0, 5.0, 0.0));

    wy3d::Sketch3DExtendGraph graph(fixture.pSketch3D, kTol);
    ASSERT_TRUE(graph.isValid());

    // Picking near the far end chooses the forward direction, where the crossing sits at 1.5.
    const wy3d::Sketch3DExtendSegment forward = graph.pick(pShort->getId(), wy::Vector3(8.0, 0.0, 0.0));
    ASSERT_TRUE(forward.isValid());
    EXPECT_NEAR(forward.startKnot.getParam(), 0.0, 1e-9);
    EXPECT_NEAR(forward.endKnot.getParam(), 1.5, 1e-9);
    EXPECT_NEAR((forward.endKnot.getPosition() - wy::Vector3(15.0, 0.0, 0.0)).length(), 0.0, 1e-9);

    // Picking near the start chooses the backward direction, where nothing is reachable, so the
    // answer stays the untouched curve.
    const wy3d::Sketch3DExtendSegment backward = graph.pick(pShort->getId(), wy::Vector3(2.0, 0.0, 0.0));
    ASSERT_TRUE(backward.isValid());
    EXPECT_NEAR(backward.startKnot.getParam(), 0.0, 1e-9);
    EXPECT_NEAR(backward.endKnot.getParam(), 1.0, 1e-9);
}

// Of the curves ahead of it, the nearest one wins: an extend always takes the first thing it would
// run into.
TEST(Sketch3DExtendGraph, TheNearestCrossingAheadIsTheOneTaken)
{
    Fixture fixture;
    wy3d::SketchLine3D* pShort = fixture.makeLine(wy::Vector3::kZero, wy::Vector3(10.0, 0.0, 0.0));
    fixture.makeLine(wy::Vector3(15.0, -5.0, 0.0), wy::Vector3(15.0, 5.0, 0.0));
    fixture.makeLine(wy::Vector3(25.0, -5.0, 0.0), wy::Vector3(25.0, 5.0, 0.0));

    wy3d::Sketch3DExtendGraph graph(fixture.pSketch3D, kTol);
    ASSERT_TRUE(graph.isValid());

    const wy3d::Sketch3DExtendSegment segment = graph.pick(pShort->getId(), wy::Vector3(8.0, 0.0, 0.0));
    ASSERT_TRUE(segment.isValid());
    EXPECT_NEAR(segment.endKnot.getParam(), 1.5, 1e-9);
    EXPECT_NEAR((segment.endKnot.getPosition() - wy::Vector3(15.0, 0.0, 0.0)).length(), 0.0, 1e-9);
}

// A closed curve has no end to push out, so the graph refuses it outright.
TEST(Sketch3DExtendGraph, AClosedCurveCannotBeExtended)
{
    Fixture fixture;
    wy3d::SketchCircle3D* pCircle = fixture.makeCircle(wy::Vector3::kZero, 5.0);
    fixture.makeLine(wy::Vector3(8.0, -5.0, 0.0), wy::Vector3(8.0, 5.0, 0.0));

    wy3d::Sketch3DExtendGraph graph(fixture.pSketch3D, kTol);
    ASSERT_TRUE(graph.isValid());

    EXPECT_FALSE(graph.pick(pCircle->getId(), wy::Vector3(0.0, 5.0, 0.0)).isValid());
}

// Nothing anywhere near it means there is no direction to grow in, so the curve is refused rather
// than reported as already extended.
TEST(Sketch3DExtendGraph, ACurveWithNothingInReachIsRefused)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLonely = fixture.makeLine(wy::Vector3::kZero, wy::Vector3(10.0, 0.0, 0.0));

    wy3d::Sketch3DExtendGraph graph(fixture.pSketch3D, kTol);
    ASSERT_TRUE(graph.isValid());

    EXPECT_FALSE(graph.pick(pLonely->getId(), wy::Vector3(8.0, 0.0, 0.0)).isValid());
}

// An arc grows around the circle it was cut from, so a crossing on that circle but off the sweep
// comes back as a parameter past the sweep's end.
TEST(Sketch3DExtendGraph, AnArcGrowsAroundItsOwnCircle)
{
    Fixture fixture;
    // A quarter arc in the second quadrant, from 90 to 180 degrees.
    wy3d::SketchArc3D* pArc = fixture.makeArc(wy::Vector3::kZero, 5.0, wy3d::PI_2, wy3d::PI);

    // Tangent to the circle at 270 degrees, which is 90 degrees short of the arc's end. The segment
    // is kept short so it meets neither the circle's other crossing nor the arc itself.
    const wy::Vector3 dir(std::sqrt(0.5), std::sqrt(0.5), 0.0);
    const wy::Vector3 touch(0.0, -5.0, 0.0);
    fixture.makeLine(touch - dir, touch + dir);

    wy3d::Sketch3DExtendGraph graph(fixture.pSketch3D, kTol);
    ASSERT_TRUE(graph.isValid());

    // 270 degrees is a half turn past the start of a 90 degree sweep, so its parameter is 2.
    // Picking at 170 degrees is nearer the end, so the arc grows forward to reach it.
    const wy3d::Sketch3DExtendSegment forward = graph.pick(pArc->getId(), wy::Vector3(-4.92, 0.87, 0.0));
    ASSERT_TRUE(forward.isValid());
    EXPECT_NEAR(forward.startKnot.getParam(), 0.0, 1e-9);
    EXPECT_NEAR(forward.endKnot.getParam(), 2.0, 1e-6);
    EXPECT_NEAR((forward.endKnot.getPosition() - touch).length(), 0.0, 1e-6);

    // Picking at 100 degrees is nearer the start, so it grows backward instead. The same crossing is
    // then a full turn and a half away, which is the direction that loses.
    const wy3d::Sketch3DExtendSegment backward = graph.pick(pArc->getId(), wy::Vector3(-0.87, 4.92, 0.0));
    ASSERT_TRUE(backward.isValid());
    EXPECT_NEAR(backward.startKnot.getParam(), 2.0, 1e-6);
    EXPECT_NEAR(backward.endKnot.getParam(), 1.0, 1e-9);
}

// A spline leaves along its end tangent rather than along its own curve, which is the one case the
// support curve cannot express; the graph hands the intersection an explicit ray for it.
TEST(Sketch3DExtendGraph, ASplineGrowsAlongItsEndTangent)
{
    Fixture fixture;
    wy3d::SketchSpline3D* pSpline = fixture.makeSpline(3, archPoles());

    // The arch ends at (10,0,0) heading along (7,5,0). A short transversal segment crosses that
    // tangent 3 units out, far enough that it misses the spline itself.
    const wy::Vector3 endDir(7.0 / std::sqrt(74.0), 5.0 / std::sqrt(74.0), 0.0);
    const wy::Vector3 across(-endDir.y(), endDir.x(), 0.0);
    const wy::Vector3 touch = wy::Vector3(10.0, 0.0, 0.0) + endDir * 3.0;
    fixture.makeLine(touch - across, touch + across);

    wy3d::Sketch3DExtendGraph graph(fixture.pSketch3D, kTol);
    ASSERT_TRUE(graph.isValid());

    const wy3d::Sketch3DExtendSegment forward = graph.pick(pSpline->getId(), pSpline->getPointAt(0.9));
    ASSERT_TRUE(forward.isValid());
    EXPECT_NEAR(forward.startKnot.getParam(), 0.0, 1e-9);
    // 1 for the curve's own end plus the 3 units of tangent past it.
    EXPECT_NEAR(forward.endKnot.getParam(), 4.0, 1e-6);
    EXPECT_NEAR((forward.endKnot.getPosition() - touch).length(), 0.0, 1e-6);

    // Ahead of the start there is nothing, so the answer stays the untouched curve.
    const wy3d::Sketch3DExtendSegment backward = graph.pick(pSpline->getId(), pSpline->getPointAt(0.1));
    ASSERT_TRUE(backward.isValid());
    EXPECT_NEAR(backward.startKnot.getParam(), 0.0, 1e-9);
    EXPECT_NEAR(backward.endKnot.getParam(), 1.0, 1e-9);
}

TEST(Sketch3DExtendGraph, PickingAnUnknownCurveIsRefused)
{
    Fixture fixture;
    fixture.makeLine(wy::Vector3::kZero, wy::Vector3(10.0, 0.0, 0.0));

    wy3d::Sketch3DExtendGraph graph(fixture.pSketch3D, kTol);
    ASSERT_TRUE(graph.isValid());

    EXPECT_FALSE(graph.pick(wydb::ElementId::kNull, wy::Vector3::kZero).isValid());
}

// The mirror of the case above, and the one that used to be missed: a crossing a little *behind*
// the start. A periodic curve's parameter is not signed, so this arrives as a value greater than 1
// rather than less than 0, and reading it as "nothing to grow into" is what made an arc extend at
// one end and not the other.
TEST(Sketch3DExtendGraph, AnArcGrowsBackwardTooAndReportsAParameterAboveOne)
{
    Fixture fixture;
    // A quarter arc, from 0 to 90 degrees.
    wy3d::SketchArc3D* pArc = fixture.makeArc(wy::Vector3::kZero, 5.0, 0.0, wy3d::PI_2);

    // Crossing the circle 30 degrees behind the start, well clear of the arc itself.
    const double behind = -30.0 * wy3d::PI / 180.0;
    const wy::Vector3 touch(5.0 * std::cos(behind), 5.0 * std::sin(behind), 0.0);
    const wy::Vector3 radial(std::cos(behind), std::sin(behind), 0.0);
    fixture.makeLine(touch - radial * 2.0, touch + radial * 2.0);

    wy3d::Sketch3DExtendGraph graph(fixture.pSketch3D, kTol);
    ASSERT_TRUE(graph.isValid());

    // Picking at 20 degrees is nearer the start, so the arc grows backward. 330 degrees is 3.667
    // quarter-turns forward of 0, which is the only form the parameter ever takes.
    const wy::Vector3 pickPnt(5.0 * std::cos(20.0 * wy3d::PI / 180.0),
        5.0 * std::sin(20.0 * wy3d::PI / 180.0), 0.0);
    const wy3d::Sketch3DExtendSegment backward = graph.pick(pArc->getId(), pickPnt);
    ASSERT_TRUE(backward.isValid());
    EXPECT_NEAR(backward.startKnot.getParam(), 330.0 / 90.0, 1e-6);
    EXPECT_NEAR(backward.endKnot.getParam(), 1.0, 1e-9);
    EXPECT_NEAR((backward.startKnot.getPosition() - touch).length(), 0.0, 1e-6);

    // Growing the start means keeping the old end, so the pair is what the command branches on: the
    // parameter that stayed at 1 is the end, and the one above 1 is the start.
    EXPECT_GT(backward.startKnot.getParam(), 1.0);
    EXPECT_EQ(backward.endKnot.getParam(), 1.0);
}

// An arc whose growth behind the start is the only thing in reach still has to be reported, rather
// than read as "the whole curve", which the command treats as nothing to do.
TEST(Sketch3DExtendGraph, AReachableKnotBehindTheStartIsNotTheWholeCurve)
{
    Fixture fixture;
    wy3d::SketchArc3D* pArc = fixture.makeArc(wy::Vector3::kZero, 5.0, 0.0, wy3d::PI_2);

    const double behind = -30.0 * wy3d::PI / 180.0;
    const wy::Vector3 touch(5.0 * std::cos(behind), 5.0 * std::sin(behind), 0.0);
    const wy::Vector3 radial(std::cos(behind), std::sin(behind), 0.0);
    fixture.makeLine(touch - radial * 2.0, touch + radial * 2.0);

    wy3d::Sketch3DExtendGraph graph(fixture.pSketch3D, kTol);
    ASSERT_TRUE(graph.isValid());

    // However the cursor is placed, a segment that is not the untouched curve comes back.
    for (double degree : {10.0, 20.0, 45.0, 80.0})
    {
        const wy::Vector3 pickPnt(5.0 * std::cos(degree * wy3d::PI / 180.0),
            5.0 * std::sin(degree * wy3d::PI / 180.0), 0.0);
        const wy3d::Sketch3DExtendSegment segment = graph.pick(pArc->getId(), pickPnt);
        ASSERT_TRUE(segment.isValid());
        EXPECT_FALSE(segment.startKnot.getParam() == 0.0 && segment.endKnot.getParam() == 1.0);
    }
}

// An arc is only ever grown around the circle it was cut from, so a crossing that lands behind the
// start is still reachable even though the arc's own sweep never goes near it.
TEST(Sketch3DExtendGraph, AnArcBehindItsStartIsReachableFromEitherHalfOfTheSweep)
{
    Fixture fixture;
    // A half arc from 0 to 180 degrees, so both of its ends are far from the crossing.
    wy3d::SketchArc3D* pArc = fixture.makeArc(wy::Vector3::kZero, 5.0, 0.0, wy3d::PI);

    // 300 degrees, which is 60 degrees behind the start of the half circle.
    const double behind = 300.0 * wy3d::PI / 180.0;
    const wy::Vector3 touch(5.0 * std::cos(behind), 5.0 * std::sin(behind), 0.0);
    const wy::Vector3 radial(std::cos(behind), std::sin(behind), 0.0);
    fixture.makeLine(touch - radial * 2.0, touch + radial * 2.0);

    wy3d::Sketch3DExtendGraph graph(fixture.pSketch3D, kTol);
    ASSERT_TRUE(graph.isValid());

    // Near the end the long way round is the only route: 300 degrees reached by running forward
    // from 180. Reported as the end knot, at 1 + 120/180.
    const wy3d::Sketch3DExtendSegment forward = graph.pick(pArc->getId(), wy::Vector3(-4.9, 0.9, 0.0));
    ASSERT_TRUE(forward.isValid());
    EXPECT_NEAR(forward.startKnot.getParam(), 0.0, 1e-9);
    EXPECT_NEAR(forward.endKnot.getParam(), 1.0 + 120.0 / 180.0, 1e-6);
    EXPECT_NEAR((forward.endKnot.getPosition() - touch).length(), 0.0, 1e-6);

    // Near the start it is 60 degrees back, which is the short way and the one that should win.
    const wy3d::Sketch3DExtendSegment backward = graph.pick(pArc->getId(), wy::Vector3(4.9, 0.9, 0.0));
    ASSERT_TRUE(backward.isValid());
    EXPECT_NEAR(backward.startKnot.getParam(), 300.0 / 180.0, 1e-6);
    EXPECT_NEAR(backward.endKnot.getParam(), 1.0, 1e-9);
    EXPECT_NEAR((backward.startKnot.getPosition() - touch).length(), 0.0, 1e-6);
}

// A line's knots are signed, so the mirror of the forward case is a *negative* parameter - the one
// convention arcs are not allowed to use.
TEST(Sketch3DExtendGraph, ALineGrowsBackwardWithANegativeParameter)
{
    Fixture fixture;
    wy3d::SketchLine3D* pShort = fixture.makeLine(wy::Vector3(10.0, 0.0, 0.0), wy::Vector3(20.0, 0.0, 0.0));
    fixture.makeLine(wy::Vector3(5.0, -5.0, 0.0), wy::Vector3(5.0, 5.0, 0.0));

    wy3d::Sketch3DExtendGraph graph(fixture.pSketch3D, kTol);
    ASSERT_TRUE(graph.isValid());

    const wy3d::Sketch3DExtendSegment backward = graph.pick(pShort->getId(), wy::Vector3(12.0, 0.0, 0.0));
    ASSERT_TRUE(backward.isValid());
    // The crossing is 5 units behind a 10 unit line, so half a length before the start.
    EXPECT_NEAR(backward.startKnot.getParam(), -0.5, 1e-9);
    EXPECT_NEAR(backward.endKnot.getParam(), 1.0, 1e-9);
    EXPECT_NEAR((backward.startKnot.getPosition() - wy::Vector3(5.0, 0.0, 0.0)).length(), 0.0, 1e-9);

    // Nothing ahead of it, so the forward direction stays the untouched curve.
    const wy3d::Sketch3DExtendSegment forward = graph.pick(pShort->getId(), wy::Vector3(18.0, 0.0, 0.0));
    ASSERT_TRUE(forward.isValid());
    EXPECT_NEAR(forward.startKnot.getParam(), 0.0, 1e-9);
    EXPECT_NEAR(forward.endKnot.getParam(), 1.0, 1e-9);
}

// The start tangent ray is a separate operand from the end one, and growing backwards is the only
// thing that exercises it.
TEST(Sketch3DExtendGraph, ASplineGrowsBackwardAlongItsStartTangent)
{
    Fixture fixture;
    wy3d::SketchSpline3D* pSpline = fixture.makeSpline(3, archPoles());

    // The arch starts at (-10,0,0) heading along (-7,5,0). A short transversal crosses that tangent
    // 3 units back, far enough that it misses the spline itself.
    const wy::Vector3 startDir(-7.0 / std::sqrt(74.0), 5.0 / std::sqrt(74.0), 0.0);
    const wy::Vector3 across(-startDir.y(), startDir.x(), 0.0);
    const wy::Vector3 touch = wy::Vector3(-10.0, 0.0, 0.0) + startDir * 3.0;
    fixture.makeLine(touch - across, touch + across);

    wy3d::Sketch3DExtendGraph graph(fixture.pSketch3D, kTol);
    ASSERT_TRUE(graph.isValid());

    const wy3d::Sketch3DExtendSegment backward = graph.pick(pSpline->getId(), pSpline->getPointAt(0.1));
    ASSERT_TRUE(backward.isValid());
    // 3 units of tangent before the start, which is the signed convention a spline shares with a
    // line rather than the wrapped one an arc uses.
    EXPECT_NEAR(backward.startKnot.getParam(), -3.0, 1e-6);
    EXPECT_NEAR(backward.endKnot.getParam(), 1.0, 1e-9);
    EXPECT_NEAR((backward.startKnot.getPosition() - touch).length(), 0.0, 1e-6);

    // Nothing past the far end, so that direction stays the untouched curve.
    const wy3d::Sketch3DExtendSegment forward = graph.pick(pSpline->getId(), pSpline->getPointAt(0.9));
    ASSERT_TRUE(forward.isValid());
    EXPECT_NEAR(forward.startKnot.getParam(), 0.0, 1e-9);
    EXPECT_NEAR(forward.endKnot.getParam(), 1.0, 1e-9);
}

// An ellipse arc is the other periodic curve, and it has to grow at both ends the same way an arc
// does - including reporting a knot behind its start as a parameter above 1.
TEST(Sketch3DExtendGraph, AnEllipseArcGrowsAtBothEnds)
{
    Fixture fixture;
    // A quarter of an ellipse, from 0 to 90 degrees, 10 by 4.
    wy3d::SketchEllipseArc3D* pEllipseArc = nullptr;
    ASSERT_EQ(wy3d::SketchEllipseArc3D::create(fixture.pTrans, wy::Vector3::kZero,
        wy::Vector3(0.0, 0.0, 1.0), wy::Vector3(1.0, 0.0, 0.0), 10.0, 0.4, 0.0, wy3d::PI_2, pEllipseArc),
        wy::ErrorStatus::Ok);
    ASSERT_EQ(fixture.pSketch3D->addEntity(pEllipseArc), wy::ErrorStatus::Ok);

    // One radial segment crossing the ellipse ahead of the end, at 135 degrees, and another behind
    // the start, at -45. Each is kept from touching the sweep itself.
    const auto makeRadial = [&](double polarDeg) {
        const double polar = polarDeg * wy3d::PI / 180.0;
        const double anomaly = wy3d::ellipsePolarAngleToParametricAngle(polar, 10.0, 4.0);
        const wy::Vector3 touch(10.0 * std::cos(anomaly), 4.0 * std::sin(anomaly), 0.0);
        const wy::Vector3 unit = touch * (1.0 / touch.length());
        fixture.makeLine(touch - unit * 2.0, touch + unit * 2.0);
        return touch;
    };
    const wy::Vector3 aheadPnt = makeRadial(135.0);
    const wy::Vector3 behindPnt = makeRadial(-45.0);

    wy3d::Sketch3DExtendGraph graph(fixture.pSketch3D, kTol);
    ASSERT_TRUE(graph.isValid());

    // Hovering near the end grows it forward to 135 degrees: 1 + 45/90.
    const auto pointAt = [&](double polarDeg) {
        const double polar = polarDeg * wy3d::PI / 180.0;
        const double anomaly = wy3d::ellipsePolarAngleToParametricAngle(polar, 10.0, 4.0);
        return wy::Vector3(10.0 * std::cos(anomaly), 4.0 * std::sin(anomaly), 0.0);
    };

    const wy3d::Sketch3DExtendSegment forward = graph.pick(pEllipseArc->getId(), pointAt(80.0));
    ASSERT_TRUE(forward.isValid());
    EXPECT_NEAR(forward.startKnot.getParam(), 0.0, 1e-9);
    EXPECT_NEAR(forward.endKnot.getParam(), 1.5, 1e-6);
    EXPECT_NEAR((forward.endKnot.getPosition() - aheadPnt).length(), 0.0, 1e-6);

    // Hovering near the start grows it back to -45, which wraps forward as 315 degrees: 3.5.
    const wy3d::Sketch3DExtendSegment backward = graph.pick(pEllipseArc->getId(), pointAt(10.0));
    ASSERT_TRUE(backward.isValid());
    EXPECT_NEAR(backward.startKnot.getParam(), 3.5, 1e-6);
    EXPECT_NEAR(backward.endKnot.getParam(), 1.0, 1e-9);
    EXPECT_NEAR((backward.startKnot.getPosition() - behindPnt).length(), 0.0, 1e-6);
}

// Of the curves behind it, the nearest one wins, the same way it does ahead.
TEST(Sketch3DExtendGraph, TheNearestCrossingBehindIsTheOneTaken)
{
    Fixture fixture;
    wy3d::SketchLine3D* pShort = fixture.makeLine(wy::Vector3(20.0, 0.0, 0.0), wy::Vector3(30.0, 0.0, 0.0));
    fixture.makeLine(wy::Vector3(15.0, -5.0, 0.0), wy::Vector3(15.0, 5.0, 0.0));
    fixture.makeLine(wy::Vector3(5.0, -5.0, 0.0), wy::Vector3(5.0, 5.0, 0.0));

    wy3d::Sketch3DExtendGraph graph(fixture.pSketch3D, kTol);
    ASSERT_TRUE(graph.isValid());

    const wy3d::Sketch3DExtendSegment segment = graph.pick(pShort->getId(), wy::Vector3(22.0, 0.0, 0.0));
    ASSERT_TRUE(segment.isValid());
    EXPECT_NEAR(segment.startKnot.getParam(), -0.5, 1e-9);
    EXPECT_NEAR((segment.startKnot.getPosition() - wy::Vector3(15.0, 0.0, 0.0)).length(), 0.0, 1e-9);
}

// With something reachable on both sides the cursor's half of the curve is the only thing deciding
// which end grows.
TEST(Sketch3DExtendGraph, TheCursorHalfChoosesWhichEndGrows)
{
    Fixture fixture;
    wy3d::SketchLine3D* pMiddle = fixture.makeLine(wy::Vector3(10.0, 0.0, 0.0), wy::Vector3(20.0, 0.0, 0.0));
    fixture.makeLine(wy::Vector3(5.0, -5.0, 0.0), wy::Vector3(5.0, 5.0, 0.0));
    fixture.makeLine(wy::Vector3(25.0, -5.0, 0.0), wy::Vector3(25.0, 5.0, 0.0));

    wy3d::Sketch3DExtendGraph graph(fixture.pSketch3D, kTol);
    ASSERT_TRUE(graph.isValid());

    const wy3d::Sketch3DExtendSegment forward = graph.pick(pMiddle->getId(), wy::Vector3(18.0, 0.0, 0.0));
    ASSERT_TRUE(forward.isValid());
    EXPECT_NEAR(forward.startKnot.getParam(), 0.0, 1e-9);
    EXPECT_NEAR(forward.endKnot.getParam(), 1.5, 1e-9);

    // Just past the midpoint is enough to flip it, so the two answers are the two ends and nothing
    // in between is ambiguous.
    const wy3d::Sketch3DExtendSegment backward = graph.pick(pMiddle->getId(), wy::Vector3(12.0, 0.0, 0.0));
    ASSERT_TRUE(backward.isValid());
    EXPECT_NEAR(backward.startKnot.getParam(), -0.5, 1e-9);
    EXPECT_NEAR(backward.endKnot.getParam(), 1.0, 1e-9);
}

// An arc whose sweep runs through the seam is the case the angle bookkeeping is most likely to get
// wrong: 350 to 30 degrees, with a target 20 degrees behind the start.
TEST(Sketch3DExtendGraph, AnArcAcrossTheSeamGrowsBothWays)
{
    Fixture fixture;
    const double deg = wy3d::PI / 180.0;
    wy3d::SketchArc3D* pArc = fixture.makeArc(wy::Vector3::kZero, 5.0, 350.0 * deg, 30.0 * deg);
    ASSERT_NEAR(pArc->getTotalAngle(), 40.0 * deg, 1e-9);

    // Crossing the circle at 330 degrees, 20 behind the start, and at 60 degrees, 30 past the end.
    // Both are kept short so neither meets the arc itself.
    const auto makeRadial = [&](double polarDeg) {
        const double polar = polarDeg * deg;
        const wy::Vector3 touch(5.0 * std::cos(polar), 5.0 * std::sin(polar), 0.0);
        const wy::Vector3 unit(std::cos(polar), std::sin(polar), 0.0);
        fixture.makeLine(touch - unit * 1.5, touch + unit * 1.5);
        return touch;
    };
    const wy::Vector3 behindPnt = makeRadial(330.0);
    const wy::Vector3 aheadPnt = makeRadial(60.0);

    wy3d::Sketch3DExtendGraph graph(fixture.pSketch3D, kTol);
    ASSERT_TRUE(graph.isValid());

    const wy3d::Sketch3DExtendSegment forward = graph.pick(pArc->getId(),
        wy::Vector3(5.0 * std::cos(25.0 * deg), 5.0 * std::sin(25.0 * deg), 0.0));
    ASSERT_TRUE(forward.isValid());
    EXPECT_NEAR(forward.startKnot.getParam(), 0.0, 1e-9);
    EXPECT_NEAR(forward.endKnot.getParam(), 1.0 + 30.0 / 40.0, 1e-6);
    EXPECT_NEAR((forward.endKnot.getPosition() - aheadPnt).length(), 0.0, 1e-6);

    // A wrapped-forward parameter measures the distance *on past the end*, not the one behind the
    // start: 330 degrees is 300 past the end of a 40 degree sweep, so it reads as 1 + 300/40 rather
    // than the 1 + 20/40 the backward distance would suggest.
    const wy3d::Sketch3DExtendSegment backward = graph.pick(pArc->getId(),
        wy::Vector3(5.0 * std::cos(355.0 * deg), 5.0 * std::sin(355.0 * deg), 0.0));
    ASSERT_TRUE(backward.isValid());
    EXPECT_NEAR(backward.startKnot.getParam(), 1.0 + 300.0 / 40.0, 1e-6);
    EXPECT_NEAR(backward.endKnot.getParam(), 1.0, 1e-9);
    EXPECT_NEAR((backward.startKnot.getPosition() - behindPnt).length(), 0.0, 1e-6);
}

// A sweep wider than a half turn is where a naive reading of which of the two stored angles is the
// smaller would flip the two ends.
TEST(Sketch3DExtendGraph, ASweepWiderThanAHalfTurnGrowsBothWays)
{
    Fixture fixture;
    const double deg = wy3d::PI / 180.0;
    // Three quarters of a turn, from 10 to 280 degrees.
    wy3d::SketchArc3D* pArc = fixture.makeArc(wy::Vector3::kZero, 5.0, 10.0 * deg, 280.0 * deg);
    ASSERT_NEAR(pArc->getTotalAngle(), 270.0 * deg, 1e-9);

    const auto makeRadial = [&](double polarDeg) {
        const double polar = polarDeg * deg;
        const wy::Vector3 touch(5.0 * std::cos(polar), 5.0 * std::sin(polar), 0.0);
        const wy::Vector3 unit(std::cos(polar), std::sin(polar), 0.0);
        fixture.makeLine(touch - unit * 1.5, touch + unit * 1.5);
        return touch;
    };
    const wy::Vector3 aheadPnt = makeRadial(310.0);
    const wy::Vector3 behindPnt = makeRadial(350.0);

    wy3d::Sketch3DExtendGraph graph(fixture.pSketch3D, kTol);
    ASSERT_TRUE(graph.isValid());

    const wy3d::Sketch3DExtendSegment forward = graph.pick(pArc->getId(),
        wy::Vector3(5.0 * std::cos(270.0 * deg), 5.0 * std::sin(270.0 * deg), 0.0));
    ASSERT_TRUE(forward.isValid());
    EXPECT_NEAR(forward.endKnot.getParam(), 1.0 + 30.0 / 270.0, 1e-6);
    EXPECT_NEAR((forward.endKnot.getPosition() - aheadPnt).length(), 0.0, 1e-6);

    // 350 degrees is 70 past the end of a 270 degree sweep, so it reads as 1 + 70/270.
    const wy3d::Sketch3DExtendSegment backward = graph.pick(pArc->getId(),
        wy::Vector3(5.0 * std::cos(20.0 * deg), 5.0 * std::sin(20.0 * deg), 0.0));
    ASSERT_TRUE(backward.isValid());
    EXPECT_NEAR(backward.startKnot.getParam(), 1.0 + 70.0 / 270.0, 1e-6);
    EXPECT_NEAR((backward.startKnot.getPosition() - behindPnt).length(), 0.0, 1e-6);
}

// A curve that cannot answer for a point at all has no extension to offer. Its crossings are still
// found - the arc is a full circle to the intersection utility no matter how much of it is left -
// so the refusal has to come from the sentinel parameter being caught downstream rather than from
// there being nothing recorded.
TEST(Sketch3DExtendGraph, ACurveThatCannotReportAParameterIsRefused)
{
    Fixture fixture;
    // A zero sweep: getTotalAngle normalizes it away, so getParamOfArc has nothing to divide by.
    const double deg = wy3d::PI / 180.0;
    wy3d::SketchArc3D* pDegenerate = fixture.makeArc(wy::Vector3::kZero, 5.0, 45.0 * deg, 45.0 * deg);
    ASSERT_NEAR(pDegenerate->getTotalAngle(), 0.0, 1e-12);

    fixture.makeLine(wy::Vector3(3.0, 3.0, 0.0), wy::Vector3(9.0, 9.0, 0.0));

    wy3d::Sketch3DExtendGraph graph(fixture.pSketch3D, kTol);
    ASSERT_TRUE(graph.isValid());

    EXPECT_FALSE(graph.pick(pDegenerate->getId(), wy::Vector3(3.5, 3.5, 0.0)).isValid());
    // The segment itself is what refuses it: a sentinel parameter is not a valid knot, and a
    // segment with an invalid knot is not a valid segment.
    EXPECT_FALSE(wy3d::Sketch3DExtendKnot(wy::Vector3::kZero, DBL_MAX).isValid());
}
