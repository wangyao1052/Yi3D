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
#include <wy3dMath.h>
#include <wy3dSketchArc3D.h>
#include <wy3dSketchCircle3D.h>
#include <wy3dSketchCurve3D.h>
#include <wy3dSketchEllipse3D.h>
#include <wy3dSketchEllipseArc3D.h>
#include <wy3dSketchLine3D.h>
#include <wy3dSketchSpline3D.h>

#include <cfloat>
#include <cmath>
#include <memory>
#include <vector>

namespace
{
using Param = wy3d::Sketch3DCurveParam;

// The analytic curves are recovered by atan2 arithmetic on the very frame the entity evaluates
// in, so they come back to within a couple of ulps. The spline goes through an OCCT projection,
// which is iterative and settles around 1e-8.
const double kAnalyticTol = 1e-9;
const double kSplineTol = 1e-7;

const double kDeg = wy3d::PI / 180.0;

const wy::Vector3 kAxisZ(0.0, 0.0, 1.0);
const wy::Vector3 kAxisX(1.0, 0.0, 0.0);
const wy::Vector3 kNegZ(0.0, 0.0, -1.0);

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

    wy3d::SketchEllipse3D* makeEllipse(const wy::Vector3& center, const wy::Vector3& normal,
        const wy::Vector3& xDir, double majorRadius, double radiusRatio)
    {
        wy3d::SketchEllipse3D* pEllipse = nullptr;
        EXPECT_EQ(wy3d::SketchEllipse3D::create(pTrans, center, normal, xDir, majorRadius, radiusRatio, pEllipse),
            wy::ErrorStatus::Ok);
        return pEllipse;
    }

    wy3d::SketchEllipseArc3D* makeEllipseArc(const wy::Vector3& center, const wy::Vector3& normal,
        const wy::Vector3& xDir, double majorRadius, double radiusRatio,
        double startAngle, double endAngle)
    {
        wy3d::SketchEllipseArc3D* pEllipseArc = nullptr;
        EXPECT_EQ(wy3d::SketchEllipseArc3D::create(pTrans, center, normal, xDir, majorRadius, radiusRatio,
            startAngle, endAngle, pEllipseArc), wy::ErrorStatus::Ok);
        return pEllipseArc;
    }

    wy3d::SketchSpline3D* makeSpline(std::uint32_t degree, const std::vector<wy::Vector3>& controlPoints)
    {
        wy3d::SketchSpline3D* pSpline = nullptr;
        EXPECT_EQ(wy3d::SketchSpline3D::create(pTrans, degree, controlPoints, pSpline), wy::ErrorStatus::Ok);
        return pSpline;
    }
};

// A degree 3 spline whose poles form a downward arch: y(t) = -15 t (1 - t). Its start tangent
// points along (7,-5,0) and its end tangent along (7,5,0), both normalized.
std::vector<wy::Vector3> archPoles()
{
    return {wy::Vector3(-10.0, 0.0, 0.0), wy::Vector3(-3.0, -5.0, 0.0),
        wy::Vector3(3.0, -5.0, 0.0), wy::Vector3(10.0, 0.0, 0.0)};
}

const std::vector<double> kSampleTs = {0.0, 0.25, 0.5, 0.999};

// t -> getPointAt(t) -> getParamOfXxx -> t, for every entity type. This is the whole contract of
// the class: a knot kept across a trim stores a position, and its parameter is recovered from
// whatever the curve looks like afterwards.
void expectRoundTrip(const wy3d::SketchCurve3D* pCurve, double (*pParamOf)(const wy3d::SketchCurve3D*,
    const wy::Vector3&), double tol)
{
    for (double t : kSampleTs)
    {
        const wy::Vector3 pos = pCurve->getPointAt(t);
        EXPECT_NEAR(pParamOf(pCurve, pos), t, tol) << "t = " << t;
    }
}

double paramOfLine(const wy3d::SketchCurve3D* pCurve, const wy::Vector3& pos)
{
    return Param::getParamOfLine(static_cast<const wy3d::SketchLine3D*>(pCurve), pos);
}

double paramOfCircle(const wy3d::SketchCurve3D* pCurve, const wy::Vector3& pos)
{
    return Param::getParamOfCircle(static_cast<const wy3d::SketchCircle3D*>(pCurve), pos);
}

double paramOfArc(const wy3d::SketchCurve3D* pCurve, const wy::Vector3& pos)
{
    return Param::getParamOfArc(static_cast<const wy3d::SketchArc3D*>(pCurve), pos);
}

double paramOfEllipse(const wy3d::SketchCurve3D* pCurve, const wy::Vector3& pos)
{
    return Param::getParamOfEllipse(static_cast<const wy3d::SketchEllipse3D*>(pCurve), pos);
}

double paramOfEllipseArc(const wy3d::SketchCurve3D* pCurve, const wy::Vector3& pos)
{
    return Param::getParamOfEllipseArc(static_cast<const wy3d::SketchEllipseArc3D*>(pCurve), pos);
}

double paramOfSpline(const wy3d::SketchCurve3D* pCurve, const wy::Vector3& pos)
{
    return Param::getParamOfSpline(static_cast<const wy3d::SketchSpline3D*>(pCurve), pos);
}
} // namespace

TEST(Sketch3DCurveParam, LineRoundTrip)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine = fixture.makeLine(wy::Vector3(2.0, 3.0, -1.0), wy::Vector3(12.0, 3.0, -1.0));
    expectRoundTrip(pLine, paramOfLine, kAnalyticTol);
}

TEST(Sketch3DCurveParam, LineUsesTheTrueProjectionNotTheLengthRatio)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine = fixture.makeLine(wy::Vector3::kZero, wy::Vector3(10.0, 0.0, 0.0));

    // Off the curve by 7 units. The 2D helper would answer |v|/L = sqrt(74)/10 = 0.860; a 3D pick
    // lands near the curve rather than on it, so the projection is the only useful answer.
    EXPECT_NEAR(Param::getParamOfLine(pLine, wy::Vector3(5.0, 7.0, 0.0)), 0.5, kAnalyticTol);
}

TEST(Sketch3DCurveParam, LineLeavesTheParameterUnclampedPastTheEnds)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine = fixture.makeLine(wy::Vector3::kZero, wy::Vector3(10.0, 0.0, 0.0));

    // Extending finds its target as a parameter outside [0,1], so these must not be clamped.
    EXPECT_NEAR(Param::getParamOfLine(pLine, wy::Vector3(25.0, 0.0, 0.0)), 2.5, kAnalyticTol);
    EXPECT_NEAR(Param::getParamOfLine(pLine, wy::Vector3(-4.0, 0.0, 0.0)), -0.4, kAnalyticTol);
}

TEST(Sketch3DCurveParam, CircleRoundTrip)
{
    Fixture fixture;
    wy3d::SketchCircle3D* pCircle = fixture.makeCircle(wy::Vector3(3.0, -2.0, 5.0), kAxisZ, kAxisX, 7.0);
    expectRoundTrip(pCircle, paramOfCircle, kAnalyticTol);

    // The seam: t = 1 evaluates to the same point as t = 0, and the parameter is [0,1) there.
    EXPECT_NEAR((pCircle->getPointAt(1.0) - pCircle->getPointAt(0.0)).length(), 0.0, 1e-12);
    EXPECT_NEAR(Param::getParamOfCircle(pCircle, pCircle->getPointAt(1.0)), 0.0, kAnalyticTol);
}

TEST(Sketch3DCurveParam, CircleRoundTripOnAFlippedOrRotatedFrame)
{
    Fixture fixture;

    // normal = -Z flips yDir, so the circle is walked the other way around in world space. The
    // round trip is frame-intrinsic and must not care.
    wy3d::SketchCircle3D* pFlipped = fixture.makeCircle(wy::Vector3::kZero, kNegZ, kAxisX, 4.0);
    expectRoundTrip(pFlipped, paramOfCircle, kAnalyticTol);

    const wy::Vector3 xDir45(std::cos(45.0 * kDeg), std::sin(45.0 * kDeg), 0.0);
    wy3d::SketchCircle3D* pRotated = fixture.makeCircle(wy::Vector3::kZero, kAxisZ, xDir45, 4.0);
    expectRoundTrip(pRotated, paramOfCircle, kAnalyticTol);
}

TEST(Sketch3DCurveParam, ArcRoundTrip)
{
    Fixture fixture;
    wy3d::SketchArc3D* pArc = fixture.makeArc(wy::Vector3::kZero, kAxisZ, kAxisX, 6.0, 20.0 * kDeg, 210.0 * kDeg);
    expectRoundTrip(pArc, paramOfArc, kAnalyticTol);
}

TEST(Sketch3DCurveParam, ArcRoundTripAcrossTheSeam)
{
    Fixture fixture;

    // 350 deg to 380 deg: the sweep wraps past 0, so the polar angle of a point in the middle is
    // small while the arc's own start angle is nearly 2PI.
    wy3d::SketchArc3D* pArc = fixture.makeArc(wy::Vector3::kZero, kAxisZ, kAxisX, 5.0, 350.0 * kDeg, 380.0 * kDeg);
    expectRoundTrip(pArc, paramOfArc, kAnalyticTol);

    EXPECT_NEAR(Param::getParamOfArc(pArc, pArc->getPointAt(0.5)), 0.5, kAnalyticTol);
}

TEST(Sketch3DCurveParam, ArcWithANegativeStartAngleRoundTrips)
{
    Fixture fixture;

    // The setters do not normalize, so _startAngle may be negative while getPointAt still uses it
    // verbatim. The inverse has to normalize the same way it does.
    wy3d::SketchArc3D* pArc = fixture.makeArc(wy::Vector3::kZero, kAxisZ, kAxisX, 5.0, -30.0 * kDeg, 60.0 * kDeg);
    expectRoundTrip(pArc, paramOfArc, kAnalyticTol);
}

TEST(Sketch3DCurveParam, EllipseRoundTrip)
{
    Fixture fixture;
    wy3d::SketchEllipse3D* pEllipse = fixture.makeEllipse(wy::Vector3(1.0, 2.0, 3.0), kAxisZ, kAxisX, 10.0, 0.4);
    expectRoundTrip(pEllipse, paramOfEllipse, kAnalyticTol);

    EXPECT_NEAR((pEllipse->getPointAt(1.0) - pEllipse->getPointAt(0.0)).length(), 0.0, 1e-12);
    EXPECT_NEAR(Param::getParamOfEllipse(pEllipse, pEllipse->getPointAt(1.0)), 0.0, kAnalyticTol);
}

// The one that pins the polar angle trap. SketchEllipseArc3D stores polar angles, getPointAt
// feeds them through ellipsePolarAngleToParametricAngle before evaluating, and Geom_Ellipse in
// turn is parameterized by the eccentric anomaly. An inverse that routed the answer back through
// that conversion - or that read OCCT's parameter - lands somewhere else entirely here, and the
// frame variants below make sure the answer is intrinsic rather than a coincidence of +Z/+X.
TEST(Sketch3DCurveParam, EllipseArcRoundTripIgnoresTheFrame)
{
    Fixture fixture;

    const double startAngle = 30.0 * kDeg;
    const double endAngle = 200.0 * kDeg;
    const wy::Vector3 xDir45(std::cos(45.0 * kDeg), std::sin(45.0 * kDeg), 0.0);

    wy3d::SketchEllipseArc3D* pArc = fixture.makeEllipseArc(wy::Vector3::kZero, kAxisZ, kAxisX, 10.0, 0.4,
        startAngle, endAngle);
    expectRoundTrip(pArc, paramOfEllipseArc, kAnalyticTol);

    wy3d::SketchEllipseArc3D* pFlipped = fixture.makeEllipseArc(wy::Vector3::kZero, kNegZ, kAxisX, 10.0, 0.4,
        startAngle, endAngle);
    expectRoundTrip(pFlipped, paramOfEllipseArc, kAnalyticTol);

    wy3d::SketchEllipseArc3D* pRotated = fixture.makeEllipseArc(wy::Vector3(4.0, -1.0, 2.0), kAxisZ, xDir45, 10.0,
        0.4, startAngle, endAngle);
    expectRoundTrip(pRotated, paramOfEllipseArc, kAnalyticTol);

    // A polar angle of 30 deg is not where the parametric angle of 30 deg sits, so the midpoint
    // is spelled out in world coordinates: the point at polar 115 deg on a = 10, b = 4.
    const double midPolar = 0.5 * (startAngle + endAngle);
    const double midParametric = wy3d::ellipsePolarAngleToParametricAngle(midPolar, 10.0, 4.0);
    const wy::Vector3 expected(10.0 * std::cos(midParametric), 4.0 * std::sin(midParametric), 0.0);
    EXPECT_NEAR((pArc->getPointAt(0.5) - expected).length(), 0.0, 1e-12);
    EXPECT_NEAR(Param::getParamOfEllipseArc(pArc, expected), 0.5, kAnalyticTol);
}

TEST(Sketch3DCurveParam, EllipseArcRoundTripAcrossTheSeam)
{
    Fixture fixture;
    wy3d::SketchEllipseArc3D* pArc = fixture.makeEllipseArc(wy::Vector3::kZero, kAxisZ, kAxisX, 10.0, 0.4,
        340.0 * kDeg, 370.0 * kDeg);
    expectRoundTrip(pArc, paramOfEllipseArc, kAnalyticTol);
}

TEST(Sketch3DCurveParam, SplineRoundTrip)
{
    Fixture fixture;
    wy3d::SketchSpline3D* pSpline = fixture.makeSpline(3, archPoles());
    expectRoundTrip(pSpline, paramOfSpline, kSplineTol);

    // The same answer whether the caller holds the entity or has already taken the handle.
    Handle(Geom_BSplineCurve) pBSpline = pSpline->getOccSpline();
    ASSERT_FALSE(pBSpline.IsNull());
    for (double t : kSampleTs)
        EXPECT_NEAR(Param::getParamOfSpline(pBSpline, pSpline->getPointAt(t)), t, kSplineTol) << "t = " << t;
}

TEST(Sketch3DCurveParam, OffCurvePointOnASplineIsRefused)
{
    Fixture fixture;
    wy3d::SketchSpline3D* pSpline = fixture.makeSpline(3, archPoles());

    // Well inside the arch, and not on the curve nor on either tangent ray.
    EXPECT_EQ(Param::getParamOfSpline(pSpline, wy::Vector3(0.0, -2.0, 0.0)), DBL_MAX);
}

// The spline extension branch: a point off the curve is still meaningful when it sits on the
// tangent taken past an end, and its parameter comes back as the overshoot past that end.
TEST(Sketch3DCurveParam, SplineTangentRayReportsTheOvershoot)
{
    Fixture fixture;
    wy3d::SketchSpline3D* pSpline = fixture.makeSpline(3, archPoles());

    wy::Vector3 startPnt, startDir, endPnt, endDir;
    ASSERT_TRUE(Param::getBSplineInfo(pSpline->getOccSpline(), startPnt, startDir, endPnt, endDir));
    EXPECT_NEAR((startPnt - wy::Vector3(-10.0, 0.0, 0.0)).length(), 0.0, 1e-12);
    EXPECT_NEAR((endPnt - wy::Vector3(10.0, 0.0, 0.0)).length(), 0.0, 1e-12);
    EXPECT_NEAR(startDir.length(), 1.0, 1e-12);
    EXPECT_NEAR(endDir.length(), 1.0, 1e-12);
    EXPECT_NEAR(startDir.dot(endDir), (7.0 * 7.0 - 5.0 * 5.0) / 74.0, 1e-12);

    // 3 units out along the end tangent reads back as 1 + 3.
    EXPECT_NEAR(Param::getParamOfSpline(pSpline, endPnt + endDir * 3.0), 4.0, 1e-9);
    // 2 units back along the start tangent reads back as -2.
    EXPECT_NEAR(Param::getParamOfSpline(pSpline, startPnt - startDir * 2.0), -2.0, 1e-9);
}

TEST(Sketch3DCurveParam, SplineTangentRayOnlyReachesForward)
{
    Fixture fixture;
    wy3d::SketchSpline3D* pSpline = fixture.makeSpline(3, archPoles());

    wy::Vector3 startPnt, startDir, endPnt, endDir;
    ASSERT_TRUE(Param::getBSplineInfo(pSpline->getOccSpline(), startPnt, startDir, endPnt, endDir));

    // Behind the end and behind the start: the tangent line runs both ways, but only the outward
    // half of each end is an extension of the curve.
    EXPECT_EQ(Param::getParamOfSpline(pSpline, endPnt - endDir * 3.0), DBL_MAX);
    EXPECT_EQ(Param::getParamOfSpline(pSpline, startPnt + startDir * 2.0), DBL_MAX);
}

TEST(Sketch3DCurveParam, DegenerateEntitiesAnswerWithDblMax)
{
    Fixture fixture;

    wy3d::SketchLine3D* pZeroLine = fixture.makeLine(wy::Vector3(1.0, 1.0, 1.0), wy::Vector3(1.0, 1.0, 1.0));
    EXPECT_EQ(Param::getParamOfLine(pZeroLine, wy::Vector3(1.0, 1.0, 1.0)), DBL_MAX);

    // A zero sweep does not survive getTotalAngle's normalization, so the arc has no parameter
    // space to divide by.
    wy3d::SketchArc3D* pZeroArc = fixture.makeArc(wy::Vector3::kZero, kAxisZ, kAxisX, 5.0, 1.0, 1.0);
    EXPECT_EQ(Param::getParamOfArc(pZeroArc, wy::Vector3(5.0, 0.0, 0.0)), DBL_MAX);

    wy3d::SketchEllipseArc3D* pZeroEllipseArc = fixture.makeEllipseArc(wy::Vector3::kZero, kAxisZ, kAxisX, 10.0,
        0.4, 1.0, 1.0);
    EXPECT_EQ(Param::getParamOfEllipseArc(pZeroEllipseArc, wy::Vector3(10.0, 0.0, 0.0)), DBL_MAX);
}

TEST(Sketch3DCurveParam, ReviseTSnapsOnlyNearTheEnds)
{
    EXPECT_EQ(Param::reviseT(0.0), 0.0);
    EXPECT_EQ(Param::reviseT(-1e-9), 0.0);
    EXPECT_EQ(Param::reviseT(1.0), 1.0);
    EXPECT_EQ(Param::reviseT(1.0 + 1e-9), 1.0);
    EXPECT_EQ(Param::reviseT(0.5), 0.5);
    EXPECT_EQ(Param::reviseT(-0.5), -0.5);
    EXPECT_EQ(Param::reviseT(1.5), 1.5);
}

TEST(Sketch3DCurveParam, ReviseAngleWrapsIntoTheArcAndSnapsTheEnds)
{
    const double start = 1.0;
    const double end = 2.0;

    EXPECT_EQ(Param::reviseAngle(start, start, end), start);
    EXPECT_EQ(Param::reviseAngle(end, start, end), end);
    EXPECT_NEAR(Param::reviseAngle(1.5, start, end), 1.5, 1e-15);

    // Below the start means the point lies past the seam, so it comes back a full turn later.
    EXPECT_NEAR(Param::reviseAngle(0.5, start, end), 0.5 + wy3d::TWO_PI, 1e-15);
    // And just short of the start wraps rather than snapping to it.
    EXPECT_NEAR(Param::reviseAngle(start - 0.1, start, end), start - 0.1 + wy3d::TWO_PI, 1e-15);

    // Past the top is left alone: that is what an arc's extend target reads back as.
    EXPECT_NEAR(Param::reviseAngle(2.5, start, end), 2.5, 1e-15);
}

// Reading a parameter range back into angles is the inverse of getParamOfArc, and it has to agree
// with it in both directions or an extend places the wrong end.
TEST(Sketch3DCurveParam, SubArcAnglesRoundTripThroughGetParamOfArc)
{
    Fixture fixture;

    // Two quarter arcs, one whose range stays below a full turn and one that runs through the seam.
    const double starts[] = {30.0 * kDeg, 300.0 * kDeg};
    for (const double startAngle : starts)
    {
        wy3d::SketchArc3D* pArc = fixture.makeArc(wy::Vector3::kZero, kAxisZ, kAxisX, 5.0,
            startAngle, startAngle + wy3d::PI_2);

        const double totalAngle = pArc->getTotalAngle();
        for (double from : {0.0, 0.25, 0.5})
        {
            for (double to : {0.75, 1.0, 1.4, 2.3})
            {
                if (to <= from) continue;

                double newStartAngle = 0.0;
                double newEndAngle = 0.0;
                Param::subArcAngles(startAngle, totalAngle, from, to, newStartAngle, newEndAngle);

                // The pair of angles has to describe the same parameter range it was built from:
                // the point at a parameter inside the range must project back to that parameter.
                EXPECT_NEAR(newEndAngle - newStartAngle, (to - from) * totalAngle, 1e-12);
                for (double t : {0.0, 0.5, 1.0})
                {
                    const double angle = newStartAngle + (newEndAngle - newStartAngle) * t;
                    const wy::Vector3 pnt = pArc->getCenter()
                        + kAxisX * (5.0 * std::cos(angle)) + wy::Vector3(0.0, 1.0, 0.0) * (5.0 * std::sin(angle));
                    EXPECT_NEAR(Param::getParamOfArc(pArc, pnt), from + (to - from) * t, 1e-9);
                }
            }
        }
    }
}

// Growing the end is the case that already worked. The angles have to carry the sweep out past the
// old end and no further.
TEST(Sketch3DCurveParam, AnArcEndGrowsForwardFromTheStart)
{
    const double start = 0.5;
    const double total = 1.0;

    double newStartAngle = 0.0;
    double newEndAngle = 0.0;
    Param::subArcAngles(start, total, 0.0, 1.6, newStartAngle, newEndAngle);

    EXPECT_NEAR(newStartAngle, start, 1e-15);
    EXPECT_NEAR(newEndAngle, start + 1.6, 1e-15);
}

// A periodic curve's parameter is not signed: a target a quarter turn behind the start of a quarter
// arc arrives as 1.75, and the angles still have to come out a quarter turn shorter at the start.
// Getting the sign convention wrong here is what made one end of an arc extend and the other not.
TEST(Sketch3DCurveParam, AnArcStartGrowsEvenThoughItsKnotIsNotNegative)
{
    // A quarter arc from 90 to 180 degrees. A knot at 45 degrees sits half a turn before the end of
    // the circle's parameter range, so getParamOfArc reports it as (45 + 360 - 90) / 90 = 3.5.
    const double start = wy3d::PI_2;
    const double total = wy3d::PI_2;

    double newStartAngle = 0.0;
    double newEndAngle = 0.0;
    Param::subArcAngles(start, total, 3.5, 1.0, newStartAngle, newEndAngle);

    EXPECT_NEAR(newStartAngle, 45.0 * kDeg, 1e-12);
    EXPECT_NEAR(newEndAngle, wy3d::PI, 1e-12);
    EXPECT_NEAR(newEndAngle - newStartAngle, 135.0 * kDeg, 1e-12);
}

// The same start growth through the entity: the two angles have to leave the arc spanning from the
// knot to where the old end was, and no more.
TEST(Sketch3DCurveParam, GrowingAnArcStartKeepsTheOldEndWhereItWas)
{
    Fixture fixture;

    const double startAngle = 90.0 * kDeg;
    wy3d::SketchArc3D* pArc = fixture.makeArc(wy::Vector3::kZero, kAxisZ, kAxisX, 5.0,
        startAngle, 180.0 * kDeg);
    const wy::Vector3 oldEndPnt = pArc->getEndPoint();

    // 45 degrees is 3.5 quarter-turns forward of the arc's start.
    const double knotParam = Param::getParamOfArc(pArc, wy::Vector3(5.0 * std::cos(45.0 * kDeg),
        5.0 * std::sin(45.0 * kDeg), 0.0));
    EXPECT_NEAR(knotParam, 3.5, 1e-9);
    ASSERT_GT(knotParam, 1.0);

    double newStartAngle = 0.0;
    double newEndAngle = 0.0;
    Param::subArcAngles(startAngle, pArc->getTotalAngle(), knotParam, 1.0, newStartAngle, newEndAngle);
    ASSERT_EQ(pArc->setStartAngle(newStartAngle), wy::ErrorStatus::Ok);
    ASSERT_EQ(pArc->setEndAngle(newEndAngle), wy::ErrorStatus::Ok);

    EXPECT_NEAR(pArc->getTotalAngle(), 135.0 * kDeg, 1e-9);
    EXPECT_NEAR((pArc->getStartPoint() - wy::Vector3(5.0 * std::cos(45.0 * kDeg),
        5.0 * std::sin(45.0 * kDeg), 0.0)).length(), 0.0, 1e-9);
    EXPECT_NEAR((pArc->getEndPoint() - oldEndPnt).length(), 0.0, 1e-9);
    // The old end is still the far end of the sweep, so its parameter is 1 rather than 0.
    EXPECT_NEAR(Param::getParamOfArc(pArc, oldEndPnt), 1.0, 1e-9);
    EXPECT_NEAR(Param::getParamOfArc(pArc, pArc->getStartPoint()), 0.0, 1e-9);
}

// An ellipse arc is parameterized the same way, so it has to survive the same round trip.
TEST(Sketch3DCurveParam, AnEllipseArcStartGrowsTheSameWay)
{
    Fixture fixture;

    const double startAngle = 200.0 * kDeg;
    const double total = 40.0 * kDeg;
    wy3d::SketchEllipseArc3D* pEllipseArc = fixture.makeEllipseArc(wy::Vector3::kZero, kAxisZ, kAxisX,
        10.0, 0.4, startAngle, startAngle + total);

    // The polar angle, not the eccentric anomaly: the point on the ellipse that reads back as a
    // polar angle of 10 degrees.
    const double polar = 10.0 * kDeg;
    const double anomaly = wy3d::ellipsePolarAngleToParametricAngle(polar, 10.0, 4.0);
    const wy::Vector3 target(10.0 * std::cos(anomaly), 4.0 * std::sin(anomaly), 0.0);

    // Before the start, so the parameter wraps forward to a value above 1.
    const double knotParam = Param::getParamOfEllipseArc(pEllipseArc, target);
    EXPECT_NEAR(knotParam, 4.25, 1e-9);
    ASSERT_GT(knotParam, 1.0);

    double newStartAngle = 0.0;
    double newEndAngle = 0.0;
    Param::subArcAngles(wy3d::normalizeRadian(startAngle), pEllipseArc->getTotalAngle(), knotParam, 1.0,
        newStartAngle, newEndAngle);
    ASSERT_EQ(pEllipseArc->setStartAngle(newStartAngle), wy::ErrorStatus::Ok);
    ASSERT_EQ(pEllipseArc->setEndAngle(newEndAngle), wy::ErrorStatus::Ok);

    // From the knot forward round to where the old end was.
    EXPECT_NEAR(pEllipseArc->getTotalAngle(), 230.0 * kDeg, 1e-9);
    EXPECT_NEAR(Param::getParamOfEllipseArc(pEllipseArc, pEllipseArc->getStartPoint()), 0.0, 1e-9);
    EXPECT_NEAR(Param::getParamOfEllipseArc(pEllipseArc, pEllipseArc->getEndPoint()), 1.0, 1e-9);
}

// The pair an arc's backward extension comes as: the knot above 1 and the untouched end at 1, in
// that order. A preview built from it has to run the short way round the seam, not backwards
// through the whole sweep.
TEST(Sketch3DCurveParam, APairOrderedEndThenStartStillRunsForward)
{
    double fromAngle = 0.0;
    double toAngle = 0.0;
    // A quarter arc starting at 0 with the knot 30 degrees behind it, so the knot reads as 330
    // degrees, or 3.667 quarter turns.
    Param::subArcAngles(0.0, wy3d::PI_2, 330.0 / 90.0, 1.0, fromAngle, toAngle);

    EXPECT_NEAR(fromAngle, 330.0 * kDeg, 1e-12);
    EXPECT_NEAR(toAngle, 450.0 * kDeg, 1e-12);
    // The short way: 330 forward to the old end at 90, not backward through 240 degrees.
    EXPECT_NEAR(toAngle - fromAngle, 120.0 * kDeg, 1e-12);
}

// A whole circle covers a full turn, and its end normalizes straight back onto its start. Dropping
// the end there would leave a circle with no preview at all.
TEST(Sketch3DCurveParam, AWholeCircleStaysAWholeTurn)
{
    double fromAngle = 0.0;
    double toAngle = 0.0;
    Param::subArcAngles(0.0, wy3d::TWO_PI, 0.0, 1.0, fromAngle, toAngle);

    EXPECT_NEAR(fromAngle, 0.0, 1e-12);
    EXPECT_NEAR(toAngle, wy3d::TWO_PI, 1e-12);
}

// A piece of a circle off its seam still comes back as the piece that was asked for.
TEST(Sketch3DCurveParam, APieceOfACircleIsNotTurnedIntoAWholeOne)
{
    double fromAngle = 0.0;
    double toAngle = 0.0;
    Param::subArcAngles(0.0, wy3d::TWO_PI, 0.25, 0.5, fromAngle, toAngle);

    EXPECT_NEAR(fromAngle, wy3d::PI_2, 1e-12);
    EXPECT_NEAR(toAngle, wy3d::PI, 1e-12);
}

// The acceptance test for the whole operation: a knot taken off an arc, run through the angles, and
// written back has to leave the arc ending exactly on that knot, with the far end untouched. This
// is what a mis-read sign convention breaks, and it breaks it in one direction only.
TEST(Sketch3DCurveParam, ExtendingAnArcEndsItExactlyOnTheKnot)
{
    Fixture fixture;

    struct Case
    {
        double startDeg;
        double endDeg;
        double knotDeg;   // the knot, expressed as a polar angle off the arc's own x axis
        bool growEnd;
    };
    const Case cases[] = {
        {0.0, 90.0, 120.0, true},      // forward, inside the first turn
        {0.0, 90.0, 330.0, false},     // backward, which wraps forward to 330
        {200.0, 240.0, 250.0, true},   // a start away from zero
        {200.0, 240.0, 190.0, false},  // and its backward twin
        {350.0, 30.0, 60.0, true},     // a sweep across the seam
        {350.0, 30.0, 330.0, false},
        {10.0, 280.0, 300.0, true},    // a sweep wider than a half turn
        {10.0, 280.0, 0.0, false},
    };

    for (const Case& one : cases)
    {
        wy3d::SketchArc3D* pArc = fixture.makeArc(wy::Vector3::kZero, kAxisZ, kAxisX, 5.0,
            one.startDeg * kDeg, one.endDeg * kDeg);

        const double totalAngle = pArc->getTotalAngle();
        const wy::Vector3 knotPnt(5.0 * std::cos(one.knotDeg * kDeg), 5.0 * std::sin(one.knotDeg * kDeg), 0.0);
        const wy::Vector3 farEndPnt = one.growEnd ? pArc->getStartPoint() : pArc->getEndPoint();

        // The knot's parameter as the graph would report it, sign convention and all. Growing the
        // end leaves the start at 0; growing the start leaves the end at 1.
        const double knotParam = Param::getParamOfArc(pArc, knotPnt);
        const double fromParam = one.growEnd ? 0.0 : knotParam;
        const double toParam = one.growEnd ? knotParam : 1.0;
        ASSERT_GT(knotParam, 1.0) << "knot at " << one.knotDeg;

        double newStartAngle = 0.0;
        double newEndAngle = 0.0;
        Param::subArcAngles(pArc->getStartAngle(), totalAngle, fromParam, toParam,
            newStartAngle, newEndAngle);
        ASSERT_EQ(pArc->setStartAngle(newStartAngle), wy::ErrorStatus::Ok);
        ASSERT_EQ(pArc->setEndAngle(newEndAngle), wy::ErrorStatus::Ok);

        EXPECT_NEAR((pArc->getStartPoint() - (one.growEnd ? farEndPnt : knotPnt)).length(), 0.0, 1e-9)
            << "start, knot at " << one.knotDeg;
        EXPECT_NEAR((pArc->getEndPoint() - (one.growEnd ? knotPnt : farEndPnt)).length(), 0.0, 1e-9)
            << "end, knot at " << one.knotDeg;
        // And the knot is now an end of the arc rather than a point along it.
        EXPECT_NEAR(Param::getParamOfArc(pArc, knotPnt), one.growEnd ? 1.0 : 0.0, 1e-9);
    }
}

// The same acceptance test for the other periodic curve. An ellipse arc stores polar angles and
// evaluates through the eccentric anomaly, so the two conversions have to cancel out here too.
TEST(Sketch3DCurveParam, ExtendingAnEllipseArcEndsItExactlyOnTheKnot)
{
    Fixture fixture;

    struct Case
    {
        double startDeg;
        double endDeg;
        double knotDeg;
        bool growEnd;
    };
    const Case cases[] = {
        {0.0, 90.0, 135.0, true},
        {0.0, 90.0, 315.0, false},
        {150.0, 300.0, 330.0, true},
        {150.0, 300.0, 100.0, false},
    };

    for (const Case& one : cases)
    {
        wy3d::SketchEllipseArc3D* pEllipseArc = fixture.makeEllipseArc(wy::Vector3::kZero, kAxisZ, kAxisX,
            10.0, 0.4, one.startDeg * kDeg, one.endDeg * kDeg);

        const double polar = one.knotDeg * kDeg;
        const double anomaly = wy3d::ellipsePolarAngleToParametricAngle(polar, 10.0, 4.0);
        const wy::Vector3 knotPnt(10.0 * std::cos(anomaly), 4.0 * std::sin(anomaly), 0.0);
        const wy::Vector3 farEndPnt = one.growEnd ? pEllipseArc->getStartPoint() : pEllipseArc->getEndPoint();

        const double knotParam = Param::getParamOfEllipseArc(pEllipseArc, knotPnt);
        const double fromParam = one.growEnd ? 0.0 : knotParam;
        const double toParam = one.growEnd ? knotParam : 1.0;

        double newStartAngle = 0.0;
        double newEndAngle = 0.0;
        Param::subArcAngles(pEllipseArc->getStartAngle(), pEllipseArc->getTotalAngle(), fromParam,
            toParam, newStartAngle, newEndAngle);
        ASSERT_EQ(pEllipseArc->setStartAngle(newStartAngle), wy::ErrorStatus::Ok);
        ASSERT_EQ(pEllipseArc->setEndAngle(newEndAngle), wy::ErrorStatus::Ok);

        EXPECT_NEAR((pEllipseArc->getStartPoint() - (one.growEnd ? farEndPnt : knotPnt)).length(), 0.0, 1e-9)
            << "start, knot at " << one.knotDeg;
        EXPECT_NEAR((pEllipseArc->getEndPoint() - (one.growEnd ? knotPnt : farEndPnt)).length(), 0.0, 1e-9)
            << "end, knot at " << one.knotDeg;
    }
}

// The angles are what a preview draws, so a range wider than a half turn has to survive being read
// back: the two ends are ordered by angle, never by which of the two stored angles is smaller.
TEST(Sketch3DCurveParam, SubArcAnglesKeepsASweepWiderThanAHalfTurn)
{
    const double start = 10.0 * kDeg;
    const double total = 270.0 * kDeg;

    // A knot 60 degrees past the end of a 10 to 280 degree sweep sits at 340, and reads as
    // 1 + 60/270. Growing the start back to it leaves 300 degrees running from 340 on to 280.
    double fromAngle = 0.0;
    double toAngle = 0.0;
    Param::subArcAngles(start, total, 1.0 + 60.0 / 270.0, 1.0, fromAngle, toAngle);

    EXPECT_NEAR(fromAngle, 340.0 * kDeg, 1e-12);
    EXPECT_NEAR(toAngle, 640.0 * kDeg, 1e-12);
    EXPECT_NEAR(toAngle - fromAngle, 300.0 * kDeg, 1e-12);
}

// A start angle stored outside [0, 2PI) - which a previous extension leaves behind - reads the same
// as its normalized twin, so extending an already extended arc keeps working.
TEST(Sketch3DCurveParam, SubArcAnglesIgnoresAnUnnormalizedStartAngle)
{
    double fromAngle = 0.0;
    double toAngle = 0.0;
    Param::subArcAngles(330.0 * kDeg, 120.0 * kDeg, 0.0, 1.0, fromAngle, toAngle);
    EXPECT_NEAR(fromAngle, 330.0 * kDeg, 1e-9);
    EXPECT_NEAR(toAngle, 450.0 * kDeg, 1e-9);

    double fromAngle2 = 0.0;
    double toAngle2 = 0.0;
    Param::subArcAngles(-30.0 * kDeg, 120.0 * kDeg, 0.0, 1.0, fromAngle2, toAngle2);
    EXPECT_NEAR(fromAngle2, fromAngle, 1e-9);
    EXPECT_NEAR(toAngle2, toAngle, 1e-9);

    double fromAngle3 = 0.0;
    double toAngle3 = 0.0;
    Param::subArcAngles(690.0 * kDeg, 120.0 * kDeg, 0.0, 1.0, fromAngle3, toAngle3);
    EXPECT_NEAR(fromAngle3, fromAngle, 1e-9);
    EXPECT_NEAR(toAngle3, toAngle, 1e-9);
}
