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

#include "topo/ShellTopoShapeComparer.h"
#include <cassert>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>

NS_WY3D_BEG

ShellTopoShapeComparer::ShellTopoShapeComparer(
    BRepOffsetAPI_MakeThickSolid& mkShell, const TopoDS_Shape& oldShape)
    : TopoShapeComparer(mkShell, oldShape), _mkShell(mkShell)
{
}

ShellTopoShapeComparer::~ShellTopoShapeComparer()
{
}


void ShellTopoShapeComparer::recordModified()
{
    // 修改的边
    for (const TopoShapeInfo& oldEdgeInfo : _oldEdgeInfoSet)
    {
        const TopTools_ListOfShape& modified = _mkShape.Modified(oldEdgeInfo.shape);
        int size = modified.Size();
        if (0 == size)
        {
            continue;
        }
        else if (1 == size)
        {
            const TopoDS_Shape& shape = modified.First();
            assert(!shape.IsNull());
            assert(shape.ShapeType() == TopAbs_ShapeEnum::TopAbs_EDGE);
            _edgeDelta.modified[oldEdgeInfo.shape] = shape;
        }
        else // > 1 这个在实践中是否存在待进一步观察
        {
            assert(false); // 如果后续存在请注释掉此代码
            unsigned int index(1); // 序号从1开始
            for (TopTools_ListIteratorOfListOfShape iter(modified); iter.More(); iter.Next(), ++index)
            {
                assert(!iter.Value().IsNull());
                assert(iter.Value().ShapeType() == TopAbs_ShapeEnum::TopAbs_EDGE);
                _edgeDelta.addedSingle[iter.Value()] = ShapeDelta::SingleSourceInfo::split(oldEdgeInfo.shape, index);
            }
        }
    }

    // 修改的面
    for (const TopoShapeInfo& oldFaceInfo : _oldFaceInfoSet)
    {
        const TopTools_ListOfShape& modified = _mkShape.Modified(oldFaceInfo.shape);
        int size = modified.Size();
        if (0 == size)
        {
            continue;
        }
        else if (1 == size)
        {
            const TopoDS_Shape& shape = modified.First();
            assert(!shape.IsNull());
            assert(shape.ShapeType() == TopAbs_ShapeEnum::TopAbs_FACE);
            _faceDelta.modified[oldFaceInfo.shape] = shape;
        }
        else // > 1 这个在实践中是存在的,具体请参看issue-002.y3dt
        {
            unsigned int index(1); // 序号从1开始
            for (TopTools_ListIteratorOfListOfShape iter(modified); iter.More(); iter.Next(), ++index)
            {
                assert(!iter.Value().IsNull());
                assert(iter.Value().ShapeType() == TopAbs_ShapeEnum::TopAbs_FACE);
                _faceDelta.addedSingle[iter.Value()] = ShapeDelta::SingleSourceInfo::split(oldFaceInfo.shape, index);
            }
        }
    }
}

static std::pair<TopoDS_Shape, TopoDS_Shape> makeOrderedPair(
    const TopoDS_Shape& f1, const TopoDS_Shape& f2)
{
    ShapeHasher hasher;
    return hasher(f1) <= hasher(f2) ? makeTopoShapePair(f1, f2) : makeTopoShapePair(f2, f1);
}

void ShellTopoShapeComparer::recordAdded()
{
    // 旧边 >>>偏移>>> 新边(或新面)
    for (const TopoShapeInfo& oldEdgeInfo : _oldEdgeInfoSet)
    {
        const TopTools_ListOfShape& generated = _mkShape.Generated(oldEdgeInfo.shape);
        if (generated.IsEmpty()) continue;

        unsigned int index(0);
        if (generated.Size() > 1)
        {
            assert(false); // 目前理论上应该是1>>>1的关系,如果后续实践中确认存在1>>>多的关系,请删除此代码
            index = 1;
        }
        for (TopTools_ListIteratorOfListOfShape iter(generated); iter.More(); iter.Next())
        {
            assert(!iter.Value().IsNull());
            TopAbs_ShapeEnum shapeEnum = iter.Value().ShapeType();
            if (shapeEnum == TopAbs_ShapeEnum::TopAbs_EDGE)
            {
                _edgeDelta.addedSingle[iter.Value()] = 0 == index
                    ? ShapeDelta::SingleSourceInfo::generated(oldEdgeInfo.shape)
                    : ShapeDelta::SingleSourceInfo::generatedMultiple(oldEdgeInfo.shape, index);
            }
            else if (shapeEnum == TopAbs_ShapeEnum::TopAbs_FACE)
            {
                _faceDelta.addedSingle[iter.Value()] = 0 == index
                    ? ShapeDelta::SingleSourceInfo::generated(oldEdgeInfo.shape)
                    : ShapeDelta::SingleSourceInfo::generatedMultiple(oldEdgeInfo.shape, index);
            }
            else
            {
                assert(false); // 边偏移生成了非边/非面类型, 理论上不应该
            }
            ++index;
        }
    }

    // 旧面 >>>偏移>>> 新面(或新边)
    for (const TopoShapeInfo& oldFaceInfo : _oldFaceInfoSet)
    {
        const TopTools_ListOfShape& generated = _mkShape.Generated(oldFaceInfo.shape);
        if (generated.IsEmpty()) continue;

        unsigned int index(0);
        if (generated.Size() > 1)
        {
            assert(false); // 目前理论上应该是1>>>1的关系,如果后续实践中确认存在1>>>多的关系,请删除此代码
            index = 1;
        }
        for (TopTools_ListIteratorOfListOfShape iter(generated); iter.More(); iter.Next())
        {
            assert(!iter.Value().IsNull());
            TopAbs_ShapeEnum shapeEnum = iter.Value().ShapeType();
            if (shapeEnum == TopAbs_ShapeEnum::TopAbs_FACE)
            {
                _faceDelta.addedSingle[iter.Value()] = 0 == index
                    ? ShapeDelta::SingleSourceInfo::generated(oldFaceInfo.shape)
                    : ShapeDelta::SingleSourceInfo::generatedMultiple(oldFaceInfo.shape, index);
            }
            else if (shapeEnum == TopAbs_ShapeEnum::TopAbs_EDGE)
            {
                _edgeDelta.addedSingle[iter.Value()] = 0 == index
                    ? ShapeDelta::SingleSourceInfo::generated(oldFaceInfo.shape)
                    : ShapeDelta::SingleSourceInfo::generatedMultiple(oldFaceInfo.shape, index);
            }
            else
            {
                assert(false); // 面偏移生成了非面/非边类型, 理论上不应该
            }
            ++index;
        }
    }

    // 已经记录的边
    TopoShapeSet recordEdges;
    recordEdges.insert(_edgeDelta.kept.cbegin(), _edgeDelta.kept.cend());
    for (const auto& kvp : _edgeDelta.modified) // old <> new
    {
        recordEdges.insert(kvp.second);
    }
    for (const auto& kvp : _edgeDelta.addedSingle)
    {
        recordEdges.insert(kvp.first);
    }
    for (const auto& kvp : _edgeDelta.addedDouble)
    {
        recordEdges.insert(kvp.first);
    }
    for (const auto& kvp : _edgeDelta.addedMulti)
    {
        recordEdges.insert(kvp.first);
    }

    // 建立新生成形体中:边<>面的映射
    TopTools_IndexedDataMapOfShapeListOfShape newEdgeToFacesMap;
    TopExp::MapShapesAndAncestors(_newShape, TopAbs_EDGE, TopAbs_FACE, newEdgeToFacesMap);

    // 收集未命名的内部边, 按 (face1, face2) 对分组 (参照 Boolean 先收集再分流)
    // pair → 边列表
    std::unordered_map<std::pair<TopoDS_Shape, TopoDS_Shape>, std::vector<TopoDS_Shape>,
        TopoShapePairHasher, TopoShapePairEqual> pair2Edges;
    for (const auto& kvp : _faceDelta.addedSingle)
    {
        const TopoDS_Shape& newFace = kvp.first;
        TopTools_IndexedMapOfShape edgeMap;
        TopExp::MapShapes(newFace, TopAbs_ShapeEnum::TopAbs_EDGE, edgeMap);
        for (int i = 1; i <= edgeMap.Extent(); ++i)
        {
            const TopoDS_Shape& edgeShape = edgeMap(i);
            if (recordEdges.find(edgeShape) != recordEdges.cend())
                continue; // 已经记录过了

            // 新面的边一定是在新的形体中 (参照 ChamferFillet)
            if (!newEdgeToFacesMap.Contains(edgeShape))
            {
                assert(false);
                continue;
            }
            const TopTools_ListOfShape& faceList = newEdgeToFacesMap.FindFromKey(edgeShape);
            if (faceList.Extent() != 2)
            {
                // 单面边: 理论上不应该出现
                assert(false);
                if (faceList.Extent() == 1)
                {
                    TopTools_ListIteratorOfListOfShape faceIt(faceList);
                    _edgeDelta.addedSingle[edgeShape] = ShapeDelta::SingleSourceInfo::generated(faceIt.Value());
                    recordEdges.insert(edgeShape);
                }
                continue;
            }

            TopTools_ListIteratorOfListOfShape faceIt(faceList);
            const TopoDS_Shape& face1 = faceIt.Value();
            faceIt.Next();
            const TopoDS_Shape& face2 = faceIt.Value();
            // 归一化顺序: 按 hash 排序, 确保同对面的边归入同一组
            pair2Edges[makeOrderedPair(face1, face2)].emplace_back(edgeShape);
        }
    }

    // 处理同对面 >> 多条边: index 从 1 开始 
    for (auto& kvp : pair2Edges)  
    {
        ShapeDelta::DoubleSourceInfo info;
        info.source1 = kvp.first.first;
        info.source2 = kvp.first.second;

        if (kvp.second.size() >= 2)
        {
            unsigned int index(0);
            for (const TopoDS_Shape& edgeShape : kvp.second)
            {
                info.index = ++index;
                _edgeDelta.addedDouble[edgeShape] = info;
                recordEdges.insert(edgeShape);
            }
        }
        else
        {
            _edgeDelta.addedDouble[kvp.second.front()] = info; // index 默认 0
            recordEdges.insert(kvp.second.front());
        }
    }
}

void ShellTopoShapeComparer::init()
{
    // 基类初始化
    TopoShapeComparer::init();
}

NS_WY3D_END
