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

#include <utils/wy3dSketch3DEdgeUtil.h>
#include <wy3dSketch.h>
#include <wy3dSketchCurve.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>
#include <wy3dSketchEllipse.h>
#include <wy3dSketchEllipseArc.h>
#include <wy3dSketchSpline.h>
#include <wy3dSketch3D.h>
#include <wy3dSketchEntity3D.h>
#include <wy3dSketchLine3D.h>
#include <wy3dSketchCircle3D.h>
#include <wy3dSketchArc3D.h>
#include <wy3dSketchEllipse3D.h>
#include <wy3dSketchEllipseArc3D.h>
#include <wy3dSketchSpline3D.h>
#include <wy3dSketchPlane.h>
#include <wy3dMath.h>
#include <wyIterator.h>

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <Geom_BezierCurve.hxx>
#include <Geom2d_BSplineCurve.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_Circle.hxx>
#include <Geom_Ellipse.hxx>
#include <Geom_Line.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TColStd_Array1OfInteger.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>

#include <cmath>
#include <cstdio>
#include <memory>

namespace
{
using Result = wy3d::Sketch3DEdgeUtil::Result;
using Kind = wy3d::Sketch3DEdgeUtil::Kind;
using CurveSpec = wy3d::Sketch3DEdgeUtil::CurveSpec;

std::size_t countElements(wy3d::Database* pDb)
{
    std::size_t n(0);
    wy::Iterator<wydb::ElementId> iter = pDb->createIterator();
    while (!iter.isDone())
    {
        const wydb::Element* pElem = pDb->getElement(iter.current());
        if (pElem && !pElem->isErased()) ++n;
        iter.moveNext();
    }
    return n;
}

void expectSamePoint(const wy::Vector3& actual, const gp_Pnt& expected, double tolerance = 1e-9)
{
    EXPECT_NEAR(actual.x(), expected.X(), tolerance);
    EXPECT_NEAR(actual.y(), expected.Y(), tolerance);
    EXPECT_NEAR(actual.z(), expected.Z(), tolerance);
}

void expectSamePoint(const wy::Vector3& actual, const wy::Vector3& expected, double tolerance = 1e-9)
{
    EXPECT_NEAR((actual - expected).length(), 0.0, tolerance);
}

void expectSamePoint(const gp_Pnt& actual, const wy::Vector3& expected, double tolerance = 1e-9)
{
    expectSamePoint(wy::Vector3(actual.X(), actual.Y(), actual.Z()), expected, tolerance);
}

Handle(Geom_Circle) makeCircle(const gp_Pnt& center, const gp_Dir& normal, const gp_Dir& xDir, double radius)
{
    return new Geom_Circle(gp_Ax2(center, normal, xDir), radius);
}

Handle(Geom_Ellipse) makeEllipse(const gp_Pnt& center, const gp_Dir& normal, const gp_Dir& xDir,
    double majorRadius, double minorRadius)
{
    return new Geom_Ellipse(gp_Ax2(center, normal, xDir), majorRadius, minorRadius);
}

// Clamped B-spline: 4 poles, degree 3, knots {0,1} with end multiplicities 4.
Handle(Geom_BSplineCurve) makeClampedSpline()
{
    TColgp_Array1OfPnt poles(1, 4);
    poles.SetValue(1, gp_Pnt(0.0, 0.0, 0.0));
    poles.SetValue(2, gp_Pnt(1.0, 4.0, 0.5));
    poles.SetValue(3, gp_Pnt(5.0, -3.0, 2.0));
    poles.SetValue(4, gp_Pnt(8.0, 1.0, 3.0));

    TColStd_Array1OfReal knots(1, 2);
    knots.SetValue(1, 0.0);
    knots.SetValue(2, 1.0);

    TColStd_Array1OfInteger multiplicities(1, 2);
    multiplicities.SetValue(1, 4);
    multiplicities.SetValue(2, 4);

    return new Geom_BSplineCurve(poles, knots, multiplicities, 3);
}

struct ConvertFixture
{
    std::unique_ptr<wy3d::Database> pDb;
    wydb::Transaction* pTrans;

    ConvertFixture()
        : pDb(std::make_unique<wy3d::Database>())
        , pTrans(pDb->getTransactionManager()->startTransaction())
    {
    }

    ~ConvertFixture()
    {
        if (pTrans) pDb->getTransactionManager()->abortTransaction();
    }

    wy3d::SketchEntity3D* convert(const TopoDS_Edge& edge, Result& result)
    {
        wy3d::SketchEntity3D* pEntity(nullptr);
        result = wy3d::Sketch3DEdgeUtil::convert(pTrans, edge, pEntity);
        return pEntity;
    }
};

// A tilted plane: a curve that is not lifted into 3D cannot pass unnoticed
wy3d::SketchPlane makeTiltedPlane()
{
    wy::Vector3 normal(1.0, 1.0, 1.0);
    normal.normalize();
    wy::Vector3 xDir(1.0, -1.0, 0.0);
    xDir.normalize();
    const wy3d::SketchPlane plane(wy::Vector3(2.0, -1.0, 3.0), normal, xDir);
    EXPECT_TRUE(plane.isValid());
    return plane;
}

struct Sketch2DFixture
{
    std::unique_ptr<wy3d::Database> pDb;
    wydb::Transaction* pTrans;
    wy3d::Sketch* pSketch;
    wy3d::SketchPlane plane;

    Sketch2DFixture()
        : pDb(std::make_unique<wy3d::Database>())
        , pTrans(pDb->getTransactionManager()->startTransaction())
        , pSketch(nullptr)
        , plane(makeTiltedPlane())
    {
        EXPECT_EQ(wy3d::Sketch::create(pTrans, plane, pSketch), wy::ErrorStatus::Ok);
    }

    ~Sketch2DFixture()
    {
        if (pTrans) pDb->getTransactionManager()->abortTransaction();
    }

    wy3d::SketchEntity3D* convert(const wy3d::SketchCurve* pCurve, Result& result)
    {
        wy3d::SketchEntity3D* pEntity(nullptr);
        result = wy3d::Sketch3DEdgeUtil::convert(pTrans, pSketch, pCurve, pEntity);
        return pEntity;
    }
};
}

// --- Curve spec: analytic curves ---

TEST(Sketch3DEdgeUtil, LineSpec)
{
    Handle(Geom_Line) pLine = new Geom_Line(gp_Pnt(1.0, 2.0, 3.0), gp_Dir(0.0, 0.0, 1.0));

    CurveSpec spec;
    EXPECT_EQ(wy3d::Sketch3DEdgeUtil::makeCurveSpec(pLine, 0.0, 5.0, spec), Result::Ok);
    EXPECT_EQ(spec.kind, Kind::Line);
    expectSamePoint(spec.startPnt, gp_Pnt(1.0, 2.0, 3.0));
    expectSamePoint(spec.endPnt, gp_Pnt(1.0, 2.0, 8.0));
}

TEST(Sketch3DEdgeUtil, LineSpecInfiniteRange)
{
    Handle(Geom_Line) pLine = new Geom_Line(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(1.0, 0.0, 0.0));

    CurveSpec spec;
    EXPECT_EQ(wy3d::Sketch3DEdgeUtil::makeCurveSpec(pLine, -1.0e150, 1.0e150, spec), Result::InfiniteCurve);
}

TEST(Sketch3DEdgeUtil, LineSpecDegenerateRange)
{
    Handle(Geom_Line) pLine = new Geom_Line(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(1.0, 0.0, 0.0));

    CurveSpec spec;
    EXPECT_EQ(wy3d::Sketch3DEdgeUtil::makeCurveSpec(pLine, 2.0, 2.0 + 1e-12, spec), Result::Degenerate);
}

TEST(Sketch3DEdgeUtil, CircleSpec)
{
    Handle(Geom_Circle) pCircle = makeCircle(gp_Pnt(10.0, 0.0, 5.0), gp_Dir(0.0, 0.0, 1.0), gp_Dir(1.0, 0.0, 0.0), 25.0);

    CurveSpec spec;
    EXPECT_EQ(wy3d::Sketch3DEdgeUtil::makeCurveSpec(pCircle, 0.0, wy3d::TWO_PI, spec), Result::Ok);
    EXPECT_EQ(spec.kind, Kind::Circle);
    expectSamePoint(spec.center, gp_Pnt(10.0, 0.0, 5.0));
    expectSamePoint(spec.normal, wy::Vector3(0.0, 0.0, 1.0));
    expectSamePoint(spec.xDir, wy::Vector3(1.0, 0.0, 0.0));
    EXPECT_NEAR(spec.radius, 25.0, 1e-9);
}

TEST(Sketch3DEdgeUtil, ArcSpec)
{
    Handle(Geom_Circle) pCircle = makeCircle(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0), gp_Dir(1.0, 0.0, 0.0), 5.0);

    CurveSpec spec;
    EXPECT_EQ(wy3d::Sketch3DEdgeUtil::makeCurveSpec(pCircle, wy3d::TWO_PI / 6.0, wy3d::TWO_PI / 4.0, spec), Result::Ok);
    EXPECT_EQ(spec.kind, Kind::Arc);
    EXPECT_NEAR(spec.startAngle, wy3d::TWO_PI / 6.0, 1e-9);
    EXPECT_NEAR(spec.endAngle, wy3d::TWO_PI / 4.0, 1e-9);
}

TEST(Sketch3DEdgeUtil, ArcSpecAcrossSeam)
{
    Handle(Geom_Circle) pCircle = makeCircle(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0), gp_Dir(1.0, 0.0, 0.0), 5.0);

    CurveSpec spec;
    EXPECT_EQ(wy3d::Sketch3DEdgeUtil::makeCurveSpec(pCircle, 5.5, 7.0, spec), Result::Ok);
    EXPECT_EQ(spec.kind, Kind::Arc);
    EXPECT_NEAR(wy3d::normalizeRadian(spec.endAngle - spec.startAngle), 1.5, 1e-9);
}

TEST(Sketch3DEdgeUtil, EllipseSpec)
{
    Handle(Geom_Ellipse) pEllipse = makeEllipse(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 1.0, 0.0), gp_Dir(1.0, 0.0, 0.0), 10.0, 4.0);

    CurveSpec spec;
    EXPECT_EQ(wy3d::Sketch3DEdgeUtil::makeCurveSpec(pEllipse, 0.0, wy3d::TWO_PI, spec), Result::Ok);
    EXPECT_EQ(spec.kind, Kind::Ellipse);
    expectSamePoint(spec.center, gp_Pnt(0.0, 0.0, 0.0));
    expectSamePoint(spec.normal, wy::Vector3(0.0, 1.0, 0.0));
    expectSamePoint(spec.xDir, wy::Vector3(1.0, 0.0, 0.0));
    EXPECT_NEAR(spec.majorRadius, 10.0, 1e-9);
    EXPECT_NEAR(spec.radiusRatio, 0.4, 1e-9);
}

TEST(Sketch3DEdgeUtil, EllipseArcSpecAnglesArePolar)
{
    Handle(Geom_Ellipse) pEllipse = makeEllipse(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0), gp_Dir(1.0, 0.0, 0.0), 10.0, 4.0);

    CurveSpec spec;
    EXPECT_EQ(wy3d::Sketch3DEdgeUtil::makeCurveSpec(pEllipse, 0.3, 1.9, spec), Result::Ok);
    EXPECT_EQ(spec.kind, Kind::EllipseArc);

    // The stored angles are polar angles measured from the major axis, not the OCCT
    // parameters: check the polar angle of the source points instead.
    const wy::Vector3 yDir = spec.normal.cross(spec.xDir);
    auto polarOf = [&](const gp_Pnt& pnt)
    {
        const wy::Vector3 offset(pnt.X() - spec.center.x(), pnt.Y() - spec.center.y(), pnt.Z() - spec.center.z());
        return wy3d::normalizeRadian(std::atan2(offset.dot(yDir), offset.dot(spec.xDir)));
    };

    EXPECT_NEAR(polarOf(pEllipse->Value(0.3)), spec.startAngle, 1e-9);
    EXPECT_NEAR(polarOf(pEllipse->Value(1.9)), spec.endAngle, 1e-9);

    // The arc sweeps counterclockwise from start to end, so the source midpoint has to
    // fall inside that sweep.
    EXPECT_LT(wy3d::normalizeRadian(polarOf(pEllipse->Value(1.1)) - spec.startAngle),
        wy3d::normalizeRadian(spec.endAngle - spec.startAngle) + 1e-12);
}

TEST(Sketch3DEdgeUtil, SplineSpecIsExact)
{
    Handle(Geom_BSplineCurve) pSpline = makeClampedSpline();

    CurveSpec spec;
    EXPECT_EQ(wy3d::Sketch3DEdgeUtil::makeCurveSpec(pSpline, 0.0, 1.0, spec), Result::Ok);
    EXPECT_EQ(spec.kind, Kind::ControlPointSpline);
    EXPECT_EQ(spec.degree, 3u);
    ASSERT_EQ(spec.points.size(), 4u);
    ASSERT_EQ(spec.knots.size(), 2u);
    EXPECT_NEAR(spec.knots[0], 0.0, 1e-12);
    EXPECT_NEAR(spec.knots[1], 1.0, 1e-12);
    ASSERT_EQ(spec.multiplicities.size(), 2u);
    EXPECT_EQ(spec.multiplicities[0], 4u);
    EXPECT_EQ(spec.multiplicities[1], 4u);
    for (Standard_Integer i = 1; i <= 4; ++i)
    {
        expectSamePoint(spec.points[static_cast<std::size_t>(i - 1)], pSpline->Pole(i));
    }
}

TEST(Sketch3DEdgeUtil, HighDegreeSplineFallsBackToSampling)
{
    // Degree 9, one above what a sketch spline can hold. Ten poles take sum(mults) = 20.
    TColgp_Array1OfPnt poles(1, 10);
    for (Standard_Integer i = 1; i <= 10; ++i)
    {
        poles.SetValue(i, gp_Pnt(static_cast<double>(i), (i % 2 == 0) ? 3.0 : 0.0, 0.0));
    }
    TColStd_Array1OfReal knots(1, 2);
    knots.SetValue(1, 0.0);
    knots.SetValue(2, 1.0);
    TColStd_Array1OfInteger multiplicities(1, 2);
    multiplicities.SetValue(1, 10);
    multiplicities.SetValue(2, 10);
    Handle(Geom_BSplineCurve) pSpline = new Geom_BSplineCurve(poles, knots, multiplicities, 9);
    ASSERT_EQ(pSpline->Degree(), 9);

    CurveSpec spec;
    EXPECT_EQ(wy3d::Sketch3DEdgeUtil::makeCurveSpec(pSpline, 0.0, 1.0, spec), Result::Ok);
    EXPECT_EQ(spec.kind, Kind::FitPointSpline);
    EXPECT_GT(spec.points.size(), 2u);
}

// The other side of the same window: at the limit the curve is carried across rather than
// sampled, so the sketch gets the geometry the model actually has. This window and the limit
// SketchSpline3D enforces are the same number and are meant to stay in step.
TEST(Sketch3DEdgeUtil, SplineAtTheDegreeLimitIsCarriedAcross)
{
    TColgp_Array1OfPnt poles(1, 9);
    for (Standard_Integer i = 1; i <= 9; ++i)
    {
        poles.SetValue(i, gp_Pnt(static_cast<double>(i), (i % 2 == 0) ? 3.0 : 0.0, 0.0));
    }
    TColStd_Array1OfReal knots(1, 2);
    knots.SetValue(1, 0.0);
    knots.SetValue(2, 1.0);
    TColStd_Array1OfInteger multiplicities(1, 2);
    multiplicities.SetValue(1, 9);
    multiplicities.SetValue(2, 9);
    Handle(Geom_BSplineCurve) pSpline = new Geom_BSplineCurve(poles, knots, multiplicities, 8);
    ASSERT_EQ(pSpline->Degree(), 8);

    CurveSpec spec;
    EXPECT_EQ(wy3d::Sketch3DEdgeUtil::makeCurveSpec(pSpline, 0.0, 1.0, spec), Result::Ok);
    EXPECT_EQ(spec.kind, Kind::ControlPointSpline);
    EXPECT_EQ(spec.degree, 8u);
    EXPECT_EQ(spec.points.size(), 9u);
}

TEST(Sketch3DEdgeUtil, RationalSplineFallsBackToSampling)
{
    // A quarter circle expressed as a rational quadratic spline.
    TColgp_Array1OfPnt poles(1, 3);
    poles.SetValue(1, gp_Pnt(1.0, 0.0, 0.0));
    poles.SetValue(2, gp_Pnt(1.0, 1.0, 0.0));
    poles.SetValue(3, gp_Pnt(0.0, 1.0, 0.0));
    TColStd_Array1OfReal weights(1, 3);
    weights.SetValue(1, 1.0);
    weights.SetValue(2, std::sqrt(2.0) / 2.0);
    weights.SetValue(3, 1.0);
    TColStd_Array1OfReal knots(1, 2);
    knots.SetValue(1, 0.0);
    knots.SetValue(2, 1.0);
    TColStd_Array1OfInteger multiplicities(1, 2);
    multiplicities.SetValue(1, 3);
    multiplicities.SetValue(2, 3);
    Handle(Geom_BSplineCurve) pSpline = new Geom_BSplineCurve(poles, weights, knots, multiplicities, 2);
    ASSERT_TRUE(pSpline->IsRational());

    CurveSpec spec;
    EXPECT_EQ(wy3d::Sketch3DEdgeUtil::makeCurveSpec(pSpline, 0.0, 1.0, spec), Result::Ok);
    EXPECT_EQ(spec.kind, Kind::FitPointSpline);

    // The sampled points sit on the source curve, so the arc stays on the unit circle.
    for (const wy::Vector3& pnt : spec.points)
    {
        EXPECT_NEAR(std::sqrt(pnt.x() * pnt.x() + pnt.y() * pnt.y()), 1.0, 1e-6);
    }
}

TEST(Sketch3DEdgeUtil, BezierFallsBackToSampling)
{
    TColgp_Array1OfPnt poles(1, 4);
    poles.SetValue(1, gp_Pnt(0.0, 0.0, 0.0));
    poles.SetValue(2, gp_Pnt(3.0, 6.0, 0.0));
    poles.SetValue(3, gp_Pnt(9.0, -6.0, 0.0));
    poles.SetValue(4, gp_Pnt(12.0, 0.0, 0.0));
    Handle(Geom_BezierCurve) pBezier = new Geom_BezierCurve(poles);

    CurveSpec spec;
    EXPECT_EQ(wy3d::Sketch3DEdgeUtil::makeCurveSpec(pBezier, 0.0, 1.0, spec), Result::Ok);
    EXPECT_EQ(spec.kind, Kind::FitPointSpline);
}

// --- Edge conversion ---

TEST(Sketch3DEdgeUtil, ConvertLineEdge)
{
    ConvertFixture fixture;
    BRepBuilderAPI_MakeEdge edgeMaker(gp_Pnt(0.0, 0.0, 0.0), gp_Pnt(10.0, 0.0, 0.0));
    ASSERT_TRUE(edgeMaker.IsDone());

    Result result = Result::Ok;
    wy3d::SketchEntity3D* pEntity = fixture.convert(edgeMaker.Edge(), result);
    ASSERT_EQ(result, Result::Ok);
    ASSERT_NE(pEntity, nullptr);

    const wy3d::SketchLine3D* pLine = wy3d::SketchLine3D::cast(pEntity);
    ASSERT_NE(pLine, nullptr);
    expectSamePoint(pLine->getStartPoint(), gp_Pnt(0.0, 0.0, 0.0));
    expectSamePoint(pLine->getEndPoint(), gp_Pnt(10.0, 0.0, 0.0));
    EXPECT_FALSE(pLine->isDegenerate(1e-7));
}

TEST(Sketch3DEdgeUtil, ConvertFullCircleEdge)
{
    ConvertFixture fixture;
    BRepBuilderAPI_MakeEdge edgeMaker(makeCircle(gp_Pnt(3.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0), gp_Dir(1.0, 0.0, 0.0), 5.0));
    ASSERT_TRUE(edgeMaker.IsDone());

    Result result = Result::Ok;
    wy3d::SketchEntity3D* pEntity = fixture.convert(edgeMaker.Edge(), result);
    ASSERT_EQ(result, Result::Ok);
    ASSERT_NE(pEntity, nullptr);

    // A full period must become a circle, not an arc.
    const wy3d::SketchCircle3D* pCircle = wy3d::SketchCircle3D::cast(pEntity);
    ASSERT_NE(pCircle, nullptr);
    expectSamePoint(pCircle->getCenter(), gp_Pnt(3.0, 0.0, 0.0));
    EXPECT_NEAR(pCircle->getRadius(), 5.0, 1e-9);
    EXPECT_FALSE(pCircle->isDegenerate(1e-7));
}

TEST(Sketch3DEdgeUtil, ConvertArcEdge)
{
    ConvertFixture fixture;
    Handle(Geom_Circle) pCircle = makeCircle(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0), gp_Dir(1.0, 0.0, 0.0), 5.0);
    const double first = wy3d::TWO_PI / 6.0;
    const double last = wy3d::TWO_PI / 3.0;
    BRepBuilderAPI_MakeEdge edgeMaker(pCircle, first, last);
    ASSERT_TRUE(edgeMaker.IsDone());

    Result result = Result::Ok;
    wy3d::SketchEntity3D* pEntity = fixture.convert(edgeMaker.Edge(), result);
    ASSERT_EQ(result, Result::Ok);
    ASSERT_NE(pEntity, nullptr);

    const wy3d::SketchArc3D* pArc = wy3d::SketchArc3D::cast(pEntity);
    ASSERT_NE(pArc, nullptr);
    expectSamePoint(pArc->getPointAt(0.0), pCircle->Value(first), 1e-9);
    expectSamePoint(pArc->getPointAt(1.0), pCircle->Value(last), 1e-9);
    expectSamePoint(pArc->getPointAt(0.5), pCircle->Value(0.5 * (first + last)), 1e-9);
}

TEST(Sketch3DEdgeUtil, ConvertEllipseArcEdge)
{
    ConvertFixture fixture;
    Handle(Geom_Ellipse) pEllipse = makeEllipse(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0), gp_Dir(1.0, 0.0, 0.0), 10.0, 4.0);
    const double first = 0.3;
    const double last = 1.9;
    BRepBuilderAPI_MakeEdge edgeMaker(pEllipse, first, last);
    ASSERT_TRUE(edgeMaker.IsDone());

    Result result = Result::Ok;
    wy3d::SketchEntity3D* pEntity = fixture.convert(edgeMaker.Edge(), result);
    ASSERT_EQ(result, Result::Ok);
    ASSERT_NE(pEntity, nullptr);

    const wy3d::SketchEllipseArc3D* pEllipseArc = wy3d::SketchEllipseArc3D::cast(pEntity);
    ASSERT_NE(pEllipseArc, nullptr);
    expectSamePoint(pEllipseArc->getPointAt(0.0), pEllipse->Value(first), 1e-9);
    expectSamePoint(pEllipseArc->getPointAt(1.0), pEllipse->Value(last), 1e-9);

    // The entity parameterizes the arc by polar angle, so its midpoint is the point
    // whose polar angle is the mean of the two endpoint polar angles.
    const gp_Pnt& center = pEllipse->Location();
    const wy::Vector3 xDir(1.0, 0.0, 0.0);
    const wy::Vector3 yDir(0.0, 1.0, 0.0);
    auto polarOf = [&](const gp_Pnt& pnt)
    {
        const wy::Vector3 offset(pnt.X() - center.X(), pnt.Y() - center.Y(), pnt.Z() - center.Z());
        return std::atan2(offset.dot(yDir), offset.dot(xDir));
    };
    const double midPolar = 0.5 * (polarOf(pEllipse->Value(first)) + polarOf(pEllipse->Value(last)));
    const double midParam = std::atan2(pEllipse->MajorRadius() * std::sin(midPolar),
        pEllipse->MinorRadius() * std::cos(midPolar));
    expectSamePoint(pEllipseArc->getPointAt(0.5), pEllipse->Value(midParam), 1e-9);
}

TEST(Sketch3DEdgeUtil, ConvertSplineEdgeKeepsKnots)
{
    ConvertFixture fixture;
    Handle(Geom_BSplineCurve) pSpline = makeClampedSpline();
    BRepBuilderAPI_MakeEdge edgeMaker(pSpline);
    ASSERT_TRUE(edgeMaker.IsDone());

    Result result = Result::Ok;
    wy3d::SketchEntity3D* pEntity = fixture.convert(edgeMaker.Edge(), result);
    ASSERT_EQ(result, Result::Ok);
    ASSERT_NE(pEntity, nullptr);

    const wy3d::SketchSpline3D* pSketchSpline = wy3d::SketchSpline3D::cast(pEntity);
    ASSERT_NE(pSketchSpline, nullptr);
    EXPECT_EQ(pSketchSpline->getMode(), wy3d::SplineMode::ControlPoints);
    EXPECT_EQ(pSketchSpline->getDegree(), 3u);
    EXPECT_EQ(pSketchSpline->getPoints().size(), 4u);
    ASSERT_EQ(pSketchSpline->getKnots().size(), 2u);
    EXPECT_NEAR(pSketchSpline->getKnots()[0], 0.0, 1e-12);
    EXPECT_NEAR(pSketchSpline->getKnots()[1], 1.0, 1e-12);
    ASSERT_EQ(pSketchSpline->getMultiplicities().size(), 2u);
    EXPECT_EQ(pSketchSpline->getMultiplicities()[0], 4u);
    EXPECT_EQ(pSketchSpline->getMultiplicities()[1], 4u);
    EXPECT_FALSE(pSketchSpline->isDegenerate(1e-7));

    for (int i = 0; i <= 8; ++i)
    {
        const double t = static_cast<double>(i) / 8.0;
        expectSamePoint(pSketchSpline->getPointAt(t), pSpline->Value(t), 1e-9);
    }
}

TEST(Sketch3DEdgeUtil, ConvertNullEdge)
{
    ConvertFixture fixture;

    Result result = Result::Ok;
    wy3d::SketchEntity3D* pEntity = fixture.convert(TopoDS_Edge(), result);
    EXPECT_EQ(result, Result::NullCurve);
    EXPECT_EQ(pEntity, nullptr);
}

TEST(Sketch3DEdgeUtil, ConvertWithoutTransaction)
{
    BRepBuilderAPI_MakeEdge edgeMaker(gp_Pnt(0.0, 0.0, 0.0), gp_Pnt(10.0, 0.0, 0.0));
    ASSERT_TRUE(edgeMaker.IsDone());

    wy3d::SketchEntity3D* pEntity(nullptr);
    EXPECT_EQ(wy3d::Sketch3DEdgeUtil::convert(nullptr, edgeMaker.Edge(), pEntity), Result::CreateFailed);
    EXPECT_EQ(pEntity, nullptr);
}

TEST(Sketch3DEdgeUtil, FailedConversionLeavesNoElement)
{
    ConvertFixture fixture;
    const std::size_t before = countElements(fixture.pDb.get());

    Result result = Result::Ok;
    fixture.convert(TopoDS_Edge(), result);
    EXPECT_EQ(result, Result::NullCurve);

    BRepBuilderAPI_MakeEdge lineMaker(gp_Pnt(0.0, 0.0, 0.0), gp_Pnt(1.0, 0.0, 0.0));
    ASSERT_TRUE(lineMaker.IsDone());
    TopoDS_Edge lineEdge = lineMaker.Edge();

    // A degenerate line: the range collapses to a single parameter.
    BRepBuilderAPI_MakeEdge degenerateMaker(new Geom_Line(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(1.0, 0.0, 0.0)), 0.0, 1e-12);
    if (degenerateMaker.IsDone() && !degenerateMaker.Edge().IsNull())
    {
        Result degenerateResult = Result::Ok;
        fixture.convert(degenerateMaker.Edge(), degenerateResult);
        EXPECT_EQ(degenerateResult, Result::Degenerate);
    }

    EXPECT_EQ(countElements(fixture.pDb.get()), before);
    (void)lineEdge;
}

// --- SketchSpline3D with a custom knot vector ---

TEST(Sketch3DEdgeUtil, SplineCustomKnotsCreate)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    const std::vector<wy::Vector3> poles = {
        wy::Vector3(0.0, 0.0, 0.0), wy::Vector3(1.0, 3.0, 0.0),
        wy::Vector3(4.0, -2.0, 1.0), wy::Vector3(7.0, 1.0, 2.0) };
    const std::vector<double> knots = { 0.0, 0.5, 1.0 };
    const std::vector<std::uint32_t> multiplicities = { 3, 1, 3 };

    wy3d::SketchSpline3D* pSpline(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        EXPECT_EQ(wy3d::SketchSpline3D::create(pTrans, 2, poles, knots, multiplicities, pSpline), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSpline, nullptr);
    EXPECT_EQ(pSpline->getMode(), wy3d::SplineMode::ControlPoints);
    EXPECT_EQ(pSpline->getDegree(), 2u);
    EXPECT_EQ(pSpline->getKnots(), knots);
    EXPECT_EQ(pSpline->getMultiplicities(), multiplicities);

    Handle(Geom_BSplineCurve) pCurve = pSpline->getOccSpline();
    ASSERT_FALSE(pCurve.IsNull());
    EXPECT_EQ(pCurve->NbKnots(), 3);
    EXPECT_EQ(pCurve->Degree(), 2);
    EXPECT_NEAR(pCurve->Knot(2), 0.5, 1e-12);
    EXPECT_EQ(pCurve->Multiplicity(2), 1);
    EXPECT_FALSE(pSpline->isDegenerate(1e-7));
    EXPECT_GT(pSpline->getLength(), 0.0);
    expectSamePoint(pSpline->getPointAt(0.0), pCurve->Value(0.0), 1e-9);
}

TEST(Sketch3DEdgeUtil, SplineCustomKnotsRejectsInvalidInput)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    const std::size_t before = countElements(pDb.get());

    const std::vector<wy::Vector3> poles = {
        wy::Vector3(0.0, 0.0, 0.0), wy::Vector3(1.0, 3.0, 0.0),
        wy::Vector3(4.0, -2.0, 1.0), wy::Vector3(7.0, 1.0, 2.0) };

    struct RejectCase
    {
        std::uint32_t degree;
        std::vector<double> knots;
        std::vector<std::uint32_t> multiplicities;
    };

    // Ten poles and degree 9 are consistent with each other: sum(mults) has to be 20, which
    // { 10, 10 } on { 0, 1 } supplies. The degree limit is the only thing left to reject it.
    {
        std::vector<wy::Vector3> manyPoles;
        for (int i = 0; i < 10; ++i) manyPoles.push_back(wy::Vector3(i * 1.0, 0.0, 0.0));

        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::SketchSpline3D* pSpline(nullptr);
        EXPECT_EQ(wy3d::SketchSpline3D::create(pTrans, 9, manyPoles, { 0.0, 1.0 }, { 10u, 10u }, pSpline),
            wy::ErrorStatus::InvalidInput);
        EXPECT_EQ(pSpline, nullptr);
        EXPECT_EQ(pMgr->abortTransaction(), wy::ErrorStatus::Ok);
    }

    // The four poles make degree 3 want sum(mults) = 8.
    const std::vector<RejectCase> cases = {
        { 3u, { 0.0, 0.5, 1.0 }, { 4u, 4u } },         // knots/multiplicities size mismatch
        { 3u, { 0.0, 1.0 }, { 4u, 3u } },              // sum(mults) < nbPoles + degree + 1
        { 3u, { 0.0, 1.0 }, { 4u, 5u } },              // multiplicity > degree + 1
        { 3u, { 1.0, 0.0 }, { 4u, 4u } },              // knots not increasing
    };

    for (const RejectCase& testCase : cases)
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::SketchSpline3D* pSpline(nullptr);
        EXPECT_EQ(wy3d::SketchSpline3D::create(pTrans, testCase.degree, poles, testCase.knots,
            testCase.multiplicities, pSpline), wy::ErrorStatus::InvalidInput);
        EXPECT_EQ(pSpline, nullptr);
        EXPECT_EQ(pMgr->abortTransaction(), wy::ErrorStatus::Ok);
    }

    EXPECT_EQ(countElements(pDb.get()), before);
}

// The knot vector builder carries an order bound of its own, and it has to sit one above the
// degree limit rather than at it. Set one short, this call hands back a null curve, and a spline
// with a null curve makes the whole sketch read as degenerate - which is a silent failure, not
// an error the caller sees.
TEST(Sketch3DEdgeUtil, SplineCustomKnotsAtTheDegreeLimit)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    // Degree 8 over 9 poles takes sum(mults) = 18, so a clamped { 9, 9 } across { 0, 1 }.
    std::vector<wy::Vector3> poles;
    for (int i = 0; i < 9; ++i) poles.push_back(wy::Vector3(i * 5.0, std::sin(i * 0.8) * 4.0, 0.0));

    wy3d::SketchSpline3D* pSpline(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        EXPECT_EQ(wy3d::SketchSpline3D::create(pTrans, 8, poles, { 0.0, 1.0 }, { 9u, 9u }, pSpline),
            wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSpline, nullptr);
    EXPECT_EQ(pSpline->getDegree(), 8u);
    ASSERT_FALSE(pSpline->getOccSpline().IsNull());
    EXPECT_EQ(pSpline->getOccSpline()->Degree(), 8);
    EXPECT_FALSE(pSpline->isDegenerate(1e-7));
    EXPECT_GT(pSpline->getLength(), 0.0);
}

// The 2D spline has no unit test file of its own, so it is covered here, where a 2D spline is
// already built as a source for the conversion tests. Its degree limit is the same as the 3D
// one, and the two are meant to move together.
TEST(Sketch3DEdgeUtil, SketchSplineDegreeLimit)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    std::vector<wy::Vector2> controlPoints;
    for (int i = 0; i < 9; ++i) controlPoints.push_back(wy::Vector2(i * 10.0, std::sin(i * 0.6) * 4.0));

    wy3d::SketchSpline* pSpline(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        EXPECT_EQ(wy3d::SketchSpline::create(pTrans, 8, controlPoints, pSpline), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pSpline, nullptr);
    EXPECT_EQ(pSpline->getDegree(), 8u);
    EXPECT_GT(pSpline->getLength(), 0.0);

    std::vector<wy::Vector2> tenPoints = controlPoints;
    tenPoints.push_back(wy::Vector2(90.0, 0.0));
    wy3d::SketchSpline* pTooHigh(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        EXPECT_EQ(wy3d::SketchSpline::create(pTrans, 9, tenPoints, pTooHigh), wy::ErrorStatus::InvalidInput);
        EXPECT_EQ(pMgr->abortTransaction(), wy::ErrorStatus::Ok);
    }
    EXPECT_EQ(pTooHigh, nullptr);
}

TEST(Sketch3DEdgeUtil, SplineCustomKnotsIORoundTrip)
{
    std::string filePath("./test_spline3d_knots.wy3dt");
    wydb::ElementId splineId = wydb::ElementId::kNull;

    const std::vector<wy::Vector3> poles = {
        wy::Vector3(0.0, 0.0, 0.0), wy::Vector3(1.0, 3.0, 0.0),
        wy::Vector3(4.0, -2.0, 1.0), wy::Vector3(7.0, 1.0, 2.0) };
    const std::vector<double> knots = { 0.0, 0.5, 1.0 };
    const std::vector<std::uint32_t> multiplicities = { 3, 1, 3 };

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        wydb::TransactionManager* pMgr = pDb->getTransactionManager();

        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::SketchSpline3D* pSpline(nullptr);
        EXPECT_EQ(wy3d::SketchSpline3D::create(pTrans, 2, poles, knots, multiplicities, pSpline), wy::ErrorStatus::Ok);
        ASSERT_NE(pSpline, nullptr);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
        splineId = pSpline->getId();

        EXPECT_EQ(pDb->writeFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);
    }

    {
        std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
        EXPECT_EQ(pDb->readFile(filePath, { wydb::FileType::Text }), wy::ErrorStatus::Ok);

        const wy3d::SketchSpline3D* pSpline = wy3d::SketchSpline3D::cast(pDb->getElement(splineId));
        ASSERT_NE(pSpline, nullptr);
        EXPECT_EQ(pSpline->getMode(), wy3d::SplineMode::ControlPoints);
        EXPECT_EQ(pSpline->getDegree(), 2u);
        EXPECT_EQ(pSpline->getKnots(), knots);
        EXPECT_EQ(pSpline->getMultiplicities(), multiplicities);
        EXPECT_EQ(pSpline->getPoints().size(), poles.size());
        EXPECT_FALSE(pSpline->isDegenerate(1e-7));
        EXPECT_GT(pSpline->getLength(), 0.0);
    }

    std::remove(filePath.c_str());
}

TEST(Sketch3DEdgeUtil, SplineWithoutKnotsKeepsUniformDefaults)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    const std::vector<wy::Vector3> poles = {
        wy::Vector3(0.0, 0.0, 0.0), wy::Vector3(1.0, 3.0, 0.0),
        wy::Vector3(4.0, -2.0, 1.0), wy::Vector3(7.0, 1.0, 2.0) };

    wy3d::SketchSpline3D* pControlSpline(nullptr);
    wy3d::SketchSpline3D* pFitSpline(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        EXPECT_EQ(wy3d::SketchSpline3D::create(pTrans, 3, poles, pControlSpline), wy::ErrorStatus::Ok);
        EXPECT_EQ(wy3d::SketchSpline3D::create(pTrans, poles, pFitSpline), wy::ErrorStatus::Ok);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pControlSpline, nullptr);
    ASSERT_NE(pFitSpline, nullptr);

    // Drawing commands leave the knots empty: the uniform path must stay untouched.
    EXPECT_TRUE(pControlSpline->getKnots().empty());
    EXPECT_TRUE(pControlSpline->getMultiplicities().empty());
    EXPECT_TRUE(pFitSpline->getKnots().empty());

    Handle(Geom_BSplineCurve) pCurve = pControlSpline->getOccSpline();
    ASSERT_FALSE(pCurve.IsNull());
    EXPECT_EQ(pCurve->Degree(), 3);
    EXPECT_EQ(pCurve->NbKnots(), 2);
    EXPECT_NEAR(pCurve->Knot(1), 0.0, 1e-12);
    EXPECT_NEAR(pCurve->Knot(pCurve->NbKnots()), 1.0, 1e-12);
    EXPECT_EQ(pCurve->Multiplicity(1), 4);
    EXPECT_EQ(pCurve->Multiplicity(pCurve->NbKnots()), 4);
}

// --- 2D sketch curves are lifted into 3D through their own sketch plane ---

TEST(Sketch3DEdgeUtil, Convert2DSketchLine)
{
    Sketch2DFixture fixture;
    ASSERT_NE(fixture.pSketch, nullptr);

    wy3d::SketchLine* pLine(nullptr);
    ASSERT_EQ(wy3d::SketchLine::create(fixture.pTrans, wy::Vector2(1.0, 2.0), wy::Vector2(5.0, 7.0), pLine),
        wy::ErrorStatus::Ok);
    ASSERT_NE(pLine, nullptr);
    EXPECT_EQ(fixture.pSketch->addEntity(pLine), wy::ErrorStatus::Ok);

    Result result(Result::CreateFailed);
    wy3d::SketchEntity3D* pEntity = fixture.convert(pLine, result);
    EXPECT_EQ(result, Result::Ok);
    const wy3d::SketchLine3D* pLine3D = wy3d::SketchLine3D::cast(pEntity);
    ASSERT_NE(pLine3D, nullptr);
    expectSamePoint(pLine3D->getStartPoint(), fixture.plane.value(wy::Vector2(1.0, 2.0)));
    expectSamePoint(pLine3D->getEndPoint(), fixture.plane.value(wy::Vector2(5.0, 7.0)));
}

TEST(Sketch3DEdgeUtil, Convert2DSketchCircle)
{
    Sketch2DFixture fixture;
    ASSERT_NE(fixture.pSketch, nullptr);

    wy3d::SketchCircle* pCircle(nullptr);
    ASSERT_EQ(wy3d::SketchCircle::create(fixture.pTrans, wy::Vector2(3.0, -2.0), 4.0, pCircle), wy::ErrorStatus::Ok);
    ASSERT_NE(pCircle, nullptr);
    EXPECT_EQ(fixture.pSketch->addEntity(pCircle), wy::ErrorStatus::Ok);

    Result result(Result::CreateFailed);
    wy3d::SketchEntity3D* pEntity = fixture.convert(pCircle, result);
    EXPECT_EQ(result, Result::Ok);
    const wy3d::SketchCircle3D* pCircle3D = wy3d::SketchCircle3D::cast(pEntity);
    ASSERT_NE(pCircle3D, nullptr);
    expectSamePoint(pCircle3D->getCenter(), fixture.plane.value(wy::Vector2(3.0, -2.0)));
    expectSamePoint(pCircle3D->getNormal(), fixture.plane.getNormal());
    expectSamePoint(pCircle3D->getXDir(), fixture.plane.getXDir());
    EXPECT_NEAR(pCircle3D->getRadius(), 4.0, 1e-12);
}

TEST(Sketch3DEdgeUtil, Convert2DSketchArc)
{
    Sketch2DFixture fixture;
    ASSERT_NE(fixture.pSketch, nullptr);

    wy3d::SketchArc* pArc(nullptr);
    ASSERT_EQ(wy3d::SketchArc::create(fixture.pTrans, wy::Vector2(1.0, 1.0), 2.0, 0.3, 1.2, pArc),
        wy::ErrorStatus::Ok);
    ASSERT_NE(pArc, nullptr);
    EXPECT_EQ(fixture.pSketch->addEntity(pArc), wy::ErrorStatus::Ok);

    Result result(Result::CreateFailed);
    wy3d::SketchEntity3D* pEntity = fixture.convert(pArc, result);
    EXPECT_EQ(result, Result::Ok);
    const wy3d::SketchArc3D* pArc3D = wy3d::SketchArc3D::cast(pEntity);
    ASSERT_NE(pArc3D, nullptr);
    expectSamePoint(pArc3D->getCenter(), fixture.plane.value(wy::Vector2(1.0, 1.0)));
    expectSamePoint(pArc3D->getNormal(), fixture.plane.getNormal());
    expectSamePoint(pArc3D->getXDir(), fixture.plane.getXDir());
    EXPECT_NEAR(pArc3D->getRadius(), 2.0, 1e-12);
    EXPECT_NEAR(pArc3D->getStartAngle(), 0.3, 1e-12);
    EXPECT_NEAR(pArc3D->getEndAngle(), 1.2, 1e-12);
}

TEST(Sketch3DEdgeUtil, Convert2DSketchEllipse)
{
    Sketch2DFixture fixture;
    ASSERT_NE(fixture.pSketch, nullptr);

    const wy::Vector2 majorAxis(4.0, 3.0);  // length is the major radius: 5
    wy3d::SketchEllipse* pEllipse(nullptr);
    ASSERT_EQ(wy3d::SketchEllipse::create(fixture.pTrans, wy::Vector2(-1.0, 0.5), majorAxis, 0.5, pEllipse),
        wy::ErrorStatus::Ok);
    ASSERT_NE(pEllipse, nullptr);
    EXPECT_EQ(fixture.pSketch->addEntity(pEllipse), wy::ErrorStatus::Ok);

    Result result(Result::CreateFailed);
    wy3d::SketchEntity3D* pEntity = fixture.convert(pEllipse, result);
    EXPECT_EQ(result, Result::Ok);
    const wy3d::SketchEllipse3D* pEllipse3D = wy3d::SketchEllipse3D::cast(pEntity);
    ASSERT_NE(pEllipse3D, nullptr);
    expectSamePoint(pEllipse3D->getCenter(), fixture.plane.value(wy::Vector2(-1.0, 0.5)));
    expectSamePoint(pEllipse3D->getNormal(), fixture.plane.getNormal());
    EXPECT_NEAR(pEllipse3D->getMajorRadius(), majorAxis.length(), 1e-12);
    EXPECT_NEAR(pEllipse3D->getRadiusRatio(), 0.5, 1e-12);

    wy::Vector3 xDir = fixture.plane.value(majorAxis) - fixture.plane.value(wy::Vector2(0.0, 0.0));
    xDir.normalize();
    expectSamePoint(pEllipse3D->getXDir(), xDir);
}

TEST(Sketch3DEdgeUtil, Convert2DSketchEllipseArcMatchesGeometry)
{
    Sketch2DFixture fixture;
    ASSERT_NE(fixture.pSketch, nullptr);

    wy3d::SketchEllipseArc* pArc(nullptr);
    ASSERT_EQ(wy3d::SketchEllipseArc::create(fixture.pTrans, wy::Vector2(0.0, 1.0), wy::Vector2(5.0, 1.0), 0.6,
        0.4, 2.2, pArc), wy::ErrorStatus::Ok);
    ASSERT_NE(pArc, nullptr);
    EXPECT_EQ(fixture.pSketch->addEntity(pArc), wy::ErrorStatus::Ok);

    Result result(Result::CreateFailed);
    wy3d::SketchEntity3D* pEntity = fixture.convert(pArc, result);
    EXPECT_EQ(result, Result::Ok);
    const wy3d::SketchEllipseArc3D* pArc3D = wy3d::SketchEllipseArc3D::cast(pEntity);
    ASSERT_NE(pArc3D, nullptr);

    // Angle conventions differ between the 2D entity and its OCCT curve, so compare points
    for (int i = 0; i <= 4; ++i)
    {
        const double t = 0.25 * i;
        expectSamePoint(pArc3D->getPointAt(t), fixture.plane.value(pArc->getPointAt(t)), 1e-9);
    }
}

TEST(Sketch3DEdgeUtil, Convert2DSketchSplineIsExact)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();
    const wy3d::SketchPlane plane = makeTiltedPlane();

    const std::vector<wy::Vector2> fitPoints = {
        wy::Vector2(0.0, 0.0), wy::Vector2(2.0, 4.0), wy::Vector2(6.0, -3.0), wy::Vector2(9.0, 2.0) };
    wydb::ElementId sketchId = wydb::ElementId::kNull;
    wydb::ElementId splineId = wydb::ElementId::kNull;
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        wy3d::Sketch* pSketch(nullptr);
        wy3d::SketchSpline* pSpline(nullptr);
        ASSERT_EQ(wy3d::Sketch::create(pTrans, plane, pSketch), wy::ErrorStatus::Ok);
        ASSERT_EQ(wy3d::SketchSpline::create(pTrans, fitPoints, pSpline), wy::ErrorStatus::Ok);
        ASSERT_NE(pSketch, nullptr);
        ASSERT_NE(pSpline, nullptr);
        EXPECT_EQ(pSketch->addEntity(pSpline), wy::ErrorStatus::Ok);
        // The curve cache of a spline is built when the transaction is committed
        ASSERT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
        sketchId = pSketch->getId();
        splineId = pSpline->getId();
    }

    wydb::Transaction* pTrans = pMgr->startTransaction();
    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(sketchId));
    const wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pDb->getElement(splineId));
    ASSERT_NE(pSketch, nullptr);
    ASSERT_NE(pSpline, nullptr);

    wy3d::SketchEntity3D* pEntity(nullptr);
    ASSERT_EQ(wy3d::Sketch3DEdgeUtil::convert(pTrans, pSketch, pSpline, pEntity), Result::Ok);
    const wy3d::SketchSpline3D* pSpline3D = wy3d::SketchSpline3D::cast(pEntity);
    ASSERT_NE(pSpline3D, nullptr);

    // Poles, knots and multiplicities must survive the lift into 3D unchanged
    Handle(Geom2d_BSplineCurve) pSource = pSpline->getOccSpline();
    Handle(Geom_BSplineCurve) pTarget = pSpline3D->getOccSpline();
    ASSERT_FALSE(pSource.IsNull());
    ASSERT_FALSE(pTarget.IsNull());
    EXPECT_EQ(pTarget->Degree(), pSource->Degree());
    ASSERT_EQ(pTarget->NbPoles(), pSource->NbPoles());
    for (Standard_Integer i = 1; i <= pSource->NbPoles(); ++i)
    {
        const gp_Pnt2d& pnt = pSource->Pole(i);
        expectSamePoint(pTarget->Pole(i), plane.value(wy::Vector2(pnt.X(), pnt.Y())), 1e-9);
    }
    // The knots are renormalized into [0,1], which only reparameterizes the curve
    ASSERT_EQ(pTarget->NbKnots(), pSource->NbKnots());
    const double knotFirst = pSource->Knot(1);
    const double knotRange = pSource->Knot(pSource->NbKnots()) - knotFirst;
    ASSERT_GT(knotRange, 0.0);
    for (Standard_Integer i = 1; i <= pSource->NbKnots(); ++i)
    {
        EXPECT_NEAR(pTarget->Knot(i), (pSource->Knot(i) - knotFirst) / knotRange, 1e-12);
        EXPECT_EQ(pTarget->Multiplicity(i), pSource->Multiplicity(i));
    }

    // ... and the curve itself is the same curve, lifted into 3D
    for (int i = 0; i <= 4; ++i)
    {
        const double t = 0.25 * i;
        expectSamePoint(pSpline3D->getPointAt(t), plane.value(pSpline->getPointAt(t)), 1e-9);
    }

    EXPECT_EQ(pMgr->abortTransaction(), wy::ErrorStatus::Ok);
}

TEST(Sketch3DEdgeUtil, Convert2DUncommittedSplineReturnsNullCurve)
{
    Sketch2DFixture fixture;
    ASSERT_NE(fixture.pSketch, nullptr);

    const std::vector<wy::Vector2> fitPoints = {
        wy::Vector2(0.0, 0.0), wy::Vector2(2.0, 4.0), wy::Vector2(6.0, -3.0) };
    wy3d::SketchSpline* pSpline(nullptr);
    ASSERT_EQ(wy3d::SketchSpline::create(fixture.pTrans, fitPoints, pSpline), wy::ErrorStatus::Ok);
    ASSERT_NE(pSpline, nullptr);
    EXPECT_EQ(fixture.pSketch->addEntity(pSpline), wy::ErrorStatus::Ok);

    // Within the open transaction the curve cache is not built yet: report it instead of
    // walking into the builder assert
    Result result(Result::CreateFailed);
    EXPECT_EQ(fixture.convert(pSpline, result), nullptr);
    EXPECT_EQ(result, Result::NullCurve);
}

// --- entities of another 3D sketch ---

TEST(Sketch3DEdgeUtil, Convert3DSketchEntityKeepsGeometry)
{
    ConvertFixture fixture;

    wy3d::SketchLine3D* pLine(nullptr);
    wy3d::SketchCircle3D* pCircle(nullptr);
    wy3d::SketchArc3D* pArc(nullptr);
    wy3d::SketchEllipse3D* pEllipse(nullptr);
    wy3d::SketchEllipseArc3D* pEllipseArc(nullptr);
    wy3d::SketchSpline3D* pSpline(nullptr);
    ASSERT_EQ(wy3d::SketchLine3D::create(fixture.pTrans, wy::Vector3(1.0, 2.0, 3.0), wy::Vector3(4.0, 6.0, 8.0),
        pLine), wy::ErrorStatus::Ok);
    ASSERT_EQ(wy3d::SketchCircle3D::create(fixture.pTrans, wy::Vector3(0.0, 0.0, 1.0),
        wy::Vector3(0.0, 1.0, 0.0), wy::Vector3(1.0, 0.0, 0.0), 3.0, pCircle), wy::ErrorStatus::Ok);
    ASSERT_EQ(wy3d::SketchArc3D::create(fixture.pTrans, wy::Vector3(1.0, 0.0, 0.0), wy::Vector3::kZAxis,
        wy::Vector3::kXAxis, 2.0, 0.5, 2.0, pArc), wy::ErrorStatus::Ok);
    ASSERT_EQ(wy3d::SketchEllipse3D::create(fixture.pTrans, wy::Vector3(0.0, 0.0, 0.0), wy::Vector3::kZAxis,
        wy::Vector3::kXAxis, 5.0, 0.4, pEllipse), wy::ErrorStatus::Ok);
    ASSERT_EQ(wy3d::SketchEllipseArc3D::create(fixture.pTrans, wy::Vector3(0.0, 0.0, 0.0), wy::Vector3::kZAxis,
        wy::Vector3::kXAxis, 5.0, 0.4, 0.3, 2.1, pEllipseArc), wy::ErrorStatus::Ok);
    const std::vector<wy::Vector3> poles = {
        wy::Vector3(0.0, 0.0, 0.0), wy::Vector3(1.0, 3.0, 0.0),
        wy::Vector3(4.0, -2.0, 1.0), wy::Vector3(7.0, 1.0, 2.0) };
    ASSERT_EQ(wy3d::SketchSpline3D::create(fixture.pTrans, 3, poles, pSpline), wy::ErrorStatus::Ok);

    const std::size_t before = countElements(fixture.pDb.get());

    const std::vector<const wy3d::SketchEntity3D*> sources = { pLine, pCircle, pArc, pEllipse,
        pEllipseArc, pSpline };
    for (const wy3d::SketchEntity3D* pSource : sources)
    {
        wy3d::SketchEntity3D* pOut(nullptr);
        EXPECT_EQ(wy3d::Sketch3DEdgeUtil::convert(fixture.pTrans, pSource, pOut), Result::Ok);
        ASSERT_NE(pOut, nullptr);
        // The entity keeps its type across the round trip through OCCT
        EXPECT_EQ(pOut->getClassInfo(), pSource->getClassInfo());
    }

    EXPECT_EQ(countElements(fixture.pDb.get()), before + sources.size());

    // The sources must not be touched: the included curve is a snapshot
    expectSamePoint(pLine->getStartPoint(), wy::Vector3(1.0, 2.0, 3.0));
    expectSamePoint(pLine->getEndPoint(), wy::Vector3(4.0, 6.0, 8.0));
    EXPECT_NEAR(pCircle->getRadius(), 3.0, 1e-12);
    EXPECT_NEAR(pArc->getStartAngle(), 0.5, 1e-12);
    EXPECT_NEAR(pEllipse->getRadiusRatio(), 0.4, 1e-12);
    EXPECT_NEAR(pEllipseArc->getEndAngle(), 2.1, 1e-12);
    ASSERT_EQ(pSpline->getPoints().size(), poles.size());
    for (std::size_t i = 0; i < poles.size(); ++i)
    {
        expectSamePoint(pSpline->getPoints()[i], poles[i]);
    }
}

TEST(Sketch3DEdgeUtil, Convert3DSketchSplineKeepsKnots)
{
    ConvertFixture fixture;

    const std::vector<wy::Vector3> poles = {
        wy::Vector3(0.0, 0.0, 0.0), wy::Vector3(1.0, 3.0, 0.0),
        wy::Vector3(4.0, -2.0, 1.0), wy::Vector3(7.0, 1.0, 2.0) };
    const std::vector<double> knots = { 0.0, 0.5, 1.0 };
    const std::vector<std::uint32_t> multiplicities = { 3, 1, 3 };

    wy3d::SketchSpline3D* pSpline(nullptr);
    ASSERT_EQ(wy3d::SketchSpline3D::create(fixture.pTrans, 2, poles, knots, multiplicities, pSpline),
        wy::ErrorStatus::Ok);
    ASSERT_NE(pSpline, nullptr);

    Result result(Result::CreateFailed);
    wy3d::SketchEntity3D* pEntity(nullptr);
    ASSERT_EQ(wy3d::Sketch3DEdgeUtil::convert(fixture.pTrans, pSpline, pEntity), Result::Ok);
    const wy3d::SketchSpline3D* pCopy = wy3d::SketchSpline3D::cast(pEntity);
    ASSERT_NE(pCopy, nullptr);
    EXPECT_NE(pCopy, pSpline);
    EXPECT_EQ(pCopy->getMode(), wy3d::SplineMode::ControlPoints);
    EXPECT_EQ(pCopy->getDegree(), 2u);
    ASSERT_EQ(pCopy->getPoints().size(), poles.size());
    for (std::size_t i = 0; i < poles.size(); ++i)
    {
        expectSamePoint(pCopy->getPoints()[i], poles[i]);
    }
    EXPECT_EQ(pCopy->getKnots(), knots);
    EXPECT_EQ(pCopy->getMultiplicities(), multiplicities);
}

TEST(Sketch3DEdgeUtil, ConvertNullSources)
{
    ConvertFixture fixture;

    wy3d::SketchEntity3D* pEntity(nullptr);
    EXPECT_EQ(wy3d::Sketch3DEdgeUtil::convert(fixture.pTrans, static_cast<const wy3d::Sketch*>(nullptr),
        nullptr, pEntity), Result::NullCurve);
    EXPECT_EQ(pEntity, nullptr);
    EXPECT_EQ(wy3d::Sketch3DEdgeUtil::convert(fixture.pTrans, static_cast<const wy3d::SketchEntity3D*>(nullptr),
        pEntity), Result::NullCurve);
    EXPECT_EQ(pEntity, nullptr);
}
