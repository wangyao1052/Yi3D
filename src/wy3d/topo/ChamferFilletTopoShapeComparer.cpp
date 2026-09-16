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

#include "topo/ChamferFilletTopoShapeComparer.h"
#include <cassert>
#include <vector>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>

NS_WY3D_BEG

ChamferFilletTopoShapeComparer::ChamferFilletTopoShapeComparer(
    BRepBuilderAPI_MakeShape& mkShape, const TopoDS_Shape& oldShape, HostType hostType)
    : TopoShapeComparer(mkShape, oldShape), _hostType(hostType), _pOldVertex2OldFaces(nullptr)
{
}

ChamferFilletTopoShapeComparer::~ChamferFilletTopoShapeComparer()
{
    if (_pOldVertex2OldFaces)
    {
        delete _pOldVertex2OldFaces;
        _pOldVertex2OldFaces = nullptr;
    }
}


void ChamferFilletTopoShapeComparer::recordModified()
{
    // 修改的边
    for (const TopoShapeInfo& oldEdgeInfo : _oldEdgeInfoSet)
    {
        const TopTools_ListOfShape& modified = _mkShape.Modified(oldEdgeInfo.shape);
        for (TopTools_ListIteratorOfListOfShape iter(modified); iter.More(); iter.Next())
        {
            assert(modified.Extent() == 1);
            assert(!iter.Value().IsNull());
            assert(iter.Value().ShapeType() == TopAbs_ShapeEnum::TopAbs_EDGE);
            _edgeDelta.modified[oldEdgeInfo.shape] = iter.Value();
            break;
        }
    }

    // 修改的面
    for (const TopoShapeInfo& oldFaceInfo : _oldFaceInfoSet)
    {
        const TopTools_ListOfShape& modified = _mkShape.Modified(oldFaceInfo.shape);
        for (TopTools_ListIteratorOfListOfShape iter(modified); iter.More(); iter.Next())
        {
            assert(modified.Extent() == 1);
            assert(!iter.Value().IsNull());
            assert(iter.Value().ShapeType() == TopAbs_ShapeEnum::TopAbs_FACE);
            _faceDelta.modified[oldFaceInfo.shape] = iter.Value();
            break;
        }
    }
}

void ChamferFilletTopoShapeComparer::recordAdded()
{
    // 新增的面(旧边>>>新面)
    // 1.建立新生成形体中:边<>面的映射
    TopTools_IndexedDataMapOfShapeListOfShape newEdgeToFacesMap;
    TopExp::MapShapesAndAncestors(_newShape, TopAbs_EDGE, TopAbs_FACE, newEdgeToFacesMap);
    // 2.遍历旧的边,找出新生成的面
    for (const TopoShapeInfo& oldEdgeInfo : _oldEdgeInfoSet)
    {
        const TopTools_ListOfShape& generated = _mkShape.Generated(oldEdgeInfo.shape);
        if (generated.IsEmpty()) continue;
        
        // added by wangyao 2025.08.16 {
        // 在做倒角和圆角时,存在选择的一条边>>>多个面的情形
        // 具体可参考:issue-001.y3dt对顶面的边倒角或圆角
        // 对于1>>>多的情形需要以起始序号为1来区分
        unsigned int index(0);
        if (generated.Size() > 1)
        {
            index = 1;
        }
        // }

        for (TopTools_ListIteratorOfListOfShape iter(generated); iter.More(); iter.Next(), ++index)
        {
            const TopoDS_Shape& newFace = iter.Value();
            assert(!newFace.IsNull());
            assert(newFace.ShapeType() == TopAbs_ShapeEnum::TopAbs_FACE);

            // 倒角圆角无论是1>>>1还是1>>>多,在拓扑命名的记录上都应该是新增而不是Split
            _faceDelta.addedSingle[newFace] = 0 == index
                ? ShapeDelta::SingleSourceInfo::generated(oldEdgeInfo.shape)
                : ShapeDelta::SingleSourceInfo::generatedMultiple(oldEdgeInfo.shape, index);

            // 记录新生成的面的边
            // 理论上:新生成面的每条边都是新增的
            TopTools_IndexedMapOfShape edgeMap;
            TopExp::MapShapes(newFace, TopAbs_ShapeEnum::TopAbs_EDGE, edgeMap);
            // Free edges of this new face, recorded after the walk (sheet hosts only)
            std::vector<TopoDS_Shape> freeEdges;
            for (int i = 1; i <= edgeMap.Extent(); ++i)
            {
                const TopoDS_Shape& newEdge = TopoDS::Edge(edgeMap(i));
                // 新面的边不可能是从原来的边修改而来的
                if (_edgeDelta.modified.find(newEdge) != _edgeDelta.modified.cend())
                {
                    assert(false); // 推测理论上应该不太可能
                    continue;
                }
                // 新面的边一定是在新的形体中
                // TODO:后续这个判断应该可以移除掉
                if (!newEdgeToFacesMap.Contains(newEdge))
                {
                    assert(false);
                    continue;
                }

                // 记录新生成的边
                ShapeDelta::DoubleSourceInfo doubleSourceInfo;
                doubleSourceInfo.source1 = oldEdgeInfo.shape; // oldEdge --> newFace --> newEdge
                doubleSourceInfo.index = index;
                const TopTools_ListOfShape& faceList = newEdgeToFacesMap.FindFromKey(newEdge);
                assert(faceList.Extent() <= 2);
                for (TopTools_ListIteratorOfListOfShape faceIt(faceList); faceIt.More(); faceIt.Next())
                {
                    const TopoDS_Face& face = TopoDS::Face(faceIt.Value());
                    if (face.IsSame(newFace)) continue;
                    doubleSourceInfo.source2 = face; // 新边的另一个面
                }
                // modified by wangyao 2025.05.18 {
                // 创建一个圆柱体,对圆柱体的顶面的边倒角,就存在新生成的倒角面的一条边只有一个source;
                // 这与OCC的圆柱体的侧面(圆柱面)有一条直线段侧边有关,这和parasolid & acis & creo不同;
                // 在parasolid和creo中,圆柱体的侧面只有上下两条圆边;
                //_edgeDelta.addedDouble[newEdge] = doubleSourceInfo;
                if (doubleSourceInfo.source2.IsNull())
                {
                    if (HostType::Sheet == _hostType)
                    {
                        // A sheet strip has free ends on the open boundary
                        freeEdges.emplace_back(newEdge);
                    }
                    else
                    {
                        // 理论上index应该为0,只有闭合的边才会出现在这个逻辑里
                        assert(0 == index);
                        _edgeDelta.addedSingle[newEdge] = 0 == index
                            ? ShapeDelta::SingleSourceInfo::generated(doubleSourceInfo.source1)
                            : ShapeDelta::SingleSourceInfo::generatedMultiple(doubleSourceInfo.source1, index);
                    }
                }
                else
                {
                    _edgeDelta.addedDouble[newEdge] = doubleSourceInfo;
                }
                // }
            }

            // On a sheet the free ends of one strip all come from the same old edge, so
            // they are named after the new face to tell them apart
            if (HostType::Sheet == _hostType && !freeEdges.empty())
            {
                if (freeEdges.size() > 1)
                {
                    unsigned int freeIndex(0);
                    for (const TopoDS_Shape& freeEdge : freeEdges)
                    {
                        _edgeDelta.addedSingle[freeEdge] =
                            ShapeDelta::SingleSourceInfo::generatedMultiple(newFace, ++freeIndex);
                    }
                }
                else
                {
                    _edgeDelta.addedSingle[freeEdges.front()] =
                        ShapeDelta::SingleSourceInfo::generated(newFace);
                }
            }
        }
    }

    // 新增的面(旧点>>>新面)
    for (const TopoDS_Shape& oldVertex : _oldVertexSet)
    {
        const TopTools_ListOfShape& generated = _mkShape.Generated(oldVertex);
        if (generated.IsEmpty()) continue;

        // added by wangyao 2025.08.16 {
        // 目前还没有想到1>>>多的情形,但先仿照上一步代码中1条边>>>多个面的情形把1>>>多的逻辑写好
        assert(generated.Extent() == 1); // 如果后期遇到了1>>>多的实际案例,这句代码可以去掉
        unsigned int index(0);
        if (generated.Size() > 1)
        {
            index = 1;
        }
        // }

        for (TopTools_ListIteratorOfListOfShape iter(generated); iter.More(); iter.Next())
        {
            // 新面
            const TopoDS_Shape& newFace = iter.Value();
            if (newFace.ShapeType() != TopAbs_ShapeEnum::TopAbs_FACE)
            {
                assert(false);
                continue;
            }

            // 建立旧顶点<>旧面集的映射
            if (!_pOldVertex2OldFaces)
            {
                this->initOldVertex2OldFaces();
            }
            assert(_pOldVertex2OldFaces);

            // 映射: 新面 <> 旧面的集合
            for (const TopoDS_Shape& oldFace : (*_pOldVertex2OldFaces)[oldVertex])
            {
                _faceDelta.addedMulti[newFace].sources.insert(oldFace);
            }
            if (0 != index)
            {
                _faceDelta.addedMulti[newFace].index = index;
            }
            ++index;
        }
    }
}

void ChamferFilletTopoShapeComparer::init()
{
    // 基类初始化
    TopoShapeComparer::init();

    // old vertices
    {
        TopTools_IndexedMapOfShape oldVertexMap;
        TopExp::MapShapes(_oldShape, TopAbs_ShapeEnum::TopAbs_VERTEX, oldVertexMap);
        for (int i = 1; i <= oldVertexMap.Extent(); ++i)
        {
            _oldVertexSet.insert(oldVertexMap(i));
        }
    }
}

void ChamferFilletTopoShapeComparer::initOldVertex2OldFaces()
{
    if (_pOldVertex2OldFaces) return;
    _pOldVertex2OldFaces = new std::unordered_map<TopoDS_Shape, TopoShapeSet, ShapeHasher, ShapeEqual>();

    TopExp_Explorer faceExp(_oldShape, TopAbs_ShapeEnum::TopAbs_FACE);
    while (faceExp.More())
    {
        const TopoDS_Shape& face = faceExp.Current();
        TopExp_Explorer vertexExp(face, TopAbs_ShapeEnum::TopAbs_VERTEX);
        while (vertexExp.More())
        {
            const TopoDS_Shape& vertex = vertexExp.Current();
            (*_pOldVertex2OldFaces)[vertex].insert(face);
            vertexExp.Next();
        }
        faceExp.Next();
    }
}

NS_WY3D_END
