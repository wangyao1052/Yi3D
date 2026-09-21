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

#ifndef WY3D_TOPO_SHAPE_UTIL_H
#define WY3D_TOPO_SHAPE_UTIL_H

#include <vector>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS_Compound.hxx>
#include <wy3dDefs.h>
#include <wy3dSketchPlane.h>
#include <wy3dErrorCode.h>

NS_WY3D_BEG

class WY3D_EXPORT TopoShapeUtil
{
public:
    static TopoDS_Compound makeCompound(const TopoDS_Shape& shape1, const TopoDS_Shape& shape2);

    static bool getFacePlane(const TopoDS_Face& face, wy3d::SketchPlane& plane);

    static ErrorCode makeWireFromEdges(
        const std::vector<TopoDS_Edge>& edges,
        TopoDS_Wire& outWire);

    // Planar sheet from coplanar edges: nested loops merge into one face (even nesting depth
    // is an outer wire, odd is a hole), loops that do not nest become separate faces. Every
    // face is wrapped in its own shell inside a compound, as a sketch profile produces them
    static ErrorCode makePlanarSheetFromEdges(
        const std::vector<TopoDS_Edge>& edges,
        TopoDS_Shape& outShape);

    static ErrorCode makeFilledFaceFromEdges(
        const std::vector<TopoDS_Edge>& edges,
        TopoDS_Face& outFace);

    // Filled sheet from picked edges: a compound holding one shell with one face, the shape
    // both filled sketch paths produce. A single loop is filled whether or not it has a plane
    // of its own. Two or more loops must share a plane and enclose a single region, which is
    // the planar sheet's own judgement (nested loops become its holes); a second region, or
    // loops that do not share a plane, is refused
    static ErrorCode makeFilledSheetFromEdges(
        const std::vector<TopoDS_Edge>& edges,
        TopoDS_Shape& outShape);
};

NS_WY3D_END

#endif // WY3D_TOPO_SHAPE_UTIL_H