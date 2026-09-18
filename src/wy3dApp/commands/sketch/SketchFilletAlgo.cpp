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

#include "SketchFilletAlgo.h"
#include <cassert>
#include <Geom2dAPI_ProjectPointOnCurve.hxx>
#include <Geom2d_Line.hxx>
#include <Geom2d_Circle.hxx>
#include <Geom2d_TrimmedCurve.hxx>
#include <Geom2d_OffsetCurve.hxx>
#include <Geom2dAPI_InterCurveCurve.hxx>
#include <wyVector2.h>
#include <wy3dMath.h>
#include <utils/wy3dCurveIntersectionUtil.h>

std::shared_ptr<SketchFilletData> SketchFilletAlgo::filletLineLine(double R, double tol,
    const wy::Vector2& startPnt1st, const wy::Vector2& endPnt1st, const wy::Vector2& pickPosOnLine1st,
    const wy::Vector2& startPnt2nd, const wy::Vector2& endPnt2nd, const wy::Vector2& pickPosOnLine2nd)
{
    assert(R > 0.0);
    assert(tol > 0.0);
    if (R <= tol) return nullptr; // 圆角过小

    // 直线段退化为点直接返回
    wy::Vector2 lineVec1st = endPnt1st - startPnt1st;
    wy::Vector2 lineVec2nd = endPnt2nd - startPnt2nd;
    double lineLen1st = lineVec1st.length();
    double lineLen2nd = lineVec2nd.length();
    if (lineLen1st <= tol || lineLen2nd <= tol)
    {
        return nullptr;
    }

    // 直线段1和2的方向向量
    wy::Vector2 lineDir1st = lineVec1st;
    lineDir1st.normalize();
    wy::Vector2 lineDir2nd = lineVec2nd;
    lineDir2nd.normalize();

    // 求两条直线的交点
    wy::Vector2 p0;
    if (!wy3d::intersectLineLine(
        startPnt1st, endPnt1st,
        startPnt2nd, endPnt2nd,
        p0))
    {
        return nullptr; // 直线平行
    }

    // 直线段1
    double t1(0.0); // 保留的端点(0.0 or 1.0)
    double param1st = (p0 - startPnt1st).dot(lineDir1st) / lineLen1st; // 交点在线段1上的参数位置
    if (std::fabs(param1st) <= tol) // 交点在起点上
    {
        t1 = 1.0;
    }
    else if (std::fabs(param1st - 1.0) <= tol) // 交点在终点上
    {
        t1 = 0.0;
    }
    else if (param1st > 0.0 && param1st < 1.0) // 交点在直线段上
    {
        if ((pickPosOnLine1st - startPnt1st).dot(lineDir1st) <= (p0 - startPnt1st).dot(lineDir1st))
            t1 = 0.0;
        else
            t1 = 1.0;
    }
    else // 交点在直线段外
    {
        t1 = param1st > 1.0 ? 0.0 : 1.0;
    }
    wy::Vector2 p1 = (t1 == 0.0) ? startPnt1st : endPnt1st;

    // 直线段2
    double t2(0.0); // 保留的端点(0.0 or 1.0)
    double param2nd = (p0 - startPnt2nd).dot(lineDir2nd) / lineLen2nd; // 交点在线段2上的参数位置
    if (std::fabs(param2nd) <= tol) // 交点在起点上
    {
        t2 = 1.0;
    }
    else if (std::fabs(param2nd - 1.0) <= tol) // 交点在终点上
    {
        t2 = 0.0;
    }
    else if (param2nd > 0.0 && param2nd < 1.0) // 直线段上
    {
        if ((pickPosOnLine2nd - startPnt2nd).dot(lineDir2nd) <= (p0 - startPnt2nd).dot(lineDir2nd))
            t2 = 0.0;
        else
            t2 = 1.0;
    }
    else // 直线段外
    {
        t2 = param2nd > 1.0 ? 0.0 : 1.0;
    }
    wy::Vector2 p2 = (t2 == 0.0) ? startPnt2nd : endPnt2nd;

    // 两条线段的夹角
    wy::Vector2 dir10 = p1 - p0;
    if (dir10.length() <= tol) return nullptr;
    dir10.normalize();
    wy::Vector2 dir20 = p2 - p0;
    if (dir20.length() <= tol) return nullptr;
    dir20.normalize();
    double angle = wy::Vector2::angle(dir10, dir20);
    if (std::fabs(angle) <= tol || std::fabs(angle - wy3d::PI) <= tol)
    {
        return nullptr;
    }

    // 求切点
    double distance = R / std::tan(angle / 2);
    wy::Vector2 tanPnt1 = p0 + dir10 * distance;
    if ((tanPnt1 - p0).length() >= (p1 - p0).length()) return nullptr;
    wy::Vector2 tanPnt2 = p0 + dir20 * distance;
    if ((tanPnt2 - p0).length() >= (p2 - p0).length()) return nullptr;

    // 计算圆心
    wy::Vector2 dirCenter = (dir10 + dir20) / 2; // 圆心在夹角平分线上
    dirCenter.normalize();
    wy::Vector2 center = p0 + dirCenter * (R / std::sin(angle / 2));
    double startAngle = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), tanPnt1 - center);
    double endAngle = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), tanPnt2 - center);
    double middleAngle = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), p0 - center);
    startAngle = wy3d::normalizeRadian(startAngle);
    endAngle = wy3d::normalizeRadian(endAngle);
    middleAngle = wy3d::normalizeRadian(middleAngle);
    if (endAngle < startAngle) endAngle += wy3d::TWO_PI;
    if (middleAngle < startAngle) middleAngle += wy3d::TWO_PI;
    if (middleAngle > endAngle) // 反转
    {
        std::swap(startAngle, endAngle);
        startAngle = wy3d::normalizeRadian(startAngle);
        endAngle = wy3d::normalizeRadian(endAngle);
        if (endAngle < startAngle) endAngle += wy3d::TWO_PI;
    }

    // 结果
    std::shared_ptr<SketchFilletData> pFilletData = std::make_shared<SketchFilletData>();
    // 直线段1
    pFilletData->startParam1st = t1;
    pFilletData->endParam1st = (tanPnt1 - startPnt1st).dot(lineDir1st) / lineLen1st;
    if (t1 == 1.0) std::swap(pFilletData->startParam1st, pFilletData->endParam1st);
    assert(pFilletData->endParam1st > pFilletData->startParam1st);
    // 直线段2
    pFilletData->startParam2nd = t2;
    pFilletData->endParam2nd = (tanPnt2 - startPnt2nd).dot(lineDir2nd) / lineLen2nd;
    if (t2 == 1.0) std::swap(pFilletData->startParam2nd, pFilletData->endParam2nd);
    assert(pFilletData->endParam2nd > pFilletData->startParam2nd);
    // 圆角
    pFilletData->filletCenter = center;
    pFilletData->filletRadius = R;
    pFilletData->filletStartAngle = startAngle;
    pFilletData->filletEndAngle = endAngle;

    return pFilletData;
}

std::shared_ptr<SketchFilletData> SketchFilletAlgo::filletLineArc(double R, double tol,
    const wy::Vector2& lineStartPnt, const wy::Vector2& lineEndPnt, const wy::Vector2& pickPosOnLine,
    const wy::Vector2& arcCenter, double arcRadius, double arcStartAngle, double arcEndAngle, const wy::Vector2& pickPosOnArc,
    bool isSecondPickPosMajor)
{
    assert(R > 0.0);
    assert(tol > 0.0);
    assert(arcRadius > 0.0);
    if (R <= tol) return nullptr; // 圆角过小

    // 直线段的数据
    wy::Vector2 lineMidPnt = (lineStartPnt + lineEndPnt) / 2;
    wy::Vector2 lineVec = lineEndPnt - lineStartPnt;
    double lineLen = lineVec.length();
    if (lineLen <= tol) // 直线段退化为点
    {
        return nullptr;
    }
    wy::Vector2 lineDir = lineVec;
    lineDir.normalize();

    // 圆弧数据
    arcStartAngle = wy3d::normalizeRadian(arcStartAngle);
    arcEndAngle = wy3d::normalizeRadian(arcEndAngle);
    if (arcEndAngle < arcStartAngle) arcEndAngle += wy3d::TWO_PI;
    double arcTotalAngle = arcEndAngle - arcStartAngle;
    assert(arcTotalAngle >= 0.0 && arcTotalAngle < wy3d::TWO_PI);
    if (arcRadius <= tol || arcTotalAngle <= tol) // 圆弧退化
    {
        return nullptr;
    }

    // 根据直线段上拾取的点在圆的内侧还是外侧从而确定偏移圆是外扩还是内缩
    double offsetRadius = arcRadius;
    if ((pickPosOnLine - arcCenter).length() > arcRadius)
    {
        offsetRadius += R;
    }
    else
    {
        offsetRadius -= R;
        if (offsetRadius <= tol) return nullptr;
    }

    // 根据圆弧上拾取的点在直线段的哪一侧从而确定直线段的偏移向量
    wy::Vector2 lineOffsetDir(-lineDir.y(), lineDir.x());
    if ((pickPosOnArc - lineStartPnt).dot(lineOffsetDir) < 0)
    {
        lineOffsetDir = -lineOffsetDir;
    }
    wy::Vector2 offsetLineStartPnt = lineStartPnt + lineOffsetDir * R;
    wy::Vector2 offsetLineEndPnt = lineEndPnt + lineOffsetDir * R;

    // 求偏移直线和偏移圆的交点从而确定圆角的圆心
    wy::Vector2 filletCenter;
    wy::Vector2 intPnt1, intPnt2;
    unsigned int filletIntPntIndex(0);
    unsigned int numIntPnts = wy3d::intersectLineCircle(offsetLineStartPnt, offsetLineEndPnt, arcCenter, offsetRadius, intPnt1, intPnt2);
    if (2 == numIntPnts)
    {
        // 根据交点离直线段上拾取点的距离来取舍
        wy::Vector2 pickPos = isSecondPickPosMajor ? pickPosOnArc : pickPosOnLine;
        if ((intPnt1 - pickPos).length() < (intPnt2 - pickPos).length())
        {
            filletCenter = intPnt1;
            filletIntPntIndex = 1;
        }
        else
        {
            filletCenter = intPnt2;
            filletIntPntIndex = 2;
        }
    }
    else if (1 == numIntPnts)
    {
        filletCenter = intPnt1;
        filletIntPntIndex = 1;
    }
    else
    {
        assert(0 == numIntPnts);
        return nullptr;
    }

    // 圆角在直线段上的点
    wy::Vector2 filletPntOnLine = filletCenter - R * lineOffsetDir;
    double angleOfPntOnLine = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), filletPntOnLine - filletCenter);
    // 圆角在圆弧上的点
    double angleOfPntOnArc(0.0);
    if (offsetRadius < arcRadius)
    {
        angleOfPntOnArc = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), filletCenter - arcCenter);
    }
    else
    {
        angleOfPntOnArc = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), arcCenter - filletCenter);
    }
    wy::Vector2 filletPntOnArc = filletCenter;
    {
        wy::Vector2 dir = filletCenter - arcCenter;
        dir.normalize();
        if (offsetRadius < arcRadius) filletPntOnArc += R * dir;
        else filletPntOnArc -= R * dir;
    }

    // 确定圆角的起始角度和终止角度
    bool isFilletArcCCW(true); // 从圆角在直线段上的点以小于180度逆时针旋转到圆角在圆弧上的点
    double filletStartAngle = wy3d::normalizeRadian(angleOfPntOnLine);
    double filletEndAngle = wy3d::normalizeRadian(angleOfPntOnArc);
    if (filletEndAngle < filletStartAngle) filletEndAngle += wy3d::TWO_PI;
    if ((filletEndAngle - filletStartAngle) > wy3d::PI) // 圆角的角度一定是小于180度,此时反转,表明是顺时针
    {
        std::swap(filletStartAngle, filletEndAngle);
        filletStartAngle = wy3d::normalizeRadian(filletStartAngle);
        filletEndAngle = wy3d::normalizeRadian(filletEndAngle);
        if (filletEndAngle < filletStartAngle) filletEndAngle += wy3d::TWO_PI;
        isFilletArcCCW = false;
    }

    // 结果:直线段
    std::shared_ptr<SketchFilletData> pFilletData = std::make_shared<SketchFilletData>();
    {
        double filletPntParamOnLine = (filletPntOnLine - lineStartPnt).dot(lineDir) / lineLen;
        if (filletPntParamOnLine < 0.0) // 切点在直线段外
        {
            pFilletData->startParam1st = filletPntParamOnLine;
            pFilletData->endParam1st = 1.0;
        }
        else if (filletPntParamOnLine > 1.0) // 切点在直线段外
        {
            pFilletData->startParam1st = 0.0;
            pFilletData->endParam1st = filletPntParamOnLine;
        }
        else // 切点在直线段上
        {
            if ((lineStartPnt - filletPntOnLine).dot(filletPntOnArc - filletPntOnLine) < 0.0) // 夹角大于90度
            {
                pFilletData->startParam1st = 0.0;
                pFilletData->endParam1st = filletPntParamOnLine;
            }
            else
            {
                pFilletData->startParam1st = filletPntParamOnLine;
                pFilletData->endParam1st = 1.0;
            }
        }
    }

    // 结果:圆弧
    {
        double angle = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), filletPntOnArc - arcCenter);
        if (angle < arcStartAngle) angle += wy3d::TWO_PI;
        if (angle >= arcStartAngle && angle <= arcEndAngle) // 圆角点在圆弧上
        {
            // 在圆弧上为逆时针
            if ((isFilletArcCCW && offsetRadius < arcRadius)      // 内切同向
                || (!isFilletArcCCW && offsetRadius > arcRadius)) // 外切反向
            {
                pFilletData->startParam2nd = (angle - arcStartAngle) / arcTotalAngle;
                pFilletData->endParam2nd = 1.0;
            }
            else // 在圆弧上为顺时针
            {

                pFilletData->startParam2nd = 0.0;
                pFilletData->endParam2nd = (angle - arcStartAngle) / arcTotalAngle;
            }
        }
        else // 圆角点在圆弧外
        {
            pFilletData->startParam2nd = 0.0;
            pFilletData->endParam2nd = 1.0;

            // 在圆弧上为逆时针
            if ((isFilletArcCCW && offsetRadius < arcRadius)      // 内切同向
                || (!isFilletArcCCW && offsetRadius > arcRadius)) // 外切反向
            {
                if (angle > arcStartAngle) angle -= wy3d::TWO_PI;
                assert(angle < arcStartAngle);
                pFilletData->startParam2nd = (angle - arcStartAngle) / arcTotalAngle;
                pFilletData->endParam2nd = 1.0;
            }
            else // 在圆弧上为顺时针
            {
                pFilletData->startParam2nd = 0.0;
                assert(angle > arcStartAngle);
                pFilletData->endParam2nd = (angle - arcStartAngle) / arcTotalAngle;
            }
        }
    }

    // 结果:圆角
    pFilletData->filletCenter = filletCenter;
    pFilletData->filletRadius = R;
    pFilletData->filletStartAngle = filletStartAngle;
    pFilletData->filletEndAngle = filletEndAngle;

    return pFilletData;
}

std::shared_ptr<SketchFilletData> SketchFilletAlgo::filletLineCircle(double R, double tol,
    const wy::Vector2& lineStartPnt, const wy::Vector2& lineEndPnt, const wy::Vector2& pickPosOnLine,
    const wy::Vector2& circleCenter, double circleRadius, const wy::Vector2& pickPosOnCircle,
    bool isSecondPickPosMajor)
{
    assert(R > 0.0);
    assert(tol > 0.0);
    assert(circleRadius > 0.0);
    if (R <= tol) return nullptr; // 圆角过小

    // 直线段的数据
    wy::Vector2 lineMidPnt = (lineStartPnt + lineEndPnt) / 2;
    wy::Vector2 lineVec = lineEndPnt - lineStartPnt;
    double lineLen = lineVec.length();
    if (lineLen <= tol) // 直线段退化为点
    {
        return nullptr;
    }
    wy::Vector2 lineDir = lineVec;
    lineDir.normalize();

    // 圆数据
    if (circleRadius <= tol) // 圆退化
    {
        return nullptr;
    }

    // 根据直线段上拾取的点在圆的内侧还是外侧从而确定偏移圆是外扩还是内缩
    double offsetRadius = circleRadius;
    if ((pickPosOnLine - circleCenter).length() > circleRadius)
    {
        offsetRadius += R;
    }
    else
    {
        offsetRadius -= R;
        if (offsetRadius <= tol) return nullptr;
    }

    // 根据圆上拾取的点在直线段的哪一侧从而确定直线段的偏移向量
    wy::Vector2 lineOffsetDir(-lineDir.y(), lineDir.x());
    if ((pickPosOnCircle - lineStartPnt).dot(lineOffsetDir) < 0)
    {
        lineOffsetDir = -lineOffsetDir;
    }
    wy::Vector2 offsetLineStartPnt = lineStartPnt + lineOffsetDir * R;
    wy::Vector2 offsetLineEndPnt = lineEndPnt + lineOffsetDir * R;

    // 求偏移直线和偏移圆的交点从而确定圆角的圆心
    wy::Vector2 filletCenter;
    wy::Vector2 intPnt1, intPnt2;
    unsigned int filletIntPntIndex(0);
    unsigned int numIntPnts = wy3d::intersectLineCircle(offsetLineStartPnt, offsetLineEndPnt, circleCenter, offsetRadius, intPnt1, intPnt2);
    if (2 == numIntPnts)
    {
        // 根据交点离直线段上拾取点的距离来取舍
        wy::Vector2 pickPos = isSecondPickPosMajor ? pickPosOnCircle : pickPosOnLine;
        if ((intPnt1 - pickPos).length() < (intPnt2 - pickPos).length())
        {
            filletCenter = intPnt1;
            filletIntPntIndex = 1;
        }
        else
        {
            filletCenter = intPnt2;
            filletIntPntIndex = 2;
        }
    }
    else if (1 == numIntPnts)
    {
        filletCenter = intPnt1;
        filletIntPntIndex = 1;
    }
    else
    {
        assert(0 == numIntPnts);
        return nullptr;
    }

    // 圆角在直线段上的点
    wy::Vector2 filletPntOnLine = filletCenter - R * lineOffsetDir;
    double angleOfPntOnLine = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), filletPntOnLine - filletCenter);
    // 圆角在圆弧上的点
    double angleOfPntOnCircle(0.0);
    if (offsetRadius < circleRadius)
    {
        angleOfPntOnCircle = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), filletCenter - circleCenter);
    }
    else
    {
        angleOfPntOnCircle = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), circleCenter - filletCenter);
    }
    wy::Vector2 filletPntOnCircle = filletCenter;
    {
        wy::Vector2 dir = filletCenter - circleCenter;
        dir.normalize();
        if (offsetRadius < circleRadius) filletPntOnCircle += R * dir;
        else filletPntOnCircle -= R * dir;
    }

    // 确定圆角的起始角度和终止角度
    bool isFilletArcCCW(true); // 从圆角在直线段上的点以小于180度逆时针旋转到圆角在圆弧上的点
    double filletStartAngle = wy3d::normalizeRadian(angleOfPntOnLine);
    double filletEndAngle = wy3d::normalizeRadian(angleOfPntOnCircle);
    if (filletEndAngle < filletStartAngle) filletEndAngle += wy3d::TWO_PI;
    if ((filletEndAngle - filletStartAngle) > wy3d::PI) // 圆角的角度一定是小于180度,此时反转,表明是顺时针
    {
        std::swap(filletStartAngle, filletEndAngle);
        filletStartAngle = wy3d::normalizeRadian(filletStartAngle);
        filletEndAngle = wy3d::normalizeRadian(filletEndAngle);
        if (filletEndAngle < filletStartAngle) filletEndAngle += wy3d::TWO_PI;
        isFilletArcCCW = false;
    }

    // 结果:直线段
    std::shared_ptr<SketchFilletData> pFilletData = std::make_shared<SketchFilletData>();
    {
        double filletPntParamOnLine = (filletPntOnLine - lineStartPnt).dot(lineDir) / lineLen;
        if (filletPntParamOnLine < 0.0) // 切点在直线段外
        {
            pFilletData->startParam1st = filletPntParamOnLine;
            pFilletData->endParam1st = 1.0;
        }
        else if (filletPntParamOnLine > 1.0) // 切点在直线段外
        {
            pFilletData->startParam1st = 0.0;
            pFilletData->endParam1st = filletPntParamOnLine;
        }
        else // 切点在直线段上
        {
            if ((lineStartPnt - filletPntOnLine).dot(filletPntOnCircle - filletPntOnLine) < 0.0) // 夹角大于90度
            {
                pFilletData->startParam1st = 0.0;
                pFilletData->endParam1st = filletPntParamOnLine;
            }
            else
            {
                pFilletData->startParam1st = filletPntParamOnLine;
                pFilletData->endParam1st = 1.0;
            }
        }
    }

    // 结果:圆(完全保留)
    pFilletData->startParam2nd = 0.0;
    pFilletData->endParam2nd = 1.0;

    // 结果:圆角
    pFilletData->filletCenter = filletCenter;
    pFilletData->filletRadius = R;
    pFilletData->filletStartAngle = filletStartAngle;
    pFilletData->filletEndAngle = filletEndAngle;

    return pFilletData;
}

std::shared_ptr<SketchFilletData> SketchFilletAlgo::filletLineSpline(double R, double tol,
    const wy::Vector2& lineStartPnt, const wy::Vector2& lineEndPnt, const wy::Vector2& pickPosOnLine,
    Handle(Geom2d_BSplineCurve) pBSpline, const wy::Vector2& pickPosOnSpline,
    bool isSecondPickPosMajor)
{
    try
    {
        return _filletLineSpline(R, tol, lineStartPnt, lineEndPnt, pickPosOnLine, pBSpline, pickPosOnSpline, isSecondPickPosMajor);
    }
    catch (const Standard_Failure&)
    {
        assert(false);
        return nullptr;
    }
    catch (...)
    {
        assert(false);
        return nullptr;
    }
}

std::shared_ptr<SketchFilletData> SketchFilletAlgo::_filletLineSpline(double R, double tol,
    const wy::Vector2& lineStartPnt, const wy::Vector2& lineEndPnt, const wy::Vector2& pickPosOnLine,
    Handle(Geom2d_BSplineCurve) pBSpline, const wy::Vector2& pickPosOnSpline,
    bool isSecondPickPosMajor)
{
    assert(R > 0.0);
    assert(tol > 0.0);
    if (R <= tol) return nullptr; // 圆角过小

    // 直线段的数据
    wy::Vector2 lineVec = lineEndPnt - lineStartPnt;
    double lineLen = lineVec.length();
    if (lineLen <= tol) // 直线段退化为点
    {
        return nullptr;
    }
    wy::Vector2 lineDir = lineVec;
    lineDir.normalize();

    // 样条曲线数据
    if (!pBSpline)
    {
        assert(false);
        return nullptr;
    }

    // 根据B样条上拾取的点在直线段的哪一侧从而确定直线段的偏移向量
    wy::Vector2 lineOffsetDir(-lineDir.y(), lineDir.x());
    if ((pickPosOnSpline - lineStartPnt).dot(lineOffsetDir) < 0.0)
    {
        lineOffsetDir = -lineOffsetDir;
    }
    wy::Vector2 offsetLineStartPnt = lineStartPnt + lineOffsetDir * R;
    wy::Vector2 offsetLineEndPnt = lineEndPnt + lineOffsetDir * R;

    // 根据直线段上拾取的点确定B样条的偏移方向
    double bsplineOffset(R);
    Geom2dAPI_ProjectPointOnCurve projector(gp_Pnt2d(pickPosOnLine.x(), pickPosOnLine.y()), pBSpline);
    if (projector.NbPoints() > 0)
    {
        double projectionParam = projector.LowerDistanceParameter();
        gp_Pnt2d pnt2d;
        gp_Vec2d vec2d;
        pBSpline->D1(projectionParam, pnt2d, vec2d);
        wy::Vector2 projPnt(pnt2d.X(), pnt2d.Y());
        wy::Vector2 dir(vec2d.X(), vec2d.Y());
        dir.normalize();
        if (dir.length() < 0.5)
        {
            assert(false);
            return nullptr;
        }
        wy::Vector2 normal(dir.y(), -dir.x()); // 法向方向(顺时针旋转90度,Geom2d_OffsetCurve以该方向的偏移值为正)
        if ((pickPosOnLine - projPnt).dot(normal) < 0.0)
        {
            bsplineOffset = -R;
        }
    }
    else
    {
        assert(false);
        return nullptr;
    }

    // 求偏移直线和偏移样条曲线的交点从而确定圆角的圆心
    Handle(Geom2d_Curve) pOffsetBSpline = new Geom2d_OffsetCurve(pBSpline, bsplineOffset);
    //Handle(Geom2d_TrimmedCurve) pOffsetLineSeg(nullptr);
    Handle(Geom2d_Curve) pOffsetLine(nullptr);
    {
        gp_Pnt2d p1(offsetLineStartPnt.x(), offsetLineStartPnt.y());
        gp_Pnt2d p2(offsetLineEndPnt.x(), offsetLineEndPnt.y());
        gp_Vec2d vec(p1, p2);
        gp_Dir2d dir(vec);
        gp_Lin2d line(p1, dir);
        Handle(Geom2d_Line) geomLine = new Geom2d_Line(line);
        //pOffsetLineSeg = new Geom2d_TrimmedCurve(geomLine, 0.0, p1.Distance(p2));
        pOffsetLine = geomLine;
    }

    // 求偏移直线段和偏移样条曲线的交点
    Geom2dAPI_InterCurveCurve intersector;
    intersector.Init(pOffsetLine, pOffsetBSpline, tol);
    if (intersector.NbPoints() == 0)
    {
        return nullptr;
    }

    // 遍历交点确定圆角的圆心
    unsigned int numPnts = intersector.NbPoints();
    int index(-1);
    double minDistance(DBL_MAX);
    const wy::Vector2& refPickPos = isSecondPickPosMajor ? pickPosOnSpline : pickPosOnLine;
    for (int i = 1; i <= numPnts; i++)
    {
        // 获取交点
        gp_Pnt2d candidate2d = intersector.Point(i);
        wy::Vector2 candidate(candidate2d.X(), candidate2d.Y());

        // 交点在曲线上的参数
        double lineParam(0.0), splineParam(0.0);
        const IntRes2d_IntersectionPoint& intPoint =
            intersector.Intersector().Point(i);
        lineParam = intPoint.ParamOnFirst();
        splineParam = intPoint.ParamOnSecond();

        // 计算原始曲线上的点
        wy::Vector2 origLinePoint = lineStartPnt + lineDir * lineParam;
        gp_Pnt2d origSplinePoint2d;
        pBSpline->D0(splineParam, origSplinePoint2d);
        wy::Vector2 origSplinePoint(origSplinePoint2d.X(), origSplinePoint2d.Y());

        // 校验:候选圆心到原始曲线上的点的距离是否为R
        if (std::fabs((origSplinePoint - candidate).length() - R) > tol)
        {
            assert(false);
            continue;
        }
        if (std::fabs((origLinePoint - candidate).length() - R) > tol)
        {
            assert(false);
            continue;
        }

        // 找出离参考拾取点最近的交点
        if (-1 == index)
        {
            index = i;
            minDistance = (candidate - refPickPos).length();
        }
        else
        {
            double distance = (candidate - refPickPos).length();
            if (distance < minDistance)
            {
                index = i;
                minDistance = distance;
            }
        }
    }
    if (-1 == index)
    {
        return nullptr;
    }

    // 圆角圆心
    gp_Pnt2d filletCenter2d = intersector.Point(index);
    wy::Vector2 filletCenter(filletCenter2d.X(), filletCenter2d.Y());

    // 圆角在直线段上的点 + 圆角在样条曲线上的点
    wy::Vector2 filletPntOnLine;
    double filletParamOnLine(0.0);
    wy::Vector2 filletPntOnSpline;
    double filletParamOnSpline(0.0);
    {
        // 交点在曲线上的参数
        const IntRes2d_IntersectionPoint& intPoint =
            intersector.Intersector().Point(index);
        filletParamOnLine = intPoint.ParamOnFirst();
        filletParamOnSpline = intPoint.ParamOnSecond();

        // 计算原始曲线上的点
        filletPntOnLine = lineStartPnt + lineDir * filletParamOnLine;
        gp_Pnt2d origSplinePoint2d;
        pBSpline->D0(filletParamOnSpline, origSplinePoint2d);
        filletPntOnSpline.set(origSplinePoint2d.X(), origSplinePoint2d.Y());
    }

    // 确定圆角的起始角度和终止角度
    double angleOfPntOnLine = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), filletPntOnLine - filletCenter);
    double angleOfPntOnSpline = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), filletPntOnSpline - filletCenter);
    bool isFilletArcCCW(true); // 从圆角在直线段上的点以小于180度逆时针旋转到圆角在样条曲线上的点
    double filletStartAngle = angleOfPntOnLine;
    double filletEndAngle = angleOfPntOnSpline;
    if (filletEndAngle < filletStartAngle) filletEndAngle += wy3d::TWO_PI;
    if ((filletEndAngle - filletStartAngle) > wy3d::PI) // 圆角的角度一定是小于180度,此时反转,表明是顺时针
    {
        std::swap(filletStartAngle, filletEndAngle);
        filletStartAngle = wy3d::normalizeRadian(filletStartAngle);
        filletEndAngle = wy3d::normalizeRadian(filletEndAngle);
        if (filletEndAngle < filletStartAngle) filletEndAngle += wy3d::TWO_PI;
        isFilletArcCCW = false;
    }

    // 结果
    std::shared_ptr<SketchFilletData> pFilletData = std::make_shared<SketchFilletData>();
    // 1.结果:直线段
    {
        double filletPntParamOnLine = (filletPntOnLine - lineStartPnt).dot(lineDir) / lineLen;
        if (filletPntParamOnLine <= 0.0) // 切点在直线段外
        {
            pFilletData->startParam1st = filletPntParamOnLine;
            pFilletData->endParam1st = 1.0;
        }
        else if (filletPntParamOnLine >= 1.0) // 切点在直线段外
        {
            pFilletData->startParam1st = 0.0;
            pFilletData->endParam1st = filletPntParamOnLine;
        }
        else // 切点在直线段上
        {
            // 圆角在直线段连接处的切向量
            // 向量(cos(t), sin(t))顺时针旋转90度后为(sin(t), -cos(t))
            wy::Vector2 filletTangentDirOnLine(
                std::sin(angleOfPntOnLine), -std::cos(angleOfPntOnLine));
            if (!isFilletArcCCW) // 顺时针(从直线段过渡到样条曲线的圆角)
            {
                filletTangentDirOnLine *= -1;
            }

            if (filletTangentDirOnLine.dot(lineDir) > 0.0)
            {
                pFilletData->startParam1st = filletPntParamOnLine;
                pFilletData->endParam1st = 1.0;
            }
            else
            {
                pFilletData->startParam1st = 0.0;
                pFilletData->endParam1st = filletPntParamOnLine;
            }
        }
    }
    // 2.结果:样条曲线
    {
        // 圆角在样条曲线连接处的切向量
        // 向量(cos(t), sin(t))逆时针旋转90度后为(-sin(t), cos(t))
        wy::Vector2 filletTangentDirOnSpline(
            -std::sin(angleOfPntOnSpline), std::cos(angleOfPntOnSpline));
        if (!isFilletArcCCW) // 顺时针(从直线段过渡到样条曲线的圆角)
        {
            filletTangentDirOnSpline *= -1;
        }

        // 样条曲线在圆角连接处的切向量
        gp_Pnt2d pnt2d;
        gp_Vec2d vec2d;
        pBSpline->D1(filletParamOnSpline, pnt2d, vec2d);
        wy::Vector2 splineDirAtJoint(vec2d.X(), vec2d.Y());
        splineDirAtJoint.normalize();

        // 确定样条曲线范围
        double firstParam = pBSpline->FirstParameter();
        double lastParam = pBSpline->LastParameter();
        if (splineDirAtJoint.dot(filletTangentDirOnSpline) > 0.0)
        {
            pFilletData->startParam2nd = (filletParamOnSpline - firstParam) / (lastParam - firstParam);
            pFilletData->endParam2nd = 1.0;
        }
        else
        {
            pFilletData->startParam2nd = 0.0;
            pFilletData->endParam2nd = (filletParamOnSpline - firstParam) / (lastParam - firstParam);
        }
    }

    // 结果:圆角
    pFilletData->filletCenter = filletCenter;
    pFilletData->filletRadius = R;
    pFilletData->filletStartAngle = filletStartAngle;
    pFilletData->filletEndAngle = filletEndAngle;

    return pFilletData;
}

std::shared_ptr<SketchFilletData> SketchFilletAlgo::filletCircleCircle(double R, double tol,
    const wy::Vector2& center1, double radius1, const wy::Vector2& pickPos1,
    const wy::Vector2& center2, double radius2, const wy::Vector2& pickPos2)
{
    assert(R > 0.0);
    assert(tol > 0.0);
    assert(radius1 > 0.0);
    assert(radius2 > 0.0);

    if (R <= tol) return nullptr; // 圆角过小
    if (radius1 <= tol || radius2 <= tol) // 圆退化
    {
        return nullptr;
    }

    // 根据圆2上的拾取点确定偏移圆1是外切还是内切
    double offsetR1 = radius1;
    if ((pickPos2 - center1).length() > radius1)
    {
        offsetR1 += R;
    }
    else
    {
        offsetR1 -= R;
        if (offsetR1 <= tol) return nullptr;
    }

    // 根据圆1上的拾取点确定偏移圆2是外切还是内切
    double offsetR2 = radius2;
    if ((pickPos1 - center2).length() > radius2)
    {
        offsetR2 += R;
    }
    else
    {
        offsetR2 -= R;
        if (offsetR2 <= tol) return nullptr;
    }

    // 求两个偏移圆的交点
    wy::Vector2 filletCenter;
    wy::Vector2 intPnt1, intPnt2;
    unsigned int numIntPnts = wy3d::intersectCircleCircle(center1, offsetR1, center2, offsetR2, intPnt1, intPnt2);
    if (0 == numIntPnts)
    {
        return nullptr;
    }
    else if (1 == numIntPnts)
    {
        filletCenter = intPnt1;
    }
    else // 2 == numIntPnts
    {
        assert(2 == numIntPnts);
        // 依据第二个拾取点离交点的距离来取舍
        if ((pickPos2 - intPnt1).length() <= (pickPos2 - intPnt2).length())
        {
            filletCenter = intPnt1;
        }
        else
        {
            filletCenter = intPnt2;
        }
    }

    // 圆角在圆1上的切点
    wy::Vector2 filletPnt1;
    {
        wy::Vector2 dir = center1 - filletCenter;
        dir.normalize();
        if (offsetR1 > radius1) // 外切
            filletPnt1 = filletCenter + R * dir;
        else // 内切
            filletPnt1 = filletCenter - R * dir;
    }

    // 圆角在圆2上的切点
    wy::Vector2 filletPnt2;
    {
        wy::Vector2 dir = center2 - filletCenter;
        dir.normalize();
        if (offsetR2 > radius2) // 外切
            filletPnt2 = filletCenter + R * dir;
        else // 内切
            filletPnt2 = filletCenter - R * dir;
    }

    // 确定圆角的起始角度和终止角度
    double filletStartAngle = wy::Vector2::rotationAngle(wy::Vector2::kXAxis, filletPnt1 - filletCenter);
    filletStartAngle = wy3d::normalizeRadian(filletStartAngle);
    double filletEndAngle = wy::Vector2::rotationAngle(wy::Vector2::kXAxis, filletPnt2 - filletCenter);
    filletEndAngle = wy3d::normalizeRadian(filletEndAngle);
    if (filletEndAngle < filletStartAngle) filletEndAngle += wy3d::TWO_PI;
    if ((filletEndAngle - filletStartAngle) > wy3d::PI) // 圆角的角度一定是小于180度,此时反转,表明是顺时针
    {
        std::swap(filletStartAngle, filletEndAngle);
        filletStartAngle = wy3d::normalizeRadian(filletStartAngle);
        filletEndAngle = wy3d::normalizeRadian(filletEndAngle);
        if (filletEndAngle < filletStartAngle) filletEndAngle += wy3d::TWO_PI;
    }

    // 结果
    std::shared_ptr<SketchFilletData> pFilletData = std::make_shared<SketchFilletData>();
    // 圆完全保留
    pFilletData->startParam1st = 0.0;
    pFilletData->endParam1st = 1.0;
    pFilletData->startParam2nd = 0.0;
    pFilletData->endParam2nd = 1.0;
    // 结果:圆角
    pFilletData->filletCenter = filletCenter;
    pFilletData->filletRadius = R;
    pFilletData->filletStartAngle = filletStartAngle;
    pFilletData->filletEndAngle = filletEndAngle;

    return pFilletData;
}

std::shared_ptr<SketchFilletData> SketchFilletAlgo::filletCircleArc(double R, double tol,
    const wy::Vector2& center1, double radius1, const wy::Vector2& pickPos1,
    const wy::Vector2& center2, double radius2, double startAngle, double endAngle, const wy::Vector2& pickPos2,
    bool isSecondPickPosMajor)
{
    assert(R > 0.0);
    assert(tol > 0.0);
    assert(radius1 > 0.0);
    assert(radius2 > 0.0);

    if (R <= tol) return nullptr; // 圆角过小
    if (radius1 <= tol || radius2 <= tol) // 圆退化
    {
        return nullptr;
    }

    startAngle = wy3d::normalizeRadian(startAngle);
    endAngle = wy3d::normalizeRadian(endAngle);
    if (endAngle < startAngle) endAngle += wy3d::TWO_PI;
    double totalAngle = endAngle - startAngle;
    if (totalAngle <= tol) // 圆弧退化
    {
        return nullptr;
    }

    // 根据圆2上的拾取点确定偏移圆1是外切还是内切
    double offsetR1 = radius1;
    if ((pickPos2 - center1).length() > radius1)
    {
        offsetR1 += R;
    }
    else
    {
        offsetR1 -= R;
        if (offsetR1 <= tol) return nullptr;
    }

    // 根据圆1上的拾取点确定偏移圆2是外切还是内切
    double offsetR2 = radius2;
    if ((pickPos1 - center2).length() > radius2)
    {
        offsetR2 += R;
    }
    else
    {
        offsetR2 -= R;
        if (offsetR2 <= tol) return nullptr;
    }

    // 求两个偏移圆的交点
    wy::Vector2 filletCenter;
    wy::Vector2 intPnt1, intPnt2;
    unsigned int numIntPnts = wy3d::intersectCircleCircle(center1, offsetR1, center2, offsetR2, intPnt1, intPnt2);
    if (0 == numIntPnts)
    {
        return nullptr;
    }
    else if (1 == numIntPnts)
    {
        filletCenter = intPnt1;
    }
    else // 2 == numIntPnts
    {
        assert(2 == numIntPnts);
        wy::Vector2 pickPos = isSecondPickPosMajor ? pickPos2 : pickPos1;
        // 依据第二个拾取点离交点的距离来取舍
        if ((pickPos - intPnt1).length() <= (pickPos - intPnt2).length())
        {
            filletCenter = intPnt1;
        }
        else
        {
            filletCenter = intPnt2;
        }
    }

    // 圆角在圆1上的切点
    wy::Vector2 filletPnt1;
    {
        wy::Vector2 dir = center1 - filletCenter;
        dir.normalize();
        if (offsetR1 > radius1) // 外切
            filletPnt1 = filletCenter + R * dir;
        else // 内切
            filletPnt1 = filletCenter - R * dir;
    }

    // 圆角在圆2上的切点
    wy::Vector2 filletPnt2;
    {
        wy::Vector2 dir = center2 - filletCenter;
        dir.normalize();
        if (offsetR2 > radius2) // 外切
            filletPnt2 = filletCenter + R * dir;
        else // 内切
            filletPnt2 = filletCenter - R * dir;
    }

    // 确定圆角的起始角度和终止角度
    double filletStartAngle = wy::Vector2::rotationAngle(wy::Vector2::kXAxis, filletPnt1 - filletCenter);
    filletStartAngle = wy3d::normalizeRadian(filletStartAngle);
    double filletEndAngle = wy::Vector2::rotationAngle(wy::Vector2::kXAxis, filletPnt2 - filletCenter);
    filletEndAngle = wy3d::normalizeRadian(filletEndAngle);
    if (filletEndAngle < filletStartAngle) filletEndAngle += wy3d::TWO_PI;
    bool isFilletCCW(true); // 圆角逆时针
    if ((filletEndAngle - filletStartAngle) > wy3d::PI) // 圆角的角度一定是小于180度,此时反转,表明是顺时针
    {
        isFilletCCW = false;
        std::swap(filletStartAngle, filletEndAngle);
        filletStartAngle = wy3d::normalizeRadian(filletStartAngle);
        filletEndAngle = wy3d::normalizeRadian(filletEndAngle);
        if (filletEndAngle < filletStartAngle) filletEndAngle += wy3d::TWO_PI;
    }

    // 结果
    std::shared_ptr<SketchFilletData> pFilletData = std::make_shared<SketchFilletData>();
    // 结果:圆
    pFilletData->startParam1st = 0.0;
    pFilletData->endParam1st = 1.0;
    // 结果:圆弧
    {
        double angle = wy::Vector2::rotationAngle(wy::Vector2::kXAxis, filletPnt2 - center2);
        if (angle < startAngle) angle += wy3d::TWO_PI;
        if (angle >= startAngle && angle <= endAngle) // 切点在圆弧上
        {
            // 在圆弧上为逆时针
            if ((isFilletCCW && offsetR2 < radius2)      // 内切同向
                || (!isFilletCCW && offsetR2 > radius2)) // 外切反向
            {
                pFilletData->startParam2nd = (angle - startAngle) / totalAngle;
                pFilletData->endParam2nd = 1.0;
            }
            else // 在圆弧上为顺时针
            {
                pFilletData->startParam2nd = 0.0;
                pFilletData->endParam2nd = (angle - startAngle) / totalAngle;
            }
        }
        else // 切点在圆弧外
        {
            // 在圆弧上为逆时针
            if ((isFilletCCW && offsetR2 < radius2)      // 内切同向
                || (!isFilletCCW && offsetR2 > radius2)) // 外切反向
            {
                if (angle > startAngle) angle -= wy3d::TWO_PI;
                assert(angle < startAngle);
                pFilletData->startParam2nd = (angle - startAngle) / totalAngle;
                pFilletData->endParam2nd = 1.0;
            }
            else // 在圆弧上为顺时针
            {
                pFilletData->startParam2nd = 0.0;
                assert(angle > startAngle);
                pFilletData->endParam2nd = (angle - startAngle) / totalAngle;
            }
        }
    }
    // 结果:圆角
    pFilletData->filletCenter = filletCenter;
    pFilletData->filletRadius = R;
    pFilletData->filletStartAngle = filletStartAngle;
    pFilletData->filletEndAngle = filletEndAngle;

    return pFilletData;
}

std::shared_ptr<SketchFilletData> SketchFilletAlgo::filletCircleSpline(double R, double tol,
    const wy::Vector2& center, double radius, const wy::Vector2& pickPosOnCircle,
    Handle(Geom2d_BSplineCurve) pBSpline, const wy::Vector2& pickPosOnSpline,
    bool isSecondPickPosMajor)
{
    try
    {
        return _filletCircleSpline(R, tol, center, radius, pickPosOnCircle, pBSpline, pickPosOnSpline, isSecondPickPosMajor);
    }
    catch (const Standard_Failure&)
    {
        assert(false);
        return nullptr;
    }
    catch (...)
    {
        assert(false);
        return nullptr;
    }
}

std::shared_ptr<SketchFilletData> SketchFilletAlgo::_filletCircleSpline(double R, double tol,
    const wy::Vector2& center, double radius, const wy::Vector2& pickPosOnCircle,
    Handle(Geom2d_BSplineCurve) pBSpline, const wy::Vector2& pickPosOnSpline,
    bool isSecondPickPosMajor)
{
    assert(R > 0.0);
    assert(tol > 0.0);
    assert(radius > 0.0);

    if (R <= tol) return nullptr; // 圆角过小
    if (radius <= tol) // 圆退化
    {
        return nullptr;
    }

    // 样条曲线数据
    if (!pBSpline)
    {
        assert(false);
        return nullptr;
    }

    // 根据B样条上拾取的点确定偏移圆是外切还是内切
    double offsetR = radius;
    if ((pickPosOnSpline - center).length() > radius)
    {
        offsetR += R;
    }
    else
    {
        offsetR -= R;
        if (offsetR <= tol) return nullptr;
    }

    // 根据圆上拾取的点确定B样条的偏移方向
    double bsplineOffset(R);
    Geom2dAPI_ProjectPointOnCurve projector(gp_Pnt2d(pickPosOnCircle.x(), pickPosOnCircle.y()), pBSpline);
    double projectionParam(0.0);
    if (projector.NbPoints() > 0)
    {
        projectionParam = projector.LowerDistanceParameter();
    }
    // added by wangyao 2025.07.19 {
    // 如果没有投影点则计算拾取点到样条曲线的起点和终点的距离,确定投影参数是起始参数还是终止参数.
    else
    {
        double firstParam = pBSpline->FirstParameter();
        double lastParam = pBSpline->LastParameter();
        gp_Pnt2d startPoint2d, endPoint2d;
        pBSpline->D0(firstParam, startPoint2d);
        pBSpline->D0(lastParam, endPoint2d);
        wy::Vector2 startPnt(startPoint2d.X(), startPoint2d.Y());
        wy::Vector2 endPnt(endPoint2d.X(), endPoint2d.Y());
        if ((pickPosOnCircle - startPnt).length() <= (pickPosOnCircle - endPnt).length())
        {
            projectionParam = firstParam;
        }
        else
        {
            projectionParam = lastParam;
        }
    }
    // }

    {
        gp_Pnt2d pnt2d;
        gp_Vec2d vec2d;
        pBSpline->D1(projectionParam, pnt2d, vec2d);
        wy::Vector2 projPnt(pnt2d.X(), pnt2d.Y());
        wy::Vector2 dir(vec2d.X(), vec2d.Y());
        dir.normalize();
        if (dir.length() < 0.5)
        {
            assert(false);
            return nullptr;
        }
        wy::Vector2 normal(dir.y(), -dir.x()); // 法向方向(顺时针旋转90度,Geom2d_OffsetCurve以该方向的偏移值为正)
        if ((pickPosOnCircle - projPnt).dot(normal) < 0.0)
        {
            bsplineOffset = -R;
        }
    }

    // 求偏移圆和偏移样条曲线的交点
    Handle(Geom2d_Curve) pOffsetBSpline = new Geom2d_OffsetCurve(pBSpline, bsplineOffset);
    Handle(Geom2d_Curve) pOffsetCircle(nullptr);
    {
        gp_Ax22d axisCircle(gp_Pnt2d(center.x(), center.y()), gp_Dir2d(1.0, 0.0));
        gp_Circ2d circle(axisCircle, offsetR);
        pOffsetCircle = new Geom2d_Circle(circle);
    }
    Geom2dAPI_InterCurveCurve intersector;
    intersector.Init(pOffsetCircle, pOffsetBSpline, tol);
    if (intersector.NbPoints() == 0)
    {
        return nullptr;
    }

    // 遍历交点确定圆角的圆心
    unsigned int numPnts = intersector.NbPoints();
    int index(-1);
    double minDistance(DBL_MAX);
    const wy::Vector2& refPickPos = isSecondPickPosMajor ? pickPosOnSpline : pickPosOnCircle;
    for (int i = 1; i <= numPnts; i++)
    {
        // 获取交点
        gp_Pnt2d candidate2d = intersector.Point(i);
        wy::Vector2 candidate(candidate2d.X(), candidate2d.Y());

        // 交点在曲线上的参数
        double circleParam(0.0), splineParam(0.0);
        const IntRes2d_IntersectionPoint& intPoint =
            intersector.Intersector().Point(i);
        circleParam = intPoint.ParamOnFirst();
        splineParam = intPoint.ParamOnSecond();

        // 计算原始曲线上的点
        wy::Vector2 origCirclePoint = center + radius * wy::Vector2(std::cos(circleParam), std::sin(circleParam));
        gp_Pnt2d origSplinePoint2d;
        pBSpline->D0(splineParam, origSplinePoint2d);
        wy::Vector2 origSplinePoint(origSplinePoint2d.X(), origSplinePoint2d.Y());

        // 校验:候选圆心到原始曲线上的点的距离是否为R
        if (std::fabs((origSplinePoint - candidate).length() - R) > tol)
        {
            assert(false);
            continue;
        }
        if (std::fabs((origCirclePoint - candidate).length() - R) > tol)
        {
            assert(false);
            continue;
        }

        // 找出离参考拾取点最近的交点
        if (-1 == index)
        {
            index = i;
            minDistance = (candidate - refPickPos).length();
        }
        else
        {
            double distance = (candidate - refPickPos).length();
            if (distance < minDistance)
            {
                index = i;
                minDistance = distance;
            }
        }
    }
    if (-1 == index)
    {
        return nullptr;
    }

    // 圆角圆心
    gp_Pnt2d filletCenter2d = intersector.Point(index);
    wy::Vector2 filletCenter(filletCenter2d.X(), filletCenter2d.Y());

    // 圆角在圆上的点 + 圆角在样条曲线上的点
    wy::Vector2 filletPntOnCircle;
    double filletParamOnCircle(0.0);
    wy::Vector2 filletPntOnSpline;
    double filletParamOnSpline(0.0);
    {
        // 交点在曲线上的参数
        const IntRes2d_IntersectionPoint& intPoint =
            intersector.Intersector().Point(index);
        filletParamOnCircle = intPoint.ParamOnFirst();
        filletParamOnSpline = intPoint.ParamOnSecond();

        // 计算原始曲线上的点
        filletPntOnCircle = center + radius * wy::Vector2(std::cos(filletParamOnCircle), std::sin(filletParamOnCircle));
        gp_Pnt2d origSplinePoint2d;
        pBSpline->D0(filletParamOnSpline, origSplinePoint2d);
        filletPntOnSpline.set(origSplinePoint2d.X(), origSplinePoint2d.Y());
    }

    // 确定圆角的起始角度和终止角度
    double angleOfPntOnLine = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), filletPntOnCircle - filletCenter);
    double angleOfPntOnSpline = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), filletPntOnSpline - filletCenter);
    bool isFilletArcCCW(true); // 从圆角在直线段上的点以小于180度逆时针旋转到圆角在样条曲线上的点
    double filletStartAngle = angleOfPntOnLine;
    double filletEndAngle = angleOfPntOnSpline;
    if (filletEndAngle < filletStartAngle) filletEndAngle += wy3d::TWO_PI;
    if ((filletEndAngle - filletStartAngle) > wy3d::PI) // 圆角的角度一定是小于180度,此时反转,表明是顺时针
    {
        std::swap(filletStartAngle, filletEndAngle);
        filletStartAngle = wy3d::normalizeRadian(filletStartAngle);
        filletEndAngle = wy3d::normalizeRadian(filletEndAngle);
        if (filletEndAngle < filletStartAngle) filletEndAngle += wy3d::TWO_PI;
        isFilletArcCCW = false;
    }

    // 结果
    std::shared_ptr<SketchFilletData> pFilletData = std::make_shared<SketchFilletData>();
    // 1.结果:圆 完全保留
    {
        pFilletData->startParam1st = 0.0;
        pFilletData->endParam1st = 1.0;
    }
    // 2.结果:样条曲线
    {
        // 圆角在样条曲线连接处的切向量
        // 向量(cos(t), sin(t))逆时针旋转90度后为(-sin(t), cos(t))
        wy::Vector2 filletTangentDirOnSpline(
            -std::sin(angleOfPntOnSpline), std::cos(angleOfPntOnSpline));
        if (!isFilletArcCCW) // 顺时针(从直线段过渡到样条曲线的圆角)
        {
            filletTangentDirOnSpline *= -1;
        }

        // 样条曲线在圆角连接处的切向量
        gp_Pnt2d pnt2d;
        gp_Vec2d vec2d;
        pBSpline->D1(filletParamOnSpline, pnt2d, vec2d);
        wy::Vector2 splineDirAtJoint(vec2d.X(), vec2d.Y());
        splineDirAtJoint.normalize();

        // 确定样条曲线范围
        double firstParam = pBSpline->FirstParameter();
        double lastParam = pBSpline->LastParameter();
        if (splineDirAtJoint.dot(filletTangentDirOnSpline) > 0.0)
        {
            pFilletData->startParam2nd = (filletParamOnSpline - firstParam) / (lastParam - firstParam);
            pFilletData->endParam2nd = 1.0;
        }
        else
        {
            pFilletData->startParam2nd = 0.0;
            pFilletData->endParam2nd = (filletParamOnSpline - firstParam) / (lastParam - firstParam);
        }
    }

    // 结果:圆角
    pFilletData->filletCenter = filletCenter;
    pFilletData->filletRadius = R;
    pFilletData->filletStartAngle = filletStartAngle;
    pFilletData->filletEndAngle = filletEndAngle;

    return pFilletData;
}

std::shared_ptr<SketchFilletData> SketchFilletAlgo::filletArcArc(double R, double tol,
    const wy::Vector2& center1, double radius1, double startAngle1, double endAngle1, const wy::Vector2& pickPos1,
    const wy::Vector2& center2, double radius2, double startAngle2, double endAngle2, const wy::Vector2& pickPos2)
{
    assert(R > 0.0);
    assert(tol > 0.0);
    assert(radius1 > 0.0);
    assert(radius2 > 0.0);

    if (R <= tol) return nullptr; // 圆角过小
    if (radius1 <= tol || radius2 <= tol) // 圆退化
    {
        return nullptr;
    }

    // 圆弧1
    startAngle1 = wy3d::normalizeRadian(startAngle1);
    endAngle1 = wy3d::normalizeRadian(endAngle1);
    if (endAngle1 < startAngle1) endAngle1 += wy3d::TWO_PI;
    double totalAngle1 = endAngle1 - startAngle1;
    if (totalAngle1 <= tol) // 圆弧退化
    {
        return nullptr;
    }

    // 圆弧2
    startAngle2 = wy3d::normalizeRadian(startAngle2);
    endAngle2 = wy3d::normalizeRadian(endAngle2);
    if (endAngle2 < startAngle2) endAngle2 += wy3d::TWO_PI;
    double totalAngle2 = endAngle2 - startAngle2;
    if (totalAngle2 <= tol) // 圆弧退化
    {
        return nullptr;
    }

    // 根据圆2上的拾取点确定偏移圆1是外切还是内切
    double offsetR1 = radius1;
    if ((pickPos2 - center1).length() > radius1)
    {
        offsetR1 += R;
    }
    else
    {
        offsetR1 -= R;
        if (offsetR1 <= tol) return nullptr;
    }

    // 根据圆1上的拾取点确定偏移圆2是外切还是内切
    double offsetR2 = radius2;
    if ((pickPos1 - center2).length() > radius2)
    {
        offsetR2 += R;
    }
    else
    {
        offsetR2 -= R;
        if (offsetR2 <= tol) return nullptr;
    }

    // 求两个偏移圆的交点
    wy::Vector2 filletCenter;
    wy::Vector2 intPnt1, intPnt2;
    unsigned int numIntPnts = wy3d::intersectCircleCircle(center1, offsetR1, center2, offsetR2, intPnt1, intPnt2);
    if (0 == numIntPnts)
    {
        return nullptr;
    }
    else if (1 == numIntPnts)
    {
        filletCenter = intPnt1;
    }
    else // 2 == numIntPnts
    {
        assert(2 == numIntPnts);
        // 依据第二个拾取点离交点的距离来取舍
        if ((pickPos2 - intPnt1).length() <= (pickPos2 - intPnt2).length())
        {
            filletCenter = intPnt1;
        }
        else
        {
            filletCenter = intPnt2;
        }
    }

    // 圆角在圆1上的切点
    wy::Vector2 filletPnt1;
    {
        wy::Vector2 dir = center1 - filletCenter;
        dir.normalize();
        if (offsetR1 > radius1) // 外切
            filletPnt1 = filletCenter + R * dir;
        else // 内切
            filletPnt1 = filletCenter - R * dir;
    }

    // 圆角在圆2上的切点
    wy::Vector2 filletPnt2;
    {
        wy::Vector2 dir = center2 - filletCenter;
        dir.normalize();
        if (offsetR2 > radius2) // 外切
            filletPnt2 = filletCenter + R * dir;
        else // 内切
            filletPnt2 = filletCenter - R * dir;
    }

    // 确定圆角的起始角度和终止角度
    double filletStartAngle = wy::Vector2::rotationAngle(wy::Vector2::kXAxis, filletPnt1 - filletCenter);
    filletStartAngle = wy3d::normalizeRadian(filletStartAngle);
    double filletEndAngle = wy::Vector2::rotationAngle(wy::Vector2::kXAxis, filletPnt2 - filletCenter);
    filletEndAngle = wy3d::normalizeRadian(filletEndAngle);
    if (filletEndAngle < filletStartAngle) filletEndAngle += wy3d::TWO_PI;
    bool isFilletCCW(true); // 圆角逆时针
    if ((filletEndAngle - filletStartAngle) > wy3d::PI) // 圆角的角度一定是小于180度,此时反转,表明是顺时针
    {
        isFilletCCW = false;
        std::swap(filletStartAngle, filletEndAngle);
        filletStartAngle = wy3d::normalizeRadian(filletStartAngle);
        filletEndAngle = wy3d::normalizeRadian(filletEndAngle);
        if (filletEndAngle < filletStartAngle) filletEndAngle += wy3d::TWO_PI;
    }

    // 结果
    std::shared_ptr<SketchFilletData> pFilletData = std::make_shared<SketchFilletData>();
    // 结果:圆弧1
    {
        computeArcStartEndParam(filletPnt1,
            center1, radius1, startAngle1, endAngle1, totalAngle1,
            offsetR1, !isFilletCCW, pFilletData->startParam1st, pFilletData->endParam1st);
    }
    // 结果:圆弧2
    {
        computeArcStartEndParam(filletPnt2,
            center2, radius2, startAngle2, endAngle2, totalAngle2,
            offsetR2, isFilletCCW, pFilletData->startParam2nd, pFilletData->endParam2nd);
    }
    // 结果:圆角
    pFilletData->filletCenter = filletCenter;
    pFilletData->filletRadius = R;
    pFilletData->filletStartAngle = filletStartAngle;
    pFilletData->filletEndAngle = filletEndAngle;

    return pFilletData;
}

std::shared_ptr<SketchFilletData> SketchFilletAlgo::filletArcSpline(double R, double tol,
    const wy::Vector2& center, double radius, double startAngle, double endAngle, const wy::Vector2& pickPosOnArc,
    Handle(Geom2d_BSplineCurve) pBSpline, const wy::Vector2& pickPosOnSpline,
    bool isSecondPickPosMajor)
{
    try
    {
        return _filletArcSpline(R, tol, center, radius, startAngle, endAngle, pickPosOnArc, pBSpline, pickPosOnSpline, isSecondPickPosMajor);
    }
    catch (const Standard_Failure&)
    {
        assert(false);
        return nullptr;
    }
    catch (...)
    {
        assert(false);
        return nullptr;
    }
}

std::shared_ptr<SketchFilletData> SketchFilletAlgo::_filletArcSpline(double R, double tol,
    const wy::Vector2& center, double radius, double startAngle, double endAngle, const wy::Vector2& pickPosOnArc,
    Handle(Geom2d_BSplineCurve) pBSpline, const wy::Vector2& pickPosOnSpline,
    bool isSecondPickPosMajor)
{
    assert(R > 0.0);
    assert(tol > 0.0);
    assert(radius > 0.0);

    if (R <= tol) return nullptr; // 圆角过小
    if (radius <= tol) // 圆退化
    {
        return nullptr;
    }

    // 圆弧
    startAngle = wy3d::normalizeRadian(startAngle);
    endAngle = wy3d::normalizeRadian(endAngle);
    if (endAngle < startAngle) endAngle += wy3d::TWO_PI;
    double totalAngle = endAngle - startAngle;
    if (totalAngle <= tol) // 圆弧退化
    {
        return nullptr;
    }

    // 样条曲线数据
    if (!pBSpline)
    {
        assert(false);
        return nullptr;
    }

    // 根据B样条上拾取的点确定偏移圆是外切还是内切
    double offsetR = radius;
    if ((pickPosOnSpline - center).length() > radius)
    {
        offsetR += R;
    }
    else
    {
        offsetR -= R;
        if (offsetR <= tol) return nullptr;
    }

    // 根据圆弧上拾取的点确定B样条的偏移方向
    double bsplineOffset(R);
    Geom2dAPI_ProjectPointOnCurve projector(gp_Pnt2d(pickPosOnArc.x(), pickPosOnArc.y()), pBSpline);
    if (projector.NbPoints() > 0)
    {
        double projectionParam = projector.LowerDistanceParameter();
        gp_Pnt2d pnt2d;
        gp_Vec2d vec2d;
        pBSpline->D1(projectionParam, pnt2d, vec2d);
        wy::Vector2 projPnt(pnt2d.X(), pnt2d.Y());
        wy::Vector2 dir(vec2d.X(), vec2d.Y());
        dir.normalize();
        if (dir.length() < 0.5)
        {
            assert(false);
            return nullptr;
        }
        wy::Vector2 normal(dir.y(), -dir.x()); // 法向方向(顺时针旋转90度,Geom2d_OffsetCurve以该方向的偏移值为正)
        if ((pickPosOnArc - projPnt).dot(normal) < 0.0)
        {
            bsplineOffset = -R;
        }
    }
    else
    {
        assert(false);
        return nullptr;
    }

    // 求偏移圆和偏移样条曲线的交点
    Handle(Geom2d_Curve) pOffsetBSpline = new Geom2d_OffsetCurve(pBSpline, bsplineOffset);
    Handle(Geom2d_Curve) pOffsetCircle(nullptr);
    {
        gp_Ax22d axisCircle(gp_Pnt2d(center.x(), center.y()), gp_Dir2d(1.0, 0.0));
        gp_Circ2d circle(axisCircle, offsetR);
        pOffsetCircle = new Geom2d_Circle(circle);
    }
    Geom2dAPI_InterCurveCurve intersector;
    intersector.Init(pOffsetCircle, pOffsetBSpline, tol);
    if (intersector.NbPoints() == 0)
    {
        return nullptr;
    }

    // 遍历交点确定圆角的圆心
    unsigned int numPnts = intersector.NbPoints();
    int index(-1);
    double minDistance(DBL_MAX);
    const wy::Vector2& refPickPos = isSecondPickPosMajor ? pickPosOnSpline : pickPosOnArc;
    for (int i = 1; i <= numPnts; i++)
    {
        // 获取交点
        gp_Pnt2d candidate2d = intersector.Point(i);
        wy::Vector2 candidate(candidate2d.X(), candidate2d.Y());

        // 交点在曲线上的参数
        double circleParam(0.0), splineParam(0.0);
        const IntRes2d_IntersectionPoint& intPoint =
            intersector.Intersector().Point(i);
        circleParam = intPoint.ParamOnFirst();
        splineParam = intPoint.ParamOnSecond();

        // 计算原始曲线上的点
        wy::Vector2 origCirclePoint = center + radius * wy::Vector2(std::cos(circleParam), std::sin(circleParam));
        gp_Pnt2d origSplinePoint2d;
        pBSpline->D0(splineParam, origSplinePoint2d);
        wy::Vector2 origSplinePoint(origSplinePoint2d.X(), origSplinePoint2d.Y());

        // 校验:候选圆心到原始曲线上的点的距离是否为R
        if (std::fabs((origSplinePoint - candidate).length() - R) > tol)
        {
            assert(false);
            continue;
        }
        if (std::fabs((origCirclePoint - candidate).length() - R) > tol)
        {
            assert(false);
            continue;
        }

        // 找出离参考拾取点最近的交点
        if (-1 == index)
        {
            index = i;
            minDistance = (candidate - refPickPos).length();
        }
        else
        {
            double distance = (candidate - refPickPos).length();
            if (distance < minDistance)
            {
                index = i;
                minDistance = distance;
            }
        }
    }
    if (-1 == index)
    {
        return nullptr;
    }

    // 圆角圆心
    gp_Pnt2d filletCenter2d = intersector.Point(index);
    wy::Vector2 filletCenter(filletCenter2d.X(), filletCenter2d.Y());

    // 圆角在圆上的点 + 圆角在样条曲线上的点
    wy::Vector2 filletPntOnCircle;
    double filletParamOnCircle(0.0);
    wy::Vector2 filletPntOnSpline;
    double filletParamOnSpline(0.0);
    {
        // 交点在曲线上的参数
        const IntRes2d_IntersectionPoint& intPoint =
            intersector.Intersector().Point(index);
        filletParamOnCircle = intPoint.ParamOnFirst();
        filletParamOnSpline = intPoint.ParamOnSecond();

        // 计算原始曲线上的点
        filletPntOnCircle = center + radius * wy::Vector2(std::cos(filletParamOnCircle), std::sin(filletParamOnCircle));
        gp_Pnt2d origSplinePoint2d;
        pBSpline->D0(filletParamOnSpline, origSplinePoint2d);
        filletPntOnSpline.set(origSplinePoint2d.X(), origSplinePoint2d.Y());
    }

    // 确定圆角的起始角度和终止角度
    double angleOfPntOnLine = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), filletPntOnCircle - filletCenter);
    double angleOfPntOnSpline = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), filletPntOnSpline - filletCenter);
    bool isFilletArcCCW(true); // 从圆角在直线段上的点以小于180度逆时针旋转到圆角在样条曲线上的点
    double filletStartAngle = angleOfPntOnLine;
    double filletEndAngle = angleOfPntOnSpline;
    if (filletEndAngle < filletStartAngle) filletEndAngle += wy3d::TWO_PI;
    if ((filletEndAngle - filletStartAngle) > wy3d::PI) // 圆角的角度一定是小于180度,此时反转,表明是顺时针
    {
        std::swap(filletStartAngle, filletEndAngle);
        filletStartAngle = wy3d::normalizeRadian(filletStartAngle);
        filletEndAngle = wy3d::normalizeRadian(filletEndAngle);
        if (filletEndAngle < filletStartAngle) filletEndAngle += wy3d::TWO_PI;
        isFilletArcCCW = false;
    }

    // 结果
    std::shared_ptr<SketchFilletData> pFilletData = std::make_shared<SketchFilletData>();
    // 1.结果:圆弧
    {
        computeArcStartEndParam(filletPntOnCircle,
            center, radius, startAngle, endAngle, totalAngle,
            offsetR, !isFilletArcCCW, pFilletData->startParam1st, pFilletData->endParam1st);
    }
    // 2.结果:样条曲线
    {
        // 圆角在样条曲线连接处的切向量
        // 向量(cos(t), sin(t))逆时针旋转90度后为(-sin(t), cos(t))
        wy::Vector2 filletTangentDirOnSpline(
            -std::sin(angleOfPntOnSpline), std::cos(angleOfPntOnSpline));
        if (!isFilletArcCCW) // 顺时针(从直线段过渡到样条曲线的圆角)
        {
            filletTangentDirOnSpline *= -1;
        }

        // 样条曲线在圆角连接处的切向量
        gp_Pnt2d pnt2d;
        gp_Vec2d vec2d;
        pBSpline->D1(filletParamOnSpline, pnt2d, vec2d);
        wy::Vector2 splineDirAtJoint(vec2d.X(), vec2d.Y());
        splineDirAtJoint.normalize();

        // 确定样条曲线范围
        double firstParam = pBSpline->FirstParameter();
        double lastParam = pBSpline->LastParameter();
        if (splineDirAtJoint.dot(filletTangentDirOnSpline) > 0.0)
        {
            pFilletData->startParam2nd = (filletParamOnSpline - firstParam) / (lastParam - firstParam);
            pFilletData->endParam2nd = 1.0;
        }
        else
        {
            pFilletData->startParam2nd = 0.0;
            pFilletData->endParam2nd = (filletParamOnSpline - firstParam) / (lastParam - firstParam);
        }
    }
    // 3.结果:圆角
    pFilletData->filletCenter = filletCenter;
    pFilletData->filletRadius = R;
    pFilletData->filletStartAngle = filletStartAngle;
    pFilletData->filletEndAngle = filletEndAngle;

    return pFilletData;
}

std::shared_ptr<SketchFilletData> SketchFilletAlgo::filletSplineSpline(double R, double tol,
    Handle(Geom2d_BSplineCurve) pBSpline1st, const wy::Vector2& pickPosOnSpline1st,
    Handle(Geom2d_BSplineCurve) pBSpline2nd, const wy::Vector2& pickPosOnSpline2nd,
    bool isSecondPickPosMajor)
{
    try
    {
        return _filletSplineSpline(R, tol, pBSpline1st, pickPosOnSpline1st, pBSpline2nd, pickPosOnSpline2nd, isSecondPickPosMajor);
    }
    catch (const Standard_Failure&)
    {
        assert(false);
        return nullptr;
    }
    catch (...)
    {
        assert(false);
        return nullptr;
    }
}

std::shared_ptr<SketchFilletData> SketchFilletAlgo::_filletSplineSpline(double R, double tol,
    Handle(Geom2d_BSplineCurve) pBSpline1st, const wy::Vector2& pickPosOnSpline1st,
    Handle(Geom2d_BSplineCurve) pBSpline2nd, const wy::Vector2& pickPosOnSpline2nd,
    bool isSecondPickPosMajor)
{
    assert(R > 0.0);
    assert(tol > 0.0);

    if (R <= tol) return nullptr; // 圆角过小

    // 样条曲线1数据
    if (!pBSpline1st)
    {
        assert(false);
        return nullptr;
    }

    // 样条曲线2数据
    if (!pBSpline2nd)
    {
        assert(false);
        return nullptr;
    }

    // 确定B样条的偏移方向: 1st
    double bsplineOffset1st(R);
    {
        Geom2dAPI_ProjectPointOnCurve projector(gp_Pnt2d(pickPosOnSpline2nd.x(), pickPosOnSpline2nd.y()), pBSpline1st);
        if (projector.NbPoints() > 0)
        {
            double projectionParam = projector.LowerDistanceParameter();
            gp_Pnt2d pnt2d;
            gp_Vec2d vec2d;
            pBSpline1st->D1(projectionParam, pnt2d, vec2d);
            wy::Vector2 projPnt(pnt2d.X(), pnt2d.Y());
            wy::Vector2 dir(vec2d.X(), vec2d.Y());
            dir.normalize();
            if (dir.length() < 0.5)
            {
                assert(false);
                return nullptr;
            }
            wy::Vector2 normal(dir.y(), -dir.x()); // 法向方向(顺时针旋转90度,Geom2d_OffsetCurve以该方向的偏移值为正)
            if ((pickPosOnSpline2nd - projPnt).dot(normal) < 0.0)
            {
                bsplineOffset1st = -R;
            }
        }
        else
        {
            assert(false);
            return nullptr;
        }
    }

    // 确定B样条的偏移方向: 2nd
    double bsplineOffset2nd(R);
    {
        Geom2dAPI_ProjectPointOnCurve projector(gp_Pnt2d(pickPosOnSpline1st.x(), pickPosOnSpline1st.y()), pBSpline2nd);
        if (projector.NbPoints() > 0)
        {
            double projectionParam = projector.LowerDistanceParameter();
            gp_Pnt2d pnt2d;
            gp_Vec2d vec2d;
            pBSpline2nd->D1(projectionParam, pnt2d, vec2d);
            wy::Vector2 projPnt(pnt2d.X(), pnt2d.Y());
            wy::Vector2 dir(vec2d.X(), vec2d.Y());
            dir.normalize();
            if (dir.length() < 0.5)
            {
                assert(false);
                return nullptr;
            }
            wy::Vector2 normal(dir.y(), -dir.x()); // 法向方向(顺时针旋转90度,Geom2d_OffsetCurve以该方向的偏移值为正)
            if ((pickPosOnSpline1st - projPnt).dot(normal) < 0.0)
            {
                bsplineOffset2nd = -R;
            }
        }
        else
        {
            assert(false);
            return nullptr;
        }
    }

    // 求偏移样条曲线之间的交点
    Handle(Geom2d_Curve) pOffsetBSpline1st = new Geom2d_OffsetCurve(pBSpline1st, bsplineOffset1st);
    Handle(Geom2d_Curve) pOffsetBSpline2nd = new Geom2d_OffsetCurve(pBSpline2nd, bsplineOffset2nd);
    Geom2dAPI_InterCurveCurve intersector;
    intersector.Init(pOffsetBSpline1st, pOffsetBSpline2nd, tol);
    if (intersector.NbPoints() == 0)
    {
        return nullptr;
    }

    // 遍历交点确定圆角的圆心
    unsigned int numPnts = intersector.NbPoints();
    int index(-1);
    double minDistance(DBL_MAX);
    const wy::Vector2& refPickPos = isSecondPickPosMajor ? pickPosOnSpline2nd : pickPosOnSpline1st;
    for (int i = 1; i <= numPnts; i++)
    {
        // 获取交点
        gp_Pnt2d candidate2d = intersector.Point(i);
        wy::Vector2 candidate(candidate2d.X(), candidate2d.Y());

        // 交点在曲线上的参数
        double param1st(0.0), param2nd(0.0);
        const IntRes2d_IntersectionPoint& intPoint =
            intersector.Intersector().Point(i);
        param1st = intPoint.ParamOnFirst();
        param2nd = intPoint.ParamOnSecond();

        // 计算原始曲线上的点
        gp_Pnt2d origPoint2d;
        pBSpline1st->D0(param1st, origPoint2d);
        wy::Vector2 origPoint1st(origPoint2d.X(), origPoint2d.Y());
        pBSpline2nd->D0(param2nd, origPoint2d);
        wy::Vector2 origPoint2nd(origPoint2d.X(), origPoint2d.Y());

        // 校验:候选圆心到原始曲线上的点的距离是否为R
        if (std::fabs((origPoint1st - candidate).length() - R) > tol)
        {
            assert(false);
            continue;
        }
        if (std::fabs((origPoint2nd - candidate).length() - R) > tol)
        {
            assert(false);
            continue;
        }

        // 找出离参考拾取点最近的交点
        if (-1 == index)
        {
            index = i;
            minDistance = (candidate - refPickPos).length();
        }
        else
        {
            double distance = (candidate - refPickPos).length();
            if (distance < minDistance)
            {
                index = i;
                minDistance = distance;
            }
        }
    }
    if (-1 == index)
    {
        return nullptr;
    }

    // 圆角圆心
    gp_Pnt2d filletCenter2d = intersector.Point(index);
    wy::Vector2 filletCenter(filletCenter2d.X(), filletCenter2d.Y());

    // 圆角在圆上的点 + 圆角在样条曲线上的点
    wy::Vector2 filletPntOnSpline1st;
    double filletParamOnSpline1st(0.0);
    wy::Vector2 filletPntOnSpline2nd;
    double filletParamOnSpline2nd(0.0);
    {
        // 交点在曲线上的参数
        const IntRes2d_IntersectionPoint& intPoint =
            intersector.Intersector().Point(index);
        filletParamOnSpline1st = intPoint.ParamOnFirst();
        filletParamOnSpline2nd = intPoint.ParamOnSecond();

        // 计算原始曲线上的点
        gp_Pnt2d origSplinePoint2d;
        pBSpline1st->D0(filletParamOnSpline1st, origSplinePoint2d);
        filletPntOnSpline1st.set(origSplinePoint2d.X(), origSplinePoint2d.Y());
        pBSpline2nd->D0(filletParamOnSpline2nd, origSplinePoint2d);
        filletPntOnSpline2nd.set(origSplinePoint2d.X(), origSplinePoint2d.Y());
    }

    // 确定圆角的起始角度和终止角度
    double angleOfPntOnSpline1st = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), filletPntOnSpline1st - filletCenter);
    double angleOfPntOnSpline2nd = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), filletPntOnSpline2nd - filletCenter);
    bool isFilletArcCCW(true); // 从圆角在样条曲线1st上的点以小于180度逆时针旋转到圆角在样条曲线2nd上的点
    double filletStartAngle = angleOfPntOnSpline1st;
    double filletEndAngle = angleOfPntOnSpline2nd;
    if (filletEndAngle < filletStartAngle) filletEndAngle += wy3d::TWO_PI;
    if ((filletEndAngle - filletStartAngle) > wy3d::PI) // 圆角的角度一定是小于180度,此时反转,表明是顺时针
    {
        std::swap(filletStartAngle, filletEndAngle);
        filletStartAngle = wy3d::normalizeRadian(filletStartAngle);
        filletEndAngle = wy3d::normalizeRadian(filletEndAngle);
        if (filletEndAngle < filletStartAngle) filletEndAngle += wy3d::TWO_PI;
        isFilletArcCCW = false;
    }

    // 结果
    std::shared_ptr<SketchFilletData> pFilletData = std::make_shared<SketchFilletData>();
    // 1.结果:样条曲线1st
    {
        // 圆角在样条曲线连接处的切向量
        // 向量(cos(t), sin(t))顺时针旋转90度后为(sin(t), -cos(t))
        wy::Vector2 filletTangentDirOnSpline(
            std::sin(angleOfPntOnSpline1st), -std::cos(angleOfPntOnSpline1st));
        if (!isFilletArcCCW)
        {
            filletTangentDirOnSpline *= -1;
        }

        // 样条曲线在圆角连接处的切向量
        gp_Pnt2d pnt2d;
        gp_Vec2d vec2d;
        pBSpline1st->D1(filletParamOnSpline1st, pnt2d, vec2d);
        wy::Vector2 splineDirAtJoint(vec2d.X(), vec2d.Y());
        splineDirAtJoint.normalize();

        // 确定样条曲线范围
        double firstParam = pBSpline1st->FirstParameter();
        double lastParam = pBSpline1st->LastParameter();
        if (splineDirAtJoint.dot(filletTangentDirOnSpline) > 0.0)
        {
            pFilletData->startParam1st = (filletParamOnSpline1st - firstParam) / (lastParam - firstParam);
            pFilletData->endParam1st = 1.0;
        }
        else
        {
            pFilletData->startParam1st = 0.0;
            pFilletData->endParam1st = (filletParamOnSpline1st - firstParam) / (lastParam - firstParam);
        }
    }
    // 2.结果:样条曲线2nd
    {
        // 圆角在样条曲线连接处的切向量
        // 向量(cos(t), sin(t))逆时针旋转90度后为(-sin(t), cos(t))
        wy::Vector2 filletTangentDirOnSpline(
            -std::sin(angleOfPntOnSpline2nd), std::cos(angleOfPntOnSpline2nd));
        if (!isFilletArcCCW) // 顺时针(从直线段过渡到样条曲线的圆角)
        {
            filletTangentDirOnSpline *= -1;
        }

        // 样条曲线在圆角连接处的切向量
        gp_Pnt2d pnt2d;
        gp_Vec2d vec2d;
        pBSpline2nd->D1(filletParamOnSpline2nd, pnt2d, vec2d);
        wy::Vector2 splineDirAtJoint(vec2d.X(), vec2d.Y());
        splineDirAtJoint.normalize();

        // 确定样条曲线范围
        double firstParam = pBSpline2nd->FirstParameter();
        double lastParam = pBSpline2nd->LastParameter();
        if (splineDirAtJoint.dot(filletTangentDirOnSpline) > 0.0)
        {
            pFilletData->startParam2nd = (filletParamOnSpline2nd - firstParam) / (lastParam - firstParam);
            pFilletData->endParam2nd = 1.0;
        }
        else
        {
            pFilletData->startParam2nd = 0.0;
            pFilletData->endParam2nd = (filletParamOnSpline2nd - firstParam) / (lastParam - firstParam);
        }
    }
    // 3.结果:圆角
    pFilletData->filletCenter = filletCenter;
    pFilletData->filletRadius = R;
    pFilletData->filletStartAngle = filletStartAngle;
    pFilletData->filletEndAngle = filletEndAngle;

    return pFilletData;
}

void SketchFilletAlgo::computeArcStartEndParam(const wy::Vector2& filletPnt,
    const wy::Vector2& center, double radius, double startAngle, double endAngle, double totalAngle,
    double offsetRadius, bool isFilletCCW, double& startParam, double& endParam)
{
    double angle = wy::Vector2::rotationAngle(wy::Vector2::kXAxis, filletPnt - center);
    if (angle < startAngle) angle += wy3d::TWO_PI;
    if (angle >= startAngle && angle <= endAngle) // 切点在圆弧上
    {
        // 在圆弧上为逆时针
        if ((isFilletCCW && offsetRadius < radius)      // 内切同向
            || (!isFilletCCW && offsetRadius > radius)) // 外切反向
        {
            startParam = (angle - startAngle) / totalAngle;
            endParam = 1.0;
        }
        else // 在圆弧上为顺时针
        {
            startParam = 0.0;
            endParam = (angle - startAngle) / totalAngle;
        }
    }
    else // 切点在圆弧外
    {
        // 在圆弧上为逆时针
        if ((isFilletCCW && offsetRadius < radius)      // 内切同向
            || (!isFilletCCW && offsetRadius > radius)) // 外切反向
        {
            if (angle > startAngle) angle -= wy3d::TWO_PI;
            assert(angle < startAngle);
            startParam = (angle - startAngle) / totalAngle;
            endParam = 1.0;
        }
        else // 在圆弧上为顺时针
        {
            startParam = 0.0;
            assert(angle > startAngle);
            endParam = (angle - startAngle) / totalAngle;
        }
    }
}