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

#include "topo/SplitFaceTopoShapeComparer.h"
#include <cassert>
#include <TopoDS.hxx>

NS_WY3D_BEG

SplitFaceTopoShapeComparer::SplitFaceTopoShapeComparer(
    BRepAlgoAPI_Splitter& splitter, const TopoDS_Shape& oldShape)
    : TopoShapeComparer(splitter, oldShape), _splitter(splitter)
{
    splitter.Tools();
}

void SplitFaceTopoShapeComparer::init()
{
    TopoShapeComparer::init();

    // old edges
    {
        const TopTools_ListOfShape& tools = _splitter.Tools();
        for (TopTools_ListIteratorOfListOfShape iter(tools); iter.More(); iter.Next())
        {
            const TopoDS_Shape& shape = iter.Value();
            if (shape.IsNull())
            {
                assert(false);
                continue;
            }
            if (shape.ShapeType() != TopAbs_ShapeEnum::TopAbs_EDGE)
            {
                assert(false);
                continue;
            }
            _oldEdgeInfoSet.insert(TopoShapeInfo(shape, 0));
        }
    }
}

void SplitFaceTopoShapeComparer::recordModified()
{
    for (const TopoShapeInfo& oldEdgeInfo : _oldEdgeInfoSet)
    {
        const TopTools_ListOfShape& modified = _mkShape.Modified(oldEdgeInfo.shape);
        if (modified.IsEmpty()) continue;

        unsigned int index(0);
        if (modified.Size() > 1)
        {
            index = 1;
        }
        for (TopTools_ListIteratorOfListOfShape iter(modified); iter.More(); iter.Next())
        {
            const TopoDS_Shape& newEdge = iter.Value();
            if (TopAbs_ShapeEnum::TopAbs_EDGE != newEdge.ShapeType())
            {
                assert(false);
                continue;
            }
            if (newEdge.IsSame(oldEdgeInfo.shape))
            {
                continue;
            }

            if (0 == index)
            {
                _edgeDelta.modified[oldEdgeInfo.shape] = newEdge;
            }
            else
            {
                _edgeDelta.addedSingle[newEdge] = ShapeDelta::SingleSourceInfo::split(oldEdgeInfo.shape, index);
            }
            ++index;
        }
    }

    for (const TopoShapeInfo& oldFaceInfo : _oldFaceInfoSet)
    {
        const TopTools_ListOfShape& modified = _mkShape.Modified(oldFaceInfo.shape);
        if (modified.IsEmpty()) continue;

        unsigned int index(0);
        if (modified.Size() > 1)
        {
            index = 1;
        }
        for (TopTools_ListIteratorOfListOfShape iter(modified); iter.More(); iter.Next())
        {
            const TopoDS_Shape& newFace = iter.Value();
            if (TopAbs_ShapeEnum::TopAbs_FACE != newFace.ShapeType())
            {
                assert(false);
                continue;
            }
            if (newFace.IsSame(oldFaceInfo.shape)) continue;

            if (0 == index)
            {
                _faceDelta.modified[oldFaceInfo.shape] = newFace;
            }
            else
            {
                _faceDelta.addedSingle[newFace] = ShapeDelta::SingleSourceInfo::split(oldFaceInfo.shape, index);
            }
            ++index;
        }
    }
}

NS_WY3D_END
