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

#include <utils/wy3dSketch3DSplineUtil.h>

#include <algorithm>
#include <cassert>

#include <Standard_Failure.hxx>
#include <TColStd_Array1OfInteger.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <gp_Pnt.hxx>

NS_WY3D_BEG

namespace
{
bool readOut(const Handle(Geom_BSplineCurve)& pBSpline,
    unsigned int& degree,
    std::vector<wy::Vector3>& controlPoints,
    std::vector<double>& knots,
    std::vector<unsigned int>& multiplicities)
{
    try
    {
        degree = static_cast<unsigned int>(pBSpline->Degree());

        const int numPoles = pBSpline->NbPoles();
        controlPoints.reserve(numPoles);
        for (int i = 1; i <= numPoles; ++i)
        {
            const gp_Pnt& pnt = pBSpline->Pole(i);
            controlPoints.emplace_back(wy::Vector3(pnt.X(), pnt.Y(), pnt.Z()));
        }

        const int numKnots = pBSpline->NbKnots();
        knots.reserve(numKnots);
        for (int i = 1; i <= numKnots; ++i)
        {
            knots.emplace_back(pBSpline->Knot(i));
        }

        const TColStd_Array1OfInteger& mults = pBSpline->Multiplicities();
        multiplicities.reserve(mults.Size());
        for (int i = 1; i <= mults.Size(); ++i)
        {
            multiplicities.emplace_back(static_cast<unsigned int>(mults.Value(i)));
        }

        return true;
    }
    catch (const Standard_Failure&)
    {
        assert(false);
        return false;
    }
}
} // namespace

bool Sketch3DSplineUtil::segment(const Handle(Geom_BSplineCurve)& pBSpline, double startParam,
    double endParam,
    unsigned int& degree,
    std::vector<wy::Vector3>& controlPoints,
    std::vector<double>& knots,
    std::vector<unsigned int>& multiplicities)
{
    if (pBSpline.IsNull())
    {
        assert(false);
        return false;
    }
    if (startParam < 0.0 || startParam > 1.0 || endParam < 0.0 || endParam > 1.0 || startParam >= endParam)
    {
        assert(false);
        return false;
    }

    try
    {
        const double uMin = pBSpline->FirstParameter();
        const double uMax = pBSpline->LastParameter();
        const double u1 = std::clamp(uMin + startParam * (uMax - uMin), uMin, uMax);
        const double u2 = std::clamp(uMin + endParam * (uMax - uMin), uMin, uMax);

        // Copy() comes back as a Geom_Geometry, so the downcast is not optional here.
        Handle(Geom_BSplineCurve) pResultBSpline = Handle(Geom_BSplineCurve)::DownCast(pBSpline->Copy());
        if (pResultBSpline.IsNull())
        {
            assert(false);
            return false;
        }
        pResultBSpline->Segment(u1, u2);

        return readOut(pResultBSpline, degree, controlPoints, knots, multiplicities);
    }
    catch (const Standard_Failure&)
    {
        assert(false);
        return false;
    }
}

Handle(Geom_BSplineCurve) Sketch3DSplineUtil::addLineSegmentToBSpline(
    const Handle(Geom_BSplineCurve)& pBSpline, const wy::Vector3& newPoint, const bool atStart)
{
    if (pBSpline.IsNull())
    {
        assert(false);
        return nullptr;
    }

    try
    {
        const Standard_Integer origDegree = pBSpline->Degree();
        const Standard_Integer origNbPoles = pBSpline->NbPoles();
        TColgp_Array1OfPnt origPoles(1, origNbPoles);
        pBSpline->Poles(origPoles);

        const Standard_Integer origNbKnots = pBSpline->NbKnots();
        TColStd_Array1OfReal origKnots(1, origNbKnots);
        pBSpline->Knots(origKnots);
        TColStd_Array1OfInteger origMults(1, origNbKnots);
        pBSpline->Multiplicities(origMults);

        if (origNbPoles <= 0 || origNbKnots <= 0)
        {
            assert(false);
            return nullptr;
        }

        // A degree 1 segment, then raised so it can be merged into the source's knot vector.
        Handle(Geom_BSplineCurve) lineCurve;
        {
            TColgp_Array1OfPnt linePoles(1, 2);
            if (atStart)
            {
                linePoles(1) = gp_Pnt(newPoint.x(), newPoint.y(), newPoint.z());
                linePoles(2) = origPoles(1);
            }
            else
            {
                linePoles(1) = origPoles(origNbPoles);
                linePoles(2) = gp_Pnt(newPoint.x(), newPoint.y(), newPoint.z());
            }

            TColStd_Array1OfReal lineKnots(1, 2);
            lineKnots(1) = 0.0;
            lineKnots(2) = 1.0;

            TColStd_Array1OfInteger lineMults(1, 2);
            lineMults(1) = 2;
            lineMults(2) = 2;

            lineCurve = new Geom_BSplineCurve(linePoles, lineKnots, lineMults, 1);
        }
        lineCurve->IncreaseDegree(origDegree);

        const Standard_Integer lineNbPoles = lineCurve->NbPoles();
        TColgp_Array1OfPnt linePoles(1, lineNbPoles);
        lineCurve->Poles(linePoles);

        const Standard_Integer lineNbKnots = lineCurve->NbKnots();
        TColStd_Array1OfReal lineKnots(1, lineNbKnots);
        lineCurve->Knots(lineKnots);
        TColStd_Array1OfInteger lineMults(1, lineNbKnots);
        lineCurve->Multiplicities(lineMults);

        if (lineNbPoles <= 0 || lineNbKnots <= 0)
        {
            assert(false);
            return nullptr;
        }

        // The two curves already share the joint pole, so the merged pole list drops one copy of
        // it; the knot at the joint likewise drops one multiplicity.
        const Standard_Integer newNbPoles = lineNbPoles + origNbPoles - 1;
        TColgp_Array1OfPnt newPoles(1, newNbPoles);
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

        Standard_Real knotOffset(0.0);
        if (atStart)
        {
            knotOffset = lineKnots(lineNbKnots) - origKnots(1);
        }
        else
        {
            knotOffset = origKnots(origNbKnots) - lineKnots(1);
        }

        const Standard_Integer newNbKnots = lineNbKnots + origNbKnots - 1;
        TColStd_Array1OfReal newKnots(1, newNbKnots);
        TColStd_Array1OfInteger newMults(1, newNbKnots);

        if (atStart)
        {
            for (Standard_Integer i = 1; i <= lineNbKnots; i++)
            {
                newKnots(i) = lineKnots(i);
                newMults(i) = lineMults(i);
            }

            // The joint: one copy of the pole is gone, so one copy of the knot goes with it.
            newMults(lineNbKnots) = lineMults(lineNbKnots) - 1;

            for (Standard_Integer i = 2; i <= origNbKnots; i++)
            {
                const Standard_Integer idx = lineNbKnots + i - 1;
                newKnots(idx) = origKnots(i) + knotOffset;
                newMults(idx) = origMults(i);
            }
        }
        else
        {
            for (Standard_Integer i = 1; i <= origKnots.Length(); i++)
            {
                newKnots(i) = origKnots(i);
                newMults(i) = origMults(i);
            }

            newMults(origNbKnots) = origMults(origNbKnots) - 1;

            for (Standard_Integer i = 2; i <= lineNbKnots; i++)
            {
                const Standard_Integer idx = origKnots.Length() + i - 1;
                newKnots(idx) = lineKnots(i) + knotOffset;
                newMults(idx) = lineMults(i);
            }
        }

        return new Geom_BSplineCurve(newPoles, newKnots, newMults, origDegree);
    }
    catch (const Standard_Failure&)
    {
        assert(false);
        return nullptr;
    }
}

bool Sketch3DSplineUtil::getBSplineData(const Handle(Geom_BSplineCurve)& pBSpline,
    unsigned int& degree,
    std::vector<wy::Vector3>& controlPoints,
    std::vector<double>& knots,
    std::vector<unsigned int>& multiplicities)
{
    if (pBSpline.IsNull())
    {
        assert(false);
        return false;
    }

    return readOut(pBSpline, degree, controlPoints, knots, multiplicities);
}

NS_WY3D_END
