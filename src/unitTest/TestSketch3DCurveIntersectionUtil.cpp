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

#include <utils/wy3dSketch3DCurveIntersectionUtil.h>
#include <wy3dMath.h>
#include <wy3dSketchArc3D.h>
#include <wy3dSketchCircle3D.h>
#include <wy3dSketchCurve3D.h>
#include <wy3dSketchEllipse3D.h>
#include <wy3dSketchEllipseArc3D.h>
#include <wy3dSketchLine3D.h>
#include <wy3dSketchSpline3D.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

namespace
{
using Util = wy3d::Sketch3DCurveIntersectionUtil;
using Result = Util::Result;

const double kTol = 1e-6;

const wy::Vector3 kAxisZ(0.0, 0.0, 1.0);
const wy::Vector3 kAxisX(1.0, 0.0, 0.0);
const wy::Vector3 kAxisY(0.0, 1.0, 0.0);

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

    wy3d::SketchCircle3D* makeCircle(const wy::Vector3& center, double radius)
    {
        wy3d::SketchCircle3D* pCircle = nullptr;
        EXPECT_EQ(wy3d::SketchCircle3D::create(pTrans, center, kAxisZ, kAxisX, radius, pCircle),
            wy::ErrorStatus::Ok);
        return pCircle;
    }

    wy3d::SketchCircle3D* makeCircleOnPlane(const wy::Vector3& center, const wy::Vector3& normal,
        const wy::Vector3& xDir, double radius)
    {
        wy3d::SketchCircle3D* pCircle = nullptr;
        EXPECT_EQ(wy3d::SketchCircle3D::create(pTrans, center, normal, xDir, radius, pCircle),
            wy::ErrorStatus::Ok);
        return pCircle;
    }

    wy3d::SketchArc3D* makeArc(const wy::Vector3& center, double radius, double startAngle, double endAngle)
    {
        wy3d::SketchArc3D* pArc = nullptr;
        EXPECT_EQ(wy3d::SketchArc3D::create(pTrans, center, kAxisZ, kAxisX, radius, startAngle, endAngle, pArc),
            wy::ErrorStatus::Ok);
        return pArc;
    }

    wy3d::SketchEllipse3D* makeEllipse(const wy::Vector3& center, const wy::Vector3& xDir,
        double majorRadius, double radiusRatio)
    {
        wy3d::SketchEllipse3D* pEllipse = nullptr;
        EXPECT_EQ(wy3d::SketchEllipse3D::create(pTrans, center, kAxisZ, xDir, majorRadius, radiusRatio, pEllipse),
            wy::ErrorStatus::Ok);
        return pEllipse;
    }

    wy3d::SketchEllipseArc3D* makeEllipseArc(const wy::Vector3& center, const wy::Vector3& xDir,
        double majorRadius, double radiusRatio, double startAngle, double endAngle)
    {
        wy3d::SketchEllipseArc3D* pEllipseArc = nullptr;
        EXPECT_EQ(wy3d::SketchEllipseArc3D::create(pTrans, center, kAxisZ, xDir, majorRadius, radiusRatio,
            startAngle, endAngle, pEllipseArc), wy::ErrorStatus::Ok);
        return pEllipseArc;
    }

    wy3d::SketchSpline3D* makeSplineByControlPoints(std::uint32_t degree,
        const std::vector<wy::Vector3>& controlPoints)
    {
        wy3d::SketchSpline3D* pSpline = nullptr;
        EXPECT_EQ(wy3d::SketchSpline3D::create(pTrans, degree, controlPoints, pSpline), wy::ErrorStatus::Ok);
        return pSpline;
    }
};

bool hasPoint(const std::vector<wy::Vector3>& points, const wy::Vector3& expected, double tol = 1e-6)
{
    for (const wy::Vector3& pnt : points)
    {
        if ((pnt - expected).length() <= tol)
        {
            return true;
        }
    }
    return false;
}

Result intersectBounded(const wy3d::SketchCurve3D* pCurveA, const wy3d::SketchCurve3D* pCurveB,
    std::vector<wy::Vector3>& outPoints)
{
    return Util::intersect(Util::bounded(pCurveA), Util::bounded(pCurveB), outPoints, kTol);
}

// A degree 3 spline whose poles form a downward arch: y(t) = -15 t (1 - t), so the horizontal
// line y = -1 meets it exactly twice, on either arm.
std::vector<wy::Vector3> archPoles()
{
    return {wy::Vector3(-10.0, 0.0, 0.0), wy::Vector3(-3.0, -5.0, 0.0),
        wy::Vector3(3.0, -5.0, 0.0), wy::Vector3(10.0, 0.0, 0.0)};
}
} // namespace

TEST(Sketch3DCurveIntersection, LineLineCrossing)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLineA = fixture.makeLine(wy::Vector3(0.0, 0.0, 0.0), wy::Vector3(10.0, 0.0, 0.0));
    wy3d::SketchLine3D* pLineB = fixture.makeLine(wy::Vector3(5.0, -5.0, 0.0), wy::Vector3(5.0, 5.0, 0.0));

    std::vector<wy::Vector3> points;
    EXPECT_EQ(intersectBounded(pLineA, pLineB, points), Result::Ok);
    ASSERT_EQ(points.size(), 1u);
    EXPECT_TRUE(hasPoint(points, wy::Vector3(5.0, 0.0, 0.0)));
}

TEST(Sketch3DCurveIntersection, LineLineSkewIsNoIntersection)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLineA = fixture.makeLine(wy::Vector3(0.0, 0.0, 0.0), wy::Vector3(10.0, 0.0, 0.0));
    wy3d::SketchLine3D* pLineB = fixture.makeLine(wy::Vector3(5.0, -5.0, 5.0), wy::Vector3(5.0, 5.0, 5.0));

    std::vector<wy::Vector3> points;
    EXPECT_EQ(intersectBounded(pLineA, pLineB, points), Result::NoIntersection);
    EXPECT_TRUE(points.empty());
}

TEST(Sketch3DCurveIntersection, LineLineReachLetsThemMeet)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLineA = fixture.makeLine(wy::Vector3(0.0, 0.0, 0.0), wy::Vector3(1.0, 0.0, 0.0));
    wy3d::SketchLine3D* pLineB = fixture.makeLine(wy::Vector3(5.0, -5.0, 0.0), wy::Vector3(5.0, 5.0, 0.0));

    std::vector<wy::Vector3> points;
    EXPECT_EQ(intersectBounded(pLineA, pLineB, points), Result::NoIntersection);

    points.clear();
    EXPECT_EQ(Util::intersect(Util::extended(pLineA, 20.0, true, true), Util::bounded(pLineB), points, kTol),
        Result::Ok);
    ASSERT_EQ(points.size(), 1u);
    EXPECT_TRUE(hasPoint(points, wy::Vector3(5.0, 0.0, 0.0)));
}

TEST(Sketch3DCurveIntersection, LineLineCollinearIsCoincident)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLineA = fixture.makeLine(wy::Vector3(0.0, 0.0, 0.0), wy::Vector3(10.0, 0.0, 0.0));
    wy3d::SketchLine3D* pLineB = fixture.makeLine(wy::Vector3(2.0, 0.0, 0.0), wy::Vector3(8.0, 0.0, 0.0));

    std::vector<wy::Vector3> points;
    EXPECT_EQ(intersectBounded(pLineA, pLineB, points), Result::Coincident);
    EXPECT_TRUE(points.empty());
}

TEST(Sketch3DCurveIntersection, LineLineParallelApart)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLineA = fixture.makeLine(wy::Vector3(0.0, 0.0, 0.0), wy::Vector3(10.0, 0.0, 0.0));
    wy3d::SketchLine3D* pLineB = fixture.makeLine(wy::Vector3(0.0, 1.0, 0.0), wy::Vector3(10.0, 1.0, 0.0));

    std::vector<wy::Vector3> points;
    EXPECT_EQ(intersectBounded(pLineA, pLineB, points), Result::NoIntersection);
}

TEST(Sketch3DCurveIntersection, LineLineSharingAnEndpoint)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLineA = fixture.makeLine(wy::Vector3(0.0, 0.0, 0.0), wy::Vector3(10.0, 0.0, 0.0));
    wy3d::SketchLine3D* pLineB = fixture.makeLine(wy::Vector3(10.0, 0.0, 0.0), wy::Vector3(10.0, 10.0, 0.0));

    std::vector<wy::Vector3> points;
    EXPECT_EQ(intersectBounded(pLineA, pLineB, points), Result::Ok);
    ASSERT_EQ(points.size(), 1u);
    EXPECT_TRUE(hasPoint(points, wy::Vector3(10.0, 0.0, 0.0)));
}

TEST(Sketch3DCurveIntersection, LineCircleSecant)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine = fixture.makeLine(wy::Vector3(-10.0, 0.0, 0.0), wy::Vector3(10.0, 0.0, 0.0));
    wy3d::SketchCircle3D* pCircle = fixture.makeCircle(wy::Vector3::kZero, 5.0);

    std::vector<wy::Vector3> points;
    EXPECT_EQ(intersectBounded(pLine, pCircle, points), Result::Ok);
    ASSERT_EQ(points.size(), 2u);
    EXPECT_TRUE(hasPoint(points, wy::Vector3(5.0, 0.0, 0.0)));
    EXPECT_TRUE(hasPoint(points, wy::Vector3(-5.0, 0.0, 0.0)));
}

TEST(Sketch3DCurveIntersection, LineCircleTangent)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine = fixture.makeLine(wy::Vector3(-10.0, 5.0, 0.0), wy::Vector3(10.0, 5.0, 0.0));
    wy3d::SketchCircle3D* pCircle = fixture.makeCircle(wy::Vector3::kZero, 5.0);

    std::vector<wy::Vector3> points;
    EXPECT_EQ(intersectBounded(pLine, pCircle, points), Result::Ok);
    ASSERT_EQ(points.size(), 1u);
    EXPECT_TRUE(hasPoint(points, wy::Vector3(0.0, 5.0, 0.0), 1e-4));
}

TEST(Sketch3DCurveIntersection, LineCircleApartHasNoFalsePositive)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine = fixture.makeLine(wy::Vector3(-10.0, 6.0, 0.0), wy::Vector3(10.0, 6.0, 0.0));
    wy3d::SketchCircle3D* pCircle = fixture.makeCircle(wy::Vector3::kZero, 5.0);

    std::vector<wy::Vector3> points;
    EXPECT_EQ(intersectBounded(pLine, pCircle, points), Result::NoIntersection);
    EXPECT_TRUE(points.empty());
}

TEST(Sketch3DCurveIntersection, LineCircleTouchingAtTheLineEnd)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine = fixture.makeLine(wy::Vector3::kZero, wy::Vector3(5.0, 0.0, 0.0));
    wy3d::SketchCircle3D* pCircle = fixture.makeCircle(wy::Vector3::kZero, 5.0);

    std::vector<wy::Vector3> points;
    EXPECT_EQ(intersectBounded(pLine, pCircle, points), Result::Ok);
    ASSERT_EQ(points.size(), 1u);
    EXPECT_TRUE(hasPoint(points, wy::Vector3(5.0, 0.0, 0.0)));
}

// The second crossing of the circle by this line lies outside the arc's sweep, so exactly one
// point may come back. A missing range on the extrema call would return two.
TEST(Sketch3DCurveIntersection, LineArcHonoursTheSweep)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine = fixture.makeLine(wy::Vector3(-10.0, 0.0, 0.0), wy::Vector3(10.0, 0.0, 0.0));
    wy3d::SketchArc3D* pArc = fixture.makeArc(wy::Vector3::kZero, 5.0, 0.0, wy3d::PI * 0.5);

    std::vector<wy::Vector3> points;
    EXPECT_EQ(intersectBounded(pLine, pArc, points), Result::Ok);
    ASSERT_EQ(points.size(), 1u);
    EXPECT_TRUE(hasPoint(points, wy::Vector3(5.0, 0.0, 0.0)));
}

TEST(Sketch3DCurveIntersection, LineEllipseSecant)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine = fixture.makeLine(wy::Vector3(-20.0, 0.0, 0.0), wy::Vector3(20.0, 0.0, 0.0));
    wy3d::SketchEllipse3D* pEllipse = fixture.makeEllipse(wy::Vector3::kZero, kAxisX, 10.0, 0.4);

    std::vector<wy::Vector3> points;
    EXPECT_EQ(intersectBounded(pLine, pEllipse, points), Result::Ok);
    ASSERT_EQ(points.size(), 2u);
    EXPECT_TRUE(hasPoint(points, wy::Vector3(10.0, 0.0, 0.0)));
    EXPECT_TRUE(hasPoint(points, wy::Vector3(-10.0, 0.0, 0.0)));
}

TEST(Sketch3DCurveIntersection, LineEllipseArcHonoursTheSweep)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine = fixture.makeLine(wy::Vector3(-20.0, 0.0, 0.0), wy::Vector3(20.0, 0.0, 0.0));
    wy3d::SketchEllipseArc3D* pEllipseArc = fixture.makeEllipseArc(
        wy::Vector3::kZero, kAxisX, 10.0, 0.4, 0.0, wy3d::PI * 0.5);

    std::vector<wy::Vector3> points;
    EXPECT_EQ(intersectBounded(pLine, pEllipseArc, points), Result::Ok);
    ASSERT_EQ(points.size(), 1u);
    EXPECT_TRUE(hasPoint(points, wy::Vector3(10.0, 0.0, 0.0)));
}

TEST(Sketch3DCurveIntersection, CircleCircleCoplanar)
{
    Fixture fixture;
    wy3d::SketchCircle3D* pCircleA = fixture.makeCircle(wy::Vector3::kZero, 5.0);
    wy3d::SketchCircle3D* pCircleB = fixture.makeCircle(wy::Vector3(8.0, 0.0, 0.0), 5.0);

    std::vector<wy::Vector3> points;
    EXPECT_EQ(intersectBounded(pCircleA, pCircleB, points), Result::Ok);
    ASSERT_EQ(points.size(), 2u);
    EXPECT_TRUE(hasPoint(points, wy::Vector3(4.0, 3.0, 0.0), 1e-4));
    EXPECT_TRUE(hasPoint(points, wy::Vector3(4.0, -3.0, 0.0), 1e-4));
}

TEST(Sketch3DCurveIntersection, CircleCircleExternallyTangent)
{
    Fixture fixture;
    wy3d::SketchCircle3D* pCircleA = fixture.makeCircle(wy::Vector3::kZero, 5.0);
    wy3d::SketchCircle3D* pCircleB = fixture.makeCircle(wy::Vector3(10.0, 0.0, 0.0), 5.0);

    std::vector<wy::Vector3> points;
    EXPECT_EQ(intersectBounded(pCircleA, pCircleB, points), Result::Ok);
    ASSERT_EQ(points.size(), 1u);
    EXPECT_TRUE(hasPoint(points, wy::Vector3(5.0, 0.0, 0.0), 1e-3));
}

TEST(Sketch3DCurveIntersection, CircleCircleConcentricSameRadiusIsCoincident)
{
    Fixture fixture;
    wy3d::SketchCircle3D* pCircleA = fixture.makeCircle(wy::Vector3::kZero, 5.0);
    wy3d::SketchCircle3D* pCircleB = fixture.makeCircle(wy::Vector3::kZero, 5.0);

    std::vector<wy::Vector3> points;
    EXPECT_EQ(intersectBounded(pCircleA, pCircleB, points), Result::Coincident);
    EXPECT_TRUE(points.empty());
}

TEST(Sketch3DCurveIntersection, CircleCircleConcentricDifferentRadius)
{
    Fixture fixture;
    wy3d::SketchCircle3D* pCircleA = fixture.makeCircle(wy::Vector3::kZero, 5.0);
    wy3d::SketchCircle3D* pCircleB = fixture.makeCircle(wy::Vector3::kZero, 3.0);

    std::vector<wy::Vector3> points;
    EXPECT_EQ(intersectBounded(pCircleA, pCircleB, points), Result::NoIntersection);
    EXPECT_TRUE(points.empty());
}

// Two circles standing on planes that cross at right angles: they meet at the two points where
// the plane intersection line leaves the sphere of radius 5.
TEST(Sketch3DCurveIntersection, CircleCircleCrossPlane)
{
    Fixture fixture;
    wy3d::SketchCircle3D* pCircleA = fixture.makeCircleOnPlane(wy::Vector3::kZero, kAxisZ, kAxisX, 5.0);
    wy3d::SketchCircle3D* pCircleB = fixture.makeCircleOnPlane(wy::Vector3::kZero, kAxisX, kAxisY, 5.0);

    std::vector<wy::Vector3> points;
    EXPECT_EQ(intersectBounded(pCircleA, pCircleB, points), Result::Ok);
    ASSERT_EQ(points.size(), 2u);
    EXPECT_TRUE(hasPoint(points, wy::Vector3(0.0, 5.0, 0.0), 1e-4));
    EXPECT_TRUE(hasPoint(points, wy::Vector3(0.0, -5.0, 0.0), 1e-4));
}

TEST(Sketch3DCurveIntersection, CircleCircleCrossPlaneTouchingOnce)
{
    Fixture fixture;
    // The second circle stands on the plane x = 0 through (0,5,5), so it reaches (0,5,0) and
    // nothing else of the first one, which lies in z = 0 around the origin.
    wy3d::SketchCircle3D* pCircleA = fixture.makeCircleOnPlane(wy::Vector3::kZero, kAxisZ, kAxisX, 5.0);
    wy3d::SketchCircle3D* pCircleB = fixture.makeCircleOnPlane(wy::Vector3(0.0, 5.0, 5.0), kAxisX, kAxisY, 5.0);

    std::vector<wy::Vector3> points;
    EXPECT_EQ(intersectBounded(pCircleA, pCircleB, points), Result::Ok);
    ASSERT_EQ(points.size(), 1u);
    EXPECT_TRUE(hasPoint(points, wy::Vector3(0.0, 5.0, 0.0), 1e-4));
}

// The case the extrema call has the least margin on: two coplanar ellipses crossing four times.
// x^2/100 + y^2/16 = 1 against x^2/16 + y^2/100 = 1 gives x^2 = y^2 = 1600/116.
TEST(Sketch3DCurveIntersection, EllipseEllipseCoplanarFourPoints)
{
    Fixture fixture;
    wy3d::SketchEllipse3D* pEllipseA = fixture.makeEllipse(wy::Vector3::kZero, kAxisX, 10.0, 0.4);
    wy3d::SketchEllipse3D* pEllipseB = fixture.makeEllipse(wy::Vector3::kZero, kAxisY, 10.0, 0.4);

    const double coordinate = std::sqrt(1600.0 / 116.0);

    std::vector<wy::Vector3> points;
    EXPECT_EQ(intersectBounded(pEllipseA, pEllipseB, points), Result::Ok);
    ASSERT_EQ(points.size(), 4u);
    EXPECT_TRUE(hasPoint(points, wy::Vector3(coordinate, coordinate, 0.0), 1e-3));
    EXPECT_TRUE(hasPoint(points, wy::Vector3(coordinate, -coordinate, 0.0), 1e-3));
    EXPECT_TRUE(hasPoint(points, wy::Vector3(-coordinate, coordinate, 0.0), 1e-3));
    EXPECT_TRUE(hasPoint(points, wy::Vector3(-coordinate, -coordinate, 0.0), 1e-3));
}

TEST(Sketch3DCurveIntersection, ArcArcOnTheSameCircleIsCoincident)
{
    Fixture fixture;
    wy3d::SketchArc3D* pArcA = fixture.makeArc(wy::Vector3::kZero, 5.0, 0.0, wy3d::PI);
    wy3d::SketchArc3D* pArcB = fixture.makeArc(wy::Vector3::kZero, 5.0, wy3d::PI, wy3d::TWO_PI);

    std::vector<wy::Vector3> points;
    EXPECT_EQ(intersectBounded(pArcA, pArcB, points), Result::Coincident);
    EXPECT_TRUE(points.empty());
}

TEST(Sketch3DCurveIntersection, SplineLineCrossesTwice)
{
    Fixture fixture;
    wy3d::SketchSpline3D* pSpline = fixture.makeSplineByControlPoints(3u, archPoles());
    wy3d::SketchLine3D* pLine = fixture.makeLine(wy::Vector3(-20.0, -1.0, 0.0), wy::Vector3(20.0, -1.0, 0.0));

    std::vector<wy::Vector3> points;
    EXPECT_EQ(intersectBounded(pSpline, pLine, points), Result::Ok);
    ASSERT_EQ(points.size(), 2u);
    EXPECT_TRUE(hasPoint(points, wy::Vector3(-8.5064, -1.0, 0.0), 1e-3));
    EXPECT_TRUE(hasPoint(points, wy::Vector3(8.5064, -1.0, 0.0), 1e-3));
}

TEST(Sketch3DCurveIntersection, SplineCircle)
{
    Fixture fixture;
    wy3d::SketchSpline3D* pSpline = fixture.makeSplineByControlPoints(3u, archPoles());
    wy3d::SketchCircle3D* pCircle = fixture.makeCircle(wy::Vector3(0.0, -3.75, 0.0), 2.0);

    std::vector<wy::Vector3> points;
    EXPECT_EQ(intersectBounded(pSpline, pCircle, points), Result::Ok);
    EXPECT_GE(points.size(), 2u);
    for (const wy::Vector3& pnt : points)
    {
        EXPECT_NEAR((pnt - wy::Vector3(0.0, -3.75, 0.0)).length(), 2.0, 1e-3);
    }
}

// Extending a spline follows the tangent at its end, so a line standing on that tangent is only
// reached when the ray is asked for - and never in the other direction.
TEST(Sketch3DCurveIntersection, SplineTangentRayReachesForwardOnly)
{
    Fixture fixture;
    std::vector<wy::Vector3> poles = {wy::Vector3(0.0, 0.0, 0.0), wy::Vector3(10.0, 0.0, 0.0)};
    wy3d::SketchSpline3D* pSpline = fixture.makeSplineByControlPoints(1u, poles);
    wy3d::SketchLine3D* pLineAhead = fixture.makeLine(wy::Vector3(15.0, -5.0, 0.0), wy::Vector3(15.0, 5.0, 0.0));
    wy3d::SketchLine3D* pLineBehind = fixture.makeLine(wy::Vector3(-15.0, -5.0, 0.0), wy::Vector3(-15.0, 5.0, 0.0));

    std::vector<wy::Vector3> points;
    EXPECT_EQ(intersectBounded(pSpline, pLineAhead, points), Result::NoIntersection);

    points.clear();
    EXPECT_EQ(Util::intersect(Util::extended(pSpline, 30.0, false, true), Util::bounded(pLineAhead), points, kTol),
        Result::Ok);
    ASSERT_EQ(points.size(), 1u);
    EXPECT_TRUE(hasPoint(points, wy::Vector3(15.0, 0.0, 0.0), 1e-3));

    points.clear();
    EXPECT_EQ(Util::intersect(Util::extended(pSpline, 30.0, false, true), Util::bounded(pLineBehind), points, kTol),
        Result::NoIntersection);
    EXPECT_TRUE(points.empty());

    points.clear();
    EXPECT_EQ(Util::intersect(Util::extended(pSpline, 30.0, true, false), Util::bounded(pLineBehind), points, kTol),
        Result::Ok);
    ASSERT_EQ(points.size(), 1u);
    EXPECT_TRUE(hasPoint(points, wy::Vector3(-15.0, 0.0, 0.0), 1e-3));
}

TEST(Sketch3DCurveIntersection, SameCurveTwiceIsCoincident)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine = fixture.makeLine(wy::Vector3::kZero, wy::Vector3(10.0, 0.0, 0.0));

    std::vector<wy::Vector3> points;
    EXPECT_EQ(intersectBounded(pLine, pLine, points), Result::Coincident);
    EXPECT_TRUE(points.empty());
}

TEST(Sketch3DCurveIntersection, DegenerateOperandsAreRefused)
{
    Fixture fixture;
    wy3d::SketchLine3D* pLine = fixture.makeLine(wy::Vector3::kZero, wy::Vector3(10.0, 0.0, 0.0));
    wy3d::SketchLine3D* pDegenerate = fixture.makeLine(wy::Vector3::kZero, wy::Vector3::kZero);

    std::vector<wy::Vector3> points;
    EXPECT_EQ(intersectBounded(pDegenerate, pLine, points), Result::DegenerateInput);
    EXPECT_TRUE(points.empty());

    points.clear();
    EXPECT_EQ(Util::intersect(Util::bounded(nullptr), Util::bounded(pLine), points, kTol),
        Result::DegenerateInput);
    EXPECT_TRUE(points.empty());
}

TEST(Sketch3DCurveIntersection, ReachOnACircleChangesNothing)
{
    Fixture fixture;
    wy3d::SketchCircle3D* pCircle = fixture.makeCircle(wy::Vector3::kZero, 5.0);
    wy3d::SketchLine3D* pLine = fixture.makeLine(wy::Vector3(-10.0, 0.0, 0.0), wy::Vector3(10.0, 0.0, 0.0));

    std::vector<wy::Vector3> points;
    EXPECT_EQ(Util::intersect(Util::extended(pCircle, 50.0, true, true), Util::bounded(pLine), points, kTol),
        Result::Ok);
    EXPECT_EQ(points.size(), 2u);
}
