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

#ifndef WY3D_SKETCH_FILLET_ALGO_H
#define WY3D_SKETCH_FILLET_ALGO_H

#include <memory>
#include <wyVector2.h>
#include <wy3dVector2.h>
#include <wy3dDefs.h>
#include <Geom2d_BSplineCurve.hxx>

NS_WY3D_BEG

struct SketchFilletData
{
    // first curve
    double startParam1st;
    double endParam1st;

    // second curve
    double startParam2nd;
    double endParam2nd;

    // 圆角圆心
    wy::Vector2 filletCenter;
    // 圆角半径
    double filletRadius;
    // 圆角起始角度
    double filletStartAngle;
    // 圆角终止角度
    double filletEndAngle;
};

class WY3D_EXPORT SketchFilletAlgo
{
public:
    // line vs line
    static std::shared_ptr<SketchFilletData> filletLineLine(double R, double tol,
        const wy::Vector2& startPnt1st, const wy::Vector2& endPnt1st, const wy::Vector2& pickPosOnLine1st,
        const wy::Vector2& startPnt2nd, const wy::Vector2& endPnt2nd, const wy::Vector2& pickPosOnLine2nd);

    // line vs arc
    static std::shared_ptr<SketchFilletData> filletLineArc(double R, double tol,
        const wy::Vector2& lineStartPnt, const wy::Vector2& lineEndPnt, const wy::Vector2& pickPosOnLine,
        const wy::Vector2& arcCenter, double arcRadius, double arcStartAngle, double arcEndAngle, const wy::Vector2& pickPosOnArc,
        bool isSecondPickPosMajor = true);

    // line vs circle
    static std::shared_ptr<SketchFilletData> filletLineCircle(double R, double tol,
        const wy::Vector2& lineStartPnt, const wy::Vector2& lineEndPnt, const wy::Vector2& pickPosOnLine,
        const wy::Vector2& circleCenter, double circleRadius, const wy::Vector2& pickPosOnCircle,
        bool isSecondPickPosMajor = true);

    // line vs spline
    static std::shared_ptr<SketchFilletData> filletLineSpline(double R, double tol,
        const wy::Vector2& lineStartPnt, const wy::Vector2& lineEndPnt, const wy::Vector2& pickPosOnLine,
        Handle(Geom2d_BSplineCurve) pBSpline, const wy::Vector2& pickPosOnSpline,
        bool isSecondPickPosMajor = true);

    // circle vs circle
    static std::shared_ptr<SketchFilletData> filletCircleCircle(double R, double tol,
        const wy::Vector2& center1, double radius1, const wy::Vector2& pickPos1st,
        const wy::Vector2& center2, double radius2, const wy::Vector2& pickPos2nd);

    // circle vs arc
    static std::shared_ptr<SketchFilletData> filletCircleArc(double R, double tol,
        const wy::Vector2& center1, double radius1, const wy::Vector2& pickPos1,
        const wy::Vector2& center2, double radius2, double startAngle, double endAngle, const wy::Vector2& pickPos2,
        bool isSecondPickPosMajor = true);

    // circle vs spline
    static std::shared_ptr<SketchFilletData> filletCircleSpline(double R, double tol,
        const wy::Vector2& center, double radius, const wy::Vector2& pickPosOnCircle,
        Handle(Geom2d_BSplineCurve) pBSpline, const wy::Vector2& pickPosOnSpline,
        bool isSecondPickPosMajor = true);

    // arc vs arc
    static std::shared_ptr<SketchFilletData> filletArcArc(double R, double tol,
        const wy::Vector2& center1, double radius1, double startAngle1, double endAngle1, const wy::Vector2& pickPos1,
        const wy::Vector2& center2, double radius2, double startAngle2, double endAngle2, const wy::Vector2& pickPos2);

    // arc vs spline
    static std::shared_ptr<SketchFilletData> filletArcSpline(double R, double tol,
        const wy::Vector2& center, double radius, double startAngle, double endAngle, const wy::Vector2& pickPosOnArc,
        Handle(Geom2d_BSplineCurve) pBSpline, const wy::Vector2& pickPosOnSpline,
        bool isSecondPickPosMajor = true);

    // spline vs spline
    static std::shared_ptr<SketchFilletData> filletSplineSpline(double R, double tol,
        Handle(Geom2d_BSplineCurve) pBSpline1st, const wy::Vector2& pickPosOnSpline1st,
        Handle(Geom2d_BSplineCurve) pBSpline2nd, const wy::Vector2& pickPosOnSpline2nd,
        bool isSecondPickPosMajor = true);

private:
    static void computeArcStartEndParam(const wy::Vector2& filletPnt,
        const wy::Vector2& center, double radius, double startAngle, double endAngle, double totalAngle,
        double offsetRadius, bool isFilletCCW, double& startParam, double& endParam);

    static std::shared_ptr<SketchFilletData> _filletLineSpline(double R, double tol,
        const wy::Vector2& lineStartPnt, const wy::Vector2& lineEndPnt, const wy::Vector2& pickPosOnLine,
        Handle(Geom2d_BSplineCurve) pBSpline, const wy::Vector2& pickPosOnSpline,
        bool isSecondPickPosMajor = true);

    static std::shared_ptr<SketchFilletData> _filletCircleSpline(double R, double tol,
        const wy::Vector2& center, double radius, const wy::Vector2& pickPosOnCircle,
        Handle(Geom2d_BSplineCurve) pBSpline, const wy::Vector2& pickPosOnSpline,
        bool isSecondPickPosMajor = true);

    static std::shared_ptr<SketchFilletData> _filletArcSpline(double R, double tol,
        const wy::Vector2& center, double radius, double startAngle, double endAngle, const wy::Vector2& pickPosOnArc,
        Handle(Geom2d_BSplineCurve) pBSpline, const wy::Vector2& pickPosOnSpline,
        bool isSecondPickPosMajor = true);

    static std::shared_ptr<SketchFilletData> _filletSplineSpline(double R, double tol,
        Handle(Geom2d_BSplineCurve) pBSpline1st, const wy::Vector2& pickPosOnSpline1st,
        Handle(Geom2d_BSplineCurve) pBSpline2nd, const wy::Vector2& pickPosOnSpline2nd,
        bool isSecondPickPosMajor = true);
};

NS_WY3D_END

#endif // WY3D_SKETCH_FILLET_ALGO_H