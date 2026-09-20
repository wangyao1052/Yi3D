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

#include <utils/wy3dSketch3DCurveParam.h>
#include <wy3dImpl.h>
#include <utils/wy3dSketch3DFilletAlgo.h>
#include <wy3dMath.h>
#include <wy3dSketchArc3D.h>
#include <wy3dSketchCircle3D.h>
#include <wy3dSketchCurve3D.h>
#include <wy3dSketchEllipse3D.h>
#include <wy3dSketchEllipseArc3D.h>
#include <wy3dSketchLine3D.h>
#include <wy3dSketchSpline3D.h>

#include <cmath>
#include <memory>
#include <vector>

namespace
{
using Algo = wy3d::Sketch3DFilletAlgo;
using Result = Algo::Result;
using Frame = wy3d::Sketch3DFrame;
using Projected = wy3d::Sketch3DProjectedCurve;

using wy3d::Sketch3DCurveParam;

// The tolerance the 2D algorithm is driven with, the same value the 2D fillet command hands it.
const double kTol = wy3d::TOL;

const double kAnalyticTol = 1e-9;

const wy::Vector3 kAxisZ(0.0, 0.0, 1.0);
const wy::Vector3 kAxisX(1.0, 0.0, 0.0);

// A plane that is no coordinate plane, so a frame that only comes out right when it happens to be
// axis-aligned fails here. yDir follows the entities' own convention: normal x xDir.
const wy::Vector3 kTiltedNormal = wy::Vector3(1.0, 1.0, 1.0).normalized();
const wy::Vector3 kTiltedX = wy::Vector3(1.0, -1.0, 0.0).normalized();
const wy::Vector3 kTiltedY = kTiltedNormal.cross(kTiltedX);

// A point of the tilted plane, given its plane coordinates against (kTiltedX, kTiltedY).
wy::Vector3 onTilted(double u, double v)
{
    return kTiltedX * u + kTiltedY * v;
}

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

    wy3d::SketchCircle3D* makeCircle(const wy::Vector3& center, const wy::Vector3& normal,
        const wy::Vector3& xDir, double radius)
    {
        wy3d::SketchCircle3D* pCircle = nullptr;
        EXPECT_EQ(wy3d::SketchCircle3D::create(pTrans, center, normal, xDir, radius, pCircle),
            wy::ErrorStatus::Ok);
        return pCircle;
    }

    wy3d::SketchArc3D* makeArc(const wy::Vector3& center, const wy::Vector3& normal,
        const wy::Vector3& xDir, double radius, double startAngle, double endAngle)
    {
        wy3d::SketchArc3D* pArc = nullptr;
        EXPECT_EQ(wy3d::SketchArc3D::create(pTrans, center, normal, xDir, radius, startAngle, endAngle, pArc),
            wy::ErrorStatus::Ok);
        return pArc;
    }

    wy3d::SketchSpline3D* makeSpline(std::uint32_t degree, const std::vector<wy::Vector3>& controlPoints)
    {
        wy3d::SketchSpline3D* pSpline = nullptr;
        EXPECT_EQ(wy3d::SketchSpline3D::create(pTrans, degree, controlPoints, pSpline), wy::ErrorStatus::Ok);
        return pSpline;
    }

    wy3d::SketchEllipse3D* makeEllipse(const wy::Vector3& center, double majorRadius)
    {
        wy3d::SketchEllipse3D* pEllipse = nullptr;
        EXPECT_EQ(wy3d::SketchEllipse3D::create(pTrans, center, kAxisZ, kAxisX, majorRadius, 0.5, pEllipse),
            wy::ErrorStatus::Ok);
        return pEllipse;
    }
};

// The four properties every frame has to satisfy, whatever curve supplied it. Stated as properties
// rather than as expected numbers so that a frame which is merely rotated or rescaled into place
// cannot pass by coincidence.
void expectOrthonormalRightHanded(const Frame& frame)
{
    EXPECT_NEAR(frame.xDir.length(), 1.0, 1e-12);
    EXPECT_NEAR(frame.yDir.length(), 1.0, 1e-12);
    EXPECT_NEAR(frame.normal.length(), 1.0, 1e-12);
    EXPECT_NEAR(frame.xDir.dot(frame.yDir), 0.0, 1e-12);
    EXPECT_NEAR(frame.xDir.dot(frame.normal), 0.0, 1e-12);
    EXPECT_NEAR(frame.yDir.dot(frame.normal), 0.0, 1e-12);

    // normal == xDir.cross(yDir) is the invariant the angle arithmetic rests on, so it is asserted
    // as an equality rather than checked for being merely perpendicular to both.
    const wy::Vector3 handed = frame.xDir.cross(frame.yDir);
    EXPECT_LE((handed - frame.normal).length(), 1e-12);
}

void expectFrameOfConic(const Frame& frame, const wy::Vector3& center, const wy::Vector3& normal,
    const wy::Vector3& xDir)
{
    EXPECT_LE((frame.origin - center).length(), 1e-12);
    expectOrthonormalRightHanded(frame);
    EXPECT_NEAR(frame.normal.dot(normal.normalized()), 1.0, 1e-12);
    EXPECT_LE((frame.xDir - xDir.normalized()).length(), 1e-12);
}

// A spline whose poles lie in the tilted plane, with a shape that is not symmetric about the plane
// coordinates' axes, so a frame that mixes up xDir and yDir cannot pass.
std::vector<wy::Vector3> tiltedPoles()
{
    return {onTilted(0.0, 0.0), onTilted(3.0, 4.0), onTilted(7.0, -2.0), onTilted(10.0, 0.0)};
}

// A spline running up the plane as a gentle S: nearly straight, so a fillet against it is a corner
// round rather than a fit, but not collinear, so it has a plane of its own to be solved in.
std::vector<wy::Vector3> risingPoles()
{
    return {wy::Vector3(-10.0, -5.0, 0.0), wy::Vector3(-3.0, 1.5, 0.0),
        wy::Vector3(3.0, 8.5, 0.0), wy::Vector3(10.0, 15.0, 0.0)};
}

// The distance from a point to a line's support, unclamped. A fillet is tangent to the support
// rather than to the segment: the 2D algorithm is allowed to put the tangent point past an end and
// extend the curve out to it, so clamping here would report a tangency that is not one.
double distanceToLineSupport(const wy::Vector3& startPnt, const wy::Vector3& endPnt,
    const wy::Vector3& pnt)
{
    const wy::Vector3 dir = endPnt - startPnt;
    const double squaredLength = dir.dot(dir);
    if (squaredLength <= 0.0) return (pnt - startPnt).length();
    return (startPnt + dir * ((pnt - startPnt).dot(dir) / squaredLength) - pnt).length();
}

// The distance from a point to a conic's full circle. Also its own support rather than the arc's
// sweep: an arc's tangent point is on the arc, and asserting that it is would be a second thing
// this helper is not in a position to know.
double distanceToConicSupport(const wy::Vector3& center, double radius, const wy::Vector3& pnt)
{
    return std::fabs((pnt - center).length() - radius);
}

// The distance from a point to a spline, swept and then refined. The sweep brackets the minimum and
// a ternary search brings it down to machine precision, which sampling alone cannot: near a tangency
// the distance varies quadratically, so a sweep fine enough to be sharp would be very fine indeed.
double distanceToSpline(const wy3d::SketchSpline3D* pSpline, const wy::Vector3& pnt)
{
    const int samples = 400;
    double bestT = 0.0;
    double best = DBL_MAX;
    for (int i = 0; i <= samples; ++i)
    {
        const double t = static_cast<double>(i) / samples;
        const double distance = (pSpline->getPointAt(t) - pnt).length();
        if (distance < best)
        {
            best = distance;
            bestT = t;
        }
    }

    double lo = std::max(0.0, bestT - 1.0 / samples);
    double hi = std::min(1.0, bestT + 1.0 / samples);
    for (int i = 0; i < 200; ++i)
    {
        const double step = (hi - lo) / 3.0;
        const double m1 = lo + step;
        const double m2 = hi - step;
        if ((pSpline->getPointAt(m1) - pnt).length() < (pSpline->getPointAt(m2) - pnt).length())
        {
            hi = m2;
        }
        else
        {
            lo = m1;
        }
    }
    return (pSpline->getPointAt((lo + hi) * 0.5) - pnt).length();
}

// The distance from a point to a curve, by whichever of the three readings fits it.
double distanceToCurve(const wy3d::SketchCurve3D* pCurve, const wy::Vector3& pnt)
{
    if (const wy3d::SketchLine3D* pLine = wy3d::SketchLine3D::cast(pCurve))
    {
        return distanceToLineSupport(pLine->getStartPoint(), pLine->getEndPoint(), pnt);
    }
    if (const wy3d::SketchCircle3D* pCircle = wy3d::SketchCircle3D::cast(pCurve))
    {
        return distanceToConicSupport(pCircle->getCenter(), pCircle->getRadius(), pnt);
    }
    if (const wy3d::SketchArc3D* pArc = wy3d::SketchArc3D::cast(pCurve))
    {
        return distanceToConicSupport(pArc->getCenter(), pArc->getRadius(), pnt);
    }
    return distanceToSpline(wy3d::SketchSpline3D::cast(pCurve), pnt);
}

// The property that makes a fillet a fillet: its centre is exactly R from both curves. Every pair
// below asserts this, so the tests are about the geometry rather than about a remembered answer.
void expectTangentToBoth(const wy3d::Sketch3DFilletData& data,
    const wy3d::SketchCurve3D* pCurve1st, const wy3d::SketchCurve3D* pCurve2nd, double tol = 1e-9)
{
    EXPECT_NEAR(distanceToCurve(pCurve1st, data.filletCenter), data.filletRadius, tol);
    EXPECT_NEAR(distanceToCurve(pCurve2nd, data.filletCenter), data.filletRadius, tol);
}
} // namespace

// ---------------------------------------------------------------------------------------------
// The frame
// ---------------------------------------------------------------------------------------------

TEST(Sketch3DFilletAlgo, FrameOfTwoLinesInTheXYPlane)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine1st = fixture.makeLine(wy::Vector3(2.0, 3.0, 0.0), wy::Vector3(12.0, 3.0, 0.0));
    wy3d::SketchLine3D* pLine2nd = fixture.makeLine(wy::Vector3(2.0, 3.0, 0.0), wy::Vector3(2.0, 13.0, 0.0));

    Frame frame;
    ASSERT_EQ(Algo::computeFrame(pLine1st, pLine2nd, frame), Result::Ok);
    expectOrthonormalRightHanded(frame);

    // Two lines have no plane of their own, so the frame is anchored on the first one: its start
    // for the origin, its direction for xDir, and the cross product across for the normal.
    EXPECT_LE((frame.origin - wy::Vector3(2.0, 3.0, 0.0)).length(), 1e-12);
    EXPECT_LE((frame.xDir - kAxisX).length(), 1e-12);
    EXPECT_LE((frame.normal - kAxisZ).length(), 1e-12);

    // Both lines read back as they were laid out, in the frame's own plane coordinates.
    EXPECT_LE((frame.to2D(pLine1st->getEndPoint()) - wy::Vector2(10.0, 0.0)).length(), 1e-12);
    EXPECT_LE((frame.to2D(pLine2nd->getEndPoint()) - wy::Vector2(0.0, 10.0)).length(), 1e-12);

    // A consequence worth pinning down rather than rediscovering: with the normal taken as
    // d1 x d2, the second line always lands in the upper half plane. Nothing downstream depends on
    // that - every 2D branch reads the side it needs off the pick - but a change here would move
    // which of two mirror-image solutions comes back, and that is worth a failing test.
    EXPECT_GT(frame.to2D(pLine2nd->getEndPoint()).y(), 0.0);
}

TEST(Sketch3DFilletAlgo, FrameOfATiltedLinePair)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine1st = fixture.makeLine(onTilted(0.0, 0.0), onTilted(10.0, 0.0));
    wy3d::SketchLine3D* pLine2nd = fixture.makeLine(onTilted(0.0, 0.0), onTilted(0.0, 10.0));

    Frame frame;
    ASSERT_EQ(Algo::computeFrame(pLine1st, pLine2nd, frame), Result::Ok);
    expectOrthonormalRightHanded(frame);

    // The frame's normal has to be the plane's, not some axis the test happened to line up with.
    EXPECT_LE((frame.normal - kTiltedNormal).length(), 1e-12);

    // Given that, the pair reads back in plane coordinates exactly as it was built, which is what
    // makes the 2D algorithm's inputs meaningful.
    EXPECT_LE((frame.to2D(pLine1st->getEndPoint()) - wy::Vector2(10.0, 0.0)).length(), 1e-12);
    EXPECT_LE((frame.to2D(pLine2nd->getEndPoint()) - wy::Vector2(0.0, 10.0)).length(), 1e-12);
}

TEST(Sketch3DFilletAlgo, FrameOfALineAndAConicComesFromTheConic)
{
    Fixture fixture;
    const wy::Vector3 center = onTilted(5.0, 5.0);
    wy3d::SketchCircle3D* pCircle = fixture.makeCircle(center, kTiltedNormal, kTiltedX, 4.0);
    wy3d::SketchLine3D* pLine = fixture.makeLine(onTilted(-10.0, 5.0), onTilted(10.0, 5.0));

    // Either order, because the conic is the one that knows a plane and the frame must not depend
    // on which curve the caller happened to pick first.
    for (const bool circleFirst : {true, false})
    {
        Frame frame;
        ASSERT_EQ(circleFirst ? Algo::computeFrame(pCircle, pLine, frame)
            : Algo::computeFrame(pLine, pCircle, frame), Result::Ok);
        expectFrameOfConic(frame, center, kTiltedNormal, kTiltedX);
    }
}

TEST(Sketch3DFilletAlgo, FrameOfASplineComesFromItsPoles)
{
    Fixture fixture;
    wy3d::SketchSpline3D* pSpline = fixture.makeSpline(3, tiltedPoles());
    wy3d::SketchLine3D* pLine = fixture.makeLine(onTilted(-5.0, 1.0), onTilted(15.0, 1.0));

    Frame frame;
    ASSERT_EQ(Algo::computeFrame(pLine, pSpline, frame), Result::Ok);
    expectOrthonormalRightHanded(frame);

    // The plane is the poles' own: their centroid for the origin, their normal for the normal.
    EXPECT_LE((frame.origin - onTilted(5.0, 0.5)).length(), 1e-12);
    // The normal is the poles' plane's, up to its sign: planes are undirected and the poles give no
    // reason to prefer one.
    EXPECT_GE(std::fabs(frame.normal.dot(kTiltedNormal)), 1.0 - 1e-12);

    // xDir is a direction in the plane rather than a re-creation of the sketch's axes, so what is
    // asserted is that the poles all land on the plane - and, since the plane's normal is known,
    // that the frame really is that plane and not a rotation of it.
    for (const wy::Vector3& pole : tiltedPoles())
    {
        EXPECT_NEAR((pole - frame.origin).dot(frame.normal), 0.0, 1e-12);
    }
}

// ---------------------------------------------------------------------------------------------
// The projection
// ---------------------------------------------------------------------------------------------

TEST(Sketch3DFilletAlgo, TheFrameRoundTripsThroughBothDirections)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine1st = fixture.makeLine(onTilted(0.0, 0.0), onTilted(10.0, 0.0));
    wy3d::SketchLine3D* pLine2nd = fixture.makeLine(onTilted(0.0, 0.0), onTilted(0.0, 10.0));

    Frame frame;
    ASSERT_EQ(Algo::computeFrame(pLine1st, pLine2nd, frame), Result::Ok);

    // to2D(to3D(v)) == v, for arbitrary plane coordinates and not only for points that started on
    // the curve.
    const std::vector<wy::Vector2> planePnts = {
        wy::Vector2(0.0, 0.0), wy::Vector2(1.0, 0.0), wy::Vector2(0.0, 1.0),
        wy::Vector2(-7.5, 3.25), wy::Vector2(1e3, -1e-3)};
    for (const wy::Vector2& pnt : planePnts)
    {
        const wy::Vector2 roundTrip = frame.to2D(frame.to3D(pnt));
        EXPECT_LE((roundTrip - pnt).length(), 1e-12);
    }

    // The same is an isometry on the plane: distances are preserved both ways.
    for (const wy::Vector2& a : planePnts)
    {
        for (const wy::Vector2& b : planePnts)
        {
            const double flat = (a - b).length();
            const double spatial = (frame.to3D(a) - frame.to3D(b)).length();
            EXPECT_NEAR(flat, spatial, 1e-12);
        }
    }
}

// Every entity type survives point -> plane -> entity-parameter. This is the property the whole
// design leans on: a parameter handed to the 2D algorithm comes back meaning the same place.
TEST(Sketch3DFilletAlgo, ParametersSurviveTheProjectionForEveryCurveType)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine1st = fixture.makeLine(onTilted(0.0, 0.0), onTilted(10.0, 0.0));
    wy3d::SketchLine3D* pLine2nd = fixture.makeLine(onTilted(0.0, 0.0), onTilted(0.0, 10.0));

    Frame frame;
    ASSERT_EQ(Algo::computeFrame(pLine1st, pLine2nd, frame), Result::Ok);

    // The frame is the tilted one, so a frame that silently assumed the XY plane fails here.
    wy3d::SketchLine3D* pLine = fixture.makeLine(onTilted(0.0, 2.0), onTilted(10.0, 2.0));
    wy3d::SketchCircle3D* pCircle = fixture.makeCircle(onTilted(5.0, 5.0), kTiltedNormal, kTiltedX, 4.0);
    wy3d::SketchArc3D* pArc = fixture.makeArc(onTilted(5.0, 5.0), kTiltedNormal, kTiltedX, 4.0, 0.3, 2.1);
    wy3d::SketchSpline3D* pSpline = fixture.makeSpline(3, tiltedPoles());

    const std::vector<double> ts = {0.0, 0.25, 0.5, 0.75, 1.0};
    for (const double t : ts)
    {
        const wy::Vector3 onCurve = frame.to3D(frame.to2D(pLine->getPointAt(t)));
        EXPECT_NEAR(Sketch3DCurveParam::getParamOfLine(pLine, onCurve), t, kAnalyticTol);

        const wy::Vector3 onCircle = frame.to3D(frame.to2D(pCircle->getPointAt(t)));
        EXPECT_NEAR(Sketch3DCurveParam::getParamOfCircle(pCircle, onCircle), t, kAnalyticTol);

        const wy::Vector3 onArc = frame.to3D(frame.to2D(pArc->getPointAt(t)));
        EXPECT_NEAR(Sketch3DCurveParam::getParamOfArc(pArc, onArc), t, kAnalyticTol);

        const wy::Vector3 onSpline = frame.to3D(frame.to2D(pSpline->getPointAt(t)));
        EXPECT_NEAR(Sketch3DCurveParam::getParamOfSpline(pSpline, onSpline), t, 1e-7);
    }
}

// The projected curve has to agree with the entity point for point, because the 2D algorithm reads
// the projection and the command applies the result to the entity. Every assertion here is that
// agreement, curve type by curve type.
TEST(Sketch3DFilletAlgo, TheProjectedCurveAgreesWithTheEntity)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine1st = fixture.makeLine(onTilted(0.0, 0.0), onTilted(10.0, 0.0));
    wy3d::SketchLine3D* pLine2nd = fixture.makeLine(onTilted(0.0, 0.0), onTilted(0.0, 10.0));

    Frame frame;
    ASSERT_EQ(Algo::computeFrame(pLine1st, pLine2nd, frame), Result::Ok);

    const std::vector<double> ts = {0.0, 0.2, 0.5, 0.9, 1.0};

    {
        wy3d::SketchLine3D* pLine = fixture.makeLine(onTilted(-3.0, 2.0), onTilted(7.0, 2.0));
        Projected projected;
        ASSERT_EQ(Algo::projectCurve(pLine, frame, projected), Result::Ok);
        EXPECT_EQ(projected.kind, Projected::Kind::Line);
        for (const double t : ts)
        {
            const wy::Vector2 flat = projected.lineStart
                + (projected.lineEnd - projected.lineStart) * t;
            EXPECT_LE((flat - frame.to2D(pLine->getPointAt(t))).length(), 1e-12);
        }
    }

    {
        wy3d::SketchCircle3D* pCircle = fixture.makeCircle(onTilted(5.0, 5.0), kTiltedNormal, kTiltedX, 4.0);
        Projected projected;
        ASSERT_EQ(Algo::projectCurve(pCircle, frame, projected), Result::Ok);
        EXPECT_EQ(projected.kind, Projected::Kind::Circle);
        EXPECT_NEAR(projected.radius, 4.0, 1e-12);
        EXPECT_LE((projected.center - frame.to2D(pCircle->getCenter())).length(), 1e-12);
        for (const double t : ts)
        {
            const double angle = wy3d::TWO_PI * t;
            const wy::Vector2 flat = projected.center
                + wy::Vector2(std::cos(angle), std::sin(angle)) * projected.radius;
            EXPECT_LE((flat - frame.to2D(pCircle->getPointAt(t))).length(), 1e-12);
        }
    }

    // The arc's own xDir is turned 37 degrees away from the frame's, so the shift this class applies
    // has something to do: with the sign or the size of it wrong, every point below moves.
    {
        const double turned = 37.0 * wy3d::PI / 180.0;
        const wy::Vector3 turnedX = kTiltedX * std::cos(turned) + kTiltedY * std::sin(turned);
        wy3d::SketchArc3D* pArc = fixture.makeArc(onTilted(5.0, 5.0), kTiltedNormal, turnedX, 4.0, 0.4, 2.6);

        Frame arcFrame;
        ASSERT_EQ(Algo::computeFrame(pArc, pLine1st, arcFrame), Result::Ok);
        // The frame is the arc's own, so its xDir is the arc's and no shift is needed.
        EXPECT_LE((arcFrame.xDir - turnedX).length(), 1e-12);

        Projected projected;
        ASSERT_EQ(Algo::projectCurve(pArc, arcFrame, projected), Result::Ok);
        EXPECT_EQ(projected.kind, Projected::Kind::Arc);
        EXPECT_NEAR(arcFrame.deltaOf(turnedX), 0.0, 1e-12);

        // And against the tilted line pair's frame, which does need one.
        Projected shifted;
        ASSERT_EQ(Algo::projectCurve(pArc, frame, shifted), Result::Ok);
        EXPECT_NEAR(frame.deltaOf(turnedX), turned, 1e-12);

        for (const double t : ts)
        {
            const double angle = shifted.startAngle + (shifted.endAngle - shifted.startAngle) * t;
            const wy::Vector2 flat = shifted.center
                + wy::Vector2(std::cos(angle), std::sin(angle)) * shifted.radius;
            EXPECT_LE((flat - frame.to2D(pArc->getPointAt(t))).length(), 1e-12);
        }
    }

    // The spline's knots are the originals, so its parameter range is the originals' too, which is
    // what lets a normalized [0,1] parameter cross over unchanged.
    {
        wy3d::SketchSpline3D* pSpline = fixture.makeSpline(3, tiltedPoles());
        Projected projected;
        ASSERT_EQ(Algo::projectCurve(pSpline, frame, projected), Result::Ok);
        EXPECT_EQ(projected.kind, Projected::Kind::Spline);
        ASSERT_FALSE(projected.pBSpline.IsNull());

        const Handle(Geom_BSplineCurve) pSource = pSpline->getOccSpline();
        EXPECT_NEAR(projected.pBSpline->FirstParameter(), pSource->FirstParameter(), 1e-12);
        EXPECT_NEAR(projected.pBSpline->LastParameter(), pSource->LastParameter(), 1e-12);
        EXPECT_EQ(projected.pBSpline->Degree(), pSource->Degree());
        EXPECT_EQ(projected.pBSpline->NbPoles(), pSource->NbPoles());
        EXPECT_EQ(projected.pBSpline->NbKnots(), pSource->NbKnots());

        const double first = pSource->FirstParameter();
        const double last = pSource->LastParameter();
        for (const double t : ts)
        {
            const gp_Pnt2d flat = projected.pBSpline->Value(first + (last - first) * t);
            const wy::Vector2 expected = frame.to2D(pSpline->getPointAt(t));
            EXPECT_LE((wy::Vector2(flat.X(), flat.Y()) - expected).length(), 1e-9);
        }
    }
}

// ---------------------------------------------------------------------------------------------
// Coplanarity
// ---------------------------------------------------------------------------------------------

TEST(Sketch3DFilletAlgo, SkewLinesAreRefused)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine1st = fixture.makeLine(wy::Vector3(0.0, 0.0, 0.0), wy::Vector3(10.0, 0.0, 0.0));
    wy3d::SketchLine3D* pLine2nd = fixture.makeLine(wy::Vector3(5.0, -5.0, 1e-3), wy::Vector3(5.0, 5.0, 1e-3));

    Frame frame;
    EXPECT_EQ(Algo::computeFrame(pLine1st, pLine2nd, frame), Result::NotCoplanar);
}

// Parallel lines are coplanar whatever their offset, so they must not be refused as though the
// plane were missing. Two of them offset by nothing at all are the same line, and there the frame
// really is undefined.
TEST(Sketch3DFilletAlgo, ParallelLinesGetAFrameAndCollinearOnesDoNot)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine = fixture.makeLine(wy::Vector3(0.0, 0.0, 0.0), wy::Vector3(10.0, 0.0, 0.0));
    wy3d::SketchLine3D* pOffset = fixture.makeLine(wy::Vector3(0.0, 5.0, 3.0), wy::Vector3(10.0, 5.0, 3.0));
    wy3d::SketchLine3D* pCollinear = fixture.makeLine(wy::Vector3(20.0, 0.0, 0.0), wy::Vector3(30.0, 0.0, 0.0));

    Frame frame;
    ASSERT_EQ(Algo::computeFrame(pLine, pOffset, frame), Result::Ok);
    expectOrthonormalRightHanded(frame);

    // The plane holding both is the one the offset between them pins down, so the first line runs
    // along the frame's x axis from its origin and the second line's start sits straight up the
    // frame's y axis, at a distance equal to the offset itself.
    const wy::Vector2 firstStart = frame.to2D(pLine->getStartPoint());
    EXPECT_NEAR(firstStart.x(), 0.0, 1e-12);
    EXPECT_NEAR(firstStart.y(), 0.0, 1e-12);

    const double offsetLength = (pOffset->getStartPoint() - pLine->getStartPoint()).length();
    const wy::Vector2 secondStart = frame.to2D(pOffset->getStartPoint());
    EXPECT_NEAR(secondStart.x(), 0.0, 1e-12);
    EXPECT_NEAR(secondStart.y(), offsetLength, 1e-12);

    EXPECT_EQ(Algo::computeFrame(pLine, pCollinear, frame), Result::DegenerateInput);
}

TEST(Sketch3DFilletAlgo, ALineCrossingAConicsPlaneIsRefused)
{
    Fixture fixture;
    // Thirty degrees through the plane and long enough that both ends are well clear of it. The
    // plane is judged from the two ends, so this is refused on the strength of them.
    const double slope = std::tan(30.0 * wy3d::PI / 180.0);
    wy3d::SketchCircle3D* pCircle = fixture.makeCircle(onTilted(0.0, 0.0), kTiltedNormal, kTiltedX, 4.0);
    wy3d::SketchLine3D* pLine = fixture.makeLine(
        onTilted(-10.0, 0.0) - kTiltedNormal * (5.0 * slope),
        onTilted(10.0, 0.0) + kTiltedNormal * (5.0 * slope));

    Frame frame;
    EXPECT_EQ(Algo::computeFrame(pCircle, pLine, frame), Result::NotCoplanar);
}

TEST(Sketch3DFilletAlgo, ConicsInOnePlaneAreAcceptedHoweverTheyAreOffset)
{
    Fixture fixture;
    wy3d::SketchCircle3D* pCircle1st = fixture.makeCircle(onTilted(0.0, 0.0), kTiltedNormal, kTiltedX, 4.0);
    // A different centre and a different xDir, in the same plane: neither is a reason to refuse.
    wy3d::SketchCircle3D* pCircle2nd = fixture.makeCircle(onTilted(20.0, 5.0), kTiltedNormal, kTiltedY, 3.0);

    Frame frame;
    ASSERT_EQ(Algo::computeFrame(pCircle1st, pCircle2nd, frame), Result::Ok);
    expectFrameOfConic(frame, onTilted(0.0, 0.0), kTiltedNormal, kTiltedX);

    // Written this way round the two xDirs are a quarter turn apart, which the shift is expected to
    // absorb rather than trip over.
    EXPECT_NEAR(frame.deltaOf(kTiltedY), wy3d::PI * 0.5, 1e-12);
}

TEST(Sketch3DFilletAlgo, ConicsInDifferentPlanesAreRefused)
{
    Fixture fixture;
    wy3d::SketchCircle3D* pCircle = fixture.makeCircle(onTilted(0.0, 0.0), kTiltedNormal, kTiltedX, 4.0);

    // Turned out of the plane, so the normals differ. A mere shift along the normal would leave
    // the second test below as the one doing the work; a turn exercises the first.
    const wy::Vector3 turnedNormal = wy::Vector3(1.0, 1.0, 0.0).normalized();
    const wy::Vector3 turnedX = wy::Vector3(0.0, 0.0, 1.0);
    wy3d::SketchCircle3D* pTurned = fixture.makeCircle(onTilted(0.0, 0.0), turnedNormal, turnedX, 4.0);
    Frame frame;
    EXPECT_EQ(Algo::computeFrame(pCircle, pTurned, frame), Result::NotCoplanar);

    // Same plane direction, centre lifted clear of it.
    wy3d::SketchCircle3D* pLifted = fixture.makeCircle(
        onTilted(20.0, 5.0) + kTiltedNormal * 1e-3, kTiltedNormal, kTiltedX, 3.0);
    EXPECT_EQ(Algo::computeFrame(pCircle, pLifted, frame), Result::NotCoplanar);

    // Lifted by an amount under the tolerance, which is the same plane as far as a fillet cares.
    wy3d::SketchCircle3D* pNearly = fixture.makeCircle(
        onTilted(20.0, 5.0) + kTiltedNormal * 1e-9, kTiltedNormal, kTiltedX, 3.0);
    EXPECT_EQ(Algo::computeFrame(pCircle, pNearly, frame), Result::Ok);
}

TEST(Sketch3DFilletAlgo, SplinesAreJudgedByTheirPoles)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine = fixture.makeLine(onTilted(-5.0, 1.0), onTilted(15.0, 1.0));

    wy3d::SketchSpline3D* pInPlane = fixture.makeSpline(3, tiltedPoles());
    Frame frame;
    EXPECT_EQ(Algo::computeFrame(pLine, pInPlane, frame), Result::Ok);

    // One pole lifted out of the plane. A B-spline stays inside the hull of its poles, so that is
    // enough to know the curve itself leaves - no sampling needed, and no false acceptance.
    std::vector<wy::Vector3> liftedPoles = tiltedPoles();
    liftedPoles[1] += kTiltedNormal * 1e-3;
    wy3d::SketchSpline3D* pLifted = fixture.makeSpline(3, liftedPoles);
    EXPECT_EQ(Algo::computeFrame(pLine, pLifted, frame), Result::NotCoplanar);

    // Poles in a plane, but not this one.
    std::vector<wy::Vector3> otherPoles = {wy::Vector3(0.0, 0.0, 0.0), wy::Vector3(3.0, 0.0, 4.0),
        wy::Vector3(7.0, 0.0, -2.0), wy::Vector3(10.0, 0.0, 0.0)};
    EXPECT_EQ(Algo::computeFrame(pLine, fixture.makeSpline(3, otherPoles), frame), Result::NotCoplanar);
}

// A spline all of whose poles are in a line lies in every plane through that line, so there is no
// plane to solve in and the frame has nothing to report a result against. The 2D algorithm has no
// complaint about such a curve - it is a straight segment in all but name - so this is a refusal on
// this side of the boundary, made deliberately and worth stating out loud.
TEST(Sketch3DFilletAlgo, ACollinearSplineSuppliesNoPlane)
{
    Fixture fixture;
    const std::vector<wy::Vector3> collinearPoles = {wy::Vector3(0.0, 0.0, 0.0), wy::Vector3(3.0, 3.0, 3.0),
        wy::Vector3(7.0, 7.0, 7.0), wy::Vector3(10.0, 10.0, 10.0)};
    wy3d::SketchSpline3D* pCollinear = fixture.makeSpline(3, collinearPoles);
    wy3d::SketchLine3D* pLine = fixture.makeLine(wy::Vector3(0.0, 10.0, 0.0), wy::Vector3(10.0, 10.0, 0.0));

    Frame frame;
    EXPECT_EQ(Algo::computeFrame(pLine, pCollinear, frame), Result::DegenerateInput);
    // Either order, as everywhere else: which curve supplied the frame may not depend on slot order.
    EXPECT_EQ(Algo::computeFrame(pCollinear, pLine, frame), Result::DegenerateInput);
}

// ---------------------------------------------------------------------------------------------
// Refusals that do not depend on the plane at all
// ---------------------------------------------------------------------------------------------

TEST(Sketch3DFilletAlgo, EllipsesAndMissingCurvesAreRefusedBeforeAnythingElse)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine = fixture.makeLine(wy::Vector3::kZero, wy::Vector3(10.0, 0.0, 0.0));
    wy3d::SketchEllipse3D* pEllipse = fixture.makeEllipse(wy::Vector3(5.0, 5.0, 0.0), 4.0);
    wy3d::SketchCircle3D* pCircle = fixture.makeCircle(wy::Vector3(5.0, 5.0, 0.0), kAxisZ, kAxisX, 4.0);

    Frame frame;
    EXPECT_EQ(Algo::computeFrame(pLine, pEllipse, frame), Result::UnsupportedPair);
    EXPECT_EQ(Algo::computeFrame(pEllipse, pCircle, frame), Result::UnsupportedPair);
    EXPECT_EQ(Algo::computeFrame(nullptr, pLine, frame), Result::DegenerateInput);
    EXPECT_EQ(Algo::computeFrame(pLine, nullptr, frame), Result::DegenerateInput);
    EXPECT_EQ(Algo::computeFrame(pLine, pLine, frame), Result::SameEntity);

    Projected projected;
    EXPECT_EQ(Algo::projectCurve(pEllipse, frame, projected), Result::UnsupportedPair);
    EXPECT_EQ(Algo::projectCurve(nullptr, frame, projected), Result::DegenerateInput);
}

// ---------------------------------------------------------------------------------------------
// The ten pairs. Each asserts that the answer is a fillet - the centre is exactly R from both
// curves - rather than a remembered centre, so a branch that returns the other of the two mirror
// solutions still passes while one that returns something not tangent does not.
// ---------------------------------------------------------------------------------------------

TEST(Sketch3DFilletAlgo, LineAgainstLine)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine1st = fixture.makeLine(wy::Vector3::kZero, wy::Vector3(10.0, 0.0, 0.0));
    wy3d::SketchLine3D* pLine2nd = fixture.makeLine(wy::Vector3::kZero, wy::Vector3(0.0, 10.0, 0.0));

    wy3d::Sketch3DFilletData data;
    ASSERT_EQ(Algo::fillet(2.0, kTol, pLine1st, wy::Vector3(5.0, 0.0, 0.0),
        pLine2nd, wy::Vector3(0.0, 5.0, 0.0), data), Result::Ok);

    expectTangentToBoth(data, pLine1st, pLine2nd);
    EXPECT_NEAR(data.filletRadius, 2.0, 1e-12);
    // The corner is the origin and both picks lie away from it, so each curve keeps its far half.
    EXPECT_NEAR(data.startParam1st, 0.2, 1e-12);
    EXPECT_NEAR(data.endParam1st, 1.0, 1e-12);
    EXPECT_NEAR(data.startParam2nd, 0.2, 1e-12);
    EXPECT_NEAR(data.endParam2nd, 1.0, 1e-12);
}

TEST(Sketch3DFilletAlgo, LineAgainstCircle)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine = fixture.makeLine(wy::Vector3(-10.0, 0.0, 0.0), wy::Vector3(10.0, 0.0, 0.0));
    wy3d::SketchCircle3D* pCircle = fixture.makeCircle(wy::Vector3(0.0, 5.0, 0.0), kAxisZ, kAxisX, 3.0);

    wy3d::Sketch3DFilletData data;
    ASSERT_EQ(Algo::fillet(2.0, kTol, pLine, wy::Vector3(5.0, 0.0, 0.0),
        pCircle, wy::Vector3(0.0, 8.0, 0.0), data), Result::Ok);

    expectTangentToBoth(data, pLine, pCircle);
    // The line is the pair's first curve and is trimmed: the pick sits past the tangency, so the
    // line keeps the far half and the tangency is where the fillet is tangent to it. What is checked
    // is the tangency the parameters claim, not just that some range came back.
    EXPECT_NEAR(data.startParam1st, 0.7, 1e-6);
    EXPECT_NEAR(data.endParam1st, 1.0, 1e-12);
    const double tangencyParam = (4.0 - (-10.0)) / 20.0;
    EXPECT_NEAR(data.startParam1st, tangencyParam, 1e-9);
    EXPECT_LE((pLine->getPointAt(data.startParam1st) - data.filletCenter).length() - data.filletRadius,
        1e-9);
    // The circle is the second curve, and is never trimmed: one tangent point does not split it, and
    // cutting a full turn out of one would leave an arc of nothing.
    EXPECT_DOUBLE_EQ(data.startParam2nd, 0.0);
    EXPECT_DOUBLE_EQ(data.endParam2nd, 1.0);
}

TEST(Sketch3DFilletAlgo, LineAgainstArc)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine = fixture.makeLine(wy::Vector3(-10.0, -1.0, 0.0), wy::Vector3(10.0, -1.0, 0.0));
    wy3d::SketchArc3D* pArc = fixture.makeArc(wy::Vector3(0.0, 5.0, 0.0), kAxisZ, kAxisX, 3.0,
        wy3d::PI, wy3d::TWO_PI);

    wy3d::Sketch3DFilletData data;
    ASSERT_EQ(Algo::fillet(2.0, kTol, pLine, wy::Vector3(5.0, -1.0, 0.0),
        pArc, wy::Vector3(1.8, 2.6, 0.0), data), Result::Ok);

    expectTangentToBoth(data, pLine, pArc);
}

TEST(Sketch3DFilletAlgo, LineAgainstSpline)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine = fixture.makeLine(wy::Vector3(-10.0, 7.0, 0.0), wy::Vector3(10.0, 7.0, 0.0));
    wy3d::SketchSpline3D* pSpline = fixture.makeSpline(3, risingPoles());

    wy3d::Sketch3DFilletData data;
    ASSERT_EQ(Algo::fillet(1.0, kTol, pLine, wy::Vector3(5.0, 7.0, 0.0),
        pSpline, wy::Vector3(2.0, 9.0, 0.0), data), Result::Ok);

    expectTangentToBoth(data, pLine, pSpline, 1e-7);
}

TEST(Sketch3DFilletAlgo, CircleAgainstCircle)
{
    Fixture fixture;
    wy3d::SketchCircle3D* pCircle1st = fixture.makeCircle(wy::Vector3::kZero, kAxisZ, kAxisX, 3.0);
    wy3d::SketchCircle3D* pCircle2nd = fixture.makeCircle(wy::Vector3(6.0, 0.0, 0.0), kAxisZ, kAxisX, 2.0);

    wy3d::Sketch3DFilletData data;
    ASSERT_EQ(Algo::fillet(1.5, kTol, pCircle1st, wy::Vector3(0.0, 3.0, 0.0),
        pCircle2nd, wy::Vector3(6.0, 2.0, 0.0), data), Result::Ok);

    expectTangentToBoth(data, pCircle1st, pCircle2nd);
    // Neither circle is trimmed.
    EXPECT_DOUBLE_EQ(data.startParam1st, 0.0);
    EXPECT_DOUBLE_EQ(data.endParam1st, 1.0);
    EXPECT_DOUBLE_EQ(data.startParam2nd, 0.0);
    EXPECT_DOUBLE_EQ(data.endParam2nd, 1.0);
}

TEST(Sketch3DFilletAlgo, CircleAgainstArc)
{
    Fixture fixture;
    wy3d::SketchCircle3D* pCircle = fixture.makeCircle(wy::Vector3::kZero, kAxisZ, kAxisX, 3.0);
    wy3d::SketchArc3D* pArc = fixture.makeArc(wy::Vector3(6.0, 0.0, 0.0), kAxisZ, kAxisX, 2.0, 0.0, wy3d::PI);

    wy3d::Sketch3DFilletData data;
    ASSERT_EQ(Algo::fillet(1.5, kTol, pCircle, wy::Vector3(0.0, 3.0, 0.0),
        pArc, wy::Vector3(4.7, 1.5, 0.0), data), Result::Ok);

    expectTangentToBoth(data, pCircle, pArc);
    // The circle is the pair's first curve and is not trimmed; the arc is the second and is, since
    // a fillet against an arc genuinely does cut it short.
    EXPECT_DOUBLE_EQ(data.startParam1st, 0.0);
    EXPECT_DOUBLE_EQ(data.endParam1st, 1.0);
    EXPECT_GT(data.endParam2nd, 0.0);
    EXPECT_LT(data.endParam2nd, 1.0);
}

TEST(Sketch3DFilletAlgo, CircleAgainstSpline)
{
    Fixture fixture;
    wy3d::SketchCircle3D* pCircle = fixture.makeCircle(wy::Vector3(0.0, 2.0, 0.0), kAxisZ, kAxisX, 5.0);
    wy3d::SketchSpline3D* pSpline = fixture.makeSpline(3, risingPoles());

    wy3d::Sketch3DFilletData data;
    ASSERT_EQ(Algo::fillet(1.0, kTol, pCircle, wy::Vector3(-4.7, 0.3, 0.0),
        pSpline, wy::Vector3(-2.0, 3.0, 0.0), data), Result::Ok);

    expectTangentToBoth(data, pCircle, pSpline, 1e-7);
    EXPECT_DOUBLE_EQ(data.startParam1st, 0.0);
    EXPECT_DOUBLE_EQ(data.endParam1st, 1.0);
}

TEST(Sketch3DFilletAlgo, ArcAgainstArc)
{
    Fixture fixture;
    wy3d::SketchArc3D* pArc1st = fixture.makeArc(wy::Vector3::kZero, kAxisZ, kAxisX, 3.0, 0.0, wy3d::PI);
    wy3d::SketchArc3D* pArc2nd = fixture.makeArc(wy::Vector3(10.0, 0.0, 0.0), kAxisZ, kAxisX, 3.0, 0.0, wy3d::PI);

    wy3d::Sketch3DFilletData data;
    ASSERT_EQ(Algo::fillet(2.5, kTol, pArc1st, wy::Vector3(2.0, 2.0, 0.0),
        pArc2nd, wy::Vector3(8.0, 2.0, 0.0), data), Result::Ok);

    expectTangentToBoth(data, pArc1st, pArc2nd);
}

TEST(Sketch3DFilletAlgo, ArcAgainstSpline)
{
    Fixture fixture;
    wy3d::SketchArc3D* pArc = fixture.makeArc(wy::Vector3(0.0, 2.0, 0.0), kAxisZ, kAxisX, 5.0,
        wy3d::PI, wy3d::TWO_PI);
    wy3d::SketchSpline3D* pSpline = fixture.makeSpline(3, risingPoles());

    wy3d::Sketch3DFilletData data;
    ASSERT_EQ(Algo::fillet(1.0, kTol, pArc, wy::Vector3(-4.7, 0.3, 0.0),
        pSpline, wy::Vector3(-2.0, 3.0, 0.0), data), Result::Ok);

    expectTangentToBoth(data, pArc, pSpline, 1e-7);
}

TEST(Sketch3DFilletAlgo, SplineAgainstSpline)
{
    Fixture fixture;
    // The same shape reflected, so the two cross and present a corner on each arm to round.
    const std::vector<wy::Vector3> flippedPoles = {wy::Vector3(-10.0, 15.0, 0.0), wy::Vector3(-3.0, 8.5, 0.0),
        wy::Vector3(3.0, 1.5, 0.0), wy::Vector3(10.0, -5.0, 0.0)};
    wy3d::SketchSpline3D* pSpline1st = fixture.makeSpline(3, risingPoles());
    wy3d::SketchSpline3D* pSpline2nd = fixture.makeSpline(3, flippedPoles);

    wy3d::Sketch3DFilletData data;
    ASSERT_EQ(Algo::fillet(1.0, kTol, pSpline1st, wy::Vector3(-5.0, 1.0, 0.0),
        pSpline2nd, wy::Vector3(-5.0, 9.0, 0.0), data), Result::Ok);

    expectTangentToBoth(data, pSpline1st, pSpline2nd, 1e-7);
    EXPECT_NEAR(data.filletRadius, 1.0, 1e-12);
}

// ---------------------------------------------------------------------------------------------
// Order, reversal and the plane the answer is reported in
// ---------------------------------------------------------------------------------------------

// Naming the two curves the other way round has to give the same answer with the two parameter
// ranges exchanged: the centre is a point in space and does not know which curve was named first.
// A symmetric pair would hide a mix-up here, so the pair is deliberately lopsided.
TEST(Sketch3DFilletAlgo, ReversingThePairSwapsTheParamsAndKeepsEverythingElse)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine = fixture.makeLine(wy::Vector3(-10.0, -1.0, 0.0), wy::Vector3(10.0, -1.0, 0.0));
    wy3d::SketchArc3D* pArc = fixture.makeArc(wy::Vector3(0.0, 5.0, 0.0), kAxisZ, kAxisX, 3.0,
        wy3d::PI, wy3d::TWO_PI);

    const wy::Vector3 pickOnLine(5.0, -1.0, 0.0);
    const wy::Vector3 pickOnArc(1.8, 2.6, 0.0);

    wy3d::Sketch3DFilletData forward;
    ASSERT_EQ(Algo::fillet(2.0, kTol, pLine, pickOnLine, pArc, pickOnArc, forward), Result::Ok);

    wy3d::Sketch3DFilletData reversed;
    ASSERT_EQ(Algo::fillet(2.0, kTol, pArc, pickOnArc, pLine, pickOnLine, reversed), Result::Ok);

    EXPECT_LE((reversed.filletCenter - forward.filletCenter).length(), 1e-12);
    EXPECT_NEAR(reversed.filletRadius, forward.filletRadius, 1e-12);
    EXPECT_NEAR(reversed.filletStartAngle, forward.filletStartAngle, 1e-12);
    EXPECT_NEAR(reversed.filletEndAngle, forward.filletEndAngle, 1e-12);
    EXPECT_NEAR(reversed.startParam1st, forward.startParam2nd, 1e-12);
    EXPECT_NEAR(reversed.endParam1st, forward.endParam2nd, 1e-12);
    EXPECT_NEAR(reversed.startParam2nd, forward.startParam1st, 1e-12);
    EXPECT_NEAR(reversed.endParam2nd, forward.endParam1st, 1e-12);
}

TEST(Sketch3DFilletAlgo, AFilletOnAPlaneThatIsNoCoordinatePlane)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine1st = fixture.makeLine(onTilted(-10.0, 0.0), onTilted(10.0, 0.0));
    wy3d::SketchLine3D* pLine2nd = fixture.makeLine(onTilted(0.0, -10.0), onTilted(0.0, 10.0));

    wy3d::Sketch3DFilletData data;
    ASSERT_EQ(Algo::fillet(2.0, kTol, pLine1st, onTilted(5.0, 0.0),
        pLine2nd, onTilted(0.0, 5.0), data), Result::Ok);

    expectTangentToBoth(data, pLine1st, pLine2nd);
    expectOrthonormalRightHanded(data.frame);
    // The answer is carried back onto the plane it was solved in rather than left in it.
    EXPECT_NEAR((data.filletCenter - data.frame.origin).dot(data.frame.normal), 0.0, 1e-12);
}

// The arc's own xDir is turned away from the frame's, so the angles the 2D algorithm returns only
// mean something once the shift has been applied. With its sign or its size wrong the arc that gets
// built starts and ends somewhere else, so what is asserted is where the fillet actually lands.
TEST(Sketch3DFilletAlgo, AnArcTurnedWithinItsPlaneStillGetsItsFilletInTheRightPlace)
{
    Fixture fixture;
    const double turned = 37.0 * wy3d::PI / 180.0;
    const wy::Vector3 turnedX = kTiltedX * std::cos(turned) + kTiltedY * std::sin(turned);

    wy3d::SketchLine3D* pLine = fixture.makeLine(onTilted(-10.0, -1.0), onTilted(10.0, -1.0));
    wy3d::SketchArc3D* pArc = fixture.makeArc(onTilted(0.0, 5.0), kTiltedNormal, turnedX, 3.0,
        wy3d::PI, wy3d::TWO_PI);

    wy3d::Sketch3DFilletData data;
    ASSERT_EQ(Algo::fillet(2.0, kTol, pLine, onTilted(5.0, -1.0),
        pArc, onTilted(1.8, 2.6), data), Result::Ok);

    expectTangentToBoth(data, pLine, pArc);

    // The two angles are the sweep of the arc the command layer will build, and it has to be a
    // sweep that SketchArc3D::getTotalAngle leaves alone: not nothing, not a full turn.
    const double sweep = data.filletEndAngle - data.filletStartAngle;
    EXPECT_GT(sweep, 1e-6);
    EXPECT_LT(sweep, wy3d::TWO_PI);

    // Both ends of that arc are on the two curves, which is the whole of what the shift has to get
    // right: an arc between them is tangent to both by construction.
    const wy::Vector3 startPnt = data.filletCenter
        + data.frame.xDir * (std::cos(data.filletStartAngle) * data.filletRadius)
        + data.frame.yDir * (std::sin(data.filletStartAngle) * data.filletRadius);
    const wy::Vector3 endPnt = data.filletCenter
        + data.frame.xDir * (std::cos(data.filletEndAngle) * data.filletRadius)
        + data.frame.yDir * (std::sin(data.filletEndAngle) * data.filletRadius);

    // The two ends have to land one on each curve, and nothing beyond that follows: which angle
    // belongs to which curve is not fixed. The 2D algorithm swaps its start and end whenever the
    // counter-clockwise sweep from the line's tangent point to the arc's comes out wider than half
    // a turn, so here the start angle is the arc's and the end angle the line's. The wrong pairing
    // is asserted to be nowhere near zero, so two ends that both landed on one curve cannot pass.
    const double lineAtStart = distanceToLineSupport(onTilted(-10.0, -1.0), onTilted(10.0, -1.0), startPnt);
    const double conicAtStart = distanceToConicSupport(onTilted(0.0, 5.0), 3.0, startPnt);
    const double lineAtEnd = distanceToLineSupport(onTilted(-10.0, -1.0), onTilted(10.0, -1.0), endPnt);
    const double conicAtEnd = distanceToConicSupport(onTilted(0.0, 5.0), 3.0, endPnt);
    const bool isArcFirst = conicAtStart <= 1e-9 && lineAtEnd <= 1e-9;
    const bool isLineFirst = lineAtStart <= 1e-9 && conicAtEnd <= 1e-9;
    EXPECT_NE(isArcFirst, isLineFirst);
}

TEST(Sketch3DFilletAlgo, RefusalsComeBackAsTheReasonTheyHappened)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine = fixture.makeLine(wy::Vector3(-10.0, 0.0, 0.0), wy::Vector3(10.0, 0.0, 0.0));
    wy3d::SketchLine3D* pSkew = fixture.makeLine(wy::Vector3(5.0, -5.0, 1e-3), wy::Vector3(5.0, 5.0, 1e-3));
    wy3d::SketchCircle3D* pCircle = fixture.makeCircle(wy::Vector3(0.0, 5.0, 0.0), kAxisZ, kAxisX, 3.0);
    wy3d::SketchEllipse3D* pEllipse = fixture.makeEllipse(wy::Vector3(0.0, 5.0, 0.0), 4.0);

    wy3d::Sketch3DFilletData data;
    EXPECT_EQ(Algo::fillet(2.0, kTol, pLine, wy::Vector3(5.0, 0.0, 0.0),
        pSkew, wy::Vector3(5.0, 3.0, 1e-3), data), Result::NotCoplanar);
    EXPECT_EQ(Algo::fillet(2.0, kTol, pLine, wy::Vector3(5.0, 0.0, 0.0),
        pEllipse, wy::Vector3(0.0, 9.0, 0.0), data), Result::UnsupportedPair);
    EXPECT_EQ(Algo::fillet(2.0, kTol, pLine, wy::Vector3(5.0, 0.0, 0.0),
        pLine, wy::Vector3(5.0, 0.0, 0.0), data), Result::SameEntity);

    // A radius the pair cannot carry. A line and a circle admit a fillet of any radius that is large
    // enough, so what fails is one too small to reach across the gap between the two curves: at
    // 0.1 the two offset curves the centre would have to sit on come 5.2 apart at a distance of 6,
    // and never meet. The same pair at a workable radius is the LineAgainstCircle case above.
    wy3d::SketchCircle3D* pSmallCircle = fixture.makeCircle(wy::Vector3(6.0, 0.0, 0.0), kAxisZ, kAxisX, 2.0);
    wy3d::SketchCircle3D* pBigCircle = fixture.makeCircle(wy::Vector3::kZero, kAxisZ, kAxisX, 3.0);
    EXPECT_EQ(Algo::fillet(1.5, kTol, pBigCircle, wy::Vector3(0.0, 3.0, 0.0),
        pSmallCircle, wy::Vector3(6.0, 2.0, 0.0), data), Result::Ok);
    EXPECT_EQ(Algo::fillet(0.1, kTol, pBigCircle, wy::Vector3(0.0, 3.0, 0.0),
        pSmallCircle, wy::Vector3(6.0, 2.0, 0.0), data), Result::NoSolution);
}

// A closed spline is stored as a periodic curve, and an isometry does not stop being one at a seam:
// the poles carried across have to be read the way the source curve reads them.
TEST(Sketch3DFilletAlgo, AClosedSplineCanBeProjected)
{
    Fixture fixture;
    const std::vector<wy::Vector3> controlPoints = {
        wy::Vector3(0.0, 0.0, 0.0),
        wy::Vector3(20.0, 0.0, 0.0),
        wy::Vector3(20.0, 20.0, 0.0),
        wy::Vector3(0.0, 20.0, 0.0),
        wy::Vector3(0.0, 0.0, 0.0) };

    wy3d::SketchSpline3D* pSpline = fixture.makeSpline(2, controlPoints);
    ASSERT_NE(pSpline, nullptr);
    ASSERT_TRUE(pSpline->isClosed());

    Frame frame;
    frame.origin = wy::Vector3::kZero;
    frame.xDir = kAxisX;
    frame.yDir = wy::Vector3(0.0, 1.0, 0.0);
    frame.normal = kAxisZ;

    Projected projected;
    EXPECT_EQ(Algo::projectCurve(pSpline, frame, projected), Result::Ok);

    // The projection is exact, so the two curves have to agree point for point.
    ASSERT_FALSE(projected.pBSpline.IsNull());
    const double firstParam = projected.pBSpline->FirstParameter();
    const double lastParam = projected.pBSpline->LastParameter();
    for (int i = 0; i <= 8; ++i)
    {
        const double t = static_cast<double>(i) / 8.0;
        const wy::Vector3 onCurve = pSpline->getPointAt(t);
        const gp_Pnt2d flat = projected.pBSpline->Value(firstParam + t * (lastParam - firstParam));
        EXPECT_NEAR((onCurve - frame.to3D(wy::Vector2(flat.X(), flat.Y()))).length(), 0.0, kAnalyticTol);
    }
}

// The same closed spline driven all the way through the 2D algorithm, which is where a periodic
// curve would go next. Whether a fillet exists is the 2D algorithm's business and either answer is
// fine here; what is not fine is an exception leaving the call, since nothing above this catches it.
TEST(Sketch3DFilletAlgo, AClosedSplineReachesThe2DAlgorithm)
{
    Fixture fixture;
    const std::vector<wy::Vector3> controlPoints = {
        wy::Vector3(0.0, 0.0, 0.0),
        wy::Vector3(20.0, 0.0, 0.0),
        wy::Vector3(20.0, 20.0, 0.0),
        wy::Vector3(0.0, 20.0, 0.0),
        wy::Vector3(0.0, 0.0, 0.0) };

    wy3d::SketchSpline3D* pSpline = fixture.makeSpline(2, controlPoints);
    ASSERT_NE(pSpline, nullptr);
    ASSERT_TRUE(pSpline->isClosed());

    wy3d::SketchLine3D* pLine = fixture.makeLine(wy::Vector3(-10.0, 10.0, 0.0), wy::Vector3(30.0, 10.0, 0.0));

    wy3d::Sketch3DFilletData data;
    const Result result = Algo::fillet(2.0, kTol, pLine, wy::Vector3(0.0, 10.0, 0.0),
        pSpline, pSpline->getPointAt(0.0), data);
    EXPECT_TRUE(Result::Ok == result || Result::NoSolution == result);

    if (Result::Ok == result)
    {
        // Whatever it decided, the centre has to be the requested distance from both curves.
        EXPECT_NEAR(distanceToCurve(pLine, data.filletCenter), 2.0, 1e-6);
        EXPECT_NEAR(distanceToCurve(pSpline, data.filletCenter), 2.0, 1e-6);
    }
}

// A pick on the line that falls past the end of the spline. To learn which side of the spline the
// line was picked on, the 2D entry point projects that pick onto the spline, and out there the
// projection has no answer at all: Geom2dAPI_ProjectPointOnCurve reports interior extrema and gives
// back the empty set once the foot of the perpendicular falls past an end. The pick being out there
// is ordinary rather than exotic - the line is free to run on past where the spline stops - so this
// is a shape a user reaches by picking a perfectly good point. Without the guard in front of the
// dispatch the second half of this test does not fail, it ends the process.
TEST(Sketch3DFilletAlgo, PickPastTheEndOfTheSplineIsRefused)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine = fixture.makeLine(wy::Vector3(-10.0, 7.0, 0.0),
        wy::Vector3(30.0, 7.0, 0.0));
    wy3d::SketchSpline3D* pSpline = fixture.makeSpline(3, risingPoles());

    // The line picked alongside the spline: filletable, and the radius-1 fillet is tangent to both.
    wy3d::Sketch3DFilletData data;
    ASSERT_EQ(Algo::fillet(1.0, kTol, pLine, wy::Vector3(0.0, 7.0, 0.0),
        pSpline, wy::Vector3(2.0, 9.0, 0.0), data), Result::Ok);
    expectTangentToBoth(data, pLine, pSpline, 1e-7);

    // The same pair, the same spline, the pick moved to the far end of the line: refused.
    wy3d::Sketch3DFilletData far;
    EXPECT_EQ(Algo::fillet(1.0, kTol, pLine, wy::Vector3(29.0, 7.0, 0.0),
        pSpline, wy::Vector3(2.0, 9.0, 0.0), far), Result::NoSolution);
}
