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

#ifndef WY3D_SKETCH3D_SPLINE_UTIL_H
#define WY3D_SKETCH3D_SPLINE_UTIL_H

#include <vector>

#include <Geom_BSplineCurve.hxx>

#include <wyVector3.h>
#include <wy3dDefs.h>

NS_WY3D_BEG

// The 3D counterparts of the app layer's SplineUtil, which lives in the executable and so cannot
// be reached by the unit tests. The B-spline data out of all three is in the form
// SketchSpline3D::create(trans, degree, points, knots, mults, out) takes, so a caller never has
// to touch OCCT itself.
//
// Every parameter here is normalized to [0,1] over the curve's extent, matching the entities'
// own getPointAt.
class WY3D_EXPORT Sketch3DSplineUtil
{
public:
    // The piece of pBSpline between two parameters. The result keeps the original degree and may
    // not start or end where the source did: OCCT trims the poles too.
    static bool segment(const Handle(Geom_BSplineCurve)& pBSpline, double startParam, double endParam,
        unsigned int& degree,
        std::vector<wy::Vector3>& controlPoints,
        std::vector<double>& knots,
        std::vector<unsigned int>& multiplicities);

    // pBSpline with a straight segment appended at one end, reaching out to newPoint. The line is
    // raised to the source's degree and merged into it, so the two meet with a shared knot.
    static Handle(Geom_BSplineCurve) addLineSegmentToBSpline(
        const Handle(Geom_BSplineCurve)& pBSpline, const wy::Vector3& newPoint, bool atStart);

    // The degree, poles, knots and multiplicities of pBSpline as they are.
    static bool getBSplineData(const Handle(Geom_BSplineCurve)& pBSpline,
        unsigned int& degree,
        std::vector<wy::Vector3>& controlPoints,
        std::vector<double>& knots,
        std::vector<unsigned int>& multiplicities);
};

NS_WY3D_END

#endif // WY3D_SKETCH3D_SPLINE_UTIL_H
