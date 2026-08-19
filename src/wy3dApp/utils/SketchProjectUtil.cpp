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

#include <algorithm>

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
#include <wy3dMath.h>
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

// 解包TrimmedCurve，取出基础曲线
// 参数范围按各层取交集：内层的范围可能比外层更宽，直接覆盖会丢掉外层的裁剪
Handle(Geom_Curve) unwrapTrimmed(
    const Handle(Geom_Curve)& curve,
    Standard_Real& first,
    Standard_Real& last,
    bool& isTrimmed)
{
    isTrimmed = false;
    Handle(Geom_Curve) basis = curve;
    while (basis->IsKind(STANDARD_TYPE(Geom_TrimmedCurve)))
    {
        isTrimmed = true;
        Handle(Geom_TrimmedCurve) tc = Handle(Geom_TrimmedCurve)::DownCast(basis);
        first = std::max(first, tc->FirstParameter());
        last  = std::min(last,  tc->LastParameter());
        basis = tc->BasisCurve();
    }
    return basis;
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
    // SketchEllipseArc 的起止角是相对主轴的极角，不是全局极角，必须减掉主轴自身的方向角
    double refAngle = std::atan2(majorAxis.y(), majorAxis.x());
    auto polarAngle = [&](double param)
    {
        wy::Vector2 uv = toUV(ellipse->Value(param), plane);
        return wy3d::normalizeRadian(
            std::atan2(uv.y() - uvCenter.y(), uv.x() - uvCenter.x()) - refAngle);
    };
    double startAngle = polarAngle(first);
    double endAngle = polarAngle(last);
    double midAngle = polarAngle((first + last) * 0.5);

    // 弧总是从 startAngle 逆时针扫到 endAngle，中点必须落在扫掠范围内，否则方向反了
    if (wy3d::normalizeRadian(midAngle - startAngle) > wy3d::normalizeRadian(endAngle - startAngle))
        std::swap(startAngle, endAngle);

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

    if (Precision::IsInfinite(first) || Precision::IsInfinite(last))
        return ProjectResult::ProjectFailed; // 无限长的边投不出有界的草图实体
    if (last - first < Precision::PConfusion())
        return ProjectResult::Degenerate;

    // 2. 构建草图平面
    wy::Vector3 origin = plane.getOrigin();
    wy::Vector3 normal = plane.getNormal();
    Handle(Geom_Plane) geomPlane = new Geom_Plane(
        gp_Pnt(origin.x(), origin.y(), origin.z()),
        gp_Dir(normal.x(), normal.y(), normal.z()));

    // 3. 先按边的参数范围裁剪源曲线，再投影
    //    BRep_Tool::Curve 返回的是基础曲线，解析曲线是无限长的（Geom_Line 参数范围 ±2e100）。
    //    把无限曲线直接交给 ProjectOnPlane，OCCT 会返回极点在 ±1.4e100 的 BSpline，
    //    之后在 [first, last] 上求值全部因浮点抵消塌缩到原点，从而被误判为退化。
    Handle(Geom_Curve) source = curve;
    if (first > curve->FirstParameter() || last < curve->LastParameter())
        source = new Geom_TrimmedCurve(curve, first, last);

    // 退化预检：整条曲线投影到UV后收缩成一点（例如直线垂直于草图平面）时，
    // ProjectOnPlane 会直接抛异常，只能报笼统的 ProjectFailed。
    // 这里先判掉，才能给出"垂直于草图平面，投影退化为一点"这个准确提示。
    {
        const int kNumProbes = 16;
        wy::Vector2 uvFirst = toUV(source->Value(first), plane);
        double maxDist = 0.0;
        for (int i = 1; i < kNumProbes; ++i)
        {
            double t = first + (last - first) * static_cast<double>(i) / static_cast<double>(kNumProbes - 1);
            maxDist = std::max(maxDist, (toUV(source->Value(t), plane) - uvFirst).length());
        }
        if (maxDist < wy3d::TOL)
            return ProjectResult::Degenerate;
    }

    // KeepParametrization 必须传 Standard_False：
    //   传 True 时 OCCT 为了保住源参数化，会把能解析表达的结果降级成 BSpline 逼近——
    //   直线变 BSpline（产出 SketchSpline 而不是 SketchLine），
    //   倾斜的圆变 BSpline（产出 SketchSpline 而不是 SketchEllipse，且带 ~4e-7 的逼近误差）。
    //   传 False 时直线仍是 Geom_Line、倾斜圆精确地变成 Geom_Ellipse。
    Handle(Geom_Curve) projected = GeomProjLib::ProjectOnPlane(
        source, geomPlane, gp_Dir(normal.x(), normal.y(), normal.z()), Standard_False);
    if (projected.IsNull())
        return ProjectResult::ProjectFailed;

    // 4. 采用投影曲线自己的参数域
    //    KeepParametrization=False 时投影曲线用的是自身的参数化，源曲线的 first/last
    //    在这里已经没有意义（例如源 [1.0, 5.8] 会变成 [5.33, 10.13]），
    //    绝不能再拿来 clamp，否则会把圆弧静默截短。
    //    源曲线已在第3步裁剪过，所以投影结果自带的参数域就是正确范围。
    first = projected->FirstParameter();
    last  = projected->LastParameter();
    if (last - first < Precision::PConfusion())
        return ProjectResult::Degenerate;

    // 5. 退化检测
    double length = computeLength(projected, first, last);
    if (length < wy3d::TOL)
        return ProjectResult::Degenerate;

    // 6. 解包 TrimmedCurve
    bool isTrimmed = false;
    Handle(Geom_Curve) basisCurve = unwrapTrimmed(projected, first, last, isTrimmed);
    if (last - first < Precision::PConfusion())
        return ProjectResult::Degenerate;

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
