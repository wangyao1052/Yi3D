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

#include "TopoShapeUtil.h"

#include <Geom_Plane.hxx>
#include <TopoDS.hxx>
#include <TopExp.hxx>
#include <BRep_Tool.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <BRepLib.hxx>

#include <wyVector3.h>
#include <wy3dImpl.h>
#include "MathUtils.h"

bool TopoShapeUtil::getFacePlane(const TopoDS_Face& face, wy3d::Plane& plane)
{
    if (face.IsNull()) return false;
    Handle(Geom_Plane) planeSurf = Handle(Geom_Plane)::DownCast(BRep_Tool::Surface(face));
    if (planeSurf.IsNull()) return false;

    const gp_Ax3& ax3 = planeSurf->Position();
    const gp_Pnt& origin = ax3.Location();
    const gp_Dir& xAxis = ax3.XDirection();
    const gp_Dir& zAxis = ax3.Direction();

    TopAbs_Orientation orient = face.Orientation();
    if (orient == TopAbs_REVERSED)
    {
        plane = wy3d::Plane(
            MathUtils::toVector3(origin),
            MathUtils::toVector3(zAxis.Reversed()),
            MathUtils::toVector3(xAxis.Reversed()));
    }
    else
    {
        plane = wy3d::Plane(
            MathUtils::toVector3(origin),
            MathUtils::toVector3(zAxis),
            MathUtils::toVector3(xAxis));
    }
    if (plane.isValid()) return true;
    else return false;
}

bool TopoShapeUtil::getShapeFacePlane(const TopoDS_Shape& shape, unsigned int faceIndex, wy3d::Plane& plane)
{
    if (shape.IsNull()) return false;

    TopTools_IndexedMapOfShape faceMap;
    TopExp::MapShapes(shape, TopAbs_FACE, faceMap);
    faceIndex += 1; // OCC中以1为起始序号
    if (faceMap.Size() < faceIndex) return false;

    const TopoDS_Shape& faceShape = faceMap.FindKey(faceIndex);
    TopoDS_Face face = TopoDS::Face(faceShape);
    
    return getFacePlane(face, plane);
}

bool TopoShapeUtil::getShapeEdgeEndPoints(const TopoDS_Shape& shape, unsigned int edgeIndex,
    wy::Vector3& startPnt, wy::Vector3& endPnt)
{
    if (shape.IsNull()) return false;

    TopTools_IndexedMapOfShape edgeMap;
    TopExp::MapShapes(shape, TopAbs_EDGE, edgeMap);
    edgeIndex += 1; // OCC中以1为起始序号
    if (edgeIndex == 0 || edgeIndex > edgeMap.Size()) return false;

    const TopoDS_Shape& edgeShape = edgeMap.FindKey(edgeIndex);
    if (edgeShape.IsNull() || edgeShape.ShapeType() != TopAbs_ShapeEnum::TopAbs_EDGE)
    {
        return false;
    }
    TopoDS_Edge edge = TopoDS::Edge(edgeShape);

    Standard_Real first, last;
    Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, first, last);
    if (curve.IsNull()) return false;
    gp_Pnt occPntStart = curve->Value(first);
    gp_Pnt occPntEnd = curve->Value(last);

    startPnt.set(occPntStart.X(), occPntStart.Y(), occPntStart.Z());
    endPnt.set(occPntEnd.X(), occPntEnd.Y(), occPntEnd.Z());
    return true;
}

Handle(Geom_Curve) TopoShapeUtil::getShapeEdgeCurve(const TopoDS_Shape& shape, unsigned int edgeIndex)
{
    if (shape.IsNull()) return nullptr;

    TopTools_IndexedMapOfShape edgeMap;
    TopExp::MapShapes(shape, TopAbs_EDGE, edgeMap);
    edgeIndex += 1; // OCC中以1为起始序号
    if (edgeIndex == 0 || edgeIndex > edgeMap.Size()) return nullptr;

    const TopoDS_Shape& edgeShape = edgeMap.FindKey(edgeIndex);
    if (edgeShape.IsNull() || edgeShape.ShapeType() != TopAbs_ShapeEnum::TopAbs_EDGE)
    {
        return nullptr;
    }
    TopoDS_Edge edge = TopoDS::Edge(edgeShape);

    Standard_Real first(0.0), last(0.0);
    BRepLib::BuildCurve3d(edge, wy3d::TOL);
    Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, first, last);
    return curve;
}

std::pair<bool, TopoDS_Face> TopoShapeUtil::getFace(
    const TopoDS_Shape& shape,
    unsigned int index)
{
    TopTools_IndexedMapOfShape faceMap;
    TopExp::MapShapes(shape, TopAbs_FACE, faceMap);
    if (index >= faceMap.Extent()) // 索引超出范围
    {
        return std::pair<bool, TopoDS_Face>(false, TopoDS_Face());
    }
    else
    {
        return  std::pair<bool, TopoDS_Face>(true, TopoDS::Face(faceMap(index + 1)));
    }
}

std::pair<bool, TopoDS_Edge> TopoShapeUtil::getEdge(
    const TopoDS_Shape& shape,
    unsigned int index)
{
    TopTools_IndexedMapOfShape edgeMap;
    TopExp::MapShapes(shape, TopAbs_EDGE, edgeMap);
    if (index >= edgeMap.Extent()) // 索引超出范围
    {
        return std::pair<bool, TopoDS_Edge>(false, TopoDS_Edge());
    }
    else
    {
        return  std::pair<bool, TopoDS_Edge>(true, TopoDS::Edge(edgeMap(index + 1)));
    }
}