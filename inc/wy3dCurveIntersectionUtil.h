#ifndef WYGE_CURVE_INTERSECTION_MERGED_H
#define WYGE_CURVE_INTERSECTION_MERGED_H

#include <vector>
#include <cmath>
#include <cassert>
#include <memory>
#include <algorithm>

#include <Geom2d_BSplineCurve.hxx>

#include <wyVector2.h>
#include <geom/wy3dCurve2.h>
#include <geom/wy3dLineSegment2.h>
#include <geom/wy3dArc2.h>
#include <geom/wy3dCircle2.h>

NS_WY3D_BEG

// 求解一元二次方程a*t^2 + b*t + c = 0
// 返回值为解的个数
WY3D_EXPORT unsigned int solveQuadratic(double a, double b, double c, double& t1, double& t2);

// 直线段vs直线段求交
// 有交点返回true否则返回false
WY3D_EXPORT bool intersectLinesegLineseg(
    const wy::Vector2& startPnt1, const wy::Vector2& endPnt1,
    const wy::Vector2& startPnt2, const wy::Vector2& endPnt2,
    wy::Vector2& outIntPnt);

// 直线vs直线求交
// 有交点返回true否则返回false
WY3D_EXPORT bool intersectLineLine(
    const wy::Vector2& startPnt1, const wy::Vector2& endPnt1,
    const wy::Vector2& startPnt2, const wy::Vector2& endPnt2,
    wy::Vector2& outIntPnt);

// 直线段与圆求交
// 返回值为交点的个数
WY3D_EXPORT unsigned int intersectLinesegCircle(
    const wy::Vector2& startPnt, const wy::Vector2& endPnt,
    const wy::Vector2& center, double radius,
    wy::Vector2& outIntPnt1, wy::Vector2& outIntPnt2);

// 直线与圆求交
// 返回值为交点的个数
WY3D_EXPORT unsigned int intersectLineCircle(
    const wy::Vector2& startPnt, const wy::Vector2& endPnt,
    const wy::Vector2& center, double radius,
    wy::Vector2& outIntPnt1, wy::Vector2& outIntPnt2);

// 直线段与圆弧求交
WY3D_EXPORT unsigned int intersectLinesegArc(
    const wy::Vector2& startPnt, const wy::Vector2& endPnt,
    const wy::Vector2& center, double radius, double startAngle, double endAngle,
    wy::Vector2& outIntPnt1, wy::Vector2& outIntPnt2);

// 直线段与椭圆求交
WY3D_EXPORT unsigned int intersectLinesegEllipse(
    const wy::Vector2& startPnt, const wy::Vector2& endPnt,
    const wy::Vector2& center, const wy::Vector2& majorAxis, double radiusRatio,
    wy::Vector2& outIntPnt1, wy::Vector2& outIntPnt2);

// 直线与椭圆求交
WY3D_EXPORT unsigned int intersectLineEllipse(
    const wy::Vector2& startPnt, const wy::Vector2& endPnt,
    const wy::Vector2& center, const wy::Vector2& majorAxis, double radiusRatio,
    wy::Vector2& outIntPnt1, wy::Vector2& outIntPnt2);

// 直线段与椭圆弧求交
WY3D_EXPORT unsigned int intersectLinesegEllipseArc(
    const wy::Vector2& startPnt, const wy::Vector2& endPnt,
    const wy::Vector2& center, const wy::Vector2& majorAxis, double radiusRatio, double startAngle, double endAngle,
    wy::Vector2& outIntPnt1, wy::Vector2& outIntPnt2);

// 直线段与样条曲线
WY3D_EXPORT unsigned int intersectLinesegSpline(
    const wy::Vector2& startPnt, const wy::Vector2& endPnt,
    Handle(Geom2d_BSplineCurve) pBSpline,
    std::vector<wy::Vector2>& intPnts);

// 直线与样条曲线求交
WY3D_EXPORT unsigned int intersectLineSpline(
    const wy::Vector2& startPnt, const wy::Vector2& endPnt,
    Handle(Geom2d_BSplineCurve) pBSpline,
    std::vector<wy::Vector2>& intPnts);

// 圆与圆求交
WY3D_EXPORT unsigned int intersectCircleCircle(
    const wy::Vector2& center1, double radius1,
    const wy::Vector2& center2, double radius2,
    wy::Vector2& outIntPnt1, wy::Vector2& outIntPnt2);

// 圆与圆弧求交
WY3D_EXPORT unsigned int intersectCircleArc(
    const wy::Vector2& center1, double radius1,
    const wy::Vector2& center2, double radius2, double startAngle, double endAngle,
    wy::Vector2& outIntPnt1, wy::Vector2& outIntPnt2);

// 圆与椭圆求交
WY3D_EXPORT unsigned int intersectCircleEllipse(
    const wy::Vector2& center, double radius,
    const wy::Vector2& centerEllipse, const wy::Vector2& majorAxis, double radiusRatio,
    wy::Vector2& outIntPnt1, wy::Vector2& outIntPnt2, wy::Vector2& outIntPnt3, wy::Vector2& outIntPnt4);

// 圆与椭圆弧求交
WY3D_EXPORT unsigned int intersectCircleEllipseArc(
    const wy::Vector2& center, double radius,
    const wy::Vector2& centerEllipse, const wy::Vector2& majorAxis, double radiusRatio, double startAngle, double endAngle,
    wy::Vector2& outIntPnt1, wy::Vector2& outIntPnt2, wy::Vector2& outIntPnt3, wy::Vector2& outIntPnt4);

// 圆与样条曲线求交
WY3D_EXPORT unsigned int intersectCircleSpline(
    const wy::Vector2& center, double radius,
    Handle(Geom2d_BSplineCurve) pBSpline,
    std::vector<wy::Vector2>& intPnts);

// 圆弧与圆弧求交
WY3D_EXPORT unsigned int intersectArcArc(
    const wy::Vector2& center1, double radius1, double startAngle1, double endAngle1,
    const wy::Vector2& center2, double radius2, double startAngle2, double endAngle2,
    wy::Vector2& outIntPnt1, wy::Vector2& outIntPnt2);

// 圆弧与椭圆求交
WY3D_EXPORT unsigned int intersectArcEllipse(
    const wy::Vector2& center, double radius, double startAngle, double endAngle,
    const wy::Vector2& centerEllipse, const wy::Vector2& majorAxis, double radiusRatio,
    wy::Vector2& outIntPnt1, wy::Vector2& outIntPnt2, wy::Vector2& outIntPnt3, wy::Vector2& outIntPnt4);

// 圆弧与椭圆弧求交
WY3D_EXPORT unsigned int intersectArcEllipseArc(
    const wy::Vector2& center, double radius, double startAngle, double endAngle,
    const wy::Vector2& centerEllipse, const wy::Vector2& majorAxis, double radiusRatio, double startAngleEllipse, double endAngleEllipse,
    wy::Vector2& outIntPnt1, wy::Vector2& outIntPnt2, wy::Vector2& outIntPnt3, wy::Vector2& outIntPnt4);

// 圆弧与样条曲线求交
WY3D_EXPORT unsigned int intersectArcSpline(
    const wy::Vector2& center, double radius, double startAngle, double endAngle,
    Handle(Geom2d_BSplineCurve) pBSpline,
    std::vector<wy::Vector2>& intPnts);

// 椭圆与椭圆求交
WY3D_EXPORT unsigned int intersectEllipseEllipse(
    const wy::Vector2& center1, const wy::Vector2& majorAxis1, double radiusRatio1,
    const wy::Vector2& center2, const wy::Vector2& majorAxis2, double radiusRatio2,
    wy::Vector2& outIntPnt1, wy::Vector2& outIntPnt2, wy::Vector2& outIntPnt3, wy::Vector2& outIntPnt4);

// 椭圆与椭圆弧求交
WY3D_EXPORT unsigned int intersectEllipseEllipseArc(
    const wy::Vector2& center1, const wy::Vector2& majorAxis1, double radiusRatio1,
    const wy::Vector2& center2, const wy::Vector2& majorAxis2, double radiusRatio2, double startAngle, double endAngle,
    wy::Vector2& outIntPnt1, wy::Vector2& outIntPnt2, wy::Vector2& outIntPnt3, wy::Vector2& outIntPnt4);

// 椭圆与样条曲线求交
WY3D_EXPORT unsigned int intersectEllipseSpline(
    const wy::Vector2& center, const wy::Vector2& majorAxis, double radiusRatio,
    Handle(Geom2d_BSplineCurve) pBSpline,
    std::vector<wy::Vector2>& intPnts);

// 椭圆弧与椭圆弧求交
WY3D_EXPORT unsigned int intersectEllipseArcEllipseArc(
    const wy::Vector2& center1, const wy::Vector2& majorAxis1, double radiusRatio1, double startAngle1, double endAngle1,
    const wy::Vector2& center2, const wy::Vector2& majorAxis2, double radiusRatio2, double startAngle2, double endAngle2,
    wy::Vector2& outIntPnt1, wy::Vector2& outIntPnt2, wy::Vector2& outIntPnt3, wy::Vector2& outIntPnt4);

// 椭圆弧与样条曲线求交
WY3D_EXPORT unsigned int intersectEllipseArcSpline(
    const wy::Vector2& center, const wy::Vector2& majorAxis, double radiusRatio, double startAngle, double endAngle,
    Handle(Geom2d_BSplineCurve) pBSpline,
    std::vector<wy::Vector2>& intPnts);

// 样条曲线与样条曲线求交
WY3D_EXPORT unsigned int intersectSplineSpline(
    Handle(Geom2d_BSplineCurve) pBSpline1,
    Handle(Geom2d_BSplineCurve) pBSpline2,
    std::vector<wy::Vector2>& intPnts);

// 直线vs射线求交
// 有交点返回true否则返回false
WY3D_EXPORT bool intersectLineRayLine(
    const wy::Vector2& startPnt1, const wy::Vector2& endPnt1,
    const wy::Vector2& startPnt2, const wy::Vector2& endPnt2, // 射线的起点和另一个点
    wy::Vector2& outIntPnt);

// 射线vs射线求交
// 有交点返回true否则返回false
WY3D_EXPORT bool intersectRayLineRayLine(
    const wy::Vector2& startPnt1, const wy::Vector2& endPnt1, // 射线的起点和另一个点
    const wy::Vector2& startPnt2, const wy::Vector2& endPnt2, // 射线的起点和另一个点
    wy::Vector2& outIntPnt);

// 圆与射线求交
WY3D_EXPORT unsigned int intersectCircleRayLine(
    const wy::Vector2& center1, double radius1,
    const wy::Vector2& rayLineStart, const wy::Vector2& rayLineEnd, // 射线的起点和另一个点
    std::vector<wy::Vector2>& intPnts);

// 椭圆与射线求交
WY3D_EXPORT unsigned int intersectEllipseRayLine(
    const wy::Vector2& center, const wy::Vector2& majorAxis, double radiusRatio,
    const wy::Vector2& rayLineStart, const wy::Vector2& rayLineEnd, // 射线的起点和另一个点
    std::vector<wy::Vector2>& intPnts);

// 样条曲线与射线求交
WY3D_EXPORT unsigned int intersectSplineRayLine(
    Handle(Geom2d_BSplineCurve) pBSpline,
    const wy::Vector2& rayLineStart, const wy::Vector2& rayLineEnd, // 射线的起点和另一个点
    std::vector<wy::Vector2>& intPnts);

WY3D_EXPORT bool isAngleInArc(double angle, double startAngle, double endAngle, double tol = wy3d::EPS);

NS_WY3D_END

#endif // WY3D_CURVE_INTERSECTION_MERGED_H
