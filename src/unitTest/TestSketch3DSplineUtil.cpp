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

#include <utils/wy3dSketch3DSplineUtil.h>
#include <wy3dSketchSpline3D.h>

#include <GeomAPI_ProjectPointOnCurve.hxx>
#include <gp_Pnt.hxx>

#include <cfloat>
#include <memory>
#include <vector>

namespace
{
using Util = wy3d::Sketch3DSplineUtil;

// A degree 3 arch, y(t) = -15 t (1 - t), with an interior knot at t = 0.5 so the knot vector has
// something for the merge arithmetic to get wrong. Poles of the two halves are Bezier poles, so
// the curve is the arch on [0, 1] and again on [1, 2].
std::vector<wy::Vector3> archPoles()
{
    return {wy::Vector3(-10.0, 0.0, 0.0), wy::Vector3(-3.0, -5.0, 0.0),
        wy::Vector3(3.0, -5.0, 0.0), wy::Vector3(10.0, 0.0, 0.0)};
}

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

    wy3d::SketchSpline3D* makeBezier(std::uint32_t degree, const std::vector<wy::Vector3>& controlPoints)
    {
        wy3d::SketchSpline3D* pSpline = nullptr;
        EXPECT_EQ(wy3d::SketchSpline3D::create(pTrans, degree, controlPoints, pSpline), wy::ErrorStatus::Ok);
        return pSpline;
    }

    // Rebuilds an entity from B-spline data, which is exactly what a trim does with the output of
    // segment() or addLineSegmentToBSpline().
    wy3d::SketchSpline3D* makeFromData(unsigned int degree, const std::vector<wy::Vector3>& controlPoints,
        const std::vector<double>& knots, const std::vector<unsigned int>& multiplicities)
    {
        wy3d::SketchSpline3D* pSpline = nullptr;
        EXPECT_EQ(wy3d::SketchSpline3D::create(pTrans, degree, controlPoints, knots, multiplicities, pSpline),
            wy::ErrorStatus::Ok);
        return pSpline;
    }
};

double distanceTo(const Handle(Geom_BSplineCurve)& pBSpline, const wy::Vector3& pos)
{
    if (pBSpline.IsNull())
    {
        return DBL_MAX;
    }
    GeomAPI_ProjectPointOnCurve projector(gp_Pnt(pos.x(), pos.y(), pos.z()), pBSpline);
    return projector.NbPoints() > 0 ? projector.LowerDistance() : DBL_MAX;
}
} // namespace

TEST(Sketch3DSplineUtil, GetBSplineDataReadsBackWhatWasBuilt)
{
    Fixture fixture;
    wy3d::SketchSpline3D* pSpline = fixture.makeBezier(3, archPoles());
    Handle(Geom_BSplineCurve) pBSpline = pSpline->getOccSpline();
    ASSERT_FALSE(pBSpline.IsNull());

    unsigned int degree = 0;
    std::vector<wy::Vector3> controlPoints;
    std::vector<double> knots;
    std::vector<unsigned int> multiplicities;
    ASSERT_TRUE(Util::getBSplineData(pBSpline, degree, controlPoints, knots, multiplicities));

    EXPECT_EQ(degree, 3u);
    ASSERT_EQ(controlPoints.size(), archPoles().size());
    for (std::size_t i = 0; i < controlPoints.size(); ++i)
        EXPECT_NEAR((controlPoints[i] - archPoles()[i]).length(), 0.0, 1e-12) << "pole " << i;

    // Clamped at both ends of a single span, so two knots of multiplicity degree + 1.
    ASSERT_EQ(knots.size(), 2u);
    ASSERT_EQ(multiplicities.size(), 2u);
    EXPECT_NEAR(knots.front(), 0.0, 1e-12);
    EXPECT_EQ(multiplicities.front(), 4u);
    EXPECT_EQ(multiplicities.back(), 4u);
}

TEST(Sketch3DSplineUtil, SegmentKeepsTheShapeAndLandsOnTheCutPoints)
{
    Fixture fixture;
    wy3d::SketchSpline3D* pSpline = fixture.makeBezier(3, archPoles());
    Handle(Geom_BSplineCurve) pBSpline = pSpline->getOccSpline();
    ASSERT_FALSE(pBSpline.IsNull());

    unsigned int degree = 0;
    std::vector<wy::Vector3> controlPoints;
    std::vector<double> knots;
    std::vector<unsigned int> multiplicities;
    ASSERT_TRUE(Util::segment(pBSpline, 0.25, 0.75, degree, controlPoints, knots, multiplicities));

    wy3d::SketchSpline3D* pPiece = fixture.makeFromData(degree, controlPoints, knots, multiplicities);
    ASSERT_NE(pPiece, nullptr);
    EXPECT_EQ(pPiece->getDegree(), 3u);

    // The piece is reparameterized onto its own [0,1]: its ends are the cut points and its middle
    // is the source's middle.
    EXPECT_NEAR((pPiece->getPointAt(0.0) - pSpline->getPointAt(0.25)).length(), 0.0, 1e-9);
    EXPECT_NEAR((pPiece->getPointAt(1.0) - pSpline->getPointAt(0.75)).length(), 0.0, 1e-9);
    EXPECT_NEAR((pPiece->getPointAt(0.5) - pSpline->getPointAt(0.5)).length(), 0.0, 1e-9);

    // And it stays on the source everywhere, not just at the three sampled parameters.
    for (int i = 0; i <= 10; ++i)
    {
        const double t = static_cast<double>(i) / 10.0;
        EXPECT_NEAR(distanceTo(pBSpline, pPiece->getPointAt(t)), 0.0, 1e-9) << "t = " << t;
    }
}

TEST(Sketch3DSplineUtil, SegmentFromTheStartIsJustTheHead)
{
    Fixture fixture;
    wy3d::SketchSpline3D* pSpline = fixture.makeBezier(3, archPoles());
    Handle(Geom_BSplineCurve) pBSpline = pSpline->getOccSpline();
    ASSERT_FALSE(pBSpline.IsNull());

    unsigned int degree = 0;
    std::vector<wy::Vector3> controlPoints;
    std::vector<double> knots;
    std::vector<unsigned int> multiplicities;
    ASSERT_TRUE(Util::segment(pBSpline, 0.0, 0.5, degree, controlPoints, knots, multiplicities));

    wy3d::SketchSpline3D* pPiece = fixture.makeFromData(degree, controlPoints, knots, multiplicities);
    ASSERT_NE(pPiece, nullptr);

    EXPECT_NEAR((pPiece->getPointAt(0.0) - pSpline->getPointAt(0.0)).length(), 0.0, 1e-9);
    EXPECT_NEAR((pPiece->getPointAt(1.0) - pSpline->getPointAt(0.5)).length(), 0.0, 1e-9);
}

TEST(Sketch3DSplineUtil, AppendLineSegmentReachesTheNewPoint)
{
    Fixture fixture;
    wy3d::SketchSpline3D* pSpline = fixture.makeBezier(3, archPoles());
    Handle(Geom_BSplineCurve) pBSpline = pSpline->getOccSpline();
    ASSERT_FALSE(pBSpline.IsNull());

    // Out along the end tangent, which is the direction (7, 5, 0).
    const wy::Vector3 tip(13.0, 5.0, 0.0);
    Handle(Geom_BSplineCurve) pMerged = Util::addLineSegmentToBSpline(pBSpline, tip, false);
    ASSERT_FALSE(pMerged.IsNull());
    EXPECT_EQ(pMerged->Degree(), 3);

    unsigned int degree = 0;
    std::vector<wy::Vector3> controlPoints;
    std::vector<double> knots;
    std::vector<unsigned int> multiplicities;
    ASSERT_TRUE(Util::getBSplineData(pMerged, degree, controlPoints, knots, multiplicities));

    wy3d::SketchSpline3D* pResult = fixture.makeFromData(degree, controlPoints, knots, multiplicities);
    ASSERT_NE(pResult, nullptr);

    // The far end is the new point, the near end is still the source's start, and the whole
    // source is still on the result.
    EXPECT_NEAR((pResult->getPointAt(1.0) - tip).length(), 0.0, 1e-9);
    EXPECT_NEAR((pResult->getPointAt(0.0) - pSpline->getPointAt(0.0)).length(), 0.0, 1e-9);
    for (int i = 0; i <= 10; ++i)
    {
        const double t = static_cast<double>(i) / 10.0;
        EXPECT_NEAR(distanceTo(pMerged, pSpline->getPointAt(t)), 0.0, 1e-9) << "t = " << t;
    }
}

TEST(Sketch3DSplineUtil, PrependLineSegmentReachesTheNewPoint)
{
    Fixture fixture;
    wy3d::SketchSpline3D* pSpline = fixture.makeBezier(3, archPoles());
    Handle(Geom_BSplineCurve) pBSpline = pSpline->getOccSpline();
    ASSERT_FALSE(pBSpline.IsNull());

    // Out along the start tangent, which is the direction (7, -5, 0).
    const wy::Vector3 tip(-13.0, -5.0, 0.0);
    Handle(Geom_BSplineCurve) pMerged = Util::addLineSegmentToBSpline(pBSpline, tip, true);
    ASSERT_FALSE(pMerged.IsNull());
    EXPECT_EQ(pMerged->Degree(), 3);

    unsigned int degree = 0;
    std::vector<wy::Vector3> controlPoints;
    std::vector<double> knots;
    std::vector<unsigned int> multiplicities;
    ASSERT_TRUE(Util::getBSplineData(pMerged, degree, controlPoints, knots, multiplicities));

    wy3d::SketchSpline3D* pResult = fixture.makeFromData(degree, controlPoints, knots, multiplicities);
    ASSERT_NE(pResult, nullptr);

    EXPECT_NEAR((pResult->getPointAt(0.0) - tip).length(), 0.0, 1e-9);
    EXPECT_NEAR((pResult->getPointAt(1.0) - pSpline->getPointAt(1.0)).length(), 0.0, 1e-9);
    for (int i = 0; i <= 10; ++i)
    {
        const double t = static_cast<double>(i) / 10.0;
        EXPECT_NEAR(distanceTo(pMerged, pSpline->getPointAt(t)), 0.0, 1e-9) << "t = " << t;
    }
}

// The middle of a spline is what a trim keeps, so appending at one end must not disturb the span
// the source already covered: the merged curve has to be the source plus a line, no more.
TEST(Sketch3DSplineUtil, AppendedLineIsStraightAfterTheJoint)
{
    Fixture fixture;
    wy3d::SketchSpline3D* pSpline = fixture.makeBezier(3, archPoles());
    Handle(Geom_BSplineCurve) pBSpline = pSpline->getOccSpline();
    ASSERT_FALSE(pBSpline.IsNull());

    const wy::Vector3 tip(13.0, 5.0, 0.0);
    Handle(Geom_BSplineCurve) pMerged = Util::addLineSegmentToBSpline(pBSpline, tip, false);
    ASSERT_FALSE(pMerged.IsNull());

    // The line hangs off the source's end point, so from there out to the tip the merged curve is
    // the straight chord itself.
    const wy::Vector3 joint(10.0, 0.0, 0.0);
    for (int i = 1; i <= 4; ++i)
    {
        const double fraction = static_cast<double>(i) / 4.0;
        const wy::Vector3 onChord = joint + (tip - joint) * fraction;
        EXPECT_NEAR(distanceTo(pMerged, onChord), 0.0, 1e-9) << "fraction = " << fraction;
    }
}
