///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2024-2026 Wang Yao <wangyao1052@163.com>
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

#include "SketchTopoBuilder.h"

#include <cassert>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <Geom_Line.hxx>
#include <Geom_Circle.hxx>
#include <Geom_Ellipse.hxx>
#include <Geom_BSplineCurve.hxx>
#include <ElCLib.hxx>
#include <TopoDS.hxx>
#include <gp_Pln.hxx>

#include <wyVector2.h>
#include <wyVector3.h>
#include <wy3dMath.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>
#include <wy3dSketchEllipse.h>
#include <wy3dSketchEllipseArc.h>
#include <wy3dSketchSpline.h>

#include <TopExp.hxx>
#include <BRep_Tool.hxx>
#include <BRepTools_History.hxx>

NS_WY3D_BEG

SketchTopoBuilder::SketchTopoBuilder(const wy3d::Sketch* pSketch, bool recordTopoHistory)
    : _pSketch(pSketch), _recordTopoHistory(recordTopoHistory)
{
    assert(_pSketch);
}

static inline gp_Pnt toOccPnt(const wy::Vector3& vec)
{
    return gp_Pnt(vec.x(), vec.y(), vec.z());
}

static inline gp_Dir toOccDir(const wy::Vector3& vec)
{
    return gp_Dir(vec.x(), vec.y(), vec.z());
}

TopoDS_Edge SketchTopoBuilder::makeEdge(const wy3d::SketchCurve* pSketchCurve)
{
    Handle(Geom_Curve) geomCurve = this->toGeomCurve(pSketchCurve);
    if (geomCurve.IsNull())
    {
        assert(false);
        return TopoDS_Edge();
    }

    BRepBuilderAPI_MakeEdge makeEdge(geomCurve);
    if (makeEdge.IsDone())
    {
        if (_recordTopoHistory)
        {
            double first(0.0), last(0.0);
            Handle(Geom_Curve) curve = BRep_Tool::Curve(makeEdge.Edge(), first, last);
            assert(!curve.IsNull());
            _curve2Id[curve] = pSketchCurve->getId().value();
        }
        return makeEdge.Edge();
    }
    else
    {
        assert(false);
        return TopoDS_Edge();
    }
}

Handle(Geom_Curve) SketchTopoBuilder::toGeomCurve(const wy3d::SketchCurve* pSketchCurve) const
{
    assert(pSketchCurve);
    assert(_pSketch);
    const wy3d::SketchPlane& sketchPlane = _pSketch->getPlane();
    if (!sketchPlane.isValid())
    {
        assert(false);
        return nullptr;
    }

    if (const wy3d::SketchLine* pSketchLine = wy3d::SketchLine::cast(pSketchCurve))
    {
        wy::Vector3 startPnt = sketchPlane.value(pSketchLine->getStartPoint());
        wy::Vector3 endPnt = sketchPlane.value(pSketchLine->getEndPoint());
        wy::Vector3 dir = endPnt - startPnt;
        dir.normalize();
        if (dir.length() < 0.5) // 退化的直线段
        {
            return nullptr;
        }
        Handle(Geom_Line) line = new Geom_Line(toOccPnt(startPnt), toOccDir(dir));
        return new Geom_TrimmedCurve(line, 0, pSketchLine->getLength());
    }
    else if (const wy3d::SketchCircle* pSketchCircle = wy3d::SketchCircle::cast(pSketchCurve))
    {
        if (pSketchCircle->getRadius() < 1e-7)
        {
            assert(false);
            return nullptr;
        }
        wy::Vector3 centerPnt = sketchPlane.value(pSketchCircle->getCenter());
        wy::Vector3 normal = sketchPlane.getNormal();
        wy::Vector3 xDir = sketchPlane.getXDir();
        if (normal.length() < 0.5 || xDir.length() < 0.5)
        {
            assert(false);
            return nullptr;
        }
        gp_Ax2 ax2(toOccPnt(centerPnt), toOccDir(normal), toOccDir(xDir));
        return new Geom_Circle(ax2, pSketchCircle->getRadius());
    }
    else if (const wy3d::SketchArc* pSketchArc = wy3d::SketchArc::cast(pSketchCurve))
    {
        if (pSketchArc->getRadius() < 1e-7)
        {
            assert(false);
            return nullptr;
        }
        if (pSketchArc->getTotalAngle() < 1e-7)
        {
            assert(false);
            return nullptr;
        }
        wy::Vector3 centerPnt = sketchPlane.value(pSketchArc->getCenter());
        wy::Vector3 normal = sketchPlane.getNormal();
        wy::Vector3 xDir = sketchPlane.getXDir();
        if (normal.length() < 0.5 || xDir.length() < 0.5)
        {
            assert(false);
            return nullptr;
        }
        gp_Ax2 ax2(toOccPnt(centerPnt), toOccDir(normal), toOccDir(xDir));
        Handle(Geom_Circle) circle = new Geom_Circle(ax2, pSketchArc->getRadius());
        double startAngle = ElCLib::InPeriod(pSketchArc->getStartAngle(), 0, wy3d::PI * 2);
        double endAngle = startAngle + pSketchArc->getTotalAngle();
        return new Geom_TrimmedCurve(circle, startAngle, endAngle);
    }
    else if (const wy3d::SketchEllipse* pSketchEllipse = wy3d::SketchEllipse::cast(pSketchCurve))
    {
        if (pSketchEllipse->getMajorRadius() < 1e-7 || pSketchEllipse->getMinorRadius() < 1e-7)
        {
            assert(false);
            return nullptr;
        }

        // 获取椭圆的参数
        wy::Vector3 centerPnt = sketchPlane.value(pSketchEllipse->getCenter());
        wy::Vector3 normal = sketchPlane.getNormal();
        wy::Vector2 majorAxis = pSketchEllipse->getMajorAxis();
        wy::Vector3 xDir = sketchPlane.value(majorAxis) - sketchPlane.value(wy::Vector2::kZero);
        xDir.normalize();
        if (normal.length() < 0.5 || xDir.length() < 0.5)
        {
            assert(false);
            return nullptr;
        }
        gp_Ax2 ax2(toOccPnt(centerPnt), toOccDir(normal), toOccDir(xDir));
        return new Geom_Ellipse(ax2, pSketchEllipse->getMajorRadius(), pSketchEllipse->getMinorRadius());
    }
    else if (const wy3d::SketchEllipseArc* pSketchEllipseArc = wy3d::SketchEllipseArc::cast(pSketchCurve))
    {
        if (pSketchEllipseArc->getMajorRadius() < 1e-7 || pSketchEllipseArc->getMinorRadius() < 1e-7)
        {
            assert(false);
            return nullptr;
        }
        if (pSketchEllipseArc->getTotalAngle() < 1e-7)
        {
            assert(false);
            return nullptr;
        }

        // 获取椭圆的参数
        wy::Vector3 centerPnt = sketchPlane.value(pSketchEllipseArc->getCenter());
        wy::Vector3 normal = sketchPlane.getNormal();
        wy::Vector2 majorAxis = pSketchEllipseArc->getMajorAxis();
        wy::Vector3 xDir = sketchPlane.value(majorAxis) - sketchPlane.value(wy::Vector2::kZero);
        xDir.normalize();
        if (normal.length() < 0.5 || xDir.length() < 0.5)
        {
            assert(false);
            return nullptr;
        }
        gp_Ax2 ax2(toOccPnt(centerPnt), toOccDir(normal), toOccDir(xDir));
        Handle(Geom_Ellipse) ellipse = new Geom_Ellipse(ax2, pSketchEllipseArc->getMajorRadius(), pSketchEllipseArc->getMinorRadius());
        double startAngle = wy3d::normalizeRadian(pSketchEllipseArc->getStartAngle());
        double endAngle = startAngle + pSketchEllipseArc->getTotalAngle();
        // added by wangyao 2025.03.03 {
        // Geom_TrimmedCurve对应椭圆弧的UV指的是参数角度
        startAngle = wy3d::ellipsePolarAngleToParametricAngle(startAngle, pSketchEllipseArc->getMajorRadius(), pSketchEllipseArc->getMinorRadius());
        endAngle = wy3d::ellipsePolarAngleToParametricAngle(endAngle, pSketchEllipseArc->getMajorRadius(), pSketchEllipseArc->getMinorRadius());
        // }
        return new Geom_TrimmedCurve(ellipse, startAngle, endAngle);
    }
    else if (const wy3d::SketchSpline* pSketchSpline = wy3d::SketchSpline::cast(pSketchCurve))
    {
        Handle(Geom2d_BSplineCurve) pBSpline2d = pSketchSpline->getOccSpline();
        if (!pBSpline2d)
        {
            assert(false);
            return nullptr;
        }

        // 二维B样条的基本数据
        const Standard_Integer numPoles = pBSpline2d->NbPoles();
        const Standard_Integer numKnots = pBSpline2d->NbKnots();
        const Standard_Integer degree = pBSpline2d->Degree();
        const Standard_Boolean isPeriodic = pBSpline2d->IsPeriodic();
        const Standard_Boolean isRational = pBSpline2d->IsRational();

        // 控制点数组
        TColgp_Array1OfPnt poles3d(1, numPoles);
        for (Standard_Integer i = 1; i <= numPoles; ++i)
        {
            const gp_Pnt2d& pole2d = pBSpline2d->Pole(i);
            wy::Vector3 pnt = sketchPlane.value(pole2d.X(), pole2d.Y());
            poles3d.SetValue(i, gp_Pnt(pnt.x(), pnt.y(), pnt.z()));
        }

        // 节点向量和重数
        TColStd_Array1OfReal knots(1, numKnots);
        TColStd_Array1OfInteger multiplicities(1, numKnots);
        for (Standard_Integer i = 1; i <= numKnots; ++i)
        {
            knots.SetValue(i, pBSpline2d->Knot(i));
            multiplicities.SetValue(i, pBSpline2d->Multiplicity(i));
        }

        // 处理权重(若为有理曲线)
        TColStd_Array1OfReal weights;
        if (isRational)
        {
            weights.Resize(1, numPoles, false);
            for (Standard_Integer i = 1; i <= numPoles; ++i)
            {
                weights.SetValue(i, pBSpline2d->Weight(i));
            }
        }

        // 三维B样条
        if (isRational)
        {
            return new Geom_BSplineCurve(poles3d, weights, knots, multiplicities, degree, isPeriodic);
        }
        else
        {
            return new Geom_BSplineCurve(poles3d, knots, multiplicities, degree, isPeriodic);
        }
    }
    else
    {
        assert(false);
        return nullptr;
    }

    return nullptr;
}

std::pair<ErrorCode, TopoDS_Face> TopoUtil::makeFace(
    const wy3d::Sketch* pSketch,
    const SketchProfile::FaceSPtr& pSketchFace,
    std::vector<EdgeNamingInfo>& edgeNameInfos)
{
    assert(pSketch);
    assert(pSketchFace);
    edgeNameInfos.clear();

    // 草图拓扑生成器
    SketchTopoBuilder sketchTopoBuilder(pSketch, true); // true --- record topo history

    // 草图工作平面
    const wy3d::SketchPlane& sketchPlane = pSketch->getPlane();
    wy::Vector3 sketchNormal = sketchPlane.getNormal();
    assert(sketchNormal.length() > 0.5);
    wy::Vector3 sketchOrigin = sketchPlane.getOrigin();
    gp_Pln sketchPln(
        gp_Pnt(sketchOrigin.x(), sketchOrigin.y(), sketchOrigin.z()),
        gp_Dir(sketchNormal.x(), sketchNormal.y(), sketchNormal.z()));

    // 创建外Wire
    const std::vector<SketchProfile::LoopSPtr>& sketchLoops = pSketchFace->loops;
    if (sketchLoops.empty())
    {
        return std::pair<ErrorCode, TopoDS_Face>(ErrorCode::PROFILE_InvalidProfile, TopoDS_Face());
    }
    const SketchProfile::LoopSPtr& pOuterSketchLoop = sketchLoops.front();
    if (!pOuterSketchLoop || pOuterSketchLoop->curves.empty())
    {
        return std::pair<ErrorCode, TopoDS_Face>(ErrorCode::PROFILE_InvalidProfile, TopoDS_Face());
    }
    BRepBuilderAPI_MakeWire makeOuterWire;
    for (const BiCurve& curve : pOuterSketchLoop->curves)
    {
        const SketchCurve* pCurve = curve.curve;
        assert(pCurve);
        TopoDS_Edge edge = sketchTopoBuilder.makeEdge(pCurve); // 内部实现保证了SketchCurve和Geom_Curve一一对应
        if (edge.IsNull())
        {
            assert(false);
            continue; 
        }
        if (!curve.orient)
        {
            edge = TopoDS::Edge(edge.Reversed());
        }
        makeOuterWire.Add(edge);
    }
    if (!makeOuterWire.IsDone())
    {
        assert(false);
        return std::pair<ErrorCode, TopoDS_Face>(ErrorCode::TOPOSHAPE_GenerateShapeError, TopoDS_Face());
    }
    TopoDS_Wire outerWire = makeOuterWire.Wire();
    if (outerWire.IsNull())
    {
        assert(false);
        return std::pair<ErrorCode, TopoDS_Face>(ErrorCode::TOPOSHAPE_NullShapeError, TopoDS_Face());
    }
    if (pOuterSketchLoop->isClockWise) // 顺时针
    {
        outerWire = TopoDS::Wire(outerWire.Reversed());
    }

    // 创建Face
    BRepBuilderAPI_MakeFace makeFace(sketchPln, outerWire);

    // 创建内Wire
    size_t i = 0;
    for (const SketchProfile::LoopSPtr& pLoop : sketchLoops)
    {
        assert(pLoop);
        ++i;
        if (1 == i) continue; // 外轮廓
        const SketchProfile::Loop& loop = *pLoop;
        if (loop.curves.empty())
        {
            assert(false);
            continue;
        }
        BRepBuilderAPI_MakeWire makeWire;
        for (const BiCurve& curve : loop.curves)
        {
            const SketchCurve* pSketchCurve = curve.curve;
            assert(pSketchCurve);
            TopoDS_Edge edge = sketchTopoBuilder.makeEdge(pSketchCurve);
            if (edge.IsNull())
            {
                assert(false);
                continue;
            }
            if (!curve.orient)
            {
                edge = TopoDS::Edge(edge.Reversed());
            }
            makeWire.Add(edge);
        }
        if (!makeWire.IsDone())
        {
            assert(false);
            return std::pair<ErrorCode, TopoDS_Face>(ErrorCode::TOPOSHAPE_GenerateShapeError, TopoDS_Face());
        }
        TopoDS_Wire wire = makeWire.Wire();
        if (!loop.isClockWise) // 逆时针
        {
            wire = TopoDS::Wire(wire.Reversed());
        }
        makeFace.Add(wire);
    }

    makeFace.Build();

    // 面
    if (!makeFace.IsDone())
    {
        assert(false);
        return std::pair<ErrorCode, TopoDS_Face>(ErrorCode::TOPOSHAPE_GenerateShapeError, TopoDS_Face());
    }
    TopoDS_Face face = makeFace.Face();
    if (face.IsNull())
    {
        assert(false);
        return std::pair<ErrorCode, TopoDS_Face>(ErrorCode::TOPOSHAPE_NullShapeError, TopoDS_Face());
    }

    // 遍历面的Wire
    TopTools_IndexedMapOfShape idxMapOfWire;
    TopExp::MapShapes(face, TopAbs_WIRE, idxMapOfWire);
    edgeNameInfos.reserve(50);
    const std::map<Handle(Geom_Curve), unsigned int>& curve2Id = sketchTopoBuilder.getCurve2IdMap();
    for (int k = 1; k <= idxMapOfWire.Extent(); ++k)
    {
        // Wire的起始边序号
        size_t startIndex = edgeNameInfos.size();

        // Wire
        TopoDS_Wire wire = TopoDS::Wire(idxMapOfWire(k));
        assert(!wire.IsNull());

        // 记录边的命名
        recordEdgeNamesOfWire_AppendedMode(wire, curve2Id, edgeNameInfos);
    }

    return std::pair<ErrorCode, const TopoDS_Face&>(ErrorCode::NoError, face);
}

void TopoUtil::recordEdgeNamesOfWire_AppendedMode(
    const TopoDS_Wire& wire,
    const std::map<Handle(Geom_Curve), unsigned int>& curve2Id,
    std::vector<EdgeNamingInfo>& edgeNameInfos)
{
    bool isClosed = wire.Closed();
    TopTools_IndexedMapOfShape idxMapOfEdge;
    TopExp::MapShapes(wire, TopAbs_EDGE, idxMapOfEdge);

    size_t startIndex = edgeNameInfos.size();
    double first(0.0), last(0.0);
    int numEdges = idxMapOfEdge.Extent();
    for (int i = 1; i <= numEdges; ++i)
    {
        TopoDS_Edge edge = TopoDS::Edge(idxMapOfEdge(i));
        assert(!edge.IsNull());
        Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, first, last);
        assert(!curve.IsNull());

        // 查找匹配的草图元素ID
        auto iter = curve2Id.find(curve);
        if (iter != curve2Id.cend())
        {
            assert(iter->second != 0);
            EdgeNamingInfo edgeNameInfo;
            edgeNameInfo.edge = edge;
            edgeNameInfo.id = iter->second;
            if (i == numEdges)
            {
                if (isClosed)
                {
                    edgeNameInfo.sibling = startIndex;
                }
                else
                {
                    edgeNameInfo.sibling = size_t(-1);
                }
            }
            else
            {
                edgeNameInfo.sibling = size_t(-2);
            }
            edgeNameInfos.emplace_back(edgeNameInfo);
        }
        else
        {
            assert(false);
        }
    }

    size_t num = edgeNameInfos.size();
    for (size_t i = startIndex; i < num; ++i)
    {
        if (edgeNameInfos[i].sibling == size_t(-2))
        {
            if (i + 1 < num)
            {
                edgeNameInfos[i].sibling = i + 1;
            }
            else
            {
                edgeNameInfos[i].sibling = size_t(-1);
                assert(false);
            }
        }
    }
}

void TopoUtil::updateEdgeNames(BRepBuilderAPI_Transform& transform, std::vector<EdgeNamingInfo>& edgeNameInfos)
{
    for (TopoUtil::EdgeNamingInfo& edgeNameInfo : edgeNameInfos)
    {
        TopoDS_Shape modifiedShape = transform.ModifiedShape(edgeNameInfo.edge);
        assert(!modifiedShape.IsNull());
        edgeNameInfo.edge = TopoDS::Edge(modifiedShape);
    }
}

ErrorCode TopoUtil::makeWires(
    const wy3d::Sketch* pSketch,
    const std::vector<SketchProfile::LoopSPtr>& sketchLoops,
    const gp_Trsf& trsf,
    std::vector<WireInfo>& wireInfos)
{
    assert(pSketch);
    wireInfos.clear();

    if (sketchLoops.empty())
    {
        return ErrorCode::PROFILE_InvalidProfile;
    }

    // 草图拓扑生成器
    SketchTopoBuilder sketchTopoBuilder(pSketch, true); // true --- record topo history
    if (sketchLoops.empty())
    {
        return ErrorCode::PROFILE_InvalidProfile;
    }
    std::vector<WireInfo> retWireInfos;
    retWireInfos.reserve(sketchLoops.size());

    // 创建外Wire
    const SketchProfile::LoopSPtr& pOuterSketchLoop = sketchLoops.front();
    if (!pOuterSketchLoop || pOuterSketchLoop->curves.empty())
    {
        return ErrorCode::PROFILE_InvalidProfile;
    }
    BRepBuilderAPI_MakeWire makeOuterWire;
    for (const BiCurve& curve : pOuterSketchLoop->curves)
    {
        const SketchCurve* pCurve = curve.curve;
        assert(pCurve);
        TopoDS_Edge edge = sketchTopoBuilder.makeEdge(pCurve);
        if (edge.IsNull())
        {
            assert(false);
            continue;
        }
        if (!curve.orient)
        {
            edge = TopoDS::Edge(edge.Reversed());
        }
        makeOuterWire.Add(edge);
    }
    if (!makeOuterWire.IsDone())
    {
        assert(false);
        return ErrorCode::TOPOSHAPE_GenerateShapeError;
    }
    TopoDS_Wire outerWire = makeOuterWire.Wire();
    if (outerWire.IsNull())
    {
        assert(false);
        return ErrorCode::TOPOSHAPE_NullShapeError;
    }
    if (pOuterSketchLoop->isClockWise) // 顺时针
    {
        outerWire = TopoDS::Wire(outerWire.Reversed());
    }

    // 记录边的拓扑名称
    std::vector<EdgeNamingInfo> outerWireEdgeNameInfos;
    outerWireEdgeNameInfos.reserve(pOuterSketchLoop->curves.size());
    recordEdgeNamesOfWire_AppendedMode(outerWire, sketchTopoBuilder.getCurve2IdMap(), outerWireEdgeNameInfos);

    if (trsf.Form() != gp_TrsfForm::gp_Identity)
    {
        // 变换
        BRepBuilderAPI_Transform transformer(outerWire, trsf);
        outerWire = TopoDS::Wire(transformer.Shape());

        // 更新边的拓扑名称
        updateEdgeNames(transformer, outerWireEdgeNameInfos);
    }

    // 存储
    WireInfo outerWireInfo;
    outerWireInfo.wire = outerWire;
    outerWireInfo.edgeNameInfos = std::move(outerWireEdgeNameInfos);
    retWireInfos.emplace_back(std::move(outerWireInfo));

    // 创建内Wire
    size_t i = 0;
    for (const SketchProfile::LoopSPtr& pLoop : sketchLoops)
    {
        assert(pLoop);
        ++i;
        if (1 == i) continue; // 外轮廓
        const SketchProfile::Loop& loop = *pLoop;
        if (loop.curves.empty())
        {
            assert(false);
            continue;
        }
        BRepBuilderAPI_MakeWire makeWire;
        for (const BiCurve& curve : loop.curves)
        {
            const SketchCurve* pSketchCurve = curve.curve;
            assert(pSketchCurve);
            TopoDS_Edge edge = sketchTopoBuilder.makeEdge(pSketchCurve);
            if (edge.IsNull())
            {
                assert(false);
                continue;
            }
            if (!curve.orient)
            {
                edge = TopoDS::Edge(edge.Reversed());
            }
            makeWire.Add(edge);
        }
        if (!makeWire.IsDone())
        {
            assert(false);
            return ErrorCode::TOPOSHAPE_GenerateShapeError;
        }
        TopoDS_Wire wire = makeWire.Wire();
        if (!loop.isClockWise) // 逆时针
        {
            wire = TopoDS::Wire(wire.Reversed());
        }

        // 记录边的拓扑名称
        std::vector<EdgeNamingInfo> innerWireEdgeNameInfos;
        innerWireEdgeNameInfos.reserve(loop.curves.size());
        recordEdgeNamesOfWire_AppendedMode(wire, sketchTopoBuilder.getCurve2IdMap(), innerWireEdgeNameInfos);

        if (trsf.Form() != gp_TrsfForm::gp_Identity)
        {
            // 变换
            BRepBuilderAPI_Transform transformer(wire, trsf);
            wire = TopoDS::Wire(transformer.Shape());

            // 更新边的拓扑名称
            updateEdgeNames(transformer, innerWireEdgeNameInfos);
        }

        // 存储
        WireInfo wireInfo;
        wireInfo.wire = wire;
        wireInfo.edgeNameInfos = std::move(innerWireEdgeNameInfos);
        retWireInfos.emplace_back(std::move(wireInfo));
    }

    wireInfos.swap(retWireInfos);
    return ErrorCode::NoError;
}

NS_WY3D_END