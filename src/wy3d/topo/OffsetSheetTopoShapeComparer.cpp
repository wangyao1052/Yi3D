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

#include "topo/OffsetSheetTopoShapeComparer.h"
#include <cassert>
#include <TopoDS.hxx>
#include <TopTools_ListIteratorOfListOfShape.hxx>

NS_WY3D_BEG

namespace
{

BRepBuilderAPI_MakeShape& firstOffsetAlgo(
    const std::vector<std::shared_ptr<BRepOffsetAPI_MakeOffsetShape>>& offsetAlgos)
{
    assert(!offsetAlgos.empty());
    assert(offsetAlgos.front());
    return *offsetAlgos.front();
}

} // namespace

OffsetSheetTopoShapeComparer::OffsetSheetTopoShapeComparer(
    const std::vector<std::shared_ptr<BRepOffsetAPI_MakeOffsetShape>>& offsetAlgos,
    const TopoDS_Shape& oldShape,
    const TopoDS_Shape& newShape)
    : TopoShapeComparer(firstOffsetAlgo(offsetAlgos), oldShape), _offsetAlgos(offsetAlgos)
{
    _newShape = newShape;
}

void OffsetSheetTopoShapeComparer::recordAdded()
{
    for (const TopoShapeInfo& oldEdgeInfo : _oldEdgeInfoSet)
    {
        for (const std::shared_ptr<BRepOffsetAPI_MakeOffsetShape>& pOffsetAlgo : _offsetAlgos)
        {
            assert(pOffsetAlgo);
            const TopTools_ListOfShape& generated = pOffsetAlgo->Generated(oldEdgeInfo.shape);
            if (generated.IsEmpty()) continue;

            std::vector<TopoDS_Shape> newEdges;
            newEdges.reserve(static_cast<size_t>(generated.Size()));
            for (TopTools_ListIteratorOfListOfShape iter(generated); iter.More(); iter.Next())
            {
                assert(TopAbs_ShapeEnum::TopAbs_EDGE == iter.Value().ShapeType());
                newEdges.emplace_back(iter.Value());
            }
            for (size_t i = 0; i < newEdges.size(); ++i)
            {
                _edgeDelta.addedSingle[newEdges[i]] = 1 == newEdges.size()
                    ? ShapeDelta::SingleSourceInfo::generated(oldEdgeInfo.shape)
                    : ShapeDelta::SingleSourceInfo::generatedMultiple(
                        oldEdgeInfo.shape, static_cast<unsigned int>(i) + 1);
            }
            break;
        }
    }

    for (const TopoShapeInfo& oldFaceInfo : _oldFaceInfoSet)
    {
        for (const std::shared_ptr<BRepOffsetAPI_MakeOffsetShape>& pOffsetAlgo : _offsetAlgos)
        {
            assert(pOffsetAlgo);
            const TopTools_ListOfShape& generated = pOffsetAlgo->Generated(oldFaceInfo.shape);
            if (generated.IsEmpty()) continue;

            std::vector<TopoDS_Shape> newFaces;
            newFaces.reserve(static_cast<size_t>(generated.Size()));
            for (TopTools_ListIteratorOfListOfShape iter(generated); iter.More(); iter.Next())
            {
                assert(TopAbs_ShapeEnum::TopAbs_FACE == iter.Value().ShapeType());
                newFaces.emplace_back(iter.Value());
            }
            for (size_t i = 0; i < newFaces.size(); ++i)
            {
                _faceDelta.addedSingle[newFaces[i]] = 1 == newFaces.size()
                    ? ShapeDelta::SingleSourceInfo::generated(oldFaceInfo.shape)
                    : ShapeDelta::SingleSourceInfo::generatedMultiple(
                        oldFaceInfo.shape, static_cast<unsigned int>(i) + 1);
            }
            break;
        }
    }

    if (_edgeDelta.kept.size() + _edgeDelta.addedSingle.size() != _newEdgeInfoSet.size())
    {
        assert(false);
    }
}

NS_WY3D_END
