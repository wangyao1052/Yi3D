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

#include <wy3dSketchLine3D.h>
#include <wy3dSketchCircle3D.h>
#include "wy3d/topo/Sketch3DTopoBuilder.h"

#include <TopoDS_Edge.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <GeomAbs_CurveType.hxx>
#include <Geom_Line.hxx>
#include <Geom_Circle.hxx>
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>
#include <gp_Vec.hxx>

#include <cmath>

// --- helpers ---

static wy3d::SketchLine3D* createLine(wydb::Transaction* pTrans, const wy::Vector3& startPnt, const wy::Vector3& endPnt)
{
    wy3d::SketchLine3D* pLine(nullptr);
    EXPECT_EQ(wy3d::SketchLine3D::create(pTrans, startPnt, endPnt, pLine), wy::ErrorStatus::Ok);
    return pLine;
}

static wy3d::SketchCircle3D* createCircle(wydb::Transaction* pTrans, const wy::Vector3& center, const wy::Vector3& normal, const wy::Vector3& xDir, double radius)
{
    wy3d::SketchCircle3D* pCircle(nullptr);
    EXPECT_EQ(wy3d::SketchCircle3D::create(pTrans, center, normal, xDir, radius, pCircle), wy::ErrorStatus::Ok);
    return pCircle;
}

static void getShapeBounds(const TopoDS_Shape& shape, gp_Pnt& cornerMin, gp_Pnt& cornerMax)
{
    Bnd_Box bndBox;
    BRepBndLib::Add(shape, bndBox);
    cornerMin = bndBox.CornerMin();
    cornerMax = bndBox.CornerMax();
}

static Handle(Geom_Curve) edgeCurve(const TopoDS_Edge& edge)
{
    double first(0.0), last(0.0);
    return BRep_Tool::Curve(edge, first, last);
}

// --- Line ---

TEST(Sketch3DTopo, LineEdgeExactEndpoints)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    wy3d::SketchLine3D* pLine(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        pLine = createLine(pTrans, wy::Vector3(1.0, 2.0, 3.0), wy::Vector3(4.0, 6.0, 8.0));
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pLine, nullptr);

    wy3d::Sketch3DTopoBuilder builder;
    TopoDS_Edge edge = builder.makeEdge(pLine);
    ASSERT_FALSE(edge.IsNull());

    BRepAdaptor_Curve adaptor(edge);
    EXPECT_EQ(adaptor.GetType(), GeomAbs_Line);
    EXPECT_NEAR(adaptor.FirstParameter(), 0.0, 1e-9);
    EXPECT_NEAR(adaptor.LastParameter(), std::sqrt(50.0), 1e-9);

    gp_Pnt pStart = adaptor.Value(adaptor.FirstParameter());
    EXPECT_DOUBLE_EQ(pStart.X(), 1.0);
    EXPECT_DOUBLE_EQ(pStart.Y(), 2.0);
    EXPECT_DOUBLE_EQ(pStart.Z(), 3.0);
    gp_Pnt pEnd = adaptor.Value(adaptor.LastParameter());
    EXPECT_DOUBLE_EQ(pEnd.X(), 4.0);
    EXPECT_DOUBLE_EQ(pEnd.Y(), 6.0);
    EXPECT_DOUBLE_EQ(pEnd.Z(), 8.0);

    Handle(Geom_Line) geomLine = Handle(Geom_Line)::DownCast(edgeCurve(edge));
    ASSERT_FALSE(geomLine.IsNull());
    const double invLen = 1.0 / std::sqrt(50.0);
    EXPECT_NEAR(geomLine->Position().Direction().X(), 3.0 * invLen, 1e-9);
    EXPECT_NEAR(geomLine->Position().Direction().Y(), 4.0 * invLen, 1e-9);
    EXPECT_NEAR(geomLine->Position().Direction().Z(), 5.0 * invLen, 1e-9);

    gp_Pnt cornerMin, cornerMax;
    getShapeBounds(edge, cornerMin, cornerMax);
    EXPECT_NEAR(cornerMin.X(), 1.0, 1e-5);
    EXPECT_NEAR(cornerMin.Y(), 2.0, 1e-5);
    EXPECT_NEAR(cornerMin.Z(), 3.0, 1e-5);
    EXPECT_NEAR(cornerMax.X(), 4.0, 1e-5);
    EXPECT_NEAR(cornerMax.Y(), 6.0, 1e-5);
    EXPECT_NEAR(cornerMax.Z(), 8.0, 1e-5);
}

// --- Circle ---

TEST(Sketch3DTopo, CircleEdgeCenterRadiusPlane)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    wy3d::SketchCircle3D* pCircle(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        pCircle = createCircle(pTrans, wy::Vector3(10.0, 0.0, 5.0), wy::Vector3(0.0, 0.0, 1.0), wy::Vector3(1.0, 0.0, 0.0), 25.0);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pCircle, nullptr);

    wy3d::Sketch3DTopoBuilder builder;
    TopoDS_Edge edge = builder.makeEdge(pCircle);
    ASSERT_FALSE(edge.IsNull());

    BRepAdaptor_Curve adaptor(edge);
    EXPECT_EQ(adaptor.GetType(), GeomAbs_Circle);
    EXPECT_NEAR(adaptor.FirstParameter(), 0.0, 1e-9);
    EXPECT_NEAR(adaptor.LastParameter(), 2.0 * wy3d::PI, 1e-9);

    // Parametric start point = center + radius * xDir
    gp_Pnt p0 = adaptor.Value(adaptor.FirstParameter());
    EXPECT_DOUBLE_EQ(p0.X(), 35.0);
    EXPECT_DOUBLE_EQ(p0.Y(), 0.0);
    EXPECT_DOUBLE_EQ(p0.Z(), 5.0);

    Handle(Geom_Circle) geomCircle = Handle(Geom_Circle)::DownCast(edgeCurve(edge));
    ASSERT_FALSE(geomCircle.IsNull());
    EXPECT_DOUBLE_EQ(geomCircle->Location().X(), 10.0);
    EXPECT_DOUBLE_EQ(geomCircle->Location().Y(), 0.0);
    EXPECT_DOUBLE_EQ(geomCircle->Location().Z(), 5.0);
    EXPECT_DOUBLE_EQ(geomCircle->Radius(), 25.0);
    EXPECT_DOUBLE_EQ(geomCircle->Axis().Direction().X(), 0.0);
    EXPECT_DOUBLE_EQ(geomCircle->Axis().Direction().Y(), 0.0);
    EXPECT_DOUBLE_EQ(geomCircle->Axis().Direction().Z(), 1.0);
    EXPECT_DOUBLE_EQ(geomCircle->Position().XDirection().X(), 1.0);
    EXPECT_DOUBLE_EQ(geomCircle->Position().XDirection().Y(), 0.0);
    EXPECT_DOUBLE_EQ(geomCircle->Position().XDirection().Z(), 0.0);

    gp_Pnt cornerMin, cornerMax;
    getShapeBounds(edge, cornerMin, cornerMax);
    EXPECT_NEAR(cornerMin.X(), -15.0, 1e-5);
    EXPECT_NEAR(cornerMax.X(), 35.0, 1e-5);
    EXPECT_NEAR(cornerMin.Y(), -25.0, 1e-5);
    EXPECT_NEAR(cornerMax.Y(), 25.0, 1e-5);
    EXPECT_NEAR(cornerMin.Z(), 5.0, 1e-5);
    EXPECT_NEAR(cornerMax.Z(), 5.0, 1e-5);
}

TEST(Sketch3DTopo, CircleEdgeTiltedNormal)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    wy3d::SketchCircle3D* pCircle(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        pCircle = createCircle(pTrans, wy::Vector3(10.0, 0.0, 5.0), wy::Vector3(0.0, 1.0, 0.0), wy::Vector3(1.0, 0.0, 0.0), 25.0);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pCircle, nullptr);

    wy3d::Sketch3DTopoBuilder builder;
    TopoDS_Edge edge = builder.makeEdge(pCircle);
    ASSERT_FALSE(edge.IsNull());

    BRepAdaptor_Curve adaptor(edge);
    EXPECT_EQ(adaptor.GetType(), GeomAbs_Circle);
    // Parametric start point = center + radius * xDir
    gp_Pnt p0 = adaptor.Value(adaptor.FirstParameter());
    EXPECT_DOUBLE_EQ(p0.X(), 35.0);
    EXPECT_DOUBLE_EQ(p0.Y(), 0.0);
    EXPECT_DOUBLE_EQ(p0.Z(), 5.0);
    Handle(Geom_Circle) geomCircle = Handle(Geom_Circle)::DownCast(edgeCurve(edge));
    ASSERT_FALSE(geomCircle.IsNull());
    EXPECT_DOUBLE_EQ(geomCircle->Axis().Direction().X(), 0.0);
    EXPECT_DOUBLE_EQ(geomCircle->Axis().Direction().Y(), 1.0);
    EXPECT_DOUBLE_EQ(geomCircle->Axis().Direction().Z(), 0.0);
    EXPECT_DOUBLE_EQ(geomCircle->Position().XDirection().X(), 1.0);
    EXPECT_DOUBLE_EQ(geomCircle->Position().XDirection().Y(), 0.0);
    EXPECT_DOUBLE_EQ(geomCircle->Position().XDirection().Z(), 0.0);

    gp_Pnt cornerMin, cornerMax;
    getShapeBounds(edge, cornerMin, cornerMax);
    EXPECT_NEAR(cornerMin.X(), -15.0, 1e-5);
    EXPECT_NEAR(cornerMax.X(), 35.0, 1e-5);
    EXPECT_NEAR(cornerMin.Y(), 0.0, 1e-5);
    EXPECT_NEAR(cornerMax.Y(), 0.0, 1e-5);
    EXPECT_NEAR(cornerMin.Z(), -20.0, 1e-5);
    EXPECT_NEAR(cornerMax.Z(), 30.0, 1e-5);
}

// --- Degenerate / boundary input ---

TEST(Sketch3DTopo, DegenerateLineReturnsNull)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    wy3d::SketchLine3D* pLine(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        pLine = createLine(pTrans, wy::Vector3(5.0, 5.0, 5.0), wy::Vector3(5.0, 5.0, 5.0));
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pLine, nullptr);

    wy3d::Sketch3DTopoBuilder builder;
    EXPECT_TRUE(builder.makeEdge(pLine).IsNull());
}

TEST(Sketch3DTopo, MinRadiusCircleReturnsEdge)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    wy3d::SketchCircle3D* pCircle(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        pCircle = createCircle(pTrans, wy::Vector3(10.0, 0.0, 5.0), wy::Vector3(0.0, 0.0, 1.0), wy::Vector3(1.0, 0.0, 0.0), 0.001);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pCircle, nullptr);

    wy3d::Sketch3DTopoBuilder builder;
    TopoDS_Edge edge = builder.makeEdge(pCircle);
    ASSERT_FALSE(edge.IsNull());

    BRepAdaptor_Curve adaptor(edge);
    EXPECT_EQ(adaptor.GetType(), GeomAbs_Circle);
    Handle(Geom_Circle) geomCircle = Handle(Geom_Circle)::DownCast(edgeCurve(edge));
    ASSERT_FALSE(geomCircle.IsNull());
    EXPECT_DOUBLE_EQ(geomCircle->Radius(), 0.001);
}

// --- Dispatch ---

TEST(Sketch3DTopo, DispatchOnEntity3DBase)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    wy3d::SketchLine3D* pLine(nullptr);
    wy3d::SketchCircle3D* pCircle(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        pLine = createLine(pTrans, wy::Vector3(1.0, 2.0, 3.0), wy::Vector3(4.0, 6.0, 8.0));
        pCircle = createCircle(pTrans, wy::Vector3(10.0, 0.0, 5.0), wy::Vector3(0.0, 0.0, 1.0), wy::Vector3(1.0, 0.0, 0.0), 25.0);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pLine, nullptr);
    ASSERT_NE(pCircle, nullptr);

    wy3d::Sketch3DTopoBuilder builder;

    const wy3d::SketchEntity3D* pLineEntity = pLine;
    TopoDS_Edge lineEdge = builder.makeEdge(pLineEntity);
    ASSERT_FALSE(lineEdge.IsNull());
    BRepAdaptor_Curve lineAdaptor(lineEdge);
    EXPECT_EQ(lineAdaptor.GetType(), GeomAbs_Line);

    const wy3d::SketchEntity3D* pCircleEntity = pCircle;
    TopoDS_Edge circleEdge = builder.makeEdge(pCircleEntity);
    ASSERT_FALSE(circleEdge.IsNull());
    BRepAdaptor_Curve circleAdaptor(circleEdge);
    EXPECT_EQ(circleAdaptor.GetType(), GeomAbs_Circle);
}

// --- Topo history ---

TEST(Sketch3DTopo, TopoHistoryRecordsIds)
{
    std::unique_ptr<wy3d::Database> pDb = std::make_unique<wy3d::Database>();
    wydb::TransactionManager* pMgr = pDb->getTransactionManager();

    wy3d::SketchLine3D* pLine(nullptr);
    wy3d::SketchCircle3D* pCircle(nullptr);
    {
        wydb::Transaction* pTrans = pMgr->startTransaction();
        pLine = createLine(pTrans, wy::Vector3(1.0, 2.0, 3.0), wy::Vector3(4.0, 6.0, 8.0));
        pCircle = createCircle(pTrans, wy::Vector3(10.0, 0.0, 5.0), wy::Vector3(0.0, 0.0, 1.0), wy::Vector3(1.0, 0.0, 0.0), 25.0);
        EXPECT_EQ(pMgr->endTransaction(), wy::ErrorStatus::Ok);
    }
    ASSERT_NE(pLine, nullptr);
    ASSERT_NE(pCircle, nullptr);

    wy3d::Sketch3DTopoBuilder builder(true);
    TopoDS_Edge lineEdge = builder.makeEdge(pLine);
    TopoDS_Edge circleEdge = builder.makeEdge(pCircle);
    ASSERT_FALSE(lineEdge.IsNull());
    ASSERT_FALSE(circleEdge.IsNull());

    const std::map<Handle(Geom_Curve), unsigned int>& curve2Id = builder.getCurve2IdMap();
    ASSERT_EQ(curve2Id.size(), 2u);

    // Every key maps to one of the two entity ids, and keys are the curves read back from the edges
    double first(0.0), last(0.0);
    Handle(Geom_Curve) lineCurve = BRep_Tool::Curve(lineEdge, first, last);
    Handle(Geom_Curve) circleCurve = BRep_Tool::Curve(circleEdge, first, last);
    ASSERT_FALSE(lineCurve.IsNull());
    ASSERT_FALSE(circleCurve.IsNull());

    auto lineIter = curve2Id.find(lineCurve);
    auto circleIter = curve2Id.find(circleCurve);
    ASSERT_NE(lineIter, curve2Id.cend());
    ASSERT_NE(circleIter, curve2Id.cend());
    EXPECT_EQ(lineIter->second, pLine->getId().value());
    EXPECT_EQ(circleIter->second, pCircle->getId().value());
}
