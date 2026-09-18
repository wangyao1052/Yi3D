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

#ifndef WY3D_SKETCH3D_EDGE_UTIL_H
#define WY3D_SKETCH3D_EDGE_UTIL_H

#include <cstdint>
#include <vector>
#include <TopoDS_Edge.hxx>
#include <Geom_Curve.hxx>
#include <wyVector3.h>
#include <wy3dDefs.h>

namespace wydb
{
class Transaction;
}

NS_WY3D_BEG

class Sketch;
class SketchCurve;
class SketchEntity3D;

// Converts a curve that already lives somewhere in the model into a 3D sketch entity:
// a BRep edge, a curve of a 2D sketch, or an entity of another 3D sketch. The geometry
// is taken as is (a 3D sketch has no plane, so there is no projection): analytic curves
// are carried over exactly, splines are carried over exactly when their knot vector is
// representable, everything else is sampled into a fit point spline.
class WY3D_EXPORT Sketch3DEdgeUtil
{
public:
    enum class Result
    {
        Ok = 0,
        NullCurve,        // empty edge, or no curve behind it
        InfiniteCurve,    // unbounded parameter range
        Degenerate,       // zero length, radius or sweep angle
        InvalidGeometry,  // illegal geometry, or an OCCT failure
        UnsupportedType,  // cannot be turned into an entity even by sampling
        CreateFailed,     // the entity factory rejected the input
    };

    enum class Kind
    {
        Undefined = 0,
        Line,
        Circle,
        Arc,
        Ellipse,
        EllipseArc,
        ControlPointSpline,
        FitPointSpline,
    };

    struct CurveSpec
    {
        Kind kind = Kind::Undefined;
        wy::Vector3 startPnt;                                // Line
        wy::Vector3 endPnt;                                  // Line
        wy::Vector3 center;                                  // Circle/Arc/Ellipse/EllipseArc
        wy::Vector3 normal;                                  // Circle/Arc/Ellipse/EllipseArc
        wy::Vector3 xDir;                                    // Circle/Arc/Ellipse/EllipseArc
        double radius = 0.0;                                 // Circle/Arc
        double majorRadius = 0.0;                            // Ellipse/EllipseArc
        double radiusRatio = 1.0;                            // Ellipse/EllipseArc
        double startAngle = 0.0;                             // Arc/EllipseArc, radians
        double endAngle = 0.0;                               // Arc/EllipseArc, radians
        std::uint32_t degree = 0;                            // ControlPointSpline
        std::vector<wy::Vector3> points;                     // control points, or fit points
        std::vector<double> knots;                           // ControlPointSpline
        std::vector<std::uint32_t> multiplicities;           // ControlPointSpline
    };

    // Pure geometry layer: no database, no OCCT exception escapes.
    static Result makeCurveSpec(
        const Handle(Geom_Curve)& pCurve,
        double first,
        double last,
        CurveSpec& spec);

    static Result makeCurveSpec(const TopoDS_Edge& edge, CurveSpec& spec);

    static Result createEntity(
        wydb::Transaction* pTrans,
        const CurveSpec& spec,
        SketchEntity3D*& pOutEntity);

    static Result convert(
        wydb::Transaction* pTrans,
        const TopoDS_Edge& edge,
        SketchEntity3D*& pOutEntity);

    // The 2D curve is lifted into 3D through the plane of its own sketch.
    static Result convert(
        wydb::Transaction* pTrans,
        const Sketch* pSketch,
        const SketchCurve* pCurve,
        SketchEntity3D*& pOutEntity);

    static Result convert(
        wydb::Transaction* pTrans,
        const SketchEntity3D* pEntity,
        SketchEntity3D*& pOutEntity);
};

NS_WY3D_END

#endif // WY3D_SKETCH3D_EDGE_UTIL_H
