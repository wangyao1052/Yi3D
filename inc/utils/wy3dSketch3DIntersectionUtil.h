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

#ifndef WY3D_SKETCH3D_INTERSECTION_UTIL_H
#define WY3D_SKETCH3D_INTERSECTION_UTIL_H

#include <vector>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Edge.hxx>
#include <wy3dDefs.h>
#include <wy3dSketchPlane.h>

NS_WY3D_BEG

// The intersection curve of two section sources. A source is either a face of a solid or
// a sheet, or a datum plane. The distinction matters: a face is bounded, so the result is
// trimmed to it, while a datum plane stands for an unbounded plane, so the result is the
// whole curve. Turning a face into its supporting plane would be simpler but wrong - it
// would report a full circle where a bounded face only meets an arc.
//
// This is a pure geometry layer: no database, and OCCT exceptions do not escape. It only
// produces the section edges; turning those into sketch entities is Sketch3DEdgeUtil's job.
class WY3D_EXPORT Sketch3DIntersectionUtil
{
public:
    enum class Result
    {
        Ok = 0,
        NoIntersection,   // the two sources do not meet in a curve
        ParallelSources,  // coincident planar sources: there is no section, only overlap
        InfiniteSection,  // the section is an unbounded line, i.e. two datum planes
        InvalidInput,     // null shape, invalid plane, or the same source twice
        AlgorithmFailed,  // the algorithm did not complete, or reported errors
        InvalidGeometry,  // illegal geometry, or an OCCT failure
        UnsupportedCurve, // a curve the sketch cannot hold: rational, or past the degree limit
    };

    // What the intersector is allowed to do with the points it computed, i.e. what
    // BRepAlgoAPI_Section::Approximation means. Interpolate keeps them all, which for a section
    // without a closed form can only be the polyline through them; Fit lets it approximate them
    // with a smooth spline. A section with a closed form - a circle, ellipse or line - comes back
    // the same either way, so this only decides the shape of the free form results.
    enum class Mode
    {
        Interpolate = 0,
        Fit,
    };

    struct Source
    {
        bool isPlane = false;
        TopoDS_Shape shape;   // a face, when isPlane is false
        SketchPlane plane;    // an unbounded plane, when isPlane is true
    };

    // True when the sketch's own spline entity can carry this edge as it is. A rational spline
    // needs weights the entity has no place for, and past degree 8 the entity refuses it; such a
    // curve would otherwise be silently resampled into a 64 point fit spline.
    static bool isSupported(const TopoDS_Edge& edge);

    // Argument order does not matter. Degenerate and unbounded edges are dropped, so Ok always
    // means outEdges holds at least one usable edge: an exact circle, ellipse or line where the
    // section has a closed form, and otherwise whatever Mode asked for. The default keeps the
    // historical result, so a caller that does not care passes nothing.
    static Result intersect(const Source& a, const Source& b, std::vector<TopoDS_Edge>& outEdges,
        Mode mode = Mode::Interpolate);

private:
    static Result intersectImpl(const Source& a, const Source& b, std::vector<TopoDS_Edge>& outEdges,
        Mode mode = Mode::Interpolate);
};

NS_WY3D_END

#endif // WY3D_SKETCH3D_INTERSECTION_UTIL_H
