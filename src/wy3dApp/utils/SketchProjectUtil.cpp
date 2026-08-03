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

#include "SketchProjectUtil.h"

#include <BRep_Tool.hxx>
#include <Geom_Line.hxx>
#include <Geom_Circle.hxx>
#include <Geom_Ellipse.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <Geom_Plane.hxx>
#include <GeomProjLib.hxx>
#include <GeomAdaptor_Curve.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <gp_Pln.hxx>
#include <Standard_Failure.hxx>

#include <wy3dSketchLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>
#include <wy3dSketchEllipse.h>
#include <wy3dSketchEllipseArc.h>
#include <wy3dSketchSpline.h>
#include <wy3dImpl.h>
#include "MathUtils.h"

namespace
{

// 获取投影后曲线的长度（用于退化检测）
double computeLength(const Handle(Geom_Curve)& curve, double first, double last)
{
    if (curve.IsNull()) return 0.0;
    try
    {
        GeomAdaptor_Curve adaptor(curve, first, last);
        return GCPnts_AbscissaPoint::Length(adaptor, first, last);
    }
    catch (...)
    {
        return 0.0;
    }
}

// 将gp_Pnt转换为wy::Vector3，再投影到UV
wy::Vector2 toUV(const gp_Pnt& pnt, const wy3d::SketchPlane& plane)
{
    wy::Vector3 p3d(pnt.X(), pnt.Y(), pnt.Z());
    return plane.uv(p3d);
}

// 处理完整圆
SketchProjectUtil::ProjectResult createFromCircle(
    wydb::Transaction* pTrans,
    const Handle(Geom_Circle)& circle,
    const wy3d::SketchPlane& plane,
    wy3d::SketchEntity*& pOutEntity)
{
    const gp_Pnt& center = circle->Circ().Location();
    double radius = circle->Circ().Radius();

    wy::Vector2 uvCenter = toUV(center, plane);

    // 计算投影后的半径（取圆上一点投影后到中心的距离）
    gp_Pnt pntOnCircle = circle->Value(0.0);
    wy::Vector2 uvOnCircle = toUV(pntOnCircle, plane);
    double projectedRadius = (uvOnCircle - uvCenter).length();

    if (projectedRadius < wy3d::TOL)
        return SketchProjectUtil::ProjectResult::Degenerate;

    wy3d::SketchCircle* pCircle = nullptr;
    wy::ErrorStatus err = wy3d::SketchCircle::create(pTrans, uvCenter, projectedRadius, pCircle);
    if (err != wy::ErrorStatus::Ok || !pCircle)
        return SketchProjectUtil::ProjectResult::ProjectFailed;

    pOutEntity = pCircle;
    return SketchProjectUtil::ProjectResult::Ok;
}

// 处理裁剪圆（圆弧）
SketchProjectUtil::ProjectResult createFromTrimmedCircle(
    wydb::Transaction* pTrans,
    const Handle(Geom_Circle)& circle,
    double first, double last,
    const wy3d::SketchPlane& plane,
    wy3d::SketchEntity*& pOutEntity)
{
    const gp_Pnt& center = circle->Circ().Location();
    double radius = circle->Circ().Radius();

    wy::Vector2 uvCenter = toUV(center, plane);

    gp_Pnt startPnt = circle->Value(first);
    gp_Pnt endPnt = circle->Value(last);
    wy::Vector2 uvStart = toUV(startPnt, plane);
    wy::Vector2 uvEnd = toUV(endPnt, plane);

    double projectedRadius = (uvStart - uvCenter).length();
    if (projectedRadius < wy3d::TOL)
        return SketchProjectUtil::ProjectResult::Degenerate;

    // 计算起始角和终止角
    double startAngle = std::atan2(uvStart.y() - uvCenter.y(), uvStart.x() - uvCenter.x());
    double endAngle = std::atan2(uvEnd.y() - uvCenter.y(), uvEnd.x() - uvCenter.x());

    // 判断圆弧方向：取中点检查
    double midParam = (first + last) * 0.5;
    gp_Pnt midPnt = circle->Value(midParam);
    wy::Vector2 uvMid = toUV(midPnt, plane);
    double midAngle = std::atan2(uvMid.y() - uvCenter.y(), uvMid.x() - uvCenter.x());

    // 确保角度范围包含中点
    double diffCCW = std::fmod(endAngle - startAngle + wy3d::TWO_PI, wy3d::TWO_PI);
    double midCCW = std::fmod(midAngle - startAngle + wy3d::TWO_PI, wy3d::TWO_PI);
    if (std::abs(midCCW - diffCCW) > 0.01 && midCCW < diffCCW)
    {
        // CCW方向正确
    }
    else
    {
        // 需要调整
        double diffCW = std::fmod(startAngle - endAngle + wy3d::TWO_PI, wy3d::TWO_PI);
        double midCW = std::fmod(startAngle - midAngle + wy3d::TWO_PI, wy3d::TWO_PI);
        if (midCW < diffCW)
        {
            std::swap(startAngle, endAngle);
        }
    }

    wy3d::SketchArc* pArc = nullptr;
    wy::ErrorStatus err = wy3d::SketchArc::create(pTrans, uvCenter, projectedRadius, startAngle, endAngle, pArc);
    if (err != wy::ErrorStatus::Ok || !pArc)
        return SketchProjectUtil::ProjectResult::ProjectFailed;

    pOutEntity = pArc;
    return SketchProjectUtil::ProjectResult::Ok;
}

// 处理直线
SketchProjectUtil::ProjectResult createFromLine(
    wydb::Transaction* pTrans,
    const Handle(Geom_Line)& line,
    double first, double last,
    const wy3d::SketchPlane& plane,
    wy3d::SketchEntity*& pOutEntity)
{
    gp_Pnt startPnt = line->Value(first);
    gp_Pnt endPnt = line->Value(last);

    wy::Vector2 uvStart = toUV(startPnt, plane);
    wy::Vector2 uvEnd = toUV(endPnt, plane);

    if ((uvEnd - uvStart).length() < wy3d::TOL)
        return SketchProjectUtil::ProjectResult::Degenerate;

    wy3d::SketchLine* pLine = nullptr;
    wy::ErrorStatus err = wy3d::SketchLine::create(pTrans, uvStart, uvEnd, pLine);
    if (err != wy::ErrorStatus::Ok || !pLine)
        return SketchProjectUtil::ProjectResult::ProjectFailed;

    pOutEntity = pLine;
    return SketchProjectUtil::ProjectResult::Ok;
}

// 处理完整椭圆
SketchProjectUtil::ProjectResult createFromEllipse(
    wydb::Transaction* pTrans,
    const Handle(Geom_Ellipse)& ellipse,
    const wy3d::SketchPlane& plane,
    wy3d::SketchEntity*& pOutEntity)
{
    const gp_Pnt& center = ellipse->Location();
    double majorRadius = ellipse->MajorRadius();
    double minorRadius = ellipse->MinorRadius();

    wy::Vector2 uvCenter = toUV(center, plane);

    // 主轴方向
    gp_Dir xDir = ellipse->XAxis().Direction();
    gp_Pnt majorPnt = center.Translated(xDir.XYZ() * majorRadius);
    wy::Vector2 uvMajor = toUV(majorPnt, plane);
    wy::Vector2 majorAxis = uvMajor - uvCenter;

    double axisLen = majorAxis.length();
    if (axisLen < wy3d::TOL)
        return SketchProjectUtil::ProjectResult::Degenerate;

    double radiusRatio = (majorRadius > wy3d::TOL) ? (minorRadius / majorRadius) : 0.0;

    wy3d::SketchEllipse* pEllipse = nullptr;
    wy::ErrorStatus err = wy3d::SketchEllipse::create(pTrans, uvCenter, majorAxis, radiusRatio, pEllipse);
    if (err != wy::ErrorStatus::Ok || !pEllipse)
        return SketchProjectUtil::ProjectResult::ProjectFailed;

    pOutEntity = pEllipse;
    return SketchProjectUtil::ProjectResult::Ok;
}

// 处理裁剪椭圆（椭圆弧）
SketchProjectUtil::ProjectResult createFromTrimmedEllipse(
    wydb::Transaction* pTrans,
    const Handle(Geom_Ellipse)& ellipse,
    double first, double last,
    const wy3d::SketchPlane& plane,
    wy3d::SketchEntity*& pOutEntity)
{
    const gp_Pnt& center = ellipse->Location();
    double majorRadius = ellipse->MajorRadius();
    double minorRadius = ellipse->MinorRadius();

    wy::Vector2 uvCenter = toUV(center, plane);

    gp_Dir xDir = ellipse->XAxis().Direction();
    gp_Pnt majorPnt = center.Translated(xDir.XYZ() * majorRadius);
    wy::Vector2 uvMajor = toUV(majorPnt, plane);
    wy::Vector2 majorAxis = uvMajor - uvCenter;

    double axisLen = majorAxis.length();
    if (axisLen < wy3d::TOL)
        return SketchProjectUtil::ProjectResult::Degenerate;

    double radiusRatio = (majorRadius > wy3d::TOL) ? (minorRadius / majorRadius) : 0.0;

    // 计算起始角和终止角
    gp_Pnt startPnt = ellipse->Value(first);
    gp_Pnt endPnt = ellipse->Value(last);
    wy::Vector2 uvStart = toUV(startPnt, plane);
    wy::Vector2 uvEnd = toUV(endPnt, plane);

    double startAngle = std::atan2(uvStart.y() - uvCenter.y(), uvStart.x() - uvCenter.x());
    double endAngle = std::atan2(uvEnd.y() - uvCenter.y(), uvEnd.x() - uvCenter.x());

    wy3d::SketchEllipseArc* pEllipseArc = nullptr;
    wy::ErrorStatus err = wy3d::SketchEllipseArc::create(pTrans, uvCenter, majorAxis, radiusRatio, startAngle, endAngle, pEllipseArc);
    if (err != wy::ErrorStatus::Ok || !pEllipseArc)
        return SketchProjectUtil::ProjectResult::ProjectFailed;

    pOutEntity = pEllipseArc;
    return SketchProjectUtil::ProjectResult::Ok;
}

// 处理B样条
SketchProjectUtil::ProjectResult createFromBSpline(
    wydb::Transaction* pTrans,
    const Handle(Geom_BSplineCurve)& bspline,
    double first, double last,
    const wy3d::SketchPlane& plane,
    wy3d::SketchEntity*& pOutEntity)
{
    // 对B样条采样拟合点
    const int kNumSamples = 64;
    std::vector<wy::Vector2> fitPoints;
    fitPoints.reserve(kNumSamples);

    double range = last - first;
    for (int i = 0; i < kNumSamples; ++i)
    {
        double t = first + range * static_cast<double>(i) / static_cast<double>(kNumSamples - 1);
        gp_Pnt pnt = bspline->Value(t);
        fitPoints.push_back(toUV(pnt, plane));
    }

    // 去重（去除过近的点）
    std::vector<wy::Vector2> filteredPts;
    filteredPts.reserve(fitPoints.size());
    filteredPts.push_back(fitPoints.front());
    for (size_t i = 1; i < fitPoints.size(); ++i)
    {
        if ((fitPoints[i] - filteredPts.back()).length() > wy3d::TOL)
        {
            filteredPts.push_back(fitPoints[i]);
        }
    }

    if (filteredPts.size() < 2)
        return SketchProjectUtil::ProjectResult::Degenerate;

    wy3d::SketchSpline* pSpline = nullptr;
    wy::ErrorStatus err = wy3d::SketchSpline::create(pTrans, filteredPts, pSpline);
    if (err != wy::ErrorStatus::Ok || !pSpline)
        return SketchProjectUtil::ProjectResult::ProjectFailed;

    pOutEntity = pSpline;
    return SketchProjectUtil::ProjectResult::Ok;
}

} // namespace

SketchProjectUtil::ProjectResult SketchProjectUtil::projectEdgeImpl(
    wydb::Transaction* pTrans,
    const TopoDS_Edge& edge,
    const wy3d::SketchPlane& plane,
    wy3d::SketchEntity*& pOutEntity)
{
    // 1. 获取曲线 + trim范围
    Standard_Real first, last;
    Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, first, last);
    if (curve.IsNull())
        return ProjectResult::NullCurve;

    // 2. 构建草图平面
    wy::Vector3 origin = plane.getOrigin();
    wy::Vector3 normal = plane.getNormal();
    Handle(Geom_Plane) geomPlane = new Geom_Plane(
        gp_Pnt(origin.x(), origin.y(), origin.z()),
        gp_Dir(normal.x(), normal.y(), normal.z()));

    // 3. 投影
    Handle(Geom_Curve) projected = GeomProjLib::ProjectOnPlane(
        curve, geomPlane, gp_Dir(normal.x(), normal.y(), normal.z()), Standard_True);
    if (projected.IsNull())
        return ProjectResult::ProjectFailed;

    // 4. 裁剪投影曲线（用投影后曲线自己的参数范围，clamp源的first/last防止越界crash）
    Handle(Geom_Curve) trimmed = projected;
    if (!Precision::IsInfinite(first) && !Precision::IsInfinite(last))
    {
        Standard_Real projFirst = projected->FirstParameter();
        Standard_Real projLast  = projected->LastParameter();
        Standard_Real clampedFirst = std::max(first, projFirst);
        Standard_Real clampedLast  = std::min(last, projLast);
        if (clampedFirst < clampedLast - Precision::PConfusion()
            && (clampedFirst > projFirst || clampedLast < projLast))
        {
            trimmed = new Geom_TrimmedCurve(projected, clampedFirst, clampedLast);
        }
        first = clampedFirst;
        last  = clampedLast;
    }

    // 5. 退化检测
    double length = computeLength(trimmed, trimmed->FirstParameter(), trimmed->LastParameter());
    if (length < wy3d::TOL)
        return ProjectResult::Degenerate;

    // 6. 解包 TrimmedCurve
    Handle(Geom_Curve) basisCurve = trimmed;
    bool isTrimmed = false;
    while (basisCurve->IsKind(STANDARD_TYPE(Geom_TrimmedCurve)))
    {
        isTrimmed = true;
        Handle(Geom_TrimmedCurve) tc = Handle(Geom_TrimmedCurve)::DownCast(basisCurve);
        first = tc->FirstParameter();
        last = tc->LastParameter();
        basisCurve = tc->BasisCurve();
    }

    // 7. 根据基础曲线类型创建草图实体
    if (basisCurve->IsKind(STANDARD_TYPE(Geom_Line)))
    {
        Handle(Geom_Line) line = Handle(Geom_Line)::DownCast(basisCurve);
        return createFromLine(pTrans, line, first, last, plane, pOutEntity);
    }
    else if (basisCurve->IsKind(STANDARD_TYPE(Geom_Circle)))
    {
        Handle(Geom_Circle) circle = Handle(Geom_Circle)::DownCast(basisCurve);
        // 检查是否完整圆
        double period = wy3d::TWO_PI;
        if (std::abs(last - first - period) < wy3d::TOL || !isTrimmed)
        {
            return createFromCircle(pTrans, circle, plane, pOutEntity);
        }
        else
        {
            return createFromTrimmedCircle(pTrans, circle, first, last, plane, pOutEntity);
        }
    }
    else if (basisCurve->IsKind(STANDARD_TYPE(Geom_Ellipse)))
    {
        Handle(Geom_Ellipse) ellipse = Handle(Geom_Ellipse)::DownCast(basisCurve);
        double period = wy3d::TWO_PI;
        if (std::abs(last - first - period) < wy3d::TOL || !isTrimmed)
        {
            return createFromEllipse(pTrans, ellipse, plane, pOutEntity);
        }
        else
        {
            return createFromTrimmedEllipse(pTrans, ellipse, first, last, plane, pOutEntity);
        }
    }
    else if (basisCurve->IsKind(STANDARD_TYPE(Geom_BSplineCurve)))
    {
        Handle(Geom_BSplineCurve) bspline = Handle(Geom_BSplineCurve)::DownCast(basisCurve);
        return createFromBSpline(pTrans, bspline, first, last, plane, pOutEntity);
    }

    return ProjectResult::UnsupportedType;
}

SketchProjectUtil::ProjectResult SketchProjectUtil::projectEdge(
    wydb::Transaction* pTrans,
    const TopoDS_Edge& edge,
    const wy3d::SketchPlane& plane,
    wy3d::SketchEntity*& pOutEntity)
{
    pOutEntity = nullptr;

    if (edge.IsNull())
        return ProjectResult::NullCurve;

    try
    {
        return projectEdgeImpl(pTrans, edge, plane, pOutEntity);
    }
    catch (const Standard_Failure&)
    {
        return ProjectResult::ProjectFailed;
    }
    catch (...)
    {
        return ProjectResult::ProjectFailed;
    }
}
