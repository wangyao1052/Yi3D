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

#ifndef WY3D_SKETCH3D_CURVE_INTERSECTION_UTIL_H
#define WY3D_SKETCH3D_CURVE_INTERSECTION_UTIL_H

#include <vector>

#include <wyVector3.h>
#include <wy3dDefs.h>

NS_WY3D_BEG

class SketchCurve3D;

// The intersection points of two 3D sketch curves. A pure geometry layer: no database, and
// OCCT failures do not escape - they come back as InvalidGeometry.
//
// This is the sibling of Sketch3DIntersectionUtil, which sections faces and planes. The two
// differ in what they hand back: a section produces curves, this produces points.
//
// OCCT supplies the points (GeomAPI_ExtremaCurveCurve), but the parameters on the entities are
// never taken from OCCT. Each caller recomputes its own parameter from the point it stores,
// which is what lets the same result serve trimming and extending alike: an extended curve's
// parameter simply falls outside [0,1].
class WY3D_EXPORT Sketch3DCurveIntersectionUtil
{
public:
    enum class Result
    {
        Ok = 0,
        NoIntersection,   // the two operands never come closer than tol
        Coincident,       // one support contains the other, so there are no isolated points
        DegenerateInput,  // an operand has no usable geometry, or the two are the same curve
        InvalidGeometry,  // an OCCT failure
    };

    // How far an operand may leave its own ends. reach == 0 uses the curve exactly as the entity
    // holds it, which is what trimming wants. reach > 0 lets it reach that far past its own ends:
    // a line widens to the segment covering that reach, an arc or an ellipse arc widens to the
    // whole conic. A spline has no widened form - extending it means following the tangent at its
    // ends, so the caller asks for those rays explicitly with extendStart / extendEnd.
    struct Operand
    {
        const SketchCurve3D* pCurve = nullptr;
        double reach = 0.0;
        bool extendStart = false;
        bool extendEnd = false;
    };

    static Operand bounded(const SketchCurve3D* pCurve);
    static Operand extended(const SketchCurve3D* pCurve, double reach, bool atStart, bool atEnd);

    // One point per intersection, de-duplicated to tol, in no particular order. A pair sharing a
    // support returns Coincident with no points, so a caller may simply skip such pairs.
    static Result intersect(const Operand& a, const Operand& b,
        std::vector<wy::Vector3>& outPoints, double tol = kDefaultTol);

    static constexpr double kDefaultTol = 1e-6;
};

NS_WY3D_END

#endif // WY3D_SKETCH3D_CURVE_INTERSECTION_UTIL_H
