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

#include "topo/MirrorSheetTopoShapeComparer.h"
#include <cassert>
#include <TopoDS_Face.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS.hxx>
#include <TopExp.hxx>

NS_WY3D_BEG

MirrorSheetTopoShapeComparer::MirrorSheetTopoShapeComparer(
    BRepBuilderAPI_Transform& transform,
    const TopoDS_Shape& oldShape,
    const TopoDS_Shape& newShape)
    : TopoShapeComparer(transform, oldShape)
{
    _newShape = newShape;
}

MirrorSheetTopoShapeComparer::~MirrorSheetTopoShapeComparer()
{
}

void MirrorSheetTopoShapeComparer::recordAdded()
{
    for (const TopoShapeInfo& oldFaceInfo : _oldFaceInfoSet)
    {
        const TopTools_ListOfShape& modified = _mkShape.Modified(oldFaceInfo.shape);
        assert(1 == modified.Extent());
        if (modified.IsEmpty())
        {
            continue;
        }

        const TopoDS_Shape& newFace = modified.First();
        assert(!newFace.IsNull());
        assert(newFace.ShapeType() == TopAbs_ShapeEnum::TopAbs_FACE);
        assert(_newFaceInfoSet.find(TopoShapeInfo(newFace, 0)) != _newFaceInfoSet.cend());
        _faceDelta.addedSingle[newFace] = ShapeDelta::SingleSourceInfo::generated(oldFaceInfo.shape);
    }

    for (const TopoShapeInfo& oldEdgeInfo : _oldEdgeInfoSet)
    {
        const TopTools_ListOfShape& modified = _mkShape.Modified(oldEdgeInfo.shape);
        assert(1 == modified.Extent());
        if (modified.IsEmpty())
        {
            continue;
        }

        const TopoDS_Shape& newEdge = modified.First();
        assert(!newEdge.IsNull());
        assert(newEdge.ShapeType() == TopAbs_ShapeEnum::TopAbs_EDGE);
        assert(_newEdgeInfoSet.find(TopoShapeInfo(newEdge, 0)) != _newEdgeInfoSet.cend());
        _edgeDelta.addedSingle[newEdge] = ShapeDelta::SingleSourceInfo::generated(oldEdgeInfo.shape);
    }
}

NS_WY3D_END
