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

#ifndef WY3DAPP_TOPOSHAPE_UTIL_H
#define WY3DAPP_TOPOSHAPE_UTIL_H

#include <Geom_Curve.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>

#include <wyVector3.h>
#include <wy3dPlane.h>

class TopoShapeUtil
{
public:
    // 获取拓扑面的信息
    static bool getFacePlane(const TopoDS_Face& face, wy3d::Plane& plane);

    // 获取拓扑形体下的面信息
    // faceIndex --- 面索引,从0开始;
    static bool getShapeFacePlane(const TopoDS_Shape& shape, unsigned int faceIndex, wy3d::Plane& plane);
    // 获取拓扑形体下的边信息
    // edgeIndex --- 边索引,从0开始;
    static bool getShapeEdgeEndPoints(const TopoDS_Shape& shape, unsigned int edgeIndex,
        wy::Vector3& startPnt, wy::Vector3& endPnt);
    // 获取拓扑形体下的边的曲线
    // edgeIndex --- 边索引,从0开始;
    static Handle(Geom_Curve) getShapeEdgeCurve(const TopoDS_Shape& shape, unsigned int edgeIndex);

    // 获取指定索引的面
    static std::pair<bool, TopoDS_Face> getFace(
        const TopoDS_Shape& shape,
        unsigned int index);

    // 获取指定索引的边
    static std::pair<bool, TopoDS_Edge> getEdge(
        const TopoDS_Shape& shape,
        unsigned int index);
};

#endif // WY3DAPP_TOPOSHAPE_UTIL_H