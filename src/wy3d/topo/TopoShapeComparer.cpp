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

#include "topo/TopoShapeComparer.h"
#include <cassert>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>

NS_WY3D_BEG

TopoShapeComparer::TopoShapeComparer(
    BRepBuilderAPI_MakeShape& mkShape,
    const TopoDS_Shape& oldShape,
    const TopoDS_Shape& newShape)
    : _mkShape(mkShape), _oldShape(oldShape)
{
    _newShape = newShape.IsNull() ? mkShape.Shape() : newShape;
}

TopoShapeComparer::~TopoShapeComparer()
{
}

void TopoShapeComparer::init()
{
    // old edges
    {
        TopTools_IndexedMapOfShape oldEdgeMap;
        TopExp::MapShapes(_oldShape, TopAbs_ShapeEnum::TopAbs_EDGE, oldEdgeMap);
        for (int i = 1; i <= oldEdgeMap.Extent(); ++i)
        {
            _oldEdgeInfoSet.insert(TopoShapeInfo(oldEdgeMap(i), i));
        }
    }

    // new edges
    {
        TopTools_IndexedMapOfShape newEdgeMap;
        TopExp::MapShapes(_newShape, TopAbs_ShapeEnum::TopAbs_EDGE, newEdgeMap);
        for (int i = 1; i <= newEdgeMap.Extent(); ++i)
        {
            _newEdgeInfoSet.insert(TopoShapeInfo(newEdgeMap(i), i));
        }
    }

    // old faces
    {
        TopTools_IndexedMapOfShape oldFaceMap;
        TopExp::MapShapes(_oldShape, TopAbs_ShapeEnum::TopAbs_FACE, oldFaceMap);
        for (int i = 1; i <= oldFaceMap.Extent(); ++i)
        {
            _oldFaceInfoSet.insert(TopoShapeInfo(oldFaceMap(i), i));
        }
    }

    // new faces
    {
        TopTools_IndexedMapOfShape newFaceMap;
        TopExp::MapShapes(_newShape, TopAbs_ShapeEnum::TopAbs_FACE, newFaceMap);
        for (int i = 1; i <= newFaceMap.Extent(); ++i)
        {
            _newFaceInfoSet.insert(TopoShapeInfo(newFaceMap(i), i));
        }
    }
}

void TopoShapeComparer::recordKept()
{
    // 保留的边
    for (const TopoShapeInfo& oldEdgeInfo : _oldEdgeInfoSet)
    {
        if (_newEdgeInfoSet.find(oldEdgeInfo) != _newEdgeInfoSet.cend())
        {
            _edgeDelta.kept.insert(oldEdgeInfo.shape);
        }
    }

    // 保留的面
    for (const TopoShapeInfo& oldFaceInfo : _oldFaceInfoSet)
    {
        if (_newFaceInfoSet.find(oldFaceInfo) != _newFaceInfoSet.cend())
        {
            _faceDelta.kept.insert(oldFaceInfo.shape);
        }
    }
}

void TopoShapeComparer::recordDeleted()
{
    // 删除的边
    for (const TopoShapeInfo& oldEdgeInfo : _oldEdgeInfoSet)
    {
        // commented by wangyao 2025.05.17 {
        // 这个逻辑经实践是不正确的,说明OCC存在Bug.
        //if (_mkShape.IsDeleted(oldEdge) // 这个函数返回值是不准确的,经实践还需要判断在新的边中是否存在
        //    && _newEdgeSet.find(oldEdge) == _newEdgeSet.cend())
        //{
        //    _edgeDelta.deleted.insert(oldEdge);
        //}
        // }

        if (_newEdgeInfoSet.find(oldEdgeInfo) == _newEdgeInfoSet.cend() && // 在新形体中不存在 并且
            _edgeDelta.modified.find(oldEdgeInfo.shape) == _edgeDelta.modified.cend())  // 不是修改的边
        {
            _edgeDelta.deleted.insert(oldEdgeInfo.shape);
        }
    }

    // 删除的面
    for (const TopoShapeInfo& oldFaceInfo : _oldFaceInfoSet)
    {
        // commented by wangyao 2025.05.17 {
        // 这个逻辑经实践是不正确的,说明OCC存在Bug.
        //if (_mkShape.IsDeleted(oldFace) // 这个函数返回值是不准确的,经实践还需要判断在新的边中是否存在
        //    && _newFaceSet.find(oldFace) == _newFaceSet.cend())
        //{
        //    _faceDelta.deleted.insert(oldFace);
        //}
        // }

        if (_newFaceInfoSet.find(oldFaceInfo) == _newFaceInfoSet.cend() && // 在新形体中不存在 并且
            _faceDelta.modified.find(oldFaceInfo.shape) == _faceDelta.modified.cend())  // 不是修改的面
        {
            _faceDelta.deleted.insert(oldFaceInfo.shape);
        }
    }
}

void TopoShapeComparer::perform()
{
    // 初始化
    this->init();

    // 保留的
    this->recordKept();
    // 修改的
    this->recordModified();
    // 新增的
    this->recordAdded();
    // 删除的
    this->recordDeleted();
    // 最终的校验
    this->finallyCheck();
    
}

void TopoShapeComparer::finallyCheck()
{
#ifdef _DEBUG
    this->finallyCheckFace();
#endif // _DEBUG
    this->finallyCheckEdge();
}

void TopoShapeComparer::finallyCheckFace()
{
    // 经实践,在做布尔并和布尔交时,可能存在:N>=1个面 修改为 1个面的情形
    // 比如两个立方体紧挨着,如下图所示:
    // --------------
    // |     ||     |
    // |     ||     |
    // --------------
    // 做布尔并时就存在:2个底面 修改为 1个底面; 2个顶面 修改为 1个顶面;
    // 在BooleanTopoShapeComparer::recordModified()函数中已经处理了这个逻辑.
    size_t totalNumFaces = _faceDelta.kept.size() + _faceDelta.modified.size() + _faceDelta.addedSingle.size()
        + _faceDelta.addedDouble.size() + _faceDelta.addedMulti.size();
    size_t actualNumFaces = _newFaceInfoSet.size();
    if (totalNumFaces == actualNumFaces)
    {
        return;
    }
    else
    {
        assert(false);
        return;
    }
}

void TopoShapeComparer::finallyCheckEdge()
{
    // 经实践,在做布尔并和布尔交时,可能存在:N>=1条边 修改为 1条边的情形
    // 比如两个立方体紧挨着,如下图所示:
    // --------------
    // |     ||     |
    // |     ||     |
    // --------------
    // 做布尔并时就存在:2个边 修改为 1个边
    // 在BooleanTopoShapeComparer::recordModified()函数中已经处理了这个逻辑.
    size_t totalNumEdges = _edgeDelta.kept.size() + _edgeDelta.modified.size() + _edgeDelta.addedSingle.size()
        + _edgeDelta.addedDouble.size() + _edgeDelta.addedMulti.size();
    size_t actualNumEdges = _newEdgeInfoSet.size();
    if (totalNumEdges == actualNumEdges)
    {
        return;
    }
    else if (totalNumEdges > actualNumEdges)
    {
        assert(false);
        return;
    }

    // 以下逻辑是处理 totalNumEdges < actualNumEdges 的情况
    // 经实践:在做圆角以及抽壳时,存在新生成的边是在修改的面里面,从而导致了漏记录的情形
    //       |---|
    // 长方体 |   |
    // |-----|   |圆柱体  如左图所示,长方体与圆柱体做并集;在长方体的顶面与圆柱体的侧面相交的边处倒圆角,就会存在这种情形.
    // |     |   |
    // |-----|---|
    // <1>记录的边
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

    // <2>建立新生成形体中:边<>面的映射
    TopTools_IndexedDataMapOfShapeListOfShape newEdgeToFacesMap;
    TopExp::MapShapesAndAncestors(_newShape, TopAbs_EDGE, TopAbs_FACE, newEdgeToFacesMap);

    // <3>遍历记录的修改的面,找寻未被记录的新生成的边
    // 两个面相交生成多条边的情况下,需要在拓扑命名上区分出来
    // source <> index,以1为起始序号
    std::unordered_map<std::pair<TopoDS_Shape, TopoDS_Shape>, unsigned int,
        TopoShapePairHasher, TopoShapePairEqual> source2NewShapeCount;
    // 修改的新面<>旧面的索引
    // 当边由两个面相交而成时,如何确定记录这两个面的顺序
    std::unordered_map<TopoDS_Shape, unsigned int, ShapeHasher, ShapeEqual> modifiedNewface2OldIndex;
    for (const auto& kvp : _faceDelta.modified)
    {
        // kvp.first --- old
        // kvp.second --- new
        // 旧面的索引
        auto iterOld = _oldFaceInfoSet.find(TopoShapeInfo(kvp.first, 0));
        if (iterOld != _oldFaceInfoSet.cend())
        {
            modifiedNewface2OldIndex[kvp.second] = iterOld->index;
        }
        else
        {
            assert(false);
        }
    }
    for (const auto& kvp : _faceDelta.modified)
    {
        // kvp.first --- old
        // kvp.second --- new
        TopTools_IndexedMapOfShape edgeMap;
        TopExp::MapShapes(kvp.second, TopAbs_ShapeEnum::TopAbs_EDGE, edgeMap);
        for (int i = 1; i <= edgeMap.Extent(); ++i)
        {
            const TopoDS_Shape& edge = edgeMap(i);

            // 已经被记录
            if (recordEdges.find(edge) != recordEdges.cend()) continue;

            // 未被记录的新边
            const TopTools_ListOfShape& faceList = newEdgeToFacesMap.FindFromKey(edge);
            int numFaces = faceList.Extent();
            switch (numFaces)
            {
            case 2:
            {
                TopTools_ListIteratorOfListOfShape faceIt(faceList);
                const TopoDS_Shape& face1 = faceIt.Value();
                faceIt.Next();
                const TopoDS_Shape& face2 = faceIt.Value();

                auto iter1 = modifiedNewface2OldIndex.find(face1);
                auto iter2 = modifiedNewface2OldIndex.find(face2);
                ShapeDelta::DoubleSourceInfo doubleSourceInfo;
                if (iter1 != modifiedNewface2OldIndex.cend() && iter2 != modifiedNewface2OldIndex.cend())
                {
                    assert(iter1->second != iter2->second);
                    // 索引小的在前,索引大的在后
                    if (iter1->second <= iter2->second)
                    {
                        doubleSourceInfo.source1 = face1;
                        doubleSourceInfo.source2 = face2;
                    }
                    else
                    {
                        doubleSourceInfo.source1 = face2;
                        doubleSourceInfo.source2 = face1;
                    }
                }
                else
                {
                    // commented by wangyao 2025.08.15 {
                    // 经实践,在做抽壳时会存在这种情形,先不深究先注释掉.
                    // 选择如下拉伸体的顶面和AB侧面时会出现.
                    // ------------------------|
                    // |                       |
                    // |   |----|   |-----|    |
                    // |   |    |   |     |    |
                    // |---|    |---|     |----|
                    //   A        B
                    //assert(false);
                    // }
                    doubleSourceInfo.source1 = face1;
                    doubleSourceInfo.source2 = face2;
                }

                unsigned int index(1);
                std::pair<TopoDS_Shape, TopoDS_Shape> source(doubleSourceInfo.source1, doubleSourceInfo.source2);
                auto iter = source2NewShapeCount.find(source);
                if (iter == source2NewShapeCount.cend()) // 第一个
                {
                    source2NewShapeCount[source] = 1;
                }
                else
                {
                    iter->second += 1;
                    index = iter->second;
                }

                doubleSourceInfo.index = index;
                _edgeDelta.addedDouble[edge] = doubleSourceInfo;
                recordEdges.insert(edge);
            }
            break;

            case 1:
            {
                assert(false); // 感觉应该不太可能
                _edgeDelta.addedSingle[edge] = ShapeDelta::SingleSourceInfo::generated(kvp.second);
                recordEdges.insert(edge);
            }
            break;

            default:
            {
                assert(false);
            }
            break;
            }
        }
    }
}

bool TopoShapeComparer::print(const std::string& fileFullPath) const
{
#define _CRT_SECURE_NO_WARNINGS
    FILE* fp = fopen(fileFullPath.c_str(), "w");
    if (!fp)
    {
        return false;
    }

    // 新旧模型边面的统计信息
    unsigned int oldEdgeCount = static_cast<unsigned int>(_oldEdgeInfoSet.size());
    unsigned int oldFaceCount = static_cast<unsigned int>(_oldFaceInfoSet.size());
    unsigned int newEdgeCount = static_cast<unsigned int>(_newEdgeInfoSet.size());
    unsigned int newFaceCount = static_cast<unsigned int>(_newFaceInfoSet.size());
    fprintf(fp, "====== Model Summary ======\n");
    fprintf(fp, "Old Model Edges: %u\n", oldEdgeCount);
    fprintf(fp, "Old Model Faces: %u\n", oldFaceCount);
    fprintf(fp, "New Model Edges: %u\n", newEdgeCount);
    fprintf(fp, "New Model Faces: %u\n\n", newFaceCount);

    // Helper lambda to get hash code of a shape
    auto getHash = [](const TopoDS_Shape& shape) {
        if (shape.IsNull()) {
            return -1;
        }
        return shape.HashCode(INT_MAX);
    };

    fprintf(fp, "====== Edge Changes (Old: %u -> New: %u) ======\n",
        oldEdgeCount, newEdgeCount);
    // Edge delta header
    fprintf(fp, "====== Edge Delta (Total Changes: %zu) ======\n",
        _edgeDelta.deleted.size() +
        _edgeDelta.modified.size() +
        _edgeDelta.addedSingle.size() + _edgeDelta.addedDouble.size() + _edgeDelta.addedMulti.size());

    // Kept edges with count
    fprintf(fp, "Kept Edges (%zu):\n", _edgeDelta.kept.size());
    for (const auto& edge : _edgeDelta.kept) {
        fprintf(fp, "  Edge %d\n", getHash(edge));
    }

    // Deleted edges with count
    fprintf(fp, "\nDeleted Edges (%zu):\n", _edgeDelta.deleted.size());
    for (const auto& edge : _edgeDelta.deleted) {
        fprintf(fp, "  Edge %d\n", getHash(edge));
    }

    // Modified edges with count
    fprintf(fp, "\nModified Edges (%zu):\n", _edgeDelta.modified.size());
    for (const auto& entry : _edgeDelta.modified) {
        fprintf(fp, "  Edge %d (modified from Edge %d)\n",
            getHash(entry.second), getHash(entry.first));
    }

    // Added edges with count
    fprintf(fp, "\nAdded Edges (%zu):\n", _edgeDelta.addedSingle.size() 
        + _edgeDelta.addedDouble.size() + _edgeDelta.addedMulti.size());
    for (const auto& entry : _edgeDelta.addedSingle)
    {
        fprintf(fp, "  Edge %d (generated from %d evolution = %d resultIndex = %d",
            getHash(entry.first), getHash(entry.second.source),
            static_cast<int>(entry.second.evolution), entry.second.resultIndex);
        fprintf(fp, ")\n");
    }
    for (const auto& entry : _edgeDelta.addedDouble)
    {
        const auto& [newEdge, parents] = entry;
        fprintf(fp, "  Edge %d (generated from %d",
            getHash(newEdge), getHash(parents.source1));

        if (!parents.source2.IsNull()) {
            fprintf(fp, " and %d", getHash(parents.source2));
        }
        fprintf(fp, ")\n");
    }
    for (const auto& entry : _edgeDelta.addedMulti)
    {
        fprintf(fp, "  Edge %d (generated from",
            getHash(entry.first));
        for (const TopoDS_Shape& parent : entry.second.sources)
        {
            fprintf(fp, " %d", getHash(parent));
        }
        fprintf(fp, ")\n");
    }

    // Total edges
    fprintf(fp, "\nTotal Edges = (%zu):\n\n\n", _edgeDelta.kept.size()
        + _edgeDelta.modified.size()
        + _edgeDelta.addedSingle.size()
        + _edgeDelta.addedDouble.size()
        + _edgeDelta.addedMulti.size());

    fprintf(fp, "====== Face Changes (Old: %u -> New: %u) ======\n",
        oldFaceCount, newFaceCount);
    // Face delta header
    fprintf(fp, "====== Face Delta (Total Changes: %zu) ======\n",
        _faceDelta.deleted.size() +
        _faceDelta.modified.size() +
        _faceDelta.addedSingle.size() + 
        _faceDelta.addedDouble.size() + 
        _faceDelta.addedMulti.size());

    // Kept faces with count
    fprintf(fp, "Kept Faces (%zu):\n", _faceDelta.kept.size());
    for (const auto& face : _faceDelta.kept) {
        fprintf(fp, "  Face %d\n", getHash(face));
    }

    // Deleted faces with count
    fprintf(fp, "\nDeleted Faces (%zu):\n", _faceDelta.deleted.size());
    for (const auto& face : _faceDelta.deleted) {
        fprintf(fp, "  Face %d\n", getHash(face));
    }

    // Modified faces with count
    fprintf(fp, "\nModified Faces (%zu):\n", _faceDelta.modified.size());
    for (const auto& entry : _faceDelta.modified) {
        fprintf(fp, "  Face %d (modified from Face %d)\n",
            getHash(entry.second), getHash(entry.first));
    }

    // Added faces with count
    fprintf(fp, "\nAdded Faces (%zu):\n", _faceDelta.addedSingle.size()
        + _faceDelta.addedDouble.size() + _faceDelta.addedMulti.size());
    for (const auto& entry : _faceDelta.addedSingle)
    {
        fprintf(fp, "  Face %d (generated from %d evolution = %d resultIndex = %d",
            getHash(entry.first), getHash(entry.second.source),
            static_cast<int>(entry.second.evolution), entry.second.resultIndex);
        fprintf(fp, ")\n");
    }
    for (const auto& entry : _faceDelta.addedDouble) {
        const auto& [newFace, parents] = entry;
        fprintf(fp, "  Face %d (generated from %d",
            getHash(newFace), getHash(parents.source1));

        if (!parents.source2.IsNull()) {
            fprintf(fp, " and %d", getHash(parents.source2));
        }
        fprintf(fp, ")\n");
    }
    for (const auto& entry : _faceDelta.addedMulti)
    {
        fprintf(fp, "  Face %d (generated from",
            getHash(entry.first));
        for (const TopoDS_Shape& parent : entry.second.sources)
        {
            fprintf(fp, " %d", getHash(parent));
        }
        fprintf(fp, ")\n");
    }

    // Total faces
    fprintf(fp, "\nTotal Faces = (%zu):\n", _faceDelta.kept.size()
        + _faceDelta.modified.size()
        + _faceDelta.addedSingle.size()
        + _faceDelta.addedDouble.size()
        + _faceDelta.addedMulti.size());

    fclose(fp);
    return true;
#undef _CRT_SECURE_NO_WARNINGS
}

NS_WY3D_END
