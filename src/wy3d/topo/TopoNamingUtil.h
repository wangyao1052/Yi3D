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

#ifndef WY3D_TOPO_NAMING_UTIL_H
#define WY3D_TOPO_NAMING_UTIL_H

#include <vector>
#include <TopoDS_Face.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <BRepPrimAPI_MakeSweep.hxx>
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepOffsetAPI_MakeOffsetShape.hxx>
#include <TopoDS_Shape.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <wy3dDefs.h>
#include <wy3dTopoNaming.h>
#include "topo/SketchTopoBuilder.h"

NS_WY3D_BEG

class TopoNamingUtil
{
public:
    static bool getTopoName(
        const TopoNaming& topoNaming,
        const TopoDS_Shape& shape,
        TopAbs_ShapeEnum shapeType,
        unsigned int index,
        TopoName& outName);

    static bool assemblyTopoNames(
        const TopoNaming& topoNaming,
        const TopoDS_Shape& shape,
        TopAbs_ShapeEnum shapeType,
        const std::vector<unsigned int>& indices,
        TopoNameList& outNames);

    // 拉伸体&旋转体拓扑命名（Face → Solid）
    static bool naming(
        const TopoDS_Face& originalFace,
        BRepPrimAPI_MakeSweep& makeSweep,
        const std::vector<TopoUtil::EdgeNamingInfo>& edgeNameInfos,
        unsigned int elemIdValue,
        TopoNaming& topoNaming,
        unsigned int profileIndex = 0);

    // 拉伸曲面&旋转曲面拓扑命名（Wire → Shell）
    static bool naming(
        const TopoDS_Wire& originalWire,
        BRepPrimAPI_MakeSweep& makeSweep,
        const std::vector<TopoUtil::EdgeNamingInfo>& edgeNameInfos,
        unsigned int elemIdValue,
        TopoNaming& topoNaming);

    // 扫掠体拓扑命名
    static bool naming(
        const TopoDS_Wire& pathWire,
        const TopoDS_Wire& profileWire,
        BRepOffsetAPI_MakePipeShell& makePipeShell,
        const std::vector<TopoUtil::EdgeNamingInfo>& pathEdgeNameInfos,
        const std::vector<TopoUtil::EdgeNamingInfo>& profileEdgeNameInfos,
        unsigned int elemIdValue,
        TopoNaming& topoNaming);

    // 放样体拓扑命名
    static bool naming(
        const std::vector<TopoUtil::WireInfo>& profileWireInfos,
        BRepOffsetAPI_ThruSections& makeLoft,
        unsigned int elemIdValue,
        TopoNaming& topoNaming);

    // 基础体拓扑命名
    static bool primitiveNaming(
        const TopoDS_Shape& shape,
        unsigned int elemIdValue,
        TopoNaming& topoNaming);

    // 偏置曲面拓扑命名（Shell → Shell offset）
    static bool naming(
        const TopoDS_Shape& sourceShape,
        const TopoNaming& sourceNaming,
        BRepOffsetAPI_MakeOffsetShape& mkOffset,
        unsigned int elemIdValue,
        TopoNaming& topoNaming);

    // 阵列特征拓扑命名
    static bool patternNaming(
        const TopoDS_Shape& sourceShape,
        const TopoNaming& sourceNaming,
        const std::vector<unsigned int>& suffix,
        BRepBuilderAPI_Transform& transform,
        TopoNaming& topoNaming);

    // 计算新生成的面
    static TopoNameList computeNewFaces(const ShapeDelta& faceDelta, const TopoNaming& topoNaming);
    // 计算来自于某个形体的面
    static TopoNameList computeFacesFromShape(
        const ShapeDelta& faceDelta,
        const TopoNaming& topoNaming,
        const TopoDS_Shape& shape);
};

NS_WY3D_END

#endif // WY3D_TOPO_NAMING_UTIL_H
