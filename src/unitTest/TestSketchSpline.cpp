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

#include <wy3dSketchSpline.h>
#include <wy3dMath.h>
#include <geom/wy3dMatrix3.h>
#include <Geom2dAPI_Interpolate.hxx>
#include <Geom2dAPI_ProjectPointOnCurve.hxx>
#include <TColgp_HArray1OfPnt2d.hxx>

#include <cmath>
#include <functional>
#include <limits>

namespace
{
    std::vector<wy::Vector2> openFitPoints()
    {
        return {
            wy::Vector2(0.0, 0.0),
            wy::Vector2(10.0, 6.0),
            wy::Vector2(20.0, -6.0),
            wy::Vector2(30.0, 0.0) };
    }

    std::vector<wy::Vector2> closedFitPoints()
    {
        return {
            wy::Vector2(0.0, 0.0),
            wy::Vector2(10.0, 6.0),
            wy::Vector2(20.0, 0.0),
            wy::Vector2(10.0, -6.0),
            wy::Vector2(0.0, 0.0) };
    }

    Handle(Geom2d_BSplineCurve) createSpline(wy3d::Database* pDb,
        const std::vector<wy::Vector2>& fitPoints, wydb::ElementId& outId)
    {
        wydb::TransactionManager* pMgr = pDb->getTransactionManager();
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::SketchSpline* pSpline(nullptr);
        const wy::ErrorStatus error = wy3d::SketchSpline::create(pTrans, fitPoints, pSpline);
        if (wy::ErrorStatus::Ok != error)
        {
            pMgr->abortTransaction();
            return nullptr;
        }
        if (wy::ErrorStatus::Ok != pMgr->endTransaction()) return nullptr;
        outId = pSpline->getId();
        return pSpline->getOccSpline();
    }

    wy::ErrorStatus editSpline(wy3d::Database* pDb, const wydb::ElementId& id,
        const std::function<wy::ErrorStatus(wy3d::SketchSpline*)>& edit)
    {
        wydb::TransactionManager* pMgr = pDb->getTransactionManager();
        wydb::Transaction* pTrans = pMgr->startTransaction();
        if (!pTrans) return wy::ErrorStatus::Error;

        wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pTrans->getElementForWrite(id));
        if (!pSpline)
        {
            pMgr->abortTransaction();
            return wy::ErrorStatus::Error;
        }

        const wy::ErrorStatus error = edit(pSpline);
        if (wy::ErrorStatus::Ok == error)
        {
            if (wy::ErrorStatus::Ok != pMgr->endTransaction()) return wy::ErrorStatus::Error;
            return wy::ErrorStatus::Ok;
        }
        pMgr->abortTransaction();
        return error;
    }

    // Writes the table as a whole, the way the property panel does: what is already stored for the
    // other points is read back and written out with it. An index the table has no entry for is
    // taken out to that point rather than refused here, so the call reaches the checks that judge
    // the table itself
    wy::ErrorStatus setTangentAt(wy3d::SketchSpline* pSpline, std::size_t index,
        double angle, double magnitude, bool isDriving = true)
    {
        std::vector<wy3d::SketchSpline::Tangent> tangents = pSpline->getTangents();
        if (index >= tangents.size()) tangents.resize(index + 1);

        tangents[index].angle = angle;
        tangents[index].magnitude = magnitude;
        tangents[index].isDriving = isDriving;
        return pSpline->setTangents(tangents);
    }

    // The point keeps the tangent it already runs with: only the flag changes
    wy::ErrorStatus setTangentDrivingAt(wy3d::SketchSpline* pSpline, std::size_t index, bool isDriving)
    {
        std::vector<wy3d::SketchSpline::Tangent> tangents = pSpline->getTangents();
        if (index >= tangents.size()) tangents.resize(index + 1);

        tangents[index].isDriving = isDriving;
        return pSpline->setTangents(tangents);
    }

    std::vector<wy3d::SketchSpline::Tangent> tangentTable(std::size_t count,
        double angle, double magnitude, bool isDriving = false)
    {
        std::vector<wy3d::SketchSpline::Tangent> tangents(count);
        for (wy3d::SketchSpline::Tangent& tangent : tangents)
        {
            tangent.angle = angle;
            tangent.magnitude = magnitude;
            tangent.isDriving = isDriving;
        }
        return tangents;
    }

    // The point lies on the curve, so the closest point of the curve is the point itself
    double distanceToCurve(const Handle(Geom2d_BSplineCurve)& pCurve, const wy::Vector2& point)
    {
        if (pCurve.IsNull()) return std::numeric_limits<double>::max();
        Geom2dAPI_ProjectPointOnCurve projector(gp_Pnt2d(point.x(), point.y()), pCurve);
        if (projector.NbPoints() < 1) return std::numeric_limits<double>::max();
        return projector.LowerDistance();
    }

    // Angle and parametric speed of the curve at fit point <index>. Interpolated splines pass
    // through fit point i at knot i (open curves only).
    void tangentAtFitPoint(const Handle(Geom2d_BSplineCurve)& pCurve, std::size_t index,
        double& outAngle, double& outSpeed)
    {
        gp_Pnt2d pnt;
        gp_Vec2d derivative;
        pCurve->D1(pCurve->Knot(static_cast<Standard_Integer>(index) + 1), pnt, derivative);
        outAngle = std::atan2(derivative.Y(), derivative.X());
        outSpeed = derivative.Magnitude();
    }

    // A closed spline is periodic and lays its knots out differently, so the fit point has to
    // be located on the curve instead of looked up in the knot table
    void tangentAtPoint(const Handle(Geom2d_BSplineCurve)& pCurve, const wy::Vector2& point,
        double& outAngle, double& outSpeed)
    {
        Geom2dAPI_ProjectPointOnCurve projector(gp_Pnt2d(point.x(), point.y()), pCurve);
        ASSERT_GE(projector.NbPoints(), 1);
        gp_Pnt2d pnt;
        gp_Vec2d derivative;
        pCurve->D1(projector.LowerDistanceParameter(), pnt, derivative);
        outAngle = std::atan2(derivative.Y(), derivative.X());
        outSpeed = derivative.Magnitude();
    }

    std::vector<wy::Vector2> sampleCurve(const Handle(Geom2d_BSplineCurve)& pCurve, int count)
    {
        std::vector<wy::Vector2> samples;
        if (pCurve.IsNull()) return samples;
        const double first = pCurve->FirstParameter();
        const double last = pCurve->LastParameter();
        for (int i = 0; i <= count; ++i)
        {
            const gp_Pnt2d pnt = pCurve->Value(first + (last - first) * i / count);
            samples.emplace_back(pnt.X(), pnt.Y());
        }
        return samples;
    }

    double maxDeviation(const std::vector<wy::Vector2>& a, const std::vector<wy::Vector2>& b)
    {
        if (a.size() != b.size()) return std::numeric_limits<double>::max();
        double maxDeviation(0.0);
        for (std::size_t i = 0; i < a.size(); ++i)
        {
            maxDeviation = std::max(maxDeviation, (a[i] - b[i]).length());
        }
        return maxDeviation;
    }

    // A magnitude is measured against the span leaving the point, whose parameter is one chord long,
    // so the parametric speed the curve runs with there is the magnitude over that chord
    void expectSpanSpeed(double speed, double magnitude, double chordLength)
    {
        const double expected = magnitude / chordLength;
        EXPECT_NEAR(speed, expected, 1e-6 * std::fabs(expected) + 1e-9);
    }

    // The chord a fit point's magnitude is measured against: the span leaving it, or the span
    // arriving at it for the last point, which has none leaving
    double spanChord(const std::vector<wy::Vector2>& fitPoints, std::size_t index)
    {
        if (index + 1 < fitPoints.size()) return (fitPoints[index + 1] - fitPoints[index]).length();
        return (fitPoints[index] - fitPoints[index - 1]).length();
    }
}

// --- Data model ---

TEST(SketchSpline, TangentDefaultsToCurve)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId splineId = wydb::ElementId::kNull;
    const std::vector<wy::Vector2> fitPoints = openFitPoints();
    Handle(Geom2d_BSplineCurve) pCurve = createSpline(pDb.get(), fitPoints, splineId);
    ASSERT_FALSE(pCurve.IsNull());

    const wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pDb->getElement(splineId));
    ASSERT_NE(pSpline, nullptr);
    EXPECT_EQ(pSpline->getTangents().size(), fitPoints.size());
    // Nothing drives the shape, so every point holds the tangency of the curve as it was built
    for (std::size_t i = 0; i < fitPoints.size(); ++i)
    {
        const wy3d::SketchSpline::Tangent tangent = pSpline->getTangentAt(i);
        EXPECT_FALSE(tangent.isDriving);
        double angle(0.0), speed(0.0);
        tangentAtFitPoint(pCurve, i, angle, speed);
        EXPECT_NEAR(tangent.angle, wy3d::normalizeRadian(angle), 1e-9);
        EXPECT_NEAR(tangent.magnitude, speed * spanChord(fitPoints, i), 1e-9);
    }
    // Out of range reads back a default instead of asserting
    EXPECT_FALSE(pSpline->getTangentAt(fitPoints.size() + 5).isDriving);
}

TEST(SketchSpline, CachedTangentIsInert)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId splineId = wydb::ElementId::kNull;
    const std::vector<wy::Vector2> fitPoints = openFitPoints();
    Handle(Geom2d_BSplineCurve) pCurve = createSpline(pDb.get(), fitPoints, splineId);
    ASSERT_FALSE(pCurve.IsNull());

    const wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pDb->getElement(splineId));
    ASSERT_NE(pSpline, nullptr);
    // Every point carries a tangency, and the curve is still the interpolation of the points
    // alone: a tangency only reaches the curve through the flag that drives it
    for (std::size_t i = 0; i < fitPoints.size(); ++i)
    {
        EXPECT_GT(std::fabs(pSpline->getTangents()[i].magnitude), wy3d::TOL);
    }

    Handle(TColgp_HArray1OfPnt2d) pOccPoints =
        new TColgp_HArray1OfPnt2d(1, static_cast<Standard_Integer>(fitPoints.size()));
    for (std::size_t i = 0; i < fitPoints.size(); ++i)
    {
        pOccPoints->SetValue(static_cast<Standard_Integer>(i) + 1,
            gp_Pnt2d(fitPoints[i].x(), fitPoints[i].y()));
    }
    Geom2dAPI_Interpolate interpolator(pOccPoints, Standard_False, wy3d::TOL);
    interpolator.Perform();
    ASSERT_TRUE(interpolator.IsDone());
    EXPECT_LT(maxDeviation(sampleCurve(interpolator.Curve(), 40), sampleCurve(pCurve, 40)), 1e-9);
}

TEST(SketchSpline, KnotMatchesFitPoint)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId splineId = wydb::ElementId::kNull;
    const std::vector<wy::Vector2> fitPoints = openFitPoints();
    Handle(Geom2d_BSplineCurve) pCurve = createSpline(pDb.get(), fitPoints, splineId);
    ASSERT_FALSE(pCurve.IsNull());

    // What the table holds for a point rests on this: fit point i sits at knot i
    for (std::size_t i = 0; i < fitPoints.size(); ++i)
    {
        EXPECT_NEAR(distanceToCurve(pCurve, fitPoints[i]), 0.0, 1e-7);
        const gp_Pnt2d pnt = pCurve->Value(pCurve->Knot(static_cast<Standard_Integer>(i) + 1));
        EXPECT_NEAR((wy::Vector2(pnt.X(), pnt.Y()) - fitPoints[i]).length(), 0.0, 1e-7);
    }

    Standard_Integer totalMultiplicity(0);
    const TColStd_Array1OfInteger& multiplicities = pCurve->Multiplicities();
    for (Standard_Integer i = multiplicities.Lower(); i <= multiplicities.Upper(); ++i)
    {
        totalMultiplicity += multiplicities.Value(i);
    }
    EXPECT_EQ(totalMultiplicity, pCurve->NbPoles() + pCurve->Degree() + 1);
    EXPECT_EQ(multiplicities.Value(multiplicities.Lower()), pCurve->Degree() + 1);
    EXPECT_EQ(multiplicities.Value(multiplicities.Upper()), pCurve->Degree() + 1);
    for (Standard_Integer i = multiplicities.Lower() + 1; i < multiplicities.Upper(); ++i)
    {
        EXPECT_EQ(multiplicities.Value(i), 1);
    }
}

TEST(SketchSpline, StoredTangentMatchesCurve)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId splineId = wydb::ElementId::kNull;
    const std::vector<wy::Vector2> fitPoints = openFitPoints();
    Handle(Geom2d_BSplineCurve) pCurve = createSpline(pDb.get(), fitPoints, splineId);
    ASSERT_FALSE(pCurve.IsNull());

    const wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pDb->getElement(splineId));
    ASSERT_NE(pSpline, nullptr);

    // What the property panel reads is what the curve does, point by point
    wy3d::SketchSpline::Tangent shown;
    for (std::size_t i = 0; i < fitPoints.size(); ++i)
    {
        double angle(0.0), speed(0.0);
        tangentAtFitPoint(pCurve, i, angle, speed);
        const wy3d::SketchSpline::Tangent tangent = pSpline->getTangentAt(i);
        EXPECT_FALSE(tangent.isDriving);
        EXPECT_NEAR(tangent.angle, wy3d::normalizeRadian(angle), 1e-9);
        // The magnitude is that tangency measured in the span's own parameter, so it is the
        // parametric speed of the curve times the chord length of the span
        EXPECT_NEAR(tangent.magnitude, speed * spanChord(fitPoints, i), 1e-9);
        if (1 == i) shown = tangent;
    }

    // Driving the point takes over the tangency that was already there, so the panel does not jump
    EXPECT_EQ(editSpline(pDb.get(), splineId, [](wy3d::SketchSpline* pSpline)
        { return setTangentDrivingAt(pSpline, 1, true); }), wy::ErrorStatus::Ok);
    pSpline = wy3d::SketchSpline::cast(pDb->getElement(splineId));
    ASSERT_NE(pSpline, nullptr);
    EXPECT_TRUE(pSpline->getTangentAt(1).isDriving);
    EXPECT_NEAR(pSpline->getTangentAt(1).angle, shown.angle, 1e-12);
    EXPECT_DOUBLE_EQ(pSpline->getTangentAt(1).magnitude, shown.magnitude);
}

TEST(SketchSpline, RefreshOnDrivenCurve)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId splineId = wydb::ElementId::kNull;
    const std::vector<wy::Vector2> fitPoints = openFitPoints();
    ASSERT_FALSE(createSpline(pDb.get(), fitPoints, splineId).IsNull());

    const double chord = spanChord(fitPoints, 1);
    EXPECT_EQ(editSpline(pDb.get(), splineId, [chord](wy3d::SketchSpline* pSpline)
        { return setTangentAt(pSpline, 1, wy3d::PI * 0.4, 2.5 * chord); }), wy::ErrorStatus::Ok);

    const wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pDb->getElement(splineId));
    ASSERT_NE(pSpline, nullptr);
    Handle(Geom2d_BSplineCurve) pCurve = pSpline->getOccSpline();
    ASSERT_FALSE(pCurve.IsNull());

    double total(0.0);
    for (std::size_t i = 1; i < fitPoints.size(); ++i)
    {
        total += (fitPoints[i] - fitPoints[i - 1]).length();
    }
    ASSERT_GT(total, 0.0);

    // A curve with a driven point lays its fit points out along the accumulated chord length just
    // the same, which is what puts the points that are not driven on the curve
    const double first = pCurve->FirstParameter();
    const double last = pCurve->LastParameter();
    double prefix(0.0);
    for (std::size_t i = 0; i < fitPoints.size(); ++i)
    {
        if (i > 0) prefix += (fitPoints[i] - fitPoints[i - 1]).length();
        const double curveParam = first + (last - first) * (prefix / total);
        gp_Pnt2d pnt;
        gp_Vec2d derivative;
        pCurve->D1(curveParam, pnt, derivative);
        EXPECT_NEAR((wy::Vector2(pnt.X(), pnt.Y()) - fitPoints[i]).length(), 0.0, 1e-9);

        const wy3d::SketchSpline::Tangent tangent = pSpline->getTangentAt(i);
        if (1 == i)
        {
            EXPECT_TRUE(tangent.isDriving);
            continue;
        }
        EXPECT_FALSE(tangent.isDriving);
        EXPECT_NEAR(tangent.angle,
            wy3d::normalizeRadian(std::atan2(derivative.Y(), derivative.X())), 1e-9);
        EXPECT_NEAR(tangent.magnitude, derivative.Magnitude() * spanChord(fitPoints, i), 1e-9);
    }
}

TEST(SketchSpline, RefreshFollowsPointEdit)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId splineId = wydb::ElementId::kNull;
    const std::vector<wy::Vector2> fitPoints = openFitPoints();
    ASSERT_FALSE(createSpline(pDb.get(), fitPoints, splineId).IsNull());

    const wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pDb->getElement(splineId));
    ASSERT_NE(pSpline, nullptr);
    const wy3d::SketchSpline::Tangent before = pSpline->getTangentAt(1);

    // Moving a point moves the curve under the points that are not driven, and what they hold
    // follows it
    std::vector<wy::Vector2> movedPoints = fitPoints;
    movedPoints[3] = wy::Vector2(30.0, 12.0);
    EXPECT_EQ(editSpline(pDb.get(), splineId, [movedPoints](wy3d::SketchSpline* pSpline)
        { return pSpline->setPoints(movedPoints); }), wy::ErrorStatus::Ok);

    pSpline = wy3d::SketchSpline::cast(pDb->getElement(splineId));
    ASSERT_NE(pSpline, nullptr);
    Handle(Geom2d_BSplineCurve) pNewCurve = pSpline->getOccSpline();
    ASSERT_FALSE(pNewCurve.IsNull());
    double angle(0.0), speed(0.0);
    tangentAtFitPoint(pNewCurve, 1, angle, speed);
    const wy3d::SketchSpline::Tangent after = pSpline->getTangentAt(1);
    EXPECT_FALSE(after.isDriving);
    EXPECT_NEAR(after.angle, wy3d::normalizeRadian(angle), 1e-9);
    EXPECT_NEAR(after.magnitude, speed * spanChord(movedPoints, 1), 1e-9);
    EXPECT_NE(after.angle, before.angle);
    EXPECT_NE(after.magnitude, before.magnitude);
}

TEST(SketchSpline, PointCountChangeNeedsAFreshTable)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId splineId = wydb::ElementId::kNull;
    const std::vector<wy::Vector2> fitPoints = openFitPoints();
    ASSERT_FALSE(createSpline(pDb.get(), fitPoints, splineId).IsNull());

    EXPECT_EQ(editSpline(pDb.get(), splineId, [](wy3d::SketchSpline* pSpline)
        { return setTangentAt(pSpline, 0, wy3d::PI / 6.0, 20.0); }), wy::ErrorStatus::Ok);

    // The point list is replaced outright, and the table has to be replaced with it: a table
    // measured against the list it had describes points that are not these ones, and keeping the
    // two in step is the caller's job, so this is where a fresh one is written rather than the
    // constraint of the first point landing on whichever point ends up first now
    std::vector<wy::Vector2> grownPoints = fitPoints;
    grownPoints.emplace_back(40.0, 6.0);
    EXPECT_EQ(editSpline(pDb.get(), splineId, [&grownPoints](wy3d::SketchSpline* pSpline)
        {
            const wy::ErrorStatus error = pSpline->setPoints(grownPoints);
            if (wy::ErrorStatus::Ok != error) return error;
            return pSpline->setTangents(tangentTable(grownPoints.size(), 0.0, 1.0));
        }), wy::ErrorStatus::Ok);

    const wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pDb->getElement(splineId));
    ASSERT_NE(pSpline, nullptr);
    ASSERT_EQ(pSpline->getTangents().size(), grownPoints.size());
    for (const wy3d::SketchSpline::Tangent& tangent : pSpline->getTangents())
    {
        EXPECT_FALSE(tangent.isDriving);
    }
    // The fresh table rebuilt the curve from the new point list
    for (std::size_t i = 0; i < grownPoints.size(); ++i)
    {
        EXPECT_NEAR(distanceToCurve(pSpline->getOccSpline(), grownPoints[i]), 0.0, 1e-7);
    }
}

// --- How the magnitude drives the curve ---

TEST(SketchSpline, NeutralTangentKeepsShape)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId splineId = wydb::ElementId::kNull;
    const std::vector<wy::Vector2> fitPoints = openFitPoints();
    Handle(Geom2d_BSplineCurve) pCurve = createSpline(pDb.get(), fitPoints, splineId);
    ASSERT_FALSE(pCurve.IsNull());
    const std::vector<wy::Vector2> before = sampleCurve(pCurve, 40);

    double naturalAngle(0.0), naturalSpeed(0.0);
    tangentAtFitPoint(pCurve, 1, naturalAngle, naturalSpeed);
    ASSERT_GT(naturalSpeed, 0.0);

    EXPECT_EQ(editSpline(pDb.get(), splineId, [](wy3d::SketchSpline* pSpline)
        { return setTangentDrivingAt(pSpline, 1, true); }), wy::ErrorStatus::Ok);

    const wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pDb->getElement(splineId));
    ASSERT_NE(pSpline, nullptr);
    const wy3d::SketchSpline::Tangent tangent = pSpline->getTangentAt(1);
    EXPECT_TRUE(tangent.isDriving);
    EXPECT_NEAR(tangent.angle, wy3d::normalizeRadian(naturalAngle), 1e-9);
    // Driving the point takes over the tangency the spline already has, so the shape must not
    // move; the magnitude of a spline the points alone give is about the chord length there
    EXPECT_NEAR(tangent.magnitude, naturalSpeed * spanChord(fitPoints, 1), 1e-9);
    Handle(Geom2d_BSplineCurve) pNewCurve = pSpline->getOccSpline();
    ASSERT_FALSE(pNewCurve.IsNull());
    EXPECT_LT(maxDeviation(before, sampleCurve(pNewCurve, 40)), 1e-6);
    // And it is still the tangency the curve runs with
    double angle(0.0), speed(0.0);
    tangentAtFitPoint(pNewCurve, 1, angle, speed);
    expectSpanSpeed(speed, tangent.magnitude, spanChord(fitPoints, 1));
}

TEST(SketchSpline, WeightScalesTheParametricSpeed)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId splineId = wydb::ElementId::kNull;
    const std::vector<wy::Vector2> fitPoints = openFitPoints();
    ASSERT_FALSE(createSpline(pDb.get(), fitPoints, splineId).IsNull());

    // The span leaving the point is one chord long, so a magnitude of three chords asks the curve to
    // run through the point three times as fast as its parameter does
    const double chord = spanChord(fitPoints, 1);
    const double requestedAngle = wy3d::PI * 0.5;
    const double requestedWeight = 3.0 * chord;
    EXPECT_EQ(editSpline(pDb.get(), splineId,
        [requestedAngle, requestedWeight](wy3d::SketchSpline* pSpline)
        { return setTangentAt(pSpline, 1, requestedAngle, requestedWeight); }),
        wy::ErrorStatus::Ok);

    const wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pDb->getElement(splineId));
    ASSERT_NE(pSpline, nullptr);
    Handle(Geom2d_BSplineCurve) pNewCurve = pSpline->getOccSpline();
    ASSERT_FALSE(pNewCurve.IsNull());

    // The point is still interpolated and its tangent is the one that was asked for
    EXPECT_NEAR(distanceToCurve(pNewCurve, fitPoints[1]), 0.0, 1e-7);
    double angle(0.0), speed(0.0);
    tangentAtFitPoint(pNewCurve, 1, angle, speed);
    EXPECT_NEAR(wy3d::normalizeRadian(angle), wy3d::normalizeRadian(requestedAngle), 1e-6);
    EXPECT_NEAR(speed, 3.0, 1e-6);
    expectSpanSpeed(speed, requestedWeight, chord);
    // The other fit points are untouched
    for (std::size_t i = 0; i < fitPoints.size(); ++i)
    {
        if (1 == i) continue;
        EXPECT_NEAR(distanceToCurve(pNewCurve, fitPoints[i]), 0.0, 1e-7);
    }
}

TEST(SketchSpline, NegativeWeightIsRefused)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId splineId = wydb::ElementId::kNull;
    const std::vector<wy::Vector2> fitPoints = openFitPoints();
    ASSERT_FALSE(createSpline(pDb.get(), fitPoints, splineId).IsNull());

    const wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pDb->getElement(splineId));
    ASSERT_NE(pSpline, nullptr);
    const wy3d::SketchSpline::Tangent before = pSpline->getTangentAt(1);
    const std::vector<wy::Vector2> beforeShape = sampleCurve(pSpline->getOccSpline(), 40);

    // The magnitude of a tangent is a length, and a length is not negative: aiming the tangent the
    // other way is what the angle is for, so a table holding one is refused
    EXPECT_EQ(editSpline(pDb.get(), splineId, [](wy3d::SketchSpline* pSpline)
        { return setTangentAt(pSpline, 1, wy3d::PI * 0.5, -2.0); }), wy::ErrorStatus::InvalidInput);

    // Nothing of it was stored, and the curve is the one it was
    pSpline = wy3d::SketchSpline::cast(pDb->getElement(splineId));
    ASSERT_NE(pSpline, nullptr);
    EXPECT_EQ(pSpline->getTangentAt(1), before);
    EXPECT_LT(maxDeviation(beforeShape, sampleCurve(pSpline->getOccSpline(), 40)), 1e-12);

    // The tangency that write asked for - the one on the other side - is the angle's to say, and
    // told that way it goes in
    const double requestedAngle = wy3d::PI * 1.5;
    EXPECT_EQ(editSpline(pDb.get(), splineId, [requestedAngle](wy3d::SketchSpline* pSpline)
        { return setTangentAt(pSpline, 1, requestedAngle, 2.0); }), wy::ErrorStatus::Ok);

    pSpline = wy3d::SketchSpline::cast(pDb->getElement(splineId));
    ASSERT_NE(pSpline, nullptr);
    EXPECT_DOUBLE_EQ(pSpline->getTangentAt(1).magnitude, 2.0);
    EXPECT_TRUE(pSpline->getTangentAt(1).isDriving);
    double angle(0.0), speed(0.0);
    tangentAtFitPoint(pSpline->getOccSpline(), 1, angle, speed);
    EXPECT_NEAR(wy3d::normalizeRadian(angle), wy3d::normalizeRadian(requestedAngle), 1e-6);
    expectSpanSpeed(speed, 2.0, spanChord(fitPoints, 1));
}

TEST(SketchSpline, TangentRejectsZeroWeight)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId splineId = wydb::ElementId::kNull;
    const std::vector<wy::Vector2> fitPoints = openFitPoints();
    ASSERT_FALSE(createSpline(pDb.get(), fitPoints, splineId).IsNull());

    // A tangent of no size is nothing to aim the curve along
    EXPECT_EQ(editSpline(pDb.get(), splineId, [](wy3d::SketchSpline* pSpline)
        { return setTangentAt(pSpline, 1, 0.0, 0.0); }), wy::ErrorStatus::InvalidInput);
    EXPECT_EQ(editSpline(pDb.get(), splineId, [](wy3d::SketchSpline* pSpline)
        { return setTangentAt(pSpline, 1, 0.0, wy3d::TOL * 0.5); }), wy::ErrorStatus::InvalidInput);
    EXPECT_EQ(editSpline(pDb.get(), splineId, [](wy3d::SketchSpline* pSpline)
        { return setTangentAt(pSpline, 1, std::numeric_limits<double>::quiet_NaN(), 1.0); }),
        wy::ErrorStatus::InvalidInput);
    EXPECT_EQ(editSpline(pDb.get(), splineId, [](wy3d::SketchSpline* pSpline)
        { return setTangentAt(pSpline, pSpline->getPoints().size(), 0.0, 1.0); }),
        wy::ErrorStatus::InvalidInput);
    // Nothing was stored by the attempts above: the point holds the tangency of the curve
    const wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pDb->getElement(splineId));
    ASSERT_NE(pSpline, nullptr);
    EXPECT_FALSE(pSpline->getTangentAt(1).isDriving);
    double angle(0.0), speed(0.0);
    tangentAtFitPoint(pSpline->getOccSpline(), 1, angle, speed);
    EXPECT_NEAR(pSpline->getTangentAt(1).angle, wy3d::normalizeRadian(angle), 1e-9);
    EXPECT_NEAR(pSpline->getTangentAt(1).magnitude, speed * spanChord(fitPoints, 1), 1e-9);
}

TEST(SketchSpline, TangentRefusedOnControlPoints)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    const std::vector<wy::Vector2> controlPoints = {
        wy::Vector2(0.0, 0.0),
        wy::Vector2(10.0, 10.0),
        wy::Vector2(20.0, 10.0),
        wy::Vector2(30.0, 0.0) };

    wydb::ElementId splineId = wydb::ElementId::kNull;
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::SketchSpline* pSpline(nullptr);
        ASSERT_EQ(wy3d::SketchSpline::create(pTrans, 3, controlPoints, pSpline), wy::ErrorStatus::Ok);
        ASSERT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
        splineId = pSpline->getId();
    }

    EXPECT_EQ(editSpline(pDb.get(), splineId, [](wy3d::SketchSpline* pSpline)
        { return setTangentDrivingAt(pSpline, 1, true); }), wy::ErrorStatus::NotCurrentlyAllowed);
    EXPECT_EQ(editSpline(pDb.get(), splineId, [](wy3d::SketchSpline* pSpline)
        { return setTangentAt(pSpline, 1, 0.0, 1.0); }), wy::ErrorStatus::NotCurrentlyAllowed);
    // A whole table of usable tangents is refused just the same: there is no interpolation here to
    // constrain, however it is written
    const std::vector<wy3d::SketchSpline::Tangent> tangents =
        tangentTable(controlPoints.size(), 0.0, 1.0);
    EXPECT_EQ(editSpline(pDb.get(), splineId, [&tangents](wy3d::SketchSpline* pSpline)
        { return pSpline->setTangents(tangents); }), wy::ErrorStatus::NotCurrentlyAllowed);
    // Nothing was put in the table on the way, so the rows a control point spline shows read as
    // the defaults they always did
    const wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pDb->getElement(splineId));
    ASSERT_NE(pSpline, nullptr);
    EXPECT_EQ(pSpline->getTangentAt(1), wy3d::SketchSpline::Tangent());
}

TEST(SketchSpline, TangentTableTakesOnlyAFullUsableOne)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId splineId = wydb::ElementId::kNull;
    const std::vector<wy::Vector2> fitPoints = openFitPoints();
    ASSERT_FALSE(createSpline(pDb.get(), fitPoints, splineId).IsNull());

    const wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pDb->getElement(splineId));
    ASSERT_NE(pSpline, nullptr);
    const std::vector<wy3d::SketchSpline::Tangent> before = pSpline->getTangents();

    // The table speaks for every point, so one of another size says nothing about them
    EXPECT_EQ(editSpline(pDb.get(), splineId, [](wy3d::SketchSpline* pSpline)
        { return pSpline->setTangents(tangentTable(pSpline->getPoints().size() - 1, 0.0, 1.0)); }),
        wy::ErrorStatus::InvalidInput);

    // Every entry of it has to aim somewhere, the ones the curve merely runs with as much as the
    // ones that constrain it: a number for the angle, a positive length for the magnitude
    EXPECT_EQ(editSpline(pDb.get(), splineId, [](wy3d::SketchSpline* pSpline)
        {
            std::vector<wy3d::SketchSpline::Tangent> tangents =
                tangentTable(pSpline->getPoints().size(), 0.0, 1.0);
            tangents[2].angle = std::numeric_limits<double>::quiet_NaN();
            return pSpline->setTangents(tangents);
        }), wy::ErrorStatus::InvalidInput);
    EXPECT_EQ(editSpline(pDb.get(), splineId, [](wy3d::SketchSpline* pSpline)
        {
            std::vector<wy3d::SketchSpline::Tangent> tangents =
                tangentTable(pSpline->getPoints().size(), 0.0, 1.0);
            tangents[2].magnitude = wy3d::TOL * 0.5;
            return pSpline->setTangents(tangents);
        }), wy::ErrorStatus::InvalidInput);
    EXPECT_EQ(editSpline(pDb.get(), splineId, [](wy3d::SketchSpline* pSpline)
        {
            std::vector<wy3d::SketchSpline::Tangent> tangents =
                tangentTable(pSpline->getPoints().size(), 0.0, 1.0);
            tangents[2].magnitude = -1.0;
            return pSpline->setTangents(tangents);
        }), wy::ErrorStatus::InvalidInput);

    // Nothing of the attempts above was stored
    pSpline = wy3d::SketchSpline::cast(pDb->getElement(splineId));
    ASSERT_NE(pSpline, nullptr);
    EXPECT_EQ(pSpline->getTangents(), before);

    // A default entry is a tangency like any other - along the x axis, of unit length - so a table
    // of nothing but those is one the curve can be handed: it drives nothing
    EXPECT_EQ(editSpline(pDb.get(), splineId, [](wy3d::SketchSpline* pSpline)
        {
            const std::vector<wy3d::SketchSpline::Tangent> defaults(pSpline->getPoints().size());
            return pSpline->setTangents(defaults);
        }), wy::ErrorStatus::Ok);

    // A full table of usable tangents goes in: the ones the curve already runs with, taken for the
    // whole of it, which is the shape it has
    std::vector<wy3d::SketchSpline::Tangent> frozen = before;
    for (wy3d::SketchSpline::Tangent& tangent : frozen) { tangent.isDriving = true; }
    EXPECT_EQ(editSpline(pDb.get(), splineId, [&frozen](wy3d::SketchSpline* pSpline)
        { return pSpline->setTangents(frozen); }), wy::ErrorStatus::Ok);
    pSpline = wy3d::SketchSpline::cast(pDb->getElement(splineId));
    ASSERT_NE(pSpline, nullptr);
    EXPECT_EQ(pSpline->getTangents(), frozen);
    EXPECT_TRUE(pSpline->getTangentAt(2).isDriving);
}

TEST(SketchSpline, ControlPointSplineMovesAndTurns)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    const std::vector<wy::Vector2> controlPoints = {
        wy::Vector2(0.0, 0.0),
        wy::Vector2(10.0, 10.0),
        wy::Vector2(20.0, 10.0),
        wy::Vector2(30.0, 0.0) };

    wydb::ElementId splineId = wydb::ElementId::kNull;
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::SketchSpline* pSpline(nullptr);
        ASSERT_EQ(wy3d::SketchSpline::create(pTrans, 3, controlPoints, pSpline), wy::ErrorStatus::Ok);
        ASSERT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
        splineId = pSpline->getId();
    }

    // A control point spline carries no table of tangents at all, and moving, turning, scaling and
    // mirroring one is what the sketch commands do to whatever is selected
    EXPECT_EQ(editSpline(pDb.get(), splineId, [](wy3d::SketchSpline* pSpline)
        { return pSpline->translate(wy::Vector2(3.0, 4.0)); }), wy::ErrorStatus::Ok);
    EXPECT_EQ(editSpline(pDb.get(), splineId, [](wy3d::SketchSpline* pSpline)
        { return pSpline->rotateAround(wy::Vector2::kZero, wy3d::PI_2); }), wy::ErrorStatus::Ok);
    EXPECT_EQ(editSpline(pDb.get(), splineId, [](wy3d::SketchSpline* pSpline)
        {
            const wy3d::geom::Matrix3 matrix =
                wy3d::geom::Matrix3::createReflection2D(wy::Vector2(0.0, 0.0), wy::Vector2(1.0, 0.0));
            return pSpline->transform(matrix);
        }), wy::ErrorStatus::Ok);
}

TEST(SketchSpline, ControlPointSplineRoundTrip)
{
    std::string filePath("./test_sketch_spline_control.wy3dt");
    wydb::ElementId splineId = wydb::ElementId::kNull;
    Handle(Geom2d_BSplineCurve) pCurve;

    const std::vector<wy::Vector2> controlPoints = {
        wy::Vector2(0.0, 0.0),
        wy::Vector2(10.0, 20.0),
        wy::Vector2(30.0, 25.0),
        wy::Vector2(50.0, 5.0),
        wy::Vector2(60.0, 15.0) };
    const std::vector<double> knots = { 0.0, 0.5, 1.0 };
    const std::vector<std::uint32_t> multiplicities = { 4, 1, 4 };

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        wydb::TransactionManager* pMgr = pDb->getTransactionManager();
        wydb::Transaction* pTrans = pMgr->startTransaction();
        ASSERT_NE(pTrans, nullptr);

        wy3d::SketchSpline* pSpline(nullptr);
        ASSERT_EQ(wy3d::SketchSpline::create(pTrans, 3, controlPoints, knots, multiplicities, pSpline),
            wy::ErrorStatus::Ok);
        ASSERT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
        splineId = pSpline->getId();
        pCurve = pSpline->getOccSpline();
        ASSERT_FALSE(pCurve.IsNull());
        ASSERT_EQ(pDb->writeFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);
    }

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        EXPECT_EQ(pDb->readFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);

        const wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pDb->getElement(splineId));
        ASSERT_NE(pSpline, nullptr);
        EXPECT_EQ(pSpline->getMode(), wy3d::SplineMode::ControlPoints);
        EXPECT_EQ(pSpline->getDegree(), 3u);
        EXPECT_EQ(pSpline->getPoints(), controlPoints);
        EXPECT_EQ(pSpline->getKnots(), knots);
        EXPECT_EQ(pSpline->getMultiplicities(), multiplicities);
        // A control point spline carries no tangent table, so none is written for it and none is
        // read back either
        EXPECT_TRUE(pSpline->getTangents().empty());
        // The curve the file rebuilds is the one that was saved
        ASSERT_FALSE(pSpline->getOccSpline().IsNull());
        EXPECT_LT(maxDeviation(sampleCurve(pCurve, 40), sampleCurve(pSpline->getOccSpline(), 40)), 1e-9);
    }

    std::remove(filePath.c_str());
}

// --- Closed splines ---

TEST(SketchSpline, ClosedSplineTailEditLeavesTheShape)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId splineId = wydb::ElementId::kNull;
    const std::vector<wy::Vector2> fitPoints = closedFitPoints();
    Handle(Geom2d_BSplineCurve) pCurve = createSpline(pDb.get(), fitPoints, splineId);
    ASSERT_FALSE(pCurve.IsNull());
    const std::vector<wy::Vector2> before = sampleCurve(pCurve, 40);

    // The duplicated last point is dropped from the point table handed to OCCT, so its entry
    // speaks for no point of the curve: nothing fills it in, and it is read as it is stored
    const wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pDb->getElement(splineId));
    ASSERT_NE(pSpline, nullptr);
    ASSERT_TRUE(pSpline->isClosed());
    ASSERT_EQ(pSpline->getTangents().size(), fitPoints.size());
    EXPECT_EQ(pSpline->getTangentAt(fitPoints.size() - 1), wy3d::SketchSpline::Tangent());

    // Editing through that index is a write like any other, stored as typed, and the curve does
    // not read it: the shape stays where it was and the first point is left alone
    const wy3d::SketchSpline::Tangent head = pSpline->getTangentAt(0);
    EXPECT_EQ(editSpline(pDb.get(), splineId, [](wy3d::SketchSpline* pSpline)
        { return setTangentAt(pSpline, pSpline->getPoints().size() - 1, wy3d::PI * 0.25, 1.5); }),
        wy::ErrorStatus::Ok);

    pSpline = wy3d::SketchSpline::cast(pDb->getElement(splineId));
    ASSERT_NE(pSpline, nullptr);
    EXPECT_NEAR(pSpline->getTangentAt(0).angle, head.angle, 1e-12);
    EXPECT_DOUBLE_EQ(pSpline->getTangentAt(0).magnitude, head.magnitude);
    EXPECT_EQ(pSpline->getTangentAt(0).isDriving, head.isDriving);
    EXPECT_NEAR(pSpline->getTangentAt(fitPoints.size() - 1).angle, wy3d::PI * 0.25, 1e-12);
    EXPECT_DOUBLE_EQ(pSpline->getTangentAt(fitPoints.size() - 1).magnitude, 1.5);
    EXPECT_LT(maxDeviation(before, sampleCurve(pSpline->getOccSpline(), 40)), 1e-12);

    // The first point takes a constraint of its own, and the entry of the point it duplicates is
    // still the one that was put there: the two ends of the table are written apart
    EXPECT_EQ(editSpline(pDb.get(), splineId, [](wy3d::SketchSpline* pSpline)
        { return setTangentAt(pSpline, 0, wy3d::PI / 6.0, 20.0); }), wy::ErrorStatus::Ok);

    pSpline = wy3d::SketchSpline::cast(pDb->getElement(splineId));
    ASSERT_NE(pSpline, nullptr);
    EXPECT_TRUE(pSpline->getTangentAt(0).isDriving);
    EXPECT_DOUBLE_EQ(pSpline->getTangentAt(0).angle, wy3d::PI / 6.0);
    EXPECT_NEAR(pSpline->getTangentAt(fitPoints.size() - 1).angle, wy3d::PI * 0.25, 1e-12);
    EXPECT_TRUE(pSpline->getTangentAt(fitPoints.size() - 1).isDriving);
}

TEST(SketchSpline, ClosedSplineTailKeepsWhatItHad)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId splineId = wydb::ElementId::kNull;
    const std::vector<wy::Vector2> fitPoints = openFitPoints();
    ASSERT_FALSE(createSpline(pDb.get(), fitPoints, splineId).IsNull());

    // The first point drives while the last one is still a point of its own with a tangency of
    // its own in the table
    EXPECT_EQ(editSpline(pDb.get(), splineId, [](wy3d::SketchSpline* pSpline)
        { return setTangentAt(pSpline, 0, wy3d::PI / 6.0, 20.0); }), wy::ErrorStatus::Ok);

    const wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pDb->getElement(splineId));
    ASSERT_NE(pSpline, nullptr);
    const wy3d::SketchSpline::Tangent tail = pSpline->getTangentAt(fitPoints.size() - 1);

    // Dragging the last point onto the first closes the spline: the entry of that point now
    // stands for a point the curve runs through once, and it is left holding what it had
    std::vector<wy::Vector2> closedPoints = fitPoints;
    closedPoints.back() = closedPoints.front();
    EXPECT_EQ(editSpline(pDb.get(), splineId, [&closedPoints](wy3d::SketchSpline* pSpline)
        { return pSpline->setPoints(closedPoints); }), wy::ErrorStatus::Ok);

    pSpline = wy3d::SketchSpline::cast(pDb->getElement(splineId));
    ASSERT_NE(pSpline, nullptr);
    ASSERT_TRUE(pSpline->isClosed());
    ASSERT_EQ(pSpline->getTangents().size(), closedPoints.size());
    EXPECT_TRUE(pSpline->getTangentAt(0).isDriving);
    EXPECT_DOUBLE_EQ(pSpline->getTangentAt(0).angle, wy3d::PI / 6.0);
    EXPECT_EQ(pSpline->getTangentAt(closedPoints.size() - 1), tail);
}

TEST(SketchSpline, ClosedSplineNeutralTangentKeepsShape)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId splineId = wydb::ElementId::kNull;
    const std::vector<wy::Vector2> fitPoints = closedFitPoints();
    Handle(Geom2d_BSplineCurve) pCurve = createSpline(pDb.get(), fitPoints, splineId);
    ASSERT_FALSE(pCurve.IsNull());
    const std::vector<wy::Vector2> before = sampleCurve(pCurve, 40);

    // The tangency of a periodic curve is measured the same way, against the span leaving the
    // point, so driving one leaves the shape alone here too
    EXPECT_EQ(editSpline(pDb.get(), splineId, [](wy3d::SketchSpline* pSpline)
        { return setTangentDrivingAt(pSpline, 1, true); }), wy::ErrorStatus::Ok);

    const wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pDb->getElement(splineId));
    ASSERT_NE(pSpline, nullptr);
    const wy3d::SketchSpline::Tangent tangent = pSpline->getTangentAt(1);
    ASSERT_TRUE(tangent.isDriving);
    double angle(0.0), speed(0.0);
    tangentAtPoint(pCurve, fitPoints[1], angle, speed);
    EXPECT_NEAR(tangent.angle, wy3d::normalizeRadian(angle), 1e-9);
    EXPECT_NEAR(tangent.magnitude, speed * spanChord(fitPoints, 1), 1e-9);
    Handle(Geom2d_BSplineCurve) pNewCurve = pSpline->getOccSpline();
    ASSERT_FALSE(pNewCurve.IsNull());
    EXPECT_LT(maxDeviation(before, sampleCurve(pNewCurve, 40)), 1e-6);
    EXPECT_FALSE(pSpline->isDegenerate(1e-7));
}

TEST(SketchSpline, ClosedSplineWeightDrivesCurve)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId splineId = wydb::ElementId::kNull;
    const std::vector<wy::Vector2> fitPoints = closedFitPoints();
    Handle(Geom2d_BSplineCurve) pCurve = createSpline(pDb.get(), fitPoints, splineId);
    ASSERT_FALSE(pCurve.IsNull());

    const double chord = spanChord(fitPoints, 1);
    const double requestedAngle = wy3d::PI * 0.5;
    EXPECT_EQ(editSpline(pDb.get(), splineId, [requestedAngle, chord](wy3d::SketchSpline* pSpline)
        { return setTangentAt(pSpline, 1, requestedAngle, 2.0 * chord); }), wy::ErrorStatus::Ok);

    const wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pDb->getElement(splineId));
    ASSERT_NE(pSpline, nullptr);
    Handle(Geom2d_BSplineCurve) pNewCurve = pSpline->getOccSpline();
    // Whatever OCCT decides about tangency on a periodic curve, a curve must come back and the
    // fit points must still be interpolated
    ASSERT_FALSE(pNewCurve.IsNull());
    EXPECT_FALSE(pSpline->isDegenerate(1e-7));
    EXPECT_GT(pSpline->getLength(), 0.0);
    for (std::size_t i = 0; i + 1 < fitPoints.size(); ++i)
    {
        EXPECT_NEAR(distanceToCurve(pNewCurve, fitPoints[i]), 0.0, 1e-7);
    }

    double angle(0.0), speed(0.0);
    tangentAtPoint(pNewCurve, fitPoints[1], angle, speed);
    EXPECT_NEAR(wy3d::normalizeRadian(angle), wy3d::normalizeRadian(requestedAngle), 1e-6);
    EXPECT_NEAR(speed, 2.0, 1e-6);
    expectSpanSpeed(speed, 2.0 * chord, chord);
}

TEST(SketchSpline, ClosedSplineSeamSpanIsTheClosingChord)
{
    // An asymmetric closed spline, so that the chord closing the loop back to the first point is
    // not the chord arriving from the point before it: the two have to be told apart
    const std::vector<wy::Vector2> fitPoints = {
        wy::Vector2(0.0, 0.0),
        wy::Vector2(10.0, 0.0),
        wy::Vector2(10.0, 5.0),
        wy::Vector2(4.0, 9.0),
        wy::Vector2(0.0, 0.0) };
    const std::size_t seam = fitPoints.size() - 2;
    const double closingChord = (fitPoints.front() - fitPoints[seam]).length();
    const double arrivingChord = (fitPoints[seam] - fitPoints[seam - 1]).length();
    ASSERT_GT(std::fabs(closingChord - arrivingChord), 1.0);

    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId splineId = wydb::ElementId::kNull;
    Handle(Geom2d_BSplineCurve) pCurve = createSpline(pDb.get(), fitPoints, splineId);
    ASSERT_FALSE(pCurve.IsNull());

    // The magnitude of the last point that has a slot of its own is measured against the span it
    // sends the curve off on, which for a closed spline is the one closing the loop
    const double requestedAngle = wy3d::PI * 0.25;
    EXPECT_EQ(editSpline(pDb.get(), splineId, [seam, requestedAngle, closingChord](wy3d::SketchSpline* pSpline)
        { return setTangentAt(pSpline, seam, requestedAngle, 2.0 * closingChord); }), wy::ErrorStatus::Ok);

    const wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pDb->getElement(splineId));
    ASSERT_NE(pSpline, nullptr);
    ASSERT_TRUE(pSpline->isClosed());
    Handle(Geom2d_BSplineCurve) pNewCurve = pSpline->getOccSpline();
    ASSERT_FALSE(pNewCurve.IsNull());

    // The curve runs with the tangency it was given, at the speed the magnitude asks of it in that
    // span; measured in the other one the speed would be that much larger
    double angle(0.0), speed(0.0);
    tangentAtPoint(pNewCurve, fitPoints[seam], angle, speed);
    EXPECT_NEAR(wy3d::normalizeRadian(angle), wy3d::normalizeRadian(requestedAngle), 1e-6);
    expectSpanSpeed(speed, 2.0 * closingChord, closingChord);
    EXPECT_NEAR(pSpline->getTangentAt(seam).magnitude, 2.0 * closingChord, 1e-12);
    for (std::size_t i = 0; i + 1 < fitPoints.size(); ++i)
    {
        EXPECT_NEAR(distanceToCurve(pNewCurve, fitPoints[i]), 0.0, 1e-7);
    }
}

// --- Persistence and transforms ---

TEST(SketchSpline, TangentIORoundTrip)
{
    std::string filePath("./test_sketch_spline.wy3dt");
    wydb::ElementId splineId = wydb::ElementId::kNull;

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        ASSERT_FALSE(createSpline(pDb.get(), openFitPoints(), splineId).IsNull());

        EXPECT_EQ(editSpline(pDb.get(), splineId, [](wy3d::SketchSpline* pSpline)
            { return setTangentAt(pSpline, 1, wy3d::PI * 0.3, 12.5); }), wy::ErrorStatus::Ok);

        EXPECT_EQ(pDb->writeFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);
    }

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        EXPECT_EQ(pDb->readFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);

        const wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pDb->getElement(splineId));
        ASSERT_NE(pSpline, nullptr);
        const wy3d::SketchSpline::Tangent tangent = pSpline->getTangentAt(1);
        EXPECT_TRUE(tangent.isDriving);
        EXPECT_NEAR(tangent.angle, wy3d::PI * 0.3, 1e-12);
        // The stored value comes back as it was
        EXPECT_DOUBLE_EQ(tangent.magnitude, 12.5);
        // The cached curve is not serialized; the tangency must rebuild it
        ASSERT_FALSE(pSpline->getOccSpline().IsNull());
        EXPECT_GT(pSpline->getLength(), 0.0);
        for (std::size_t i = 0; i < pSpline->getPoints().size(); ++i)
        {
            EXPECT_NEAR(distanceToCurve(pSpline->getOccSpline(), pSpline->getPoints()[i]), 0.0, 1e-7);
        }
    }

    std::remove(filePath.c_str());
}

TEST(SketchSpline, TangentFollowsTransform)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId splineId = wydb::ElementId::kNull;
    const std::vector<wy::Vector2> fitPoints = openFitPoints();
    Handle(Geom2d_BSplineCurve) pCurve = createSpline(pDb.get(), fitPoints, splineId);
    ASSERT_FALSE(pCurve.IsNull());

    const double angle = wy3d::PI * 0.2;
    const double magnitude = 2.0 * spanChord(fitPoints, 1);
    EXPECT_EQ(editSpline(pDb.get(), splineId, [angle, magnitude](wy3d::SketchSpline* pSpline)
        { return setTangentAt(pSpline, 1, angle, magnitude); }), wy::ErrorStatus::Ok);

    // Translation moves the points only
    EXPECT_EQ(editSpline(pDb.get(), splineId, [](wy3d::SketchSpline* pSpline)
        { return pSpline->translate(wy::Vector2(3.0, 4.0)); }), wy::ErrorStatus::Ok);
    const wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pDb->getElement(splineId));
    ASSERT_NE(pSpline, nullptr);
    EXPECT_NEAR(pSpline->getTangentAt(1).angle, angle, 1e-12);
    EXPECT_DOUBLE_EQ(pSpline->getTangentAt(1).magnitude, magnitude);

    // Rotation turns the tangents with the points and leaves their size alone
    EXPECT_EQ(editSpline(pDb.get(), splineId, [angle](wy3d::SketchSpline* pSpline)
        { return pSpline->rotateAround(wy::Vector2(0.0, 0.0), angle); }), wy::ErrorStatus::Ok);
    pSpline = wy3d::SketchSpline::cast(pDb->getElement(splineId));
    ASSERT_NE(pSpline, nullptr);
    EXPECT_NEAR(pSpline->getTangentAt(1).angle, wy3d::normalizeRadian(2.0 * angle), 1e-12);
    EXPECT_DOUBLE_EQ(pSpline->getTangentAt(1).magnitude, magnitude);

    // A mirror turns the direction around and leaves the size of the magnitude alone
    const double mirrored = wy3d::normalizeRadian(2.0 * angle);
    EXPECT_EQ(editSpline(pDb.get(), splineId, [](wy3d::SketchSpline* pSpline)
        {
            const wy3d::geom::Matrix3 matrix =
                wy3d::geom::Matrix3::createReflection2D(wy::Vector2(0.0, 0.0), wy::Vector2(1.0, 0.0));
            return pSpline->transform(matrix);
        }), wy::ErrorStatus::Ok);
    pSpline = wy3d::SketchSpline::cast(pDb->getElement(splineId));
    ASSERT_NE(pSpline, nullptr);
    EXPECT_NEAR(pSpline->getTangentAt(1).angle, wy3d::normalizeRadian(-mirrored), 1e-12);
    EXPECT_DOUBLE_EQ(pSpline->getTangentAt(1).magnitude, magnitude);

    // A magnitude is as long as the span it is measured against, so scaling the points by ten scales
    // the magnitude by ten as well - while the tangency itself, the one the parameter sees, is the
    // same as before, exactly as SolidWorks has it
    double beforeAngle(0.0), beforeSpeed(0.0);
    tangentAtFitPoint(pSpline->getOccSpline(), 1, beforeAngle, beforeSpeed);
    EXPECT_EQ(editSpline(pDb.get(), splineId, [](wy3d::SketchSpline* pSpline)
        {
            const wy3d::geom::Matrix3 matrix = wy3d::geom::Matrix3::createScale(wy::Vector2(10.0, 10.0));
            return pSpline->transform(matrix);
        }), wy::ErrorStatus::Ok);
    pSpline = wy3d::SketchSpline::cast(pDb->getElement(splineId));
    ASSERT_NE(pSpline, nullptr);
    EXPECT_DOUBLE_EQ(pSpline->getTangentAt(1).magnitude, 10.0 * magnitude);
    EXPECT_NEAR(pSpline->getTangentAt(1).angle, wy3d::normalizeRadian(-mirrored), 1e-12);
    Handle(Geom2d_BSplineCurve) pScaledCurve = pSpline->getOccSpline();
    ASSERT_FALSE(pScaledCurve.IsNull());
    double angle2(0.0), speed2(0.0);
    tangentAtFitPoint(pScaledCurve, 1, angle2, speed2);
    // The curve runs the direction the table was left holding
    EXPECT_NEAR(wy3d::normalizeRadian(angle2), wy3d::normalizeRadian(-mirrored), 1e-6);
    EXPECT_NEAR(speed2, beforeSpeed, 1e-9);
}

TEST(SketchSpline, WeightingMatchesSolidWorks)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::ElementId splineId = wydb::ElementId::kNull;
    const std::vector<wy::Vector2> fitPoints = {
        wy::Vector2(-50.0, 0.0),
        wy::Vector2(10.75, -25.75),
        wy::Vector2(50.0, 0.0) };
    Handle(Geom2d_BSplineCurve) pCurve = createSpline(pDb.get(), fitPoints, splineId);
    ASSERT_FALSE(pCurve.IsNull());

    const wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pDb->getElement(splineId));
    ASSERT_NE(pSpline, nullptr);

    // Read off SolidWorks for this very spline, the shape being the one the points alone give
    const double expectedMagnitude[3] = { 89.075392522, 41.569955148, 57.929845238 };
    const double expectedAngleInDegrees[3] = { 315.940229608, 10.296367029, 49.529848214 };
    for (std::size_t i = 0; i < fitPoints.size(); ++i)
    {
        const wy3d::SketchSpline::Tangent tangent = pSpline->getTangentAt(i);
        EXPECT_NEAR(tangent.magnitude, expectedMagnitude[i], 1e-6);
        EXPECT_NEAR(tangent.angle,
            wy3d::normalizeRadian(wy3d::degreesToRadians(expectedAngleInDegrees[i])), 1e-9);
    }

    // Driving the middle point takes over the tangency it already has, so the shape stays put
    const std::vector<wy::Vector2> before = sampleCurve(pCurve, 40);
    EXPECT_EQ(editSpline(pDb.get(), splineId, [](wy3d::SketchSpline* pSpline)
        { return setTangentDrivingAt(pSpline, 1, true); }), wy::ErrorStatus::Ok);
    pSpline = wy3d::SketchSpline::cast(pDb->getElement(splineId));
    ASSERT_NE(pSpline, nullptr);
    EXPECT_NEAR(pSpline->getTangentAt(1).magnitude, expectedMagnitude[1], 1e-6);
    Handle(Geom2d_BSplineCurve) pNewCurve = pSpline->getOccSpline();
    ASSERT_FALSE(pNewCurve.IsNull());
    EXPECT_LT(maxDeviation(before, sampleCurve(pNewCurve, 40)), 1e-6);
}
