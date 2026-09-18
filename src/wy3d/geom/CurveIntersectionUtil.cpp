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

#include <wyVector2.h>
#include <utils/wy3dCurveIntersectionUtil.h>
#include <cmath>
#include <unordered_set>

#include <gp_Circ2d.hxx>
#include <gp_Elips2d.hxx>
#include <gp_Ax22d.hxx>
#include <gp_Pnt2d.hxx>
#include <IntAna2d_AnaIntersection.hxx>
#include <IntAna2d_IntPoint.hxx>
#include <Geom2d_Line.hxx>
#include <Geom2d_Circle.hxx>
#include <Geom2d_Ellipse.hxx>
#include <Geom2d_TrimmedCurve.hxx>
#include <Geom2dAPI_InterCurveCurve.hxx>

NS_WY3D_BEG

unsigned int solveQuadratic(double a, double b, double c, double& t1, double& t2)
{
    // 退化为一元一次方程(a = 0)
    if (std::fabs(a) < EPS)
    {
        // 处理b=0的情况
        if (std::fabs(b) < EPS)
        {
            return 0; // 0x + 0 = 0 视为无解（或无穷解，按需修改）
        }

        // 一元一次方程 bx + c = 0
        t1 = -c / b;
        return 1;
    }

    // 计算判别式
    double disc = b * b - 4 * a * c;

    // 无实根
    if (disc < -EPS)
    {
        return 0;
    }
    else if (disc < EPS)
    {
        t1 = -b / (2 * a); // 更稳定的重根计算公式
        return 1;
    }
    else
    {
        double sqrt_disc = std::sqrt(disc);

        // 计算分母（优化除法次数）
        double denom = 2 * a;

        // 根据b的正负选择更稳定的根计算方式
        if (b > 0) {
            t1 = (-b - sqrt_disc) / denom;
            t2 = c / (a * t1);
        }
        else {
            t2 = (-b + sqrt_disc) / denom;
            t1 = c / (a * t2);
        }

        // 确保 t1 <= t2
        if (t1 > t2) std::swap(t1, t2);

        return 2;
    }
}

bool intersectLinesegLineseg(
    const wy::Vector2& startPnt1, const wy::Vector2& endPnt1,
    const wy::Vector2& startPnt2, const wy::Vector2& endPnt2,
    wy::Vector2& outIntPnt)
{
    wy::Vector2 r = endPnt1 - startPnt1;
    wy::Vector2 s = endPnt2 - startPnt2;
    double rxs = r.cross(s);
    if (std::fabs(rxs) <= wy3d::EPS)
        return false;

    wy::Vector2 pq = startPnt2 - startPnt1;
    double t = pq.cross(s) / rxs;
    double u = pq.cross(r) / rxs;
    if ((t >= -wy3d::EPS) && (t <= 1.0 + wy3d::EPS) && (u >= -wy3d::EPS) && (u <= 1.0 + wy3d::EPS))
    {
        t = std::clamp(t, 0.0, 1.0);
        outIntPnt = startPnt1 + r * t;
        return true;
    }
    else
    {
        return false;
    }
}

bool intersectLineLine(
    const wy::Vector2& startPnt1, const wy::Vector2& endPnt1,
    const wy::Vector2& startPnt2, const wy::Vector2& endPnt2,
    wy::Vector2& outIntPnt)
{
    wy::Vector2 r = endPnt1 - startPnt1;
    wy::Vector2 s = endPnt2 - startPnt2;
    double rxs = r.cross(s);
    if (std::fabs(rxs) <= wy3d::EPS) // 检查是否平行或重合（容差内）
        return false;

    wy::Vector2 pq = startPnt2 - startPnt1;
    double t = pq.cross(s) / rxs;
    double u = pq.cross(r) / rxs;
    outIntPnt = startPnt1 + r * t;
    return true;
}

bool intersectLineRayLine(
    const wy::Vector2& startPnt1, const wy::Vector2& endPnt1,
    const wy::Vector2& startPnt2, const wy::Vector2& endPnt2, // 射线的起点和另一个点
    wy::Vector2& outIntPnt)
{
    wy::Vector2 r = endPnt1 - startPnt1;
    wy::Vector2 s = endPnt2 - startPnt2;
    double rxs = r.cross(s);
    if (std::fabs(rxs) <= wy3d::EPS) // 检查是否平行或重合（容差内）
        return false;

    wy::Vector2 pq = startPnt2 - startPnt1;
    double t = pq.cross(s) / rxs;
    double u = pq.cross(r) / rxs;

    // 检查交点是否在射线上（u >= 0）
    if (u < 0)
        return false;

    outIntPnt = startPnt1 + r * t;
    return true;
}

bool intersectRayLineRayLine(
    const wy::Vector2& startPnt1, const wy::Vector2& endPnt1, // 射线的起点和另一个点
    const wy::Vector2& startPnt2, const wy::Vector2& endPnt2, // 射线的起点和另一个点
    wy::Vector2& outIntPnt)
{
    wy::Vector2 r = endPnt1 - startPnt1;
    wy::Vector2 s = endPnt2 - startPnt2;
    double rxs = r.cross(s);
    if (std::fabs(rxs) <= wy3d::EPS) // 检查是否平行或重合（容差内）
        return false;

    wy::Vector2 pq = startPnt2 - startPnt1;
    double t = pq.cross(s) / rxs;
    double u = pq.cross(r) / rxs;

    if (t < 0.0 || u < 0.0)
        return false;

    outIntPnt = startPnt1 + r * t;
    return true;
}

unsigned int intersectLinesegCircle(
    const wy::Vector2& startPnt, const wy::Vector2& endPnt,
    const wy::Vector2& center, double radius,
    wy::Vector2& outIntPnt1, wy::Vector2& outIntPnt2)
{
    wy::Vector2 d = endPnt - startPnt;
    wy::Vector2 f = startPnt - center;

    double a = d.dot(d);
    double b = 2 * f.dot(d);
    double c = f.dot(f) - radius * radius;

    double t1, t2;
    unsigned int numRoots = solveQuadratic(a, b, c, t1, t2);

    unsigned int numIntersections = 0;
    if (numRoots >= 1 && t1 >= -wy3d::EPS && t1 <= 1.0 + wy3d::EPS)
    {
        t1 = std::clamp(t1, 0.0, 1.0);
        outIntPnt1 = startPnt + d * t1;
        numIntersections++;
    }
    if (numRoots == 2 && t2 >= -wy3d::EPS && t2 <= 1.0 + wy3d::EPS)
    {
        t2 = std::clamp(t2, 0.0, 1.0);
        if (numIntersections == 1)
            outIntPnt2 = startPnt + d * t2;
        else
            outIntPnt1 = startPnt + d * t2;
        numIntersections++;
    }

    return numIntersections;
}

unsigned int intersectLineCircle(
    const wy::Vector2& startPnt, const wy::Vector2& endPnt,
    const wy::Vector2& center, double radius,
    wy::Vector2& outIntPnt1, wy::Vector2& outIntPnt2)
{
    wy::Vector2 d = endPnt - startPnt;
    wy::Vector2 f = startPnt - center;
    
    double a = d.dot(d);
    double b = 2 * f.dot(d);
    double c = f.dot(f) - radius * radius;
    
    double t1, t2;
    unsigned int numRoots = solveQuadratic(a, b, c, t1, t2);
    
    unsigned int numIntersections = 0;
    if (numRoots >= 1)
    {
        outIntPnt1 = startPnt + d * t1;
        numIntersections++;
    }
    if (numRoots == 2)
    {
        outIntPnt2 = startPnt + d * t2;
        numIntersections++;
    }
    
    return numIntersections;

    if (0)
    {
        try
        {
            // 圆
            gp_Ax22d axisCircle(gp_Pnt2d(center.x(), center.y()), gp_Dir2d(1.0, 0.0));
            gp_Circ2d circle(axisCircle, radius);
            Handle(Geom2d_Circle) geomCircle = new Geom2d_Circle(circle);

            // 直线段
            wy::Vector2 vec = endPnt - startPnt;
            Handle(Geom2d_Line) geomLine = new Geom2d_Line(gp_Pnt2d(startPnt.x(), startPnt.y()), gp_Dir2d(vec.x(), vec.y()));
            Handle(Geom2d_TrimmedCurve) geomLineseg = new Geom2d_TrimmedCurve(geomLine, 0.0, vec.length());

            // 椭圆求交
            Geom2dAPI_InterCurveCurve intersector;
            intersector.Init(geomLineseg, geomCircle, 1e-4); // 设置容差
            int numPoints = intersector.NbPoints();
            for (int i = 1; i <= numPoints; ++i)
            {
                gp_Pnt2d pnt = intersector.Point(i);
                if (1 == i) outIntPnt1.set(pnt.X(), pnt.Y());
                else if (2 == i) outIntPnt2.set(pnt.X(), pnt.Y());
                else
                {
                    assert(false);
                }
            }
            return numPoints;
        }
        catch (const Standard_Failure&)
        {
            assert(false);
            return 0;
        }
    }
}

unsigned int intersectCircleRayLine(
    const wy::Vector2& center, double radius,
    const wy::Vector2& rayLineStart, const wy::Vector2& rayLineEnd, // 射线的起点和另一个点
    std::vector<wy::Vector2>& intPnts)
{
    // 清空输出交点数组
    intPnts.clear();

    // 计算射线方向向量
    wy::Vector2 rayDir = rayLineEnd - rayLineStart;

    // 防止零长度射线
    double rayLength = rayDir.length();
    if (rayLength <= wy3d::EPS)
    {
        return 0; // 射线长度为零，无交点
    }
    rayDir.normalize();

    // 计算直线与圆的交点
    wy::Vector2 lineIntPnt1, lineIntPnt2;
    unsigned int numLineIntersections = intersectLineCircle(
        rayLineStart, rayLineEnd,
        center, radius,
        lineIntPnt1, lineIntPnt2);

    // 检查每个交点是否在射线上
    for (unsigned int i = 0; i < numLineIntersections; ++i)
    {
        const wy::Vector2& intPnt = (i == 0) ? lineIntPnt1 : lineIntPnt2;
        if ((intPnt - rayLineStart).dot(rayDir) >= -wy3d::EPS)
        {
            intPnts.emplace_back(intPnt);
        }
    }

    return intPnts.size();
}

unsigned int intersectEllipseRayLine(
    const wy::Vector2& center, const wy::Vector2& majorAxis, double radiusRatio,
    const wy::Vector2& rayLineStart, const wy::Vector2& rayLineEnd, // 射线的起点和另一个点
    std::vector<wy::Vector2>& intPnts)
{
    // 清空输出交点数组
    intPnts.clear();

    // 计算射线方向向量
    wy::Vector2 rayDir = rayLineEnd - rayLineStart;

    // 防止零长度射线
    double rayLength = rayDir.length();
    if (rayLength <= wy3d::EPS)
    {
        return 0; // 射线长度为零，无交点
    }
    rayDir.normalize();

    // 计算直线与椭圆的交点
    wy::Vector2 lineIntPnt1, lineIntPnt2;
    unsigned int numLineIntersections = intersectLineEllipse(
        rayLineStart, rayLineEnd,
        center, majorAxis, radiusRatio,
        lineIntPnt1, lineIntPnt2);

    // 检查每个交点是否在射线上
    for (unsigned int i = 0; i < numLineIntersections; ++i)
    {
        const wy::Vector2& intPnt = (i == 0) ? lineIntPnt1 : lineIntPnt2;
        if ((intPnt - rayLineStart).dot(rayDir) >= -wy3d::EPS)
        {
            intPnts.emplace_back(intPnt);
        }
    }

    return intPnts.size();
}

unsigned int intersectSplineRayLine(
    Handle(Geom2d_BSplineCurve) pBSpline,
    const wy::Vector2& rayLineStart, const wy::Vector2& rayLineEnd, // 射线的起点和另一个点
    std::vector<wy::Vector2>& intPnts)
{
    // 清空输出交点数组
    intPnts.clear();

    // 计算射线方向向量
    wy::Vector2 rayDir = rayLineEnd - rayLineStart;

    // 防止零长度射线
    double rayLength = rayDir.length();
    if (rayLength <= wy3d::EPS)
    {
        return 0; // 射线长度为零，无交点
    }
    rayDir.normalize();

    // 求出直线与样条曲线的交点
    std::vector<wy::Vector2> intPntsTemp;
    intPntsTemp.reserve(5);
    intersectLineSpline(rayLineStart, rayLineEnd, pBSpline, intPntsTemp);

    // 检查每个交点是否在射线上
    for (const wy::Vector2& intPnt : intPntsTemp)
    {
        if ((intPnt - rayLineStart).dot(rayDir) >= -wy3d::EPS)
        {
            intPnts.emplace_back(intPnt);
        }
    }

    return intPnts.size();
}

bool isAngleInArc(double angle, double startAngle, double endAngle, double tol)
{
    startAngle = wy3d::normalizeRadian(startAngle);
    endAngle = wy3d::normalizeRadian(endAngle);
    angle = wy3d::normalizeRadian(angle);
    if (endAngle < startAngle) // [startAngle, endAngle]跨越2PI
    {
        endAngle += wy3d::TWO_PI;
        if (angle >= startAngle - tol && angle <= endAngle + tol) return true;
        else return angle + wy3d::TWO_PI <= endAngle + tol;
    }
    else // [startAngle, endAngle]为[0,2PI)的子集
    {
        if (angle >= startAngle - tol && angle <= endAngle + tol) return true;
        else return angle - wy3d::TWO_PI >= startAngle - tol;
    }
}

unsigned int intersectLinesegArc(
    const wy::Vector2& startPnt, const wy::Vector2& endPnt,
    const wy::Vector2& center, double radius, double startAngle, double endAngle,
    wy::Vector2& outIntPnt1, wy::Vector2& outIntPnt2)
{
    wy::Vector2 d = endPnt - startPnt;
    wy::Vector2 f = startPnt - center;

    double a = d.dot(d);
    double b = 2 * f.dot(d);
    double c = f.dot(f) - radius * radius;

    double t1, t2;
    unsigned int numRoots = solveQuadratic(a, b, c, t1, t2);

    unsigned int numIntersections = 0;
    if (numRoots >= 1 && t1 >= -wy3d::EPS && t1 <= 1.0 + wy3d::EPS)
    {
        t1 = std::clamp(t1, 0.0, 1.0);
        wy::Vector2 intPnt = startPnt + d * t1;
        double angle = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), intPnt - center);
        if (isAngleInArc(angle, startAngle, endAngle))
        {
            outIntPnt1 = intPnt;
            numIntersections++;
        }
    }
    if (numRoots == 2 && t2 >= -wy3d::EPS && t2 <= 1.0 + wy3d::EPS)
    {
        t2 = std::clamp(t2, 0.0, 1.0);
        wy::Vector2 intPnt = startPnt + d * t2;
        double angle = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), intPnt - center);
        if (isAngleInArc(angle, startAngle, endAngle))
        {
            if (numIntersections == 1)
                outIntPnt2 = startPnt + d * t2;
            else
                outIntPnt1 = startPnt + d * t2;
            numIntersections++;
        }
    }

    return numIntersections;
}

unsigned int intersectLinesegEllipse(
    const wy::Vector2& startPnt, const wy::Vector2& endPnt,
    const wy::Vector2& center, const wy::Vector2& majorAxis, double radiusRatio,
    wy::Vector2& outIntPnt1, wy::Vector2& outIntPnt2)
{
    // 椭圆
    double a = majorAxis.length(); // 长轴半径
    double b = a * radiusRatio;    // 短轴半径
    if (a < wy3d::EPS || b < wy3d::EPS) return 0; // 椭圆退化

    // 直线段
    Vector2 lineVec = endPnt - startPnt;
    if (lineVec.length() < wy3d::EPS) return 0; // 直线段退化

    // 平移到椭圆圆心
    Vector2 newStart = startPnt - center; // 平移到椭圆圆心
    Vector2 newEnd = endPnt - center;     // 平移到椭圆圆心

    // 旋转到椭圆轴对齐坐标系
    double theta = std::atan2(majorAxis.y(), majorAxis.x()); // 椭圆主轴与X轴的夹角
    double cosT = std::cos(theta);
    double sinT = std::sin(theta);
    Vector2 rotStart(newStart.x() * cosT + newStart.y() * sinT, -newStart.x() * sinT + newStart.y() * cosT);
    Vector2 rotEnd(newEnd.x() * cosT + newEnd.y() * sinT, -newEnd.x() * sinT + newEnd.y() * cosT);
    Vector2 rotLineVec = rotEnd - rotStart;

    // 联立方程求解
    double dx = rotLineVec.x();
    double dy = rotLineVec.y();
    double x1 = rotStart.x();
    double y1 = rotStart.y();
    double axa = a * a;
    double bxb = b * b;

    double A = (dx * dx) / (axa)+(dy * dy) / (bxb);
    double B = 2 * ((dx * x1) / (axa)+(dy * y1) / (bxb));
    double C = (x1 * x1) / (axa)+(y1 * y1) / (bxb)-1;
    double discriminant = B * B - 4 * A * C;

    // 没有实根
    if (discriminant < -wy3d::EPS)
    {
        return 0;
    }
    // 只有一个实根
    else if (discriminant <= wy3d::EPS)
    {
        double t = (-B) / (2 * A);
        if (t >= -wy3d::EPS && t <= 1 + wy3d::EPS)
        {
            // 将计算的点转换回原始坐标系
            Vector2 point = rotStart + (rotEnd - rotStart) * t;
            outIntPnt1.set(
                point.x() * cosT - point.y() * sinT,
                point.x() * sinT + point.y() * cosT
            );
            outIntPnt1 += center;
            return 1;
        }
        else
        {
            return 0;
        }
    }
    // 两个实根
    else
    {
        unsigned int count(0);
        double sqrtD = std::sqrt(discriminant);
        double t1 = (-B + sqrtD) / (2 * A);
        if (t1 >= -wy3d::EPS && t1 <= 1 + wy3d::EPS)
        {
            // 将计算的点转换回原始坐标系
            Vector2 point = rotStart + (rotEnd - rotStart) * t1;
            outIntPnt1.set(
                point.x() * cosT - point.y() * sinT,
                point.x() * sinT + point.y() * cosT
            );
            outIntPnt1 += center;
            ++count;
        }
        double t2 = (-B - sqrtD) / (2 * A);
        if (t2 >= -wy3d::EPS && t2 <= 1 + wy3d::EPS)
        {
            // 将计算的点转换回原始坐标系
            Vector2 point = rotStart + (rotEnd - rotStart) * t2;
            Vector2 realPoint(
                point.x() * cosT - point.y() * sinT,
                point.x() * sinT + point.y() * cosT
            );
            realPoint += center;
            if (0 == count) outIntPnt1 = realPoint;
            else outIntPnt2 = realPoint;
            ++count;
        }

        return count;
    }
}

unsigned int intersectLineEllipse(
    const wy::Vector2& startPnt, const wy::Vector2& endPnt,
    const wy::Vector2& center, const wy::Vector2& majorAxis, double radiusRatio,
    wy::Vector2& outIntPnt1, wy::Vector2& outIntPnt2)
{
    // 椭圆
    double a = majorAxis.length(); // 长轴半径
    double b = a * radiusRatio;    // 短轴半径
    if (a < wy3d::EPS || b < wy3d::EPS) return 0; // 椭圆退化

    // 直线段
    Vector2 lineVec = endPnt - startPnt;
    if (lineVec.length() < wy3d::EPS) return 0; // 直线段退化

    // 平移到椭圆圆心
    Vector2 newStart = startPnt - center; // 平移到椭圆圆心
    Vector2 newEnd = endPnt - center;     // 平移到椭圆圆心

    // 旋转到椭圆轴对齐坐标系
    double theta = std::atan2(majorAxis.y(), majorAxis.x()); // 椭圆主轴与X轴的夹角
    double cosT = std::cos(theta);
    double sinT = std::sin(theta);
    Vector2 rotStart(newStart.x() * cosT + newStart.y() * sinT, -newStart.x() * sinT + newStart.y() * cosT);
    Vector2 rotEnd(newEnd.x() * cosT + newEnd.y() * sinT, -newEnd.x() * sinT + newEnd.y() * cosT);
    Vector2 rotLineVec = rotEnd - rotStart;

    // 联立方程求解
    double dx = rotLineVec.x();
    double dy = rotLineVec.y();
    double x1 = rotStart.x();
    double y1 = rotStart.y();
    double axa = a * a;
    double bxb = b * b;

    double A = (dx * dx) / (axa)+(dy * dy) / (bxb);
    double B = 2 * ((dx * x1) / (axa)+(dy * y1) / (bxb));
    double C = (x1 * x1) / (axa)+(y1 * y1) / (bxb)-1;
    double discriminant = B * B - 4 * A * C;

    // 没有实根
    if (discriminant < -wy3d::EPS)
    {
        return 0;
    }
    // 只有一个实根
    else if (discriminant <= wy3d::EPS)
    {
        double t = (-B) / (2 * A);
        // 将计算的点转换回原始坐标系
        Vector2 point = rotStart + (rotEnd - rotStart) * t;
        outIntPnt1.set(
            point.x() * cosT - point.y() * sinT,
            point.x() * sinT + point.y() * cosT
        );
        outIntPnt1 += center;
        return 1;
    }
    // 两个实根
    else
    {
        double sqrtD = std::sqrt(discriminant);
        // 第一个交点
        double t1 = (-B + sqrtD) / (2 * A);
        // 将计算的点转换回原始坐标系
        Vector2 point = rotStart + (rotEnd - rotStart) * t1;
        outIntPnt1.set(
            point.x() * cosT - point.y() * sinT,
            point.x() * sinT + point.y() * cosT
        );
        outIntPnt1 += center;

        // 第二个交点
        double t2 = (-B - sqrtD) / (2 * A);
        // 将计算的点转换回原始坐标系
        point = rotStart + (rotEnd - rotStart) * t2;
        outIntPnt2.set(
            point.x() * cosT - point.y() * sinT,
            point.x() * sinT + point.y() * cosT
        );
        outIntPnt2 += center;

        return 2;
    }
}

unsigned int intersectLinesegEllipseArc(
    const wy::Vector2& startPnt, const wy::Vector2& endPnt,
    const wy::Vector2& center, const wy::Vector2& majorAxis, double radiusRatio, double startAngle, double endAngle,
    wy::Vector2& outIntPnt1, wy::Vector2& outIntPnt2)
{
    wy::Vector2 intPnt1;
    wy::Vector2 intPnt2;
    unsigned int num = intersectLinesegEllipse(startPnt, endPnt, center, majorAxis, radiusRatio, intPnt1, intPnt2);
    if (0 == num)
    {
        return 0;
    }

    unsigned int outNum = 0;
    if (num >= 1)
    {
        double angle = wy::Vector2::rotationAngle(majorAxis, intPnt1 - center);
        if (wy3d::isAngleInArc(angle, startAngle, endAngle))
        {
            outIntPnt1 = intPnt1;
            ++outNum;
        }
    }

    if (num == 2)
    {
        double angle = wy::Vector2::rotationAngle(majorAxis, intPnt2 - center);
        if (wy3d::isAngleInArc(angle, startAngle, endAngle))
        {
            if (0 == outNum) outIntPnt1 = intPnt2;
            else outIntPnt2 = intPnt2;
            ++outNum;
        }
    }
    return outNum;
}

unsigned int intersectLinesegSpline(
    const wy::Vector2& startPnt, const wy::Vector2& endPnt,
    Handle(Geom2d_BSplineCurve) pBSpline,
    std::vector<wy::Vector2>& intPnts)
{
    try
    {
        // 样条曲线
        if (!pBSpline)
        {
            assert(false);
            return 0;
        }

        // 直线段
        wy::Vector2 lineVec = endPnt - startPnt;
        double lineLen = lineVec.length();
        if (lineLen <= wy3d::EPS)
        {
            return 0;
        }
        Handle(Geom2d_Line) pLine = new Geom2d_Line(gp_Pnt2d(startPnt.x(), startPnt.y()), gp_Dir2d(lineVec.x(), lineVec.y()));
        Handle(Geom2d_TrimmedCurve) pLineseg = new Geom2d_TrimmedCurve(pLine, 0.0, lineLen);

        // 求交
        Geom2dAPI_InterCurveCurve intersector;
        intersector.Init(pLineseg, pBSpline, 1e-5); // 设置容差
        int numPoints = intersector.NbPoints();
        for (int i = 1; i <= numPoints; ++i)
        {
            gp_Pnt2d pnt = intersector.Point(i);
            intPnts.emplace_back(wy::Vector2(pnt.X(), pnt.Y()));
        }
        return numPoints;
    }
    catch (const Standard_Failure&)
    {
        assert(false);
        return 0;
    }
}

unsigned int intersectLineSpline(
    const wy::Vector2& startPnt, const wy::Vector2& endPnt,
    Handle(Geom2d_BSplineCurve) pBSpline,
    std::vector<wy::Vector2>& intPnts)
{
    try
    {
        // 样条曲线
        if (!pBSpline)
        {
            assert(false);
            return 0;
        }

        // 直线段
        wy::Vector2 lineVec = endPnt - startPnt;
        double lineLen = lineVec.length();
        if (lineLen <= wy3d::EPS)
        {
            return 0;
        }
        Handle(Geom2d_Line) pLine = new Geom2d_Line(gp_Pnt2d(startPnt.x(), startPnt.y()), gp_Dir2d(lineVec.x(), lineVec.y()));

        // 求交
        Geom2dAPI_InterCurveCurve intersector;
        intersector.Init(pLine, pBSpline, 1e-5); // 设置容差
        int numPoints = intersector.NbPoints();
        for (int i = 1; i <= numPoints; ++i)
        {
            gp_Pnt2d pnt = intersector.Point(i);
            intPnts.emplace_back(wy::Vector2(pnt.X(), pnt.Y()));
        }
        return numPoints;
    }
    catch (const Standard_Failure&)
    {
        assert(false);
        return 0;
    }
}

// 圆与圆求交
unsigned int intersectCircleCircle(
    const wy::Vector2& center1, double radius1,
    const wy::Vector2& center2, double radius2,
    wy::Vector2& outIntPnt1, wy::Vector2& outIntPnt2)
{
    // 计算两圆心之间的向量
    wy::Vector2 dVec = center2 - center1;
    // 计算两圆心之间的距离
    double d = dVec.length();

    // 情况1：两圆相离或内含（不包括内切），无交点
    // 情况2：两圆重合或几乎重合，有无数个交点，这里视为无有效交点
    if (d > (radius1 + radius2) || d < std::fabs(radius1 - radius2) || d < wy3d::EPS)
    {
        return 0;
    }

    // 情况3：两圆外切或内切，有一个交点
    if (std::fabs(d - (radius1 + radius2)) < wy3d::EPS || std::fabs(d - std::fabs(radius1 - radius2)) < wy3d::EPS) {
        // 计算交点位置
        double ratio = radius1 / (radius1 + radius2);
        outIntPnt1 = center1 + dVec * ratio;
        return 1;
    }

    // 情况4：两圆相交，有两个交点
    // 计算辅助变量
    double a = (radius1 * radius1 - radius2 * radius2 + d * d) / (2 * d);
    double h = std::sqrt(radius1 * radius1 - a * a);

    // 计算交点连线的中点
    wy::Vector2 p2 = center1 + dVec * (a / d);

    // 计算垂直于两圆心连线的单位向量
    wy::Vector2 perp(-dVec.y(), dVec.x());
    perp.normalize();

    // 计算两个交点
    outIntPnt1 = p2 + perp * h;
    outIntPnt2 = p2 - perp * h;

    return 2;
}

// 圆与圆弧求交
unsigned int intersectCircleArc(
    const wy::Vector2& center1, double radius1,
    const wy::Vector2& center2, double radius2, double startAngle, double endAngle,
    wy::Vector2& outIntPnt1, wy::Vector2& outIntPnt2)
{
    if (1)
    {
        // 计算两圆心之间的向量
        wy::Vector2 dVec = center2 - center1;
        // 计算两圆心之间的距离
        double d = dVec.length();

        // 两圆心几乎重合
        if (d <= wy3d::EPS)
        {
            return 0;
        }

        // 不相交
        if (d > (radius1 + radius2 + wy3d::EPS) || d < std::fabs(radius1 - radius2) - wy3d::EPS)
        {
            return 0;
        }

        // 外切
        if (std::fabs(d - (radius1 + radius2)) <= wy3d::EPS)
        {
            // 计算交点位置
            double ratio = radius1 / (radius1 + radius2);
            wy::Vector2 intPnt = center1 + dVec * ratio;
            double angle = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), intPnt - center2);
            if (isAngleInArc(angle, startAngle, endAngle))
            {
                outIntPnt1 = intPnt;
                return 1;
            }
            else
            {
                return 0;
            }
        }
        // 内切
        else if (std::fabs(d - std::fabs(radius1 - radius2)) <= wy3d::EPS)
        {
            wy::Vector2 intPnt;
            if (radius1 < radius2)
            {
                intPnt = center1 + (center1 - center2) * radius1 / d;
            }
            else
            {
                intPnt = center2 + (center2 - center1) * radius2 / d;
            }
            double angle = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), intPnt - center2);
            if (isAngleInArc(angle, startAngle, endAngle))
            {
                outIntPnt1 = intPnt;
                return 1;
            }
            else
            {
                return 0;
            }
        }

        // 情况4：两圆相交，有两个交点
        // 计算辅助变量
        double a = (radius1 * radius1 - radius2 * radius2 + d * d) / (2 * d);
        double h = std::sqrt(radius1 * radius1 - a * a);

        // 计算交点连线的中点
        wy::Vector2 p2 = center1 + dVec * (a / d);

        // 计算垂直于两圆心连线的单位向量
        wy::Vector2 perp(-dVec.y(), dVec.x());
        perp.normalize();

        // 计算两个交点
        unsigned int num(0);
        wy::Vector2 intPnt1 = p2 + perp * h;
        double angle1 = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), intPnt1 - center2);
        if (isAngleInArc(angle1, startAngle, endAngle))
        {
            outIntPnt1 = intPnt1;
            ++num;
        }
        wy::Vector2 intPnt2 = p2 - perp * h;
        double angle2 = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), intPnt2 - center2);
        if (isAngleInArc(angle2, startAngle, endAngle))
        {
            if (0 == num) outIntPnt1 = intPnt2;
            else outIntPnt2 = intPnt2;
            ++num;
        }

        return num;
    }
    else
    {
        try
        {
            // 圆
            gp_Ax22d axisCircle1(gp_Pnt2d(center1.x(), center1.y()), gp_Dir2d(1.0, 0.0));
            gp_Circ2d circle1(axisCircle1, radius1);
            Handle(Geom2d_Circle) geomCircle1 = new Geom2d_Circle(circle1);

            // 圆弧
            gp_Ax22d axisCircle2(gp_Pnt2d(center2.x(), center2.y()), gp_Dir2d(1.0, 0.0));
            gp_Circ2d circle2(axisCircle2, radius2);
            Handle(Geom2d_Circle) geomCircle2 = new Geom2d_Circle(circle2);
            Handle(Geom2d_TrimmedCurve) arc = new Geom2d_TrimmedCurve(geomCircle2, startAngle, endAngle);

            // 椭圆求交
            Geom2dAPI_InterCurveCurve intersector;
            intersector.Init(geomCircle1, arc, 1e-4); // 设置容差
            int numPoints = intersector.NbPoints();
            for (int i = 1; i <= numPoints; ++i)
            {
                gp_Pnt2d pnt = intersector.Point(i);
                if (1 == i) outIntPnt1.set(pnt.X(), pnt.Y());
                else if (2 == i) outIntPnt2.set(pnt.X(), pnt.Y());
                else
                {
                    assert(false);
                }
            }
            return numPoints;
        }
        catch (const Standard_Failure&)
        {
            assert(false);
            return 0;
        }
    }
}

unsigned int intersectCircleEllipse(
    const wy::Vector2& center, double radius,
    const wy::Vector2& centerEllipse, const wy::Vector2& majorAxis, double radiusRatio,
    wy::Vector2& outIntPnt1, wy::Vector2& outIntPnt2, wy::Vector2& outIntPnt3, wy::Vector2& outIntPnt4)
{
    // 圆退化或椭圆退化
    double a = majorAxis.length();
    if (radius < wy3d::EPS || a < wy3d::EPS || a * radiusRatio < wy3d::EPS)
    {
        return 0;
    }

    if (0)
    {
        //double radius_squared = radius * radius;
        //double a_squared = a * a;
        //const double theta = std::atan2(majorAxis.y(), majorAxis.x());
        //const double cos_t = std::cos(theta);
        //const double sin_t = std::sin(theta);
        //const double b = a * radiusRatio;
        //const double b_squared = b * b;

        //// 平移圆心到椭圆坐标系
        //const double dx = center.x() - centerEllipse.x();
        //const double dy = center.y() - centerEllipse.y();

        //// 旋转到椭圆局部坐标系
        //const double cx_prime = dx * cos_t + dy * sin_t;
        //const double cy_prime = -dx * sin_t + dy * cos_t;

        //// 构建四次方程
        //const double A = 1.0 - b_squared / a_squared;
        //const double B = -2.0 * cx_prime;
        //const double C = cx_prime * cx_prime + cy_prime * cy_prime + b_squared - radius_squared;
        //const double D = 2.0 * b * cy_prime;
        //Eigen::VectorXd coeffs(5);
        //coeffs << C * C - D * D, 2 * B * C, 2 * A * C + B * B + (D * D) / a_squared, 2 * A * B, A* A;

        //// 解四次方程
        //Eigen::PolynomialSolver<double, 4> solver;
        //solver.compute(coeffs);
        //std::vector<double> real_roots;
        //real_roots.reserve(4);
        //solver.realRoots(real_roots);

        //// 去重：合并接近的根
        //if (!real_roots.empty())
        //{
        //    std::sort(real_roots.begin(), real_roots.end());
        //    auto last = std::unique(real_roots.begin(), real_roots.end(),
        //        [](double a, double b) { return std::abs(a - b) < 1e-6; });
        //    real_roots.erase(last, real_roots.end());
        //}

        //// 验证并收集交点
        //std::vector<wy::Vector2> intersections;
        //for (double x_prime : real_roots)
        //{
        //    if (std::fabs(x_prime) > a + wy3d::EPS) continue;

        //    // 计算椭圆上的 y 坐标
        //    double x_prime_sq = x_prime * x_prime;
        //    double y_sq = b_squared * (1.0 - x_prime_sq / a_squared);
        //    if (y_sq < -wy3d::EPS) continue;
        //    double y_prime = (y_sq < 0) ? 0.0 : std::sqrt(y_sq);

        //    // 检查两个可能的 y 值 (±y_prime)
        //    for (const double y_sign : {1.0, -1.0})
        //    {
        //        double y_current = y_prime * y_sign;

        //        // 验证是否满足圆方程
        //        double dx_prime = x_prime - cx_prime;
        //        double dy_prime = y_current - cy_prime;
        //        double lhs = dx_prime * dx_prime + dy_prime * dy_prime;
        //        if (std::fabs(lhs - radius_squared) > /*wy3d::EPS*/1e-5) continue;

        //        // 坐标逆变换
        //        double x = x_prime * cos_t - y_current * sin_t + centerEllipse.x();
        //        double y = x_prime * sin_t + y_current * cos_t + centerEllipse.y();
        //        wy::Vector2 p(x, y);

        //        // 去重 (几何距离判断)
        //        bool is_duplicate = false;
        //        for (const auto& pt : intersections)
        //        {
        //            const double dx = p.x() - pt.x();
        //            const double dy = p.y() - pt.y();
        //            if (dx * dx + dy * dy < (wy3d::EPS * wy3d::EPS)) { // 更严格的 1e-8 平方
        //                is_duplicate = true;
        //                break;
        //            }
        //        }
        //        if (!is_duplicate)
        //        {
        //            intersections.push_back(p);
        //        }
        //    }
        //}

        //// 输出结果
        //unsigned int num = intersections.size();
        //if (num >= 1) outIntPnt1 = intersections[0];
        //if (num >= 2) outIntPnt2 = intersections[1];
        //if (num >= 3) outIntPnt3 = intersections[2];
        //if (num >= 4) outIntPnt4 = intersections[3];
        //return num;
    }
    else
    {
        try
        {
            // 圆
            gp_Ax22d axisCircle(gp_Pnt2d(center.x(), center.y()), gp_Dir2d(1.0, 0.0));
            gp_Circ2d circle(axisCircle, radius);
            Handle(Geom2d_Circle) geomCircle = new Geom2d_Circle(circle);

            // 椭圆
            gp_Ax22d axisEllipse(gp_Pnt2d(centerEllipse.x(), centerEllipse.y()), gp_Dir2d(majorAxis.x(), majorAxis.y()));
            gp_Elips2d ellipse(axisEllipse, majorAxis.length(), majorAxis.length() * radiusRatio);
            Handle(Geom2d_Ellipse) geomEllipse = new Geom2d_Ellipse(ellipse);

            // 椭圆求交
            Geom2dAPI_InterCurveCurve intersector;
            intersector.Init(geomCircle, geomEllipse, 1e-6); // 设置容差
            int numPoints = intersector.NbPoints();
            for (int i = 1; i <= numPoints; ++i)
            {
                gp_Pnt2d pnt = intersector.Point(i);
                if (1 == i) outIntPnt1.set(pnt.X(), pnt.Y());
                else if (2 == i) outIntPnt2.set(pnt.X(), pnt.Y());
                else if (3 == i) outIntPnt3.set(pnt.X(), pnt.Y());
                else if (4 == i) outIntPnt4.set(pnt.X(), pnt.Y());
                else
                {
                    assert(false);
                    return 4;
                }
            }
            return numPoints;
        }
        catch (const Standard_Failure&)
        {
            assert(false);
            return 0;
        }
    }
}

unsigned int intersectCircleEllipseArc(
    const wy::Vector2& center, double radius,
    const wy::Vector2& centerEllipse, const wy::Vector2& majorAxis, double radiusRatio, double startAngle, double endAngle,
    wy::Vector2& outIntPnt1, wy::Vector2& outIntPnt2, wy::Vector2& outIntPnt3, wy::Vector2& outIntPnt4)
{
    wy::Vector2 intPnt1, intPnt2, intPnt3, intPnt4;
    unsigned int num = intersectCircleEllipse(center, radius, centerEllipse, majorAxis, radiusRatio, intPnt1, intPnt2, intPnt3, intPnt4);
    if (0 == num) return 0;

    unsigned int outNum = 0;
    if (num >= 1)
    {
        double angle = wy::Vector2::rotationAngle(majorAxis, intPnt1 - centerEllipse);
        if (isAngleInArc(angle, startAngle, endAngle))
        {
            outIntPnt1 = intPnt1;
            ++outNum;
        }
    }
    if (num >= 2)
    {
        double angle = wy::Vector2::rotationAngle(majorAxis, intPnt2 - centerEllipse);
        if (isAngleInArc(angle, startAngle, endAngle))
        {
            if (0 == outNum) outIntPnt1 = intPnt2;
            else outIntPnt2 = intPnt2;
            ++outNum;
        }
    }
    if (num >= 3)
    {
        double angle = wy::Vector2::rotationAngle(majorAxis, intPnt3 - centerEllipse);
        if (isAngleInArc(angle, startAngle, endAngle))
        {
            if (0 == outNum) outIntPnt1 = intPnt3;
            else if (1 == outNum) outIntPnt2 = intPnt3;
            else outIntPnt3 = intPnt3;
            ++outNum;
        }
    }
    if (num >= 4)
    {
        double angle = wy::Vector2::rotationAngle(majorAxis, intPnt4 - centerEllipse);
        if (isAngleInArc(angle, startAngle, endAngle))
        {
            if (0 == outNum) outIntPnt1 = intPnt4;
            else if (1 == outNum) outIntPnt2 = intPnt4;
            else if (2 == outNum) outIntPnt3 = intPnt4;
            else outIntPnt4 = intPnt4;
            ++outNum;
        }
    }
    return outNum;
}

unsigned int intersectCircleSpline(
    const wy::Vector2& center, double radius,
    Handle(Geom2d_BSplineCurve) pBSpline,
    std::vector<wy::Vector2>& intPnts)
{
    try
    {
        // 样条曲线
        if (!pBSpline)
        {
            assert(false);
            return 0;
        }

        // 圆
        gp_Ax22d axisCircle(gp_Pnt2d(center.x(), center.y()), gp_Dir2d(1.0, 0.0));
        gp_Circ2d circle(axisCircle, radius);
        Handle(Geom2d_Circle) pCircle = new Geom2d_Circle(circle);

        // 求交
        Geom2dAPI_InterCurveCurve intersector;
        intersector.Init(pCircle, pBSpline, 1e-5); // 设置容差
        int numPoints = intersector.NbPoints();
        intPnts.reserve(numPoints);
        for (int i = 1; i <= numPoints; ++i)
        {
            gp_Pnt2d pnt = intersector.Point(i);
            intPnts.emplace_back(wy::Vector2(pnt.X(), pnt.Y()));
        }
        return numPoints;
    }
    catch (const Standard_Failure&)
    {
        assert(false);
        return 0;
    }
}

unsigned int intersectArcArc(
    const wy::Vector2& center1, double radius1, double startAngle1, double endAngle1,
    const wy::Vector2& center2, double radius2, double startAngle2, double endAngle2,
    wy::Vector2& outIntPnt1, wy::Vector2& outIntPnt2)
{
    wy::Vector2 intPnt1, intPnt2;
    unsigned int num = intersectCircleCircle(center1, radius1, center2, radius2, intPnt1, intPnt2);

    unsigned int outNum = 0;
    if (num >= 1)
    {
        // 检查第一个交点是否在两个圆弧上
        double angle1 = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), intPnt1 - center1);
        double angle2 = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), intPnt1 - center2);
        if (isAngleInArc(angle1, startAngle1, endAngle1) && isAngleInArc(angle2, startAngle2, endAngle2))
        {
            outIntPnt1 = intPnt1;
            ++outNum;
        }
    }

    if (num == 2)
    {
        // 检查第二个交点是否在两个圆弧上
        double angle1 = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), intPnt2 - center1);
        double angle2 = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), intPnt2 - center2);
        if (isAngleInArc(angle1, startAngle1, endAngle1) && isAngleInArc(angle2, startAngle2, endAngle2))
        {
            if (0 == outNum) outIntPnt1 = intPnt2;
            else outIntPnt2 = intPnt2;
            ++outNum;
        }
    }

    return outNum;
}

unsigned int intersectArcEllipse(
    const wy::Vector2& center, double radius, double startAngle, double endAngle,
    const wy::Vector2& centerEllipse, const wy::Vector2& majorAxis, double radiusRatio,
    wy::Vector2& outIntPnt1, wy::Vector2& outIntPnt2, wy::Vector2& outIntPnt3, wy::Vector2& outIntPnt4)
{
    wy::Vector2 intPnt1, intPnt2, intPnt3, intPnt4;
    unsigned int num = intersectCircleEllipse(center, radius, centerEllipse, majorAxis, radiusRatio, intPnt1, intPnt2, intPnt3, intPnt4);
    if (0 == num) return 0;

    unsigned int outNum = 0;
    if (num >= 1)
    {
        double angle = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), intPnt1 - center);
        if (isAngleInArc(angle, startAngle, endAngle))
        {
            outIntPnt1 = intPnt1;
            ++outNum;
        }
    }
    if (num >= 2)
    {
        double angle = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), intPnt2 - center);
        if (isAngleInArc(angle, startAngle, endAngle))
        {
            if (0 == outNum) outIntPnt1 = intPnt2;
            else outIntPnt2 = intPnt2;
            ++outNum;
        }
    }
    if (num >= 3)
    {
        double angle = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), intPnt3 - center);
        if (isAngleInArc(angle, startAngle, endAngle))
        {
            if (0 == outNum) outIntPnt1 = intPnt3;
            else if (1 == outNum) outIntPnt2 = intPnt3;
            else outIntPnt3 = intPnt3;
            ++outNum;
        }
    }
    if (num >= 4)
    {
        double angle = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), intPnt4 - center);
        if (isAngleInArc(angle, startAngle, endAngle))
        {
            if (0 == outNum) outIntPnt1 = intPnt4;
            else if (1 == outNum) outIntPnt2 = intPnt4;
            else if (2 == outNum) outIntPnt3 = intPnt4;
            else outIntPnt4 = intPnt4;
            ++outNum;
        }
    }
    return outNum;
}

unsigned int intersectArcEllipseArc(
    const wy::Vector2& center, double radius, double startAngle, double endAngle,
    const wy::Vector2& centerEllipse, const wy::Vector2& majorAxis, double radiusRatio, double startAngleEllipse, double endAngleEllipse,
    wy::Vector2& outIntPnt1, wy::Vector2& outIntPnt2, wy::Vector2& outIntPnt3, wy::Vector2& outIntPnt4)
{
    wy::Vector2 intPnt1, intPnt2, intPnt3, intPnt4;
    unsigned int num = intersectCircleEllipse(center, radius, centerEllipse, majorAxis, radiusRatio, intPnt1, intPnt2, intPnt3, intPnt4);
    if (0 == num) return 0;

    unsigned int outNum = 0;
    if (num >= 1)
    {
        double angle1 = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), intPnt1 - center);
        double angle2 = wy::Vector2::rotationAngle(majorAxis, intPnt1 - centerEllipse);
        if (isAngleInArc(angle1, startAngle, endAngle) && isAngleInArc(angle2, startAngleEllipse, endAngleEllipse))
        {
            outIntPnt1 = intPnt1;
            ++outNum;
        }
    }
    if (num >= 2)
    {
        double angle1 = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), intPnt2 - center);
        double angle2 = wy::Vector2::rotationAngle(majorAxis, intPnt2 - centerEllipse);
        if (isAngleInArc(angle1, startAngle, endAngle) && isAngleInArc(angle2, startAngleEllipse, endAngleEllipse))
        {
            if (0 == outNum) outIntPnt1 = intPnt2;
            else outIntPnt2 = intPnt2;
            ++outNum;
        }
    }
    if (num >= 3)
    {
        double angle1 = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), intPnt3 - center);
        double angle2 = wy::Vector2::rotationAngle(majorAxis, intPnt3 - centerEllipse);
        if (isAngleInArc(angle1, startAngle, endAngle) && isAngleInArc(angle2, startAngleEllipse, endAngleEllipse))
        {
            if (0 == outNum) outIntPnt1 = intPnt3;
            else if (1 == outNum) outIntPnt2 = intPnt3;
            else outIntPnt3 = intPnt3;
            ++outNum;
        }
    }
    if (num >= 4)
    {
        double angle1 = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), intPnt4 - center);
        double angle2 = wy::Vector2::rotationAngle(majorAxis, intPnt4 - centerEllipse);
        if (isAngleInArc(angle1, startAngle, endAngle) && isAngleInArc(angle2, startAngleEllipse, endAngleEllipse))
        {
            if (0 == outNum) outIntPnt1 = intPnt4;
            else if (1 == outNum) outIntPnt2 = intPnt4;
            else if (2 == outNum) outIntPnt3 = intPnt4;
            else outIntPnt4 = intPnt4;
            ++outNum;
        }
    }
    return outNum;
}

unsigned int intersectArcSpline(
    const wy::Vector2& center, double radius, double startAngle, double endAngle,
    Handle(Geom2d_BSplineCurve) pBSpline,
    std::vector<wy::Vector2>& intPnts)
{
    std::vector<wy::Vector2> pnts;
    unsigned int num = intersectCircleSpline(center, radius, pBSpline, pnts);
    if (0 == num) return 0;

    assert(static_cast<unsigned int>(pnts.size()) == num);
    unsigned int count(0);
    for (unsigned int i = 0; i < num; ++i)
    {
        const wy::Vector2& pnt = pnts[i];
        if (isAngleInArc(wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), pnt - center),
            startAngle, endAngle))
        {
            intPnts.emplace_back(pnt);
            ++count;
        }
    }
    return count;
}

#ifdef ELLIPSE_ELLIPSE_INT_BY_SELF
// 使用联立两个椭圆方程的方式来求椭圆与椭圆的交点始终都不对
// 不得已使用OCC来实现
struct EllipseCoeffs {
    double A, B, C, D, E, F;
};

EllipseCoeffs computeEllipseCoeffs(const Vector2& center, const Vector2& majorAxis, double radiusRatio)
{
    double h = center.x();
    double k = center.y();
    double a = majorAxis.length();
    double b = a * radiusRatio;
    double theta = std::atan2(majorAxis.y(), majorAxis.x());
    double cosθ = std::cos(theta);
    double sinθ = std::sin(theta);
    double a2_inv = 1.0 / (a * a);
    double b2_inv = 1.0 / (b * b);

    EllipseCoeffs coeffs;
    coeffs.A = cosθ * cosθ * a2_inv + sinθ * sinθ * b2_inv;
    coeffs.B = 2 * cosθ * sinθ * (a2_inv - b2_inv);
    coeffs.C = sinθ * sinθ * a2_inv + cosθ * cosθ * b2_inv;

    double term = a2_inv - b2_inv;
    coeffs.D = -2 * cosθ * h * a2_inv + 2 * sinθ * k * b2_inv;
    coeffs.E = -2 * sinθ * h * a2_inv - 2 * cosθ * k * b2_inv;
    coeffs.F = h * h * a2_inv + k * k * b2_inv - 1.0;

    return coeffs;
}

unsigned int intersectEllipseEllipse(
    const wy::Vector2& center1, const wy::Vector2& majorAxis1, double ratio1,
    const wy::Vector2& center2, const wy::Vector2& majorAxis2, double ratio2,
    wy::Vector2& outIntPnt1, wy::Vector2& outIntPnt2, wy::Vector2& outIntPnt3, wy::Vector2& outIntPnt4)
{
    // 计算椭圆系数
    EllipseCoeffs e1 = computeEllipseCoeffs(center1, majorAxis1, ratio1);
    EllipseCoeffs e2 = computeEllipseCoeffs(center2, majorAxis2, ratio2);

    // 步骤2：构造消元方程
    // 通过组合消除y²项：λ*Eq1 - Eq2 = 0
    const double lambda = e2.C;
    const double mu = e1.C;

    const double A = e1.A * lambda - e2.A * mu;
    const double B = e1.B * lambda - e2.B * mu;
    const double D = e1.D * lambda - e2.D * mu;
    const double E = e1.E * lambda - e2.E * mu;
    const double F = e1.F * lambda - e2.F * mu;

    // 步骤3：构造四次方程
    // 消元方程形式：A x² + B xy + D x + E y + F = 0
    // 表达式为：y = (-A x² - D x - F) / (B x + E)
    // 代入椭圆1方程得到四次方程
    Eigen::VectorXd coeffs(5); // 从x^0到x^4

    // 详细展开项
    const double A2 = A * A;
    const double B2 = B * B;
    const double D2 = D * D;
    const double E2 = E * E;
    const double F2 = F * F;

    coeffs[4] = e1.C * A2 + e1.B * A * B + e1.A * B2; // x^4

    coeffs[3] = 2 * e1.C * A * D
        + e1.B * (A * E + B * D)
        + 2 * e1.A * B * E
        + e1.D * B2; // x^3

    coeffs[2] = e1.C * (D2 + 2 * A * F)
        + e1.B * (D * E + B * F)
        + e1.A * (E2 + 2 * B * F)
        + e1.E * B * E
        + e1.F * B2
        + e1.D * 2 * B * E; // x^2

    coeffs[1] = 2 * e1.C * D * F
        + e1.B * E * F
        + 2 * e1.A * E * F
        + e1.D * E2
        + 2 * e1.F * B * E
        + e1.E * E * D; // x^1

    coeffs[0] = e1.C * F2
        + e1.E * E * F
        + e1.F * E2; // 常数项

    // 步骤4：求解四次方程
    Eigen::PolynomialSolver<double, 4> solver;
    solver.compute(coeffs);

    std::vector<double> realRoots;
    solver.realRoots(realRoots);

    // 步骤5：验证解并收集交点
    std::vector<Vector2> intersections;
    for (double x : realRoots) {
        const double denominator = B * x + E;
        if (fabs(denominator) < 1e-10) continue;

        const double y = -(A * x * x + D * x + F) / denominator;

        // 严格验证在两个椭圆上的误差
        const double err1 = e1.A * x * x + e1.B * x * y + e1.C * y * y + e1.D * x + e1.E * y + e1.F;
        const double err2 = e2.A * x * x + e2.B * x * y + e2.C * y * y + e2.D * x + e2.E * y + e2.F;
        if (fabs(err1) < 1e-6 && fabs(err2) < 1e-6) {
            intersections.emplace_back(x, y);
        }
    }

    // 步骤6：去重处理
    std::sort(intersections.begin(), intersections.end(),
        [](const Vector2& a, const Vector2& b) {
            return (a.x() != b.x()) ? (a.x() < b.x()) : (a.y() < b.y());
        });

    auto last = std::unique(intersections.begin(), intersections.end(),
        [](const Vector2& a, const Vector2& b) {
            return (a - b).length2() < 1e-8;
        });
    intersections.erase(last, intersections.end());

    // 步骤7：输出结果
    const unsigned int count = intersections.size();
    if (count >= 1) outIntPnt1 = intersections[0];
    if (count >= 2) outIntPnt2 = intersections[1];
    if (count >= 3) outIntPnt3 = intersections[2];
    if (count >= 4) outIntPnt4 = intersections[3];

    return count;
}
#else
unsigned int intersectEllipseEllipse(
    const wy::Vector2& center1, const wy::Vector2& majorAxis1, double radiusRatio1,
    const wy::Vector2& center2, const wy::Vector2& majorAxis2, double radiusRatio2,
    wy::Vector2& outIntPnt1, wy::Vector2& outIntPnt2, wy::Vector2& outIntPnt3, wy::Vector2& outIntPnt4)
{
    double RA1 = majorAxis1.length();
    double RB1 = RA1 * radiusRatio1;
    double RA2 = majorAxis2.length();
    double RB2 = RA2 * radiusRatio2;

    // 椭圆退化的情况
    if (RA1 < wy3d::EPS || RB1 < wy3d::EPS || RA2 < wy3d::EPS || RB2 < wy3d::EPS)
    {
        return 0;
    }

    try
    {
        // 椭圆1
        gp_Ax22d axis1(gp_Pnt2d(center1.x(), center1.y()), gp_Dir2d(majorAxis1.x(), majorAxis1.y()));
        gp_Elips2d ellipse1(axis1, RA1, RB1);
        Handle(Geom2d_Ellipse) geomEllipse1 = new Geom2d_Ellipse(ellipse1);

        // 椭圆2
        gp_Ax22d axis2(gp_Pnt2d(center2.x(), center2.y()), gp_Dir2d(majorAxis2.x(), majorAxis2.y()));
        gp_Elips2d ellipse2(axis2, RA2, RB2);
        Handle(Geom2d_Ellipse) geomEllipse2 = new Geom2d_Ellipse(ellipse2);

        // 椭圆求交
        Geom2dAPI_InterCurveCurve intersector;
        intersector.Init(geomEllipse1, geomEllipse2, 1e-6); // 设置容差
        int numPoints = intersector.NbPoints();
        for (int i = 1; i <= numPoints; ++i)
        {
            gp_Pnt2d pnt = intersector.Point(i);
            if (1 == i) outIntPnt1.set(pnt.X(), pnt.Y());
            else if (2 == i) outIntPnt2.set(pnt.X(), pnt.Y());
            else if (3 == i) outIntPnt3.set(pnt.X(), pnt.Y());
            else if (4 == i) outIntPnt4.set(pnt.X(), pnt.Y());
            else
            {
                assert(false);
                return 4;
            }
        }
        return numPoints;
    }
    catch (const Standard_Failure&)
    {
        assert(false);
        return 0;
    }
}
#endif

unsigned int intersectEllipseEllipseArc(
    const wy::Vector2& center1, const wy::Vector2& majorAxis1, double radiusRatio1,
    const wy::Vector2& center2, const wy::Vector2& majorAxis2, double radiusRatio2, double startAngle, double endAngle,
    wy::Vector2& outIntPnt1, wy::Vector2& outIntPnt2, wy::Vector2& outIntPnt3, wy::Vector2& outIntPnt4)
{
    wy::Vector2 intPnt1, intPnt2, intPnt3, intPnt4;
    unsigned int num = intersectEllipseEllipse(center1, majorAxis1, radiusRatio1, center2, majorAxis2, radiusRatio2, intPnt1, intPnt2, intPnt3, intPnt4);
    if (0 == num) return 0;

    unsigned int outNum = 0;
    if (num >= 1)
    {
        double angle = wy::Vector2::rotationAngle(majorAxis2, intPnt1 - center2);
        if (isAngleInArc(angle, startAngle, endAngle))
        {
            outIntPnt1 = intPnt1;
            ++outNum;
        }
    }
    if (num >= 2)
    {
        double angle = wy::Vector2::rotationAngle(majorAxis2, intPnt2 - center2);
        if (isAngleInArc(angle, startAngle, endAngle))
        {
            if (0 == outNum) outIntPnt1 = intPnt2;
            else outIntPnt2 = intPnt2;
            ++outNum;
        }
    }
    if (num >= 3)
    {
        double angle = wy::Vector2::rotationAngle(majorAxis2, intPnt3 - center2);
        if (isAngleInArc(angle, startAngle, endAngle))
        {
            if (0 == outNum) outIntPnt1 = intPnt3;
            else if (1 == outNum) outIntPnt2 = intPnt3;
            else outIntPnt3 = intPnt3;
            ++outNum;
        }
    }
    if (num >= 4)
    {
        double angle = wy::Vector2::rotationAngle(majorAxis2, intPnt4 - center2);
        if (isAngleInArc(angle, startAngle, endAngle))
        {
            if (0 == outNum) outIntPnt1 = intPnt4;
            else if (1 == outNum) outIntPnt2 = intPnt4;
            else if (2 == outNum) outIntPnt3 = intPnt4;
            else outIntPnt4 = intPnt4;
            ++outNum;
        }
    }
    return outNum;
}

unsigned int intersectEllipseSpline(
    const wy::Vector2& center, const wy::Vector2& majorAxis, double radiusRatio,
    Handle(Geom2d_BSplineCurve) pBSpline,
    std::vector<wy::Vector2>& intPnts)
{
    double RA = majorAxis.length();
    double RB = RA * radiusRatio;

    // 椭圆退化的情况
    if (RA <= wy3d::EPS || RB <= wy3d::EPS)
    {
        return 0;
    }

    try
    {
        // 样条曲线
        if (!pBSpline)
        {
            assert(false);
            return 0;
        }

        // 椭圆
        gp_Ax22d axis(gp_Pnt2d(center.x(), center.y()), gp_Dir2d(majorAxis.x(), majorAxis.y()));
        gp_Elips2d ellipse(axis, RA, RB);
        Handle(Geom2d_Ellipse) pEllipse = new Geom2d_Ellipse(ellipse);

        // 求交
        Geom2dAPI_InterCurveCurve intersector;
        intersector.Init(pEllipse, pBSpline, 1e-5); // 设置容差
        int numPoints = intersector.NbPoints();
        intPnts.reserve(numPoints);
        for (int i = 1; i <= numPoints; ++i)
        {
            gp_Pnt2d pnt = intersector.Point(i);
            intPnts.emplace_back(wy::Vector2(pnt.X(), pnt.Y()));
        }
        return numPoints;
    }
    catch (const Standard_Failure&)
    {
        assert(false);
        return 0;
    }
}

unsigned int intersectEllipseArcEllipseArc(
    const wy::Vector2& center1, const wy::Vector2& majorAxis1, double radiusRatio1, double startAngle1, double endAngle1,
    const wy::Vector2& center2, const wy::Vector2& majorAxis2, double radiusRatio2, double startAngle2, double endAngle2,
    wy::Vector2& outIntPnt1, wy::Vector2& outIntPnt2, wy::Vector2& outIntPnt3, wy::Vector2& outIntPnt4)
{
    wy::Vector2 intPnt1, intPnt2, intPnt3, intPnt4;
    unsigned int num = intersectEllipseEllipse(center1, majorAxis1, radiusRatio1, center2, majorAxis2, radiusRatio2, intPnt1, intPnt2, intPnt3, intPnt4);
    if (0 == num) return 0;

    unsigned int outNum = 0;
    if (num >= 1)
    {
        double angle1 = wy::Vector2::rotationAngle(majorAxis1, intPnt1 - center1);
        double angle2 = wy::Vector2::rotationAngle(majorAxis2, intPnt1 - center2);
        if (isAngleInArc(angle1, startAngle1, endAngle1) && isAngleInArc(angle2, startAngle2, endAngle2))
        {
            outIntPnt1 = intPnt1;
            ++outNum;
        }
    }
    if (num >= 2)
    {
        double angle1 = wy::Vector2::rotationAngle(majorAxis1, intPnt2 - center1);
        double angle2 = wy::Vector2::rotationAngle(majorAxis2, intPnt2 - center2);
        if (isAngleInArc(angle1, startAngle1, endAngle1) && isAngleInArc(angle2, startAngle2, endAngle2))
        {
            if (0 == outNum) outIntPnt1 = intPnt2;
            else outIntPnt2 = intPnt2;
            ++outNum;
        }
    }
    if (num >= 3)
    {
        double angle1 = wy::Vector2::rotationAngle(majorAxis1, intPnt3 - center1);
        double angle2 = wy::Vector2::rotationAngle(majorAxis2, intPnt3 - center2);
        if (isAngleInArc(angle1, startAngle1, endAngle1) && isAngleInArc(angle2, startAngle2, endAngle2))
        {
            if (0 == outNum) outIntPnt1 = intPnt3;
            else if (1 == outNum) outIntPnt2 = intPnt3;
            else outIntPnt3 = intPnt3;
            ++outNum;
        }
    }
    if (num >= 4)
    {
        double angle1 = wy::Vector2::rotationAngle(majorAxis1, intPnt4 - center1);
        double angle2 = wy::Vector2::rotationAngle(majorAxis2, intPnt4 - center2);
        if (isAngleInArc(angle1, startAngle1, endAngle1) && isAngleInArc(angle2, startAngle2, endAngle2))
        {
            if (0 == outNum) outIntPnt1 = intPnt4;
            else if (1 == outNum) outIntPnt2 = intPnt4;
            else if (2 == outNum) outIntPnt3 = intPnt4;
            else outIntPnt4 = intPnt4;
            ++outNum;
        }
    }
    return outNum;
}

unsigned int intersectEllipseArcSpline(
    const wy::Vector2& center, const wy::Vector2& majorAxis, double radiusRatio, double startAngle, double endAngle,
    Handle(Geom2d_BSplineCurve) pBSpline,
    std::vector<wy::Vector2>& intPnts)
{
    std::vector<wy::Vector2> pnts;
    unsigned int num = intersectEllipseSpline(center, majorAxis, radiusRatio, pBSpline, pnts);
    if (0 == num) return 0;

    assert(static_cast<unsigned int>(pnts.size()) == num);
    unsigned int count(0);
    for (unsigned int i = 0; i < num; ++i)
    {
        const wy::Vector2& pnt = pnts[i];
        if (isAngleInArc(wy::Vector2::rotationAngle(majorAxis, pnt - center),
            startAngle, endAngle))
        {
            intPnts.emplace_back(pnt);
            ++count;
        }
    }
    return count;
}

unsigned int intersectSplineSpline(
    Handle(Geom2d_BSplineCurve) pBSpline1,
    Handle(Geom2d_BSplineCurve) pBSpline2,
    std::vector<wy::Vector2>& intPnts)
{
    try
    {
        // 样条曲线
        if (!pBSpline1 || !pBSpline2)
        {
            assert(false);
            return 0;
        }

        // 求交
        Geom2dAPI_InterCurveCurve intersector;
        intersector.Init(pBSpline1, pBSpline2, 1e-5); // 设置容差
        int numPoints = intersector.NbPoints();
        intPnts.reserve(numPoints);
        for (int i = 1; i <= numPoints; ++i)
        {
            gp_Pnt2d pnt = intersector.Point(i);
            intPnts.emplace_back(wy::Vector2(pnt.X(), pnt.Y()));
        }
        return numPoints;
    }
    catch (const Standard_Failure&)
    {
        assert(false);
        return 0;
    }
}

NS_WY3D_END