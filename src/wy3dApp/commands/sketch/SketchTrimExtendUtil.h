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

#ifndef WY3DAPP_SKETCH_TRIM_EXTEND_UTIL_H
#define WY3DAPP_SKETCH_TRIM_EXTEND_UTIL_H

#include <cassert>
#include <Geom2dAPI_ProjectPointOnCurve.hxx>
#include <Geom2d_BSplineCurve.hxx>
#include <wyVector2.h>
#include <wy3dVector2.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>
#include <wy3dSketchEllipse.h>
#include <wy3dSketchEllipseArc.h>
#include <wy3dSketchSpline.h>
#include <wy3dSketchSpline.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class SketchTrimExtendUtil
{
public:
    static inline double reviseT(double t, double TOL = 1e-7)
    {
        if (std::fabs(t) <= TOL) return 0.0;
        else if (std::fabs(t - 1.0) <= TOL) return 1.0;
        else return t;
    }

    static inline double reviseAngle(double angle, double startAngle, double endAngle, double TOL = 1e-7)
    {
        assert(angle >= 0.0 && angle < wy3d::TWO_PI);
        assert(startAngle >= 0.0 && startAngle < wy3d::TWO_PI);
        assert(endAngle > startAngle);
        assert((endAngle - startAngle) < wy3d::TWO_PI);

        if (angle < startAngle) angle += wy3d::TWO_PI;

        if ((angle - startAngle) <= TOL || (angle - startAngle) >= wy3d::TWO_PI - TOL)
        {
            return startAngle;
        }
        else if (std::fabs(angle - endAngle) <= TOL)
        {
            return endAngle;
        }
        else
        {
            return angle;
        }
    }

    static inline double getParamOfLine(const wy::Vector2& startPnt, const wy::Vector2& endPnt, const wy::Vector2& pos, double TOL = 1e-7)
    {
        wy::Vector2 lineVec = endPnt - startPnt;
        double lineLength = lineVec.length();
        if (lineLength <= TOL) return DBL_MAX;
        wy::Vector2 vec = pos - startPnt;
        double t = vec.length() / lineLength;
        if (vec.dot(lineVec) < 0) t = -t;
        return reviseT(t);
    }

    static inline double getParamOfLine(const wy3d::SketchLine* pSketchLine, const wy::Vector2& pos, double TOL = 1e-7)
    {
        return getParamOfLine(pSketchLine->getStartPoint(), pSketchLine->getEndPoint(), pos, TOL);
    }

    static inline double getParamOfCenterLine(const wy3d::SketchCenterLine* pSketchCenterLine, const wy::Vector2& pos, double TOL = 1e-7)
    {
        return getParamOfLine(pSketchCenterLine->getStartPoint(), pSketchCenterLine->getEndPoint(), pos, TOL);
    }

    static inline double getParamOfCircle(const wy3d::SketchCircle* pSketchCircle, const wy::Vector2& pos)
    {
        return wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), pos - pSketchCircle->getCenter()) / wy3d::TWO_PI;
    }

    static inline double getParamOfArc(const wy3d::SketchArc* pSketchArc, const wy::Vector2& pos)
    {
        double totalAngle = pSketchArc->getTotalAngle();
        double startAngle = wy3d::normalizeRadian(pSketchArc->getStartAngle());
        double endAngle = startAngle + totalAngle;
        double angle = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), pos - pSketchArc->getCenter());
        angle = reviseAngle(angle, startAngle, endAngle);
        return (angle - startAngle) / totalAngle;
    }

    static inline double getParamOfEllipse(const wy3d::SketchEllipse* pSketchEllipse, const wy::Vector2& pos)
    {
        return wy::Vector2::rotationAngle(pSketchEllipse->getMajorAxis(), pos - pSketchEllipse->getCenter()) / wy3d::TWO_PI;
    }

    static inline double getParamOfEllipseArc(const wy3d::SketchEllipseArc* pSketchEllipseArc, const wy::Vector2& pos)
    {
        double totalAngle = pSketchEllipseArc->getTotalAngle();
        double startAngle = wy3d::normalizeRadian(pSketchEllipseArc->getStartAngle());
        double endAngle = startAngle + totalAngle;
        double angle = wy::Vector2::rotationAngle(pSketchEllipseArc->getMajorAxis(), pos - pSketchEllipseArc->getCenter());
        angle = reviseAngle(angle, startAngle, endAngle);
        return (angle - startAngle) / totalAngle;
    }

    static inline double getParamOfSpline(const wy3d::SketchSpline *pSketchSpline, const wy::Vector2& pos, double TOL = 1e-7)
    {        Handle(Geom2d_BSplineCurve) pBSpline = pSketchSpline->getOccSpline();
        if (pBSpline.IsNull()) return DBL_MAX;
        
        double firstParam = pBSpline->FirstParameter();
        double lastParam = pBSpline->LastParameter();
        double paramRange = lastParam - firstParam;
        if (paramRange <= TOL) return DBL_MAX;

        wy::Vector2 startPnt, startDir;
        wy::Vector2 endPnt, endDir;
        if (!SketchTrimExtendUtil::getBSplineInfo(pBSpline, startPnt, startDir, endPnt, endDir))
        {
            return DBL_MAX;
        }

        Geom2dAPI_ProjectPointOnCurve projector(gp_Pnt2d(pos.x(), pos.y()), pBSpline);
        if (projector.NbPoints() > 0 && projector.LowerDistance() <= 1e-5)
        {
            // 对于样条曲线而言还需要判断距离是否在容差内
            // 比如:样条曲线和直线段在样条曲线的中间处相交,在修剪掉样条曲线的[0.5,1.0]段之后;
            // 原来样条曲线的终点投影到现在的样条曲线上可能为0.97在有效范围内,但这是不合理的,会导致后续的assert.
            double t = (projector.LowerDistanceParameter() - firstParam) / paramRange;
            return reviseT(t);
        }
        else
        {
            wy::Vector2 startVec = pos - startPnt;
            wy::Vector2 endVec = pos - endPnt;
            if (std::fabs(startVec.cross(startDir)) <= TOL && startVec.dot(startDir) < 0.0)
            {
                return startVec.dot(startDir);
            }
            else if (std::fabs(endVec.cross(endDir)) <= TOL && endVec.dot(endDir) > 0.0)
            {
                return endVec.dot(endDir) + 1.0;
            }
            else
            {
                return DBL_MAX;
            }
        }
    }

    // 获取样条曲线的pick参数
    // 前提:已经pick到样条曲线了
    static inline double getPickParamOfSpline(const wy3d::SketchSpline& sketchSpline, const wy::Vector2& pos, double TOL = 1e-7)
    {
        Handle(Geom2d_BSplineCurve) pBSpline = sketchSpline.getOccSpline();
        if (pBSpline.IsNull())
        {
            assert(false);
            return DBL_MAX;
        }

        double firstParam = pBSpline->FirstParameter();
        double lastParam = pBSpline->LastParameter();
        double paramRange = lastParam - firstParam;
        if (paramRange <= TOL)
        {
            assert(false);
            return DBL_MAX;
        }

        Geom2dAPI_ProjectPointOnCurve projector(gp_Pnt2d(pos.x(), pos.y()), pBSpline);
        if (projector.NbPoints() > 0)
        {
            double t = (projector.LowerDistanceParameter() - firstParam) / paramRange;
            assert(t >= 0.0 && t <= 1.0);
            return std::clamp(t, 0.0, 1.0);
        }
        else
        {
            assert(false);
            return DBL_MAX;
        }
    }

    static inline bool getBSplineInfo(Handle(Geom2d_BSplineCurve) pBSpline,
        wy::Vector2& startPnt, wy::Vector2& startDir,
        wy::Vector2& endPnt, wy::Vector2& endDir)
    {
        if (!pBSpline)
        {
            assert(false);
            return false;
        }

        try
        {
            gp_Pnt2d startPnt2d;
            gp_Vec2d startDir2d;
            pBSpline->D1(pBSpline->FirstParameter(), startPnt2d, startDir2d);
            startDir2d.Normalize();

            gp_Pnt2d endPnt2d;
            gp_Vec2d endDir2d;
            pBSpline->D1(pBSpline->LastParameter(), endPnt2d, endDir2d);
            endDir2d.Normalize();

            startPnt.set(startPnt2d.X(), startPnt2d.Y());
            startDir.set(startDir2d.X(), startDir2d.Y());
            endPnt.set(endPnt2d.X(), endPnt2d.Y());
            endDir.set(endDir2d.X(), endDir2d.Y());
            return true;
        }
        catch (const Standard_Failure&)
        {
            return false;
        }
    }
};

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_TRIM_EXTEND_UTIL_H
