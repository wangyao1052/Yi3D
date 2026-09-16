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

#include "topo/BooleanTopoShapeComparer.h"
#include <cassert>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <BRep_Builder.hxx>

NS_WY3D_BEG

static TopoDS_Compound makeCompound(const TopoDS_Shape& shape1, const TopoDS_Shape& shape2)
{
    TopoDS_Compound compound;
    BRep_Builder brepBuilder;
    brepBuilder.MakeCompound(compound);
    brepBuilder.Add(compound, shape1);
    brepBuilder.Add(compound, shape2);
    return compound;
}

BooleanTopoShapeComparer::BooleanTopoShapeComparer(BRepAlgoAPI_BooleanOperation& booleanAlgo)
    : TopoShapeComparer(booleanAlgo, TopoDS_Shape()), _booleanAlgo(booleanAlgo)
{
    _oldShape = makeCompound(booleanAlgo.Shape1(), booleanAlgo.Shape2());
}

BooleanTopoShapeComparer::~BooleanTopoShapeComparer()
{
}

void BooleanTopoShapeComparer::init()
{
    // 基类初始化
    TopoShapeComparer::init();

    // 目标体的面
    {
        TopTools_IndexedMapOfShape faceMap;
        TopExp::MapShapes(_booleanAlgo.Shape1(), TopAbs_ShapeEnum::TopAbs_FACE, faceMap);
        for (int i = 1; i <= faceMap.Extent(); ++i)
        {
            _oldFaceSetTarget.insert(faceMap(i));
        }
    }
    // 参与体的面
    {
        TopTools_IndexedMapOfShape faceMap;
        TopExp::MapShapes(_booleanAlgo.Shape2(), TopAbs_ShapeEnum::TopAbs_FACE, faceMap);
        for (int i = 1; i <= faceMap.Extent(); ++i)
        {
            _oldFaceSetTool.insert(faceMap(i));
        }
    }

    // 目标体的边
    {
        TopTools_IndexedMapOfShape edgeMap;
        TopExp::MapShapes(_booleanAlgo.Shape1(), TopAbs_ShapeEnum::TopAbs_EDGE, edgeMap);
        for (int i = 1; i <= edgeMap.Extent(); ++i)
        {
            _oldEdgeSetTarget.insert(edgeMap(i));
        }
    }
    // 参与体的边
    {
        TopTools_IndexedMapOfShape edgeMap;
        TopExp::MapShapes(_booleanAlgo.Shape2(), TopAbs_ShapeEnum::TopAbs_EDGE, edgeMap);
        for (int i = 1; i <= edgeMap.Extent(); ++i)
        {
            _oldEdgeSetTool.insert(edgeMap(i));
        }
    }
}

void BooleanTopoShapeComparer::recordModified()
{
    struct SourceInfo
    {
        TopoDS_Shape shape;
        unsigned int index;
        SourceInfo() : shape(), index(0) {}
    };

    // 修改的边
    // 在做布尔减运算时:一条边可以被修改(分割)成多条边;
    // 在做布尔交和并运算时:多条边可以被修改成一条边
    // 但是无论如何
    // <1>修改边的源头只有一个:可能是目标体的边;也有可能是参与体的边;
    // <2>修改边的源头有N>1个:至少有一条是目标体的边;此时以目标体的边+序号来命名
    std::unordered_map<TopoDS_Shape, std::pair<SourceInfo, SourceInfo>,
        ShapeHasher, ShapeEqual> newEdge2Sources;
    for (const TopoShapeInfo& oldEdgeInfo : _oldEdgeInfoSet)
    {
        const TopTools_ListOfShape& modified = _mkShape.Modified(oldEdgeInfo.shape);
        if (modified.IsEmpty()) continue;

        unsigned int index(0);
        if (modified.Size() > 1) // 1-->多个,序号从1开始
        {
            index = 1;
        }
        for (TopTools_ListIteratorOfListOfShape iter(modified); iter.More(); iter.Next())
        {
            TopoDS_Shape newShape = iter.Value();
            if (newShape.ShapeType() != TopAbs_ShapeEnum::TopAbs_EDGE)
            {
                assert(false);
                continue;
            }

            // added by wangyao 2025.05.16 {
            // 如果修改的边和原来的边完全一样或者仅仅是方向不一致,则在前面的逻辑中已经设定为Kept了;
            // 若不加此处代码,在TopoNaming::update中,clearModify()函数会删除掉这条记录从而导致此边没有拓扑命名.
            if (newShape.IsSame(oldEdgeInfo.shape))
            {
                continue;
            }
            // }

            std::pair<SourceInfo, SourceInfo>& sourcePair = newEdge2Sources[newShape];
            if (_oldEdgeSetTarget.find(oldEdgeInfo.shape) != _oldEdgeSetTarget.cend()) // 旧边来自目标体
            {
                if (sourcePair.first.shape.IsNull())
                {
                    sourcePair.first.shape = oldEdgeInfo.shape;
                    sourcePair.first.index = index;
                }
            }
            else // 旧边来自参与体
            {
                sourcePair.second.shape = oldEdgeInfo.shape;
                sourcePair.second.index = index;
            }
            ++index;
        }
    }

    // 记录修改的边
    for (const auto& kvp : newEdge2Sources)
    {
        const std::pair<SourceInfo, SourceInfo>& sourcePair = kvp.second;
        const SourceInfo* pSourceInfo(nullptr);
        if (!sourcePair.first.shape.IsNull())
        {
            pSourceInfo = &sourcePair.first;
        }
        else if (!sourcePair.second.shape.IsNull())
        {
            pSourceInfo = &sourcePair.second;
        }
        else
        {
            assert(false);
            continue;
        }

        if (0 == pSourceInfo->index)
        {
            _edgeDelta.modified[pSourceInfo->shape] = kvp.first;
        }
        else
        {
            _edgeDelta.addedSingle[kvp.first] = ShapeDelta::SingleSourceInfo::split(
                pSourceInfo->shape, pSourceInfo->index);
        }
    }

    // 修改的面
    // 在做布尔减运算时:一个面可以被修改(分割)成多个面;
    // 在做布尔交和并运算时:多个面可以被修改成一个面
    // 但是无论如何
    // <1>修改面的源头只有一个:可能是目标体的面;也有可能是参与体的面;
    // <2>修改面的源头有N>1个:至少有一个是目标体的面;此时以目标体的面+序号来命名
    std::unordered_map<TopoDS_Shape, std::pair<SourceInfo, SourceInfo>,
        ShapeHasher, ShapeEqual> newFace2Sources;
    for (const TopoShapeInfo& oldFaceInfo : _oldFaceInfoSet)
    {
        const TopTools_ListOfShape& modified = _mkShape.Modified(oldFaceInfo.shape);
        if (modified.IsEmpty()) continue;

        unsigned int index(0);
        if (modified.Size() > 1) // 1-->多个,序号从1开始
        {
            index = 1;
        }
        for (TopTools_ListIteratorOfListOfShape iter(modified); iter.More(); iter.Next())
        {
            TopoDS_Shape newShape = iter.Value();
            if (newShape.ShapeType() != TopAbs_ShapeEnum::TopAbs_FACE)
            {
                assert(false);
                continue;
            }

            // added by wangyao 2025.05.16 {
            // 如果修改的面和原来的面完全一样或者仅仅是方向不一致,则在前面的逻辑中已经设定为Kept了;
            // 若不加此处代码,在TopoNaming::update中,clearModify()函数会删除掉这条记录从而导致此面没有拓扑命名.
            if (newShape.IsSame(oldFaceInfo.shape))
            {
                continue;
            }
            // }

            std::pair<SourceInfo, SourceInfo>& sourcePair = newFace2Sources[newShape];
            if (_oldFaceSetTarget.find(oldFaceInfo.shape) != _oldFaceSetTarget.cend()) // 旧面来自目标体
            {
                if (sourcePair.first.shape.IsNull())
                {
                    sourcePair.first.shape = oldFaceInfo.shape;
                    sourcePair.first.index = index;
                }
            }
            else // 旧面来自参与体
            {
                sourcePair.second.shape = oldFaceInfo.shape;
                sourcePair.second.index = index;
            }

            ++index;
        }
    }

    // 记录修改的面
    for (const auto& kvp : newFace2Sources)
    {
        const std::pair<SourceInfo, SourceInfo>& sourcePair = kvp.second;
        const SourceInfo* pSourceInfo(nullptr);
        if (!sourcePair.first.shape.IsNull())
        {
            pSourceInfo = &sourcePair.first;
        }
        else if (!sourcePair.second.shape.IsNull())
        {
            pSourceInfo = &sourcePair.second;
        }
        else
        {
            assert(false);
            continue;
        }

        if (0 == pSourceInfo->index)
        {
            _faceDelta.modified[pSourceInfo->shape] = kvp.first;
        }
        else
        {
            _faceDelta.addedSingle[kvp.first] = ShapeDelta::SingleSourceInfo::split(
                pSourceInfo->shape, pSourceInfo->index);
        }
    }
}

void BooleanTopoShapeComparer::recordAdded()
{
    // BRepAlgoAPI_BuilderAlgo类中关于Generated函数的描述:
    /*
    //! Returns the list  of shapes generated from the shape <theS>.
    //! In frames of Boolean Operations algorithms only Edges and Faces
    //! could have Generated elements, as only they produce new elements
    //! during intersection:
    //! - Edges can generate new vertices;
    //! - Faces can generate new edges and vertices.
    Standard_EXPORT virtual const TopTools_ListOfShape& Generated(const TopoDS_Shape & theS) Standard_OVERRIDE;
    */
    // 根据描述:旧面 >>> 新边
    // 新边一定是布尔目标体的面与布尔参与体的面相交而生成的;
    // 一个旧面可以生成>=1条新边;比如平面和圆柱面相交就有可能产生2条新边;
    // 一个圆柱面和一个椭圆柱面相交,最多可以产生4条新边;
    // 所以两个面相交生成多条边的情况下,需要在拓扑命名上区分出来以确保名称的唯一性

    if (!_booleanAlgo.HasGenerated())
    {
        return;
    }

    // newEdge <> (oldFaceTarget, oldFaceTool)
    std::unordered_map<TopoDS_Shape, std::pair<TopoDS_Shape, TopoDS_Shape>, ShapeHasher, ShapeEqual> newEdge2Source;
    // (oldFaceTarget, oldFaceTool) <> newEdges
    // 两个面相交生成多条边的情况下,需要在拓扑命名上区分出来
    std::unordered_map<std::pair<TopoDS_Shape, TopoDS_Shape>, std::vector<TopoDS_Shape>,
        TopoShapePairHasher, TopoShapePairEqual> source2NewEdges;

    // 遍历旧面获取生成的边
    std::vector<TopoDS_Shape> newEdgesByOldFace;
    newEdgesByOldFace.reserve(5);
    for (const TopoShapeInfo& oldFaceInfo : _oldFaceInfoSet)
    {
        const TopTools_ListOfShape& generated = _mkShape.Generated(oldFaceInfo.shape);
        if (generated.IsEmpty()) continue;

        // 是否是目标体的面
        bool isPrimary = _oldFaceSetTarget.find(oldFaceInfo.shape) != _oldFaceSetTarget.cend();

        // 旧面生成的边
        newEdgesByOldFace.clear();
        for (TopTools_ListIteratorOfListOfShape iter(generated); iter.More(); iter.Next())
        {
            const TopoDS_Shape& newEdge = iter.Value();
            assert(!newEdge.IsNull());

            // 有可能新生成的是顶点,直接跳过
            if (newEdge.ShapeType() != TopAbs_ShapeEnum::TopAbs_EDGE)
            {
                continue;
            }

            newEdgesByOldFace.emplace_back(newEdge);
        }

        // 遍历生成的边
        size_t numNewEdges = newEdgesByOldFace.size();
        for (size_t i = 0; i < numNewEdges; ++i)
        {
            const TopoDS_Shape& newEdge = newEdgesByOldFace[i];
            std::pair<TopoDS_Shape, TopoDS_Shape>& sourcePair = newEdge2Source[newEdge];
            if (isPrimary)
            {
                sourcePair.first = oldFaceInfo.shape;
            }
            else
            {
                assert(_oldFaceSetTool.find(oldFaceInfo.shape) != _oldFaceSetTool.cend());
                assert(sourcePair.second.IsNull());
                sourcePair.second = oldFaceInfo.shape;
            }

            // 一个面生成了多条边并且边是有两个面相交而成的,说明有可能存在两个面>>>多条边的情况
            if (numNewEdges > 1 && !sourcePair.first.IsNull() && !sourcePair.second.IsNull())
            {
                source2NewEdges[sourcePair].emplace_back(newEdge);
            }
        }
    }

    // 处理两个面>>>多条边的情形
    for (const auto& kvp : source2NewEdges)
    {
        if (kvp.second.size() < 2) continue;

        ShapeDelta::DoubleSourceInfo info;
        info.source1 = kvp.first.first;
        info.source2 = kvp.first.second;
        unsigned int index(0);
        for (const TopoDS_Shape& newEdge : kvp.second)
        {
            newEdge2Source.erase(newEdge);
            info.index = ++index;
            _edgeDelta.addedDouble[newEdge] = info;
        }
    }

    // 处理两个面>>>一条边的情形
    for (const auto& kvp : newEdge2Source)
    {
        if (kvp.second.first.IsNull() || kvp.second.second.IsNull())
        {
            assert(false);
            continue;
        }
        ShapeDelta::DoubleSourceInfo info;
        info.source1 = kvp.second.first;
        info.source2 = kvp.second.second;
        _edgeDelta.addedDouble[kvp.first] = info;
    }
}

NS_WY3D_END
