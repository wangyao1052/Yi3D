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

#ifndef WY3D_SKETCH_TOPO_BUILDER_H
#define WY3D_SKETCH_TOPO_BUILDER_H

#include <vector>
#include <map>

#include <TopoDS_Shape.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Solid.hxx>
#include <Geom_Curve.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <BRepBuilderAPI_Transform.hxx>

#include <wy3dDefs.h>
#include <wy3dSketch.h>
#include <wy3dSketchCurve.h>
#include <wy3dErrorCode.h>
#include <wy3dSketchProfile.h>
#include <wy3dTopoShapeMap.h>

NS_WY3D_BEG

class SketchTopoBuilder
{
public:
    explicit SketchTopoBuilder(const wy3d::Sketch* pSketch, bool recordTopoHistory = false);

    TopoDS_Edge makeEdge(const wy3d::SketchCurve* pSketchCurve);

    const std::map<Handle(Geom_Curve), unsigned int>& getCurve2IdMap() const
    {
        return _curve2Id;
    }

private:
    Handle(Geom_Curve) toGeomCurve(const wy3d::SketchCurve* pSketchCurve) const;

private:
    const wy3d::Sketch* _pSketch;
    bool _recordTopoHistory;
    std::map<Handle(Geom_Curve), unsigned int> _curve2Id;
};

class TopoUtil
{
public:
    // 创建面
    struct EdgeNamingInfo
    {
        TopoDS_Edge edge;
        unsigned int id;
        size_t sibling;

        EdgeNamingInfo() : edge(), id(0), sibling(-1) {}
    };
    static std::pair<ErrorCode, TopoDS_Face> makeFace(
        const wy3d::Sketch* pSketch,
        const SketchProfile::FaceSPtr& pSketchFace,
        std::vector<EdgeNamingInfo>& edgeNameInfos);

    // 创建Wires
    struct WireInfo
    {
        TopoDS_Wire wire;
        std::vector<EdgeNamingInfo> edgeNameInfos;
    };
    static ErrorCode makeWires(
        const wy3d::Sketch* pSketch,
        const std::vector<SketchProfile::LoopSPtr>& sketchLoops,
        const gp_Trsf& trsf,
        std::vector<WireInfo>& wireInfos);

public:
    // 记录Wire的边名
    // 往edgeNameInfos后面追加
    static void recordEdgeNamesOfWire_AppendedMode(
        const TopoDS_Wire& wire,
        const std::map<Handle(Geom_Curve), unsigned int>& curve2Id,
        std::vector<EdgeNamingInfo>& edgeNameInfos);

    // 更新边名
    static void updateEdgeNames(BRepBuilderAPI_Transform& transform, std::vector<EdgeNamingInfo>& edgeNameInfos);
};

NS_WY3D_END

#endif // WY3D_SKETCH_TOPO_BUILDER_H