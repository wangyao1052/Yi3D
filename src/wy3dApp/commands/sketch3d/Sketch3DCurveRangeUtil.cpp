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

#include "Sketch3DCurveRangeUtil.h"

#include <cassert>

#include <Geom_BSplineCurve.hxx>

#include <utils/wy3dSketch3DCurveParam.h>
#include <utils/wy3dSketch3DSplineUtil.h>
#include <wy3dImpl.h>
#include <wy3dMath.h>
#include <wy3dSketchArc3D.h>
#include <wy3dSketchCircle3D.h>
#include <wy3dSketchCurve3D.h>
#include <wy3dSketchLine3D.h>
#include <wy3dSketchSpline3D.h>

bool Sketch3DCurveRangeUtil::apply(wy3d::SketchCurve3D* pCurve, double startParam, double endParam)
{
    assert(pCurve);

    // A range of nothing is refused rather than applied: it would leave a zero-length curve behind,
    // which is degenerate rather than merely small, and neither a fillet nor a chamfer needs one.
    if ((endParam - startParam) <= wy3d::TOL)
    {
        return false;
    }

    // The whole curve, which is what the algorithms report for every pair containing a circle.
    if (startParam == 0.0 && endParam == 1.0)
    {
        return true;
    }

    if (wy3d::SketchLine3D* pLine = wy3d::SketchLine3D::cast(pCurve))
    {
        // Both read before either is written: the second set ends up needing the original start.
        const wy::Vector3 startPnt = pLine->getStartPoint();
        const wy::Vector3 lineVec = pLine->getEndPoint() - startPnt;

        if (wy::ErrorStatus::Ok != pLine->setStartPoint(startPnt + lineVec * startParam)) return false;
        if (wy::ErrorStatus::Ok != pLine->setEndPoint(startPnt + lineVec * endParam)) return false;
        return true;
    }

    if (wy3d::SketchCircle3D::cast(pCurve))
    {
        // Unreachable: a circle's parameter range is 0..1 by definition, and a single tangency does
        // not cut one. Cutting the 0 degree point out of a circle would leave a degenerate arc, so
        // the algorithms are written never to ask for it.
        assert(false);
        return true;
    }

    if (wy3d::SketchArc3D* pArc = wy3d::SketchArc3D::cast(pCurve))
    {
        // subArcAngles rather than plain arithmetic, because the range may run past the arc's ends
        // and the angles that come back are the ones an extension lands on. Feeding these parameters
        // to getParamOfArc would be reading them backwards: that one wraps a target behind the start
        // round to a parameter above 1.
        double newStartAngle = 0.0;
        double newEndAngle = 0.0;
        wy3d::Sketch3DCurveParam::subArcAngles(wy3d::normalizeRadian(pArc->getStartAngle()),
            pArc->getTotalAngle(), startParam, endParam, newStartAngle, newEndAngle);
        if (wy::ErrorStatus::Ok != pArc->setStartAngle(newStartAngle)) return false;
        if (wy::ErrorStatus::Ok != pArc->setEndAngle(newEndAngle)) return false;
        return true;
    }

    if (wy3d::SketchSpline3D* pSpline = wy3d::SketchSpline3D::cast(pCurve))
    {
        // A spline is the one curve a range cannot be run past: segment works in the curve's own
        // domain, so an extension there would have to invent geometry rather than reveal it.
        if (startParam < 0.0 || endParam > 1.0)
        {
            return false;
        }

        // Taken once: setPoints and friends rebuild the curve, so a second getOccSpline would read
        // the trimmed spline back instead of the original.
        const Handle(Geom_BSplineCurve) pBSpline = pSpline->getOccSpline();
        if (pBSpline.IsNull())
        {
            assert(false);
            return false;
        }

        unsigned int degree = 0;
        std::vector<wy::Vector3> controlPoints;
        std::vector<double> knots;
        std::vector<unsigned int> multiplicities;
        if (!wy3d::Sketch3DSplineUtil::segment(pBSpline, startParam, endParam, degree, controlPoints,
            knots, multiplicities))
        {
            assert(false);
            return false;
        }
        if (wy::ErrorStatus::Ok != pSpline->setMode(wy3d::SplineMode::ControlPoints)) return false;
        if (wy::ErrorStatus::Ok != pSpline->setDegree(degree)) return false;
        if (wy::ErrorStatus::Ok != pSpline->setPoints(controlPoints)) return false;
        if (wy::ErrorStatus::Ok != pSpline->setKnots(knots)) return false;
        if (wy::ErrorStatus::Ok != pSpline->setMultiplicities(multiplicities)) return false;
        return true;
    }

    assert(false);
    return false;
}
