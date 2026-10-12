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

#include "utils/SplineUtil.h"
#include <cassert>
#include <wyVector2.h>
#include <wyVector3.h>
#include <wy3dSketchSpline.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

bool SplineUtil::segment(const wy3d::SketchSpline* pSketchSpline, double startParam, double endParam,
    unsigned int& degree,
    std::vector<wy::Vector2>& controlPoints,
    std::vector<double>& knots,
    std::vector<unsigned int>& multiplicities)
{
    if (!pSketchSpline)
    {
        assert(false);
        return false;
    }
    if (startParam < 0.0 || startParam > 1.0 || endParam < 0.0 || endParam > 1.0 || startParam >= endParam)
    {
        assert(false);
        return false;
    }    Handle(Geom2d_BSplineCurve) pBSpline = pSketchSpline->getOccSpline();
    if (!pBSpline)
    {
        assert(false);
        return false;
    }

    try
    {
        double UMin = pBSpline->FirstParameter();
        double UMax = pBSpline->LastParameter();
        double U1 = std::clamp(UMin + startParam * (UMax - UMin), UMin, UMax);
        double U2 = std::clamp(UMin + endParam * (UMax - UMin), UMin, UMax);
        Handle(Geom2d_BSplineCurve) pResultBSpline =
            Handle(Geom2d_BSplineCurve)::DownCast(pBSpline->Copy());
        pResultBSpline->Segment(U1, U2); // 截取样条曲线

        // 输出
        // <1>次数
        degree = pResultBSpline->Degree();
        // <2>控制点
        int numPoles = pResultBSpline->NbPoles();
        controlPoints.reserve(numPoles);
        for (int i = 1; i <= numPoles; ++i)
        {
            const gp_Pnt2d& pnt2d = pResultBSpline->Pole(i);
            controlPoints.emplace_back(wy::Vector2(pnt2d.X(), pnt2d.Y()));
        }
        // 节点
        int numKnots = pResultBSpline->NbKnots();
        knots.reserve(numKnots);
        for (int i = 1; i <= numKnots; ++i)
        {
            knots.emplace_back(pResultBSpline->Knot(i));
        }
        // 重数
        const TColStd_Array1OfInteger& mults = pResultBSpline->Multiplicities();
        multiplicities.reserve(mults.Size());
        for (int i = 1; i <= mults.Size(); ++i)
        {
            multiplicities.emplace_back(mults.Value(i));
        }

        return true;
    }
    catch (const Standard_Failure&)
    {
        assert(false);
        return false;
    }
}

Handle(Geom2d_BSplineCurve) SplineUtil::addLineSegmentToBSpline(
    const Handle(Geom2d_BSplineCurve)& origCurve,
    const wy::Vector2& newPoint,
    const bool atStart)
{
    if (!origCurve)
    {
        assert(false);
        return nullptr;
    }

    try
    {
        //---------------------------------
        // 1.获取原始B样条数据
        //---------------------------------
        // 次数
        const Standard_Integer origDegree = origCurve->Degree();
        // 控制点
        const Standard_Integer origNbPoles = origCurve->NbPoles();
        TColgp_Array1OfPnt2d origPoles(1, origNbPoles);
        origCurve->Poles(origPoles);
        // 节点
        const Standard_Integer origNbKnots = origCurve->NbKnots();
        TColStd_Array1OfReal origKnots(1, origNbKnots);
        origCurve->Knots(origKnots);
        // 重数
        TColStd_Array1OfInteger origMults(1, origNbKnots);
        origCurve->Multiplicities(origMults);
        // 校验
        if (origNbPoles <= 0 || origNbKnots <= 0)
        {
            assert(false);
            return nullptr;
        }

        //---------------------------------
        // 2.创建直线段B样条
        //---------------------------------
        // 1次B样条
        Handle(Geom2d_BSplineCurve) lineCurve;
        {
            // 控制点
            TColgp_Array1OfPnt2d linePoles(1, 2);
            if (atStart)
            {
                linePoles(1) = gp_Pnt2d(newPoint.x(), newPoint.y());
                linePoles(2) = origPoles(1);
            }
            else
            {
                linePoles(1) = origPoles(origNbPoles);
                linePoles(2) = gp_Pnt2d(newPoint.x(), newPoint.y());
            }
            // 节点
            TColStd_Array1OfReal lineKnots(1, 2);
            lineKnots(1) = 0.0;
            lineKnots(2) = 1.0;
            // 重数
            TColStd_Array1OfInteger lineMults(1, 2);
            lineMults(1) = 2;
            lineMults(2) = 2;
            // 直线段B样条
            lineCurve = new Geom2d_BSplineCurve(linePoles, lineKnots, lineMults, 1);
        }
        lineCurve->IncreaseDegree(origDegree); // 提升到原始曲线阶数

        //---------------------------------
        // 3.获取提升后的直线段数据
        //---------------------------------
        // 控制点
        const Standard_Integer lineNbPoles = lineCurve->NbPoles();
        TColgp_Array1OfPnt2d linePoles(1, lineNbPoles);
        lineCurve->Poles(linePoles);
        // 节点
        const Standard_Integer lineNbKnots = lineCurve->NbKnots();
        TColStd_Array1OfReal lineKnots(1, lineNbKnots);
        lineCurve->Knots(lineKnots);
        // 重数
        TColStd_Array1OfInteger lineMults(1, lineNbKnots);
        lineCurve->Multiplicities(lineMults);
        // 校验
        if (lineNbPoles <= 0 || lineNbKnots <= 0)
        {
            assert(false);
            return nullptr;
        }

        //---------------------------------
        // 4.计算合并后样条曲线的数据
        //---------------------------------
        // 控制点计算
        Standard_Integer newNbPoles = lineNbPoles + origNbPoles - 1;
        TColgp_Array1OfPnt2d newPoles(1, newNbPoles);
        if (atStart)
        {
            for (Standard_Integer i = 1; i <= lineNbPoles; i++)
            {
                newPoles(i) = linePoles(i);
            }
            for (Standard_Integer i = 2; i <= origNbPoles; i++)
            {
                newPoles(lineNbPoles + i - 1) = origPoles(i);
            }
        }
        else
        {
            for (Standard_Integer i = 1; i <= origNbPoles - 1; i++)
            {
                newPoles(i) = origPoles(i);
            }
            for (Standard_Integer i = 1; i <= lineNbPoles; i++)
            {
                newPoles(origNbPoles - 1 + i) = linePoles(i);
            }
        }

        // 计算节点偏移量
        Standard_Real knotOffset(0.0);
        if (atStart) {
            knotOffset = lineKnots(lineNbKnots) - origKnots(1);
        }
        else {
            knotOffset = origKnots(origNbKnots) - lineKnots(1);
        }

        // 计算新节点数量：全部节点合并（不跳过任何节点）
        Standard_Integer newNbKnots = lineNbKnots + origNbKnots - 1;
        TColStd_Array1OfReal newKnots(1, newNbKnots);
        TColStd_Array1OfInteger newMults(1, newNbKnots);

        if (atStart)
        {
            // 起点添加：所有直线段节点 + 原始曲线节点(偏移并跳过第一个)
            for (Standard_Integer i = 1; i <= lineNbKnots; i++)
            {
                newKnots(i) = lineKnots(i);
                newMults(i) = lineMults(i);
            }

            // 连接点处重数减1（避免重复）
            newMults(lineNbKnots) = lineMults(lineNbKnots) - 1;

            for (Standard_Integer i = 2; i <= origNbKnots; i++)
            {
                Standard_Integer idx = lineNbKnots + i - 1;
                newKnots(idx) = origKnots(i) + knotOffset;
                newMults(idx) = origMults(i);
            }
        }
        else
        {
            // 终点添加：所有原始曲线节点 + 直线段节点(偏移并跳过第一个)
            for (Standard_Integer i = 1; i <= origKnots.Length(); i++)
            {
                newKnots(i) = origKnots(i);
                newMults(i) = origMults(i);
            }

            // 连接点处重数减1（避免重复）
            newMults(origNbKnots) = origMults(origNbKnots) - 1;

            for (Standard_Integer i = 2; i <= lineNbKnots; i++)
            {
                Standard_Integer idx = origKnots.Length() + i - 1;
                newKnots(idx) = lineKnots(i) + knotOffset;
                newMults(idx) = lineMults(i);
            }
        }

        //---------------------------------
        // 5.创建合并后的新B样条
        //---------------------------------
        Handle(Geom2d_BSplineCurve) newCurve = new Geom2d_BSplineCurve(newPoles, newKnots, newMults, origDegree);
        return newCurve;
    }
    catch (const Standard_Failure&)
    {
        assert(false);
        return nullptr;
    }
}

bool SplineUtil::getBSplineData(const Handle(Geom2d_BSplineCurve)& pBSpline,
    unsigned int& degree,
    std::vector<wy::Vector2>& controlPoints,
    std::vector<double>& knots,
    std::vector<unsigned int>& multiplicities)
{
    if (!pBSpline)
    {
        assert(false);
        return false;
    }

    try
    {
        // 输出
        // <1>次数
        degree = pBSpline->Degree();
        // <2>控制点
        int numPoles = pBSpline->NbPoles();
        controlPoints.reserve(numPoles);
        for (int i = 1; i <= numPoles; ++i)
        {
            const gp_Pnt2d& pnt2d = pBSpline->Pole(i);
            controlPoints.emplace_back(wy::Vector2(pnt2d.X(), pnt2d.Y()));
        }
        // 节点
        int numKnots = pBSpline->NbKnots();
        knots.reserve(numKnots);
        for (int i = 1; i <= numKnots; ++i)
        {
            knots.emplace_back(pBSpline->Knot(i));
        }
        // 重数
        const TColStd_Array1OfInteger& mults = pBSpline->Multiplicities();
        multiplicities.reserve(mults.Size());
        for (int i = 1; i <= mults.Size(); ++i)
        {
            multiplicities.emplace_back(mults.Value(i));
        }

        return true;
    }
    catch (const Standard_Failure&)
    {
        assert(false);
        return false;
    }
}

Handle(Geom_BSplineCurve) SplineUtil::convertToBSplineCurve3D(
    const Handle(Geom2d_BSplineCurve)& pBSpline2d,
    const wy3d::Plane& plane)
{
    if (pBSpline2d.IsNull() || !plane.isValid())
    {
        assert(false);
        return nullptr;
    }

    try
    {
        const int degree = pBSpline2d->Degree();
        const TColgp_Array1OfPnt2d& poles2d = pBSpline2d->Poles();  // 控制点数组
        const TColStd_Array1OfReal& knots = pBSpline2d->Knots();    // 节点向量
        const TColStd_Array1OfInteger& multiplicities = pBSpline2d->Multiplicities();  // 重数数组
        const bool isPeriodic = pBSpline2d->IsPeriodic();

        if (degree < 1 || 
            poles2d.Length() < 2 ||
            knots.Length() < 1 ||
            multiplicities.Length() != knots.Length())
        {
            assert(false);
            return nullptr;
        }

        int totalKnots = 0;
        for (int i = multiplicities.Lower(); i <= multiplicities.Upper(); ++i)
        {
            if (multiplicities(i) < 1 || multiplicities(i) > degree + 1)
            {
                assert(false);
                return nullptr;
            }
            totalKnots += multiplicities(i);
        }
        if (totalKnots != poles2d.Length() + degree + 1)
        {
            assert(false);
            return nullptr;
        }

        TColgp_Array1OfPnt poles3d(poles2d.Lower(), poles2d.Upper());
        for (int i = poles2d.Lower(); i <= poles2d.Upper(); ++i)
        {
            const gp_Pnt2d& pnt2d = poles2d(i);
            const wy::Vector3 pnt3d = plane.value(wy::Vector2(pnt2d.X(), pnt2d.Y()));
            poles3d(i) = gp_Pnt(pnt3d.x(), pnt3d.y(), pnt3d.z());
        }

        Handle(Geom_BSplineCurve) pBSpline3d;
        if (pBSpline2d->IsRational())
        {
            TColStd_Array1OfReal weights(poles2d.Lower(), poles2d.Upper());
            for (int i = weights.Lower(); i <= weights.Upper(); ++i)
            {
                weights(i) = pBSpline2d->Weight(i);
            }
            pBSpline3d = new Geom_BSplineCurve(poles3d, weights, knots, multiplicities, degree, isPeriodic);
        }
        else
        {
            pBSpline3d = new Geom_BSplineCurve(poles3d, knots, multiplicities, degree, isPeriodic);
        }
        return pBSpline3d;
    }
    catch (const Standard_Failure& e)
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

}
