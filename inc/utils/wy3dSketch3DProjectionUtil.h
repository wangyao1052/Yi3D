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

#ifndef WY3D_SKETCH3D_PROJECTION_UTIL_H
#define WY3D_SKETCH3D_PROJECTION_UTIL_H

#include <vector>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <wy3dDefs.h>
#include <wy3dSketchPlane.h>

NS_WY3D_BEG

class Sketch;

// Projects a sketch onto a face or a datum plane: the SolidWorks "sketch on face" projected curve.
// The source is a planar sketch, the direction is that sketch's own plane normal - the feature has
// no direction of its own, only a reverse flag and a bidirectional one, and the direction is not
// one of its properties - and the result is a set of BRep edges lying on the target.
//
// The two targets are not interchangeable, the same way Sketch3DIntersectionUtil's are not: a face
// is bounded, so the result is trimmed to it, while a datum plane stands for an unbounded plane, so
// the whole projection comes back.
//
// This is a pure geometry layer: the database is only read, never written, and OCCT exceptions do
// not escape. It only produces the edges; turning those into sketch entities is Sketch3DEdgeUtil's
// job.
class WY3D_EXPORT Sketch3DProjectionUtil
{
public:
    enum class Result
    {
        Ok = 0,
        NoProjection,      // nothing the source holds lands on the target
        AlgorithmFailed,   // the section algorithm did not complete
        InvalidInput,      // null sketch, null face, invalid plane, or an empty source
        InvalidGeometry,   // illegal geometry, or an OCCT failure
        UnsupportedCurve,  // a curve the sketch cannot hold: rational, or past the degree limit
    };

    // SolidWorks' IProjectionCurveFeatureData flags, with the same names and the same meaning.
    struct Options
    {
        bool reverse = false;
        bool bidirectional = false;
    };

    // The sketch's curves, lifted into world space through the plane of their own sketch, as BRep
    // edges. Construction geometry and center lines are left out, as everywhere else in the model.
    // Ok only says the sketch was readable: a sketch holding nothing but construction geometry
    // comes back Ok with an empty outEdges, which is not the same as a caller mistake and does not
    // deserve the same message.
    static Result collectSourceEdges(const Sketch* pSketch, std::vector<TopoDS_Edge>& outEdges);

    // Sweeps each source edge along the sketch plane normal by just enough to reach the target, and
    // sections the swept surface with it. The one sided sweep is what carries the two flags:
    // bidirectional sweeps both sides, otherwise reverse picks which side of the sketch plane the
    // sweep runs on. Ok always means outEdges holds at least one usable edge.
    static Result project(const SketchPlane& sketchPlane,
        const std::vector<TopoDS_Edge>& sourceEdges, const TopoDS_Face& targetFace,
        const Options& options, std::vector<TopoDS_Edge>& outEdges);

    // The datum plane target, i.e. a datum plane's own plane. Nothing trims the result, so what
    // comes back is the whole projection - including the part of it that a face in the same place
    // would have cut off at its edge.
    static Result project(const SketchPlane& sketchPlane,
        const std::vector<TopoDS_Edge>& sourceEdges, const SketchPlane& targetPlane,
        const Options& options, std::vector<TopoDS_Edge>& outEdges);
};

NS_WY3D_END

#endif // WY3D_SKETCH3D_PROJECTION_UTIL_H
