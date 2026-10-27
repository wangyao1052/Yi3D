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

#include "topo/DraftTopoShapeComparer.h"
#include <cassert>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>

NS_WY3D_BEG

DraftTopoShapeComparer::DraftTopoShapeComparer(BRepOffsetAPI_DraftAngle& mkDraft, const TopoDS_Shape& oldShape)
    : TopoShapeComparer(mkDraft, oldShape), _mkDraft(mkDraft)
{
}

void DraftTopoShapeComparer::recordModified()
{
    // 修改的边
    // 应该都是为空的
    for (const TopoShapeInfo& oldEdgeInfo : _oldEdgeInfoSet)
    {
        const TopTools_ListOfShape& modified = _mkShape.Modified(oldEdgeInfo.shape);
        if (modified.IsEmpty()) continue;

        for (TopTools_ListIteratorOfListOfShape iter(modified); iter.More(); iter.Next())
        {
            assert(modified.Extent() == 1);
            assert(!iter.Value().IsNull());
            assert(iter.Value().ShapeType() == TopAbs_ShapeEnum::TopAbs_EDGE);
            _edgeDelta.modified[oldEdgeInfo.shape] = iter.Value();
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
        else // > 1
        {
            assert(false); // 目前理论上认为不可能,但仍然以序号为1开始命名以确保万一出现了在Release版本下也可以运行
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

void DraftTopoShapeComparer::recordAdded()
{
    //// 新增的边(旧边>>>新边)
    //for (const TopoShapeInfo& oldEdgeInfo : _oldEdgeInfoSet)
    //{
    //    const TopTools_ListOfShape& generated = _mkShape.Generated(oldEdgeInfo.shape);
    //    if (generated.IsEmpty()) continue;

    //    assert(generated.Extent() == 1);
    //    for (TopTools_ListIteratorOfListOfShape iter(generated); iter.More(); iter.Next())
    //    {
    //        assert(!iter.Value().IsNull());
    //        assert(iter.Value().ShapeType() == TopAbs_ShapeEnum::TopAbs_EDGE);
    //        _edgeDelta.addedSingle[iter.Value()] = ShapeDelta::SingleSourceInfo::generated(oldEdgeInfo.shape);
    //    }
    //}

    // 新增的面(旧面>>>新面)
    // 1.建立新生成形体中:边<>面的映射
    TopTools_IndexedDataMapOfShapeListOfShape newEdgeToFacesMap;
    TopExp::MapShapesAndAncestors(_newShape, TopAbs_EDGE, TopAbs_FACE, newEdgeToFacesMap);
    // 2.遍历旧的面,找出新生成的面
    for (const TopoShapeInfo& oldFaceInfo : _oldFaceInfoSet)
    {
        const TopTools_ListOfShape& generated = _mkShape.Generated(oldFaceInfo.shape);
        if (generated.IsEmpty()) continue;

        assert(generated.Extent() == 1);
        for (TopTools_ListIteratorOfListOfShape iter(generated); iter.More(); iter.Next())
        {
            assert(!iter.Value().IsNull());
            if (TopAbs_ShapeEnum::TopAbs_FACE != iter.Value().ShapeType())
            {
                assert(false);
                continue;
            }
            TopoDS_Face newFace = TopoDS::Face(iter.Value());
            _faceDelta.addedSingle[newFace] = ShapeDelta::SingleSourceInfo::generated(oldFaceInfo.shape);

            // 记录新生成的面的边
            // 理论上:新生成面的每条边都是新增的
            TopTools_IndexedMapOfShape edgeMap;
            TopExp::MapShapes(newFace, TopAbs_ShapeEnum::TopAbs_EDGE, edgeMap);
            for (int i = 1; i <= edgeMap.Extent(); ++i)
            {
                const TopoDS_Shape& newEdge = TopoDS::Edge(edgeMap(i));
                // 新面的边不可能是从原来的边修改而来的
                if (_edgeDelta.modified.find(newEdge) != _edgeDelta.modified.cend())
                {
                    assert(false); // 推测理论上应该不太可能
                    continue;
                }
                // 记录新生成的边
                ShapeDelta::DoubleSourceInfo doubleSourceInfo;
                doubleSourceInfo.source1 = oldFaceInfo.shape;
                const TopTools_ListOfShape* pFaceList = newEdgeToFacesMap.Seek(newEdge);
                if (!pFaceList)
                {
                    // The edge belongs to a face that is not part of the new body.
                    assert(false);
                    continue;
                }
                const TopTools_ListOfShape& faceList = *pFaceList;
                assert(faceList.Extent() <= 2);
                for (TopTools_ListIteratorOfListOfShape faceIt(faceList); faceIt.More(); faceIt.Next())
                {
                    const TopoDS_Face& face = TopoDS::Face(faceIt.Value());
                    if (face.IsSame(newFace)) continue;
                    doubleSourceInfo.source2 = face; // 新边的另一个面
                }
                // modified by wangyao 2025.05.18 {
                // 创建一个圆柱体,对圆柱体的侧面拔模,就存在新生成的倒角面的一条边只有一个source;
                // 这与OCC的圆柱体的侧面(圆柱面)有一条直线段侧边有关,这和parasolid & acis & creo不同;
                // 在parasolid和creo中,圆柱体的侧面只有上下两条圆边;
                //_edgeDelta.addedDouble[newEdge] = doubleSourceInfo;
                if (doubleSourceInfo.source2.IsNull())
                {
                    _edgeDelta.addedSingle[newEdge] =
                        ShapeDelta::SingleSourceInfo::generated(doubleSourceInfo.source1);
                }
                else
                {
                    _edgeDelta.addedDouble[newEdge] = doubleSourceInfo;
                }
                // }
            }
        }
    }
}

void DraftTopoShapeComparer::init()
{
    // 基类初始化
    TopoShapeComparer::init();
}

NS_WY3D_END
