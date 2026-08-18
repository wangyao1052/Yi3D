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

#include <topo/TopoNamingUtil.h>
#include <TopoDS_Face.hxx>
#include <TopoDS.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>

NS_WY3D_BEG

bool TopoNamingUtil::getTopoName(
    const TopoNaming& topoNaming,
    const TopoDS_Shape& shape,
    TopAbs_ShapeEnum shapeType,
    unsigned int index,
    TopoName& outName)
{
    outName.clear();

    // 获取子类型Shape
    TopTools_IndexedMapOfShape subShapes;
    TopExp::MapShapes(shape, shapeType, subShapes);
    unsigned int numSubShapes = subShapes.Extent();
    if (index >= numSubShapes)
    {
        return false;
    }

    // 获取拓扑名
    if (!topoNaming.getName(subShapes(index + 1), outName) || outName.empty())
    {
        assert(false);
        return false;
    }

    return true;
}

bool TopoNamingUtil::assemblyTopoNames(
    const TopoNaming& topoNaming,
    const TopoDS_Shape& shape,
    TopAbs_ShapeEnum shapeType,
    const std::vector<unsigned int>& indices,
    TopoNameList& outNames)
{
    outNames.clear();
    if (indices.empty())
    {
        return false;
    }

    TopoNameList resultNames;
    resultNames.reserve(indices.size());

    // 获取子类型Shape
    TopTools_IndexedMapOfShape subShapes;
    TopExp::MapShapes(shape, shapeType, subShapes);
    unsigned int numSubShapes = subShapes.Extent();

    // 遍历组装拓扑名称
    for (unsigned int index : indices)
    {
        if (index >= numSubShapes)
        {
            return false;
        }
        TopoName name;
        if (!topoNaming.getName(subShapes(index + 1), name) || name.empty())
        {
            assert(false);
            return false;
        }
        resultNames.emplace_back(std::move(name));
    }

    outNames.swap(resultNames);
    return true;
}

bool TopoNamingUtil::naming(
    const TopoDS_Face& originalFace,
    BRepPrimAPI_MakeSweep& makeSweep,
    const std::vector<TopoUtil::EdgeNamingInfo>& edgeNameInfos,
    unsigned int elemIdValue,
    TopoNaming& topoNaming,
    unsigned int profileIndex/* = 0*/)
{
    // 建立所有侧面的拓扑命名
    // 格式: 草图曲线的ID
    for (const TopoUtil::EdgeNamingInfo& edgeNameInfo : edgeNameInfos)
    {
        const TopTools_ListOfShape& generated = makeSweep.Generated(edgeNameInfo.edge);
        assert(generated.Extent() == 1 || generated.Extent() == 0); // 对矩形轮廓做360度旋转,有两条边没有生成面
        for (TopTools_ListIteratorOfListOfShape iter(generated); iter.More(); iter.Next())
        {
            const TopoDS_Shape& shape = iter.Value();
            assert(!shape.IsNull());
            assert(shape.ShapeType() == TopAbs_ShapeEnum::TopAbs_FACE);
            topoNaming.setName(shape, TopoNameBuilder().id(edgeNameInfo.id).build());
        }
    }

    // 建立底面和顶面的拓扑命名
    // 底面格式: 特征ID
    // 顶面格式: 特征ID + 特征ID 
    TopoDS_Face bottomFace = TopoDS::Face(makeSweep.FirstShape());
    assert(!bottomFace.IsNull());
    TopoDS_Face topFace = TopoDS::Face(makeSweep.LastShape());
    assert(!topFace.IsNull());
    std::vector<TopoDS_Face> unnamedFaces; // 未命名的面,底面和顶面无效时存在,在最后命名
    unnamedFaces.reserve(20);
    std::set<unsigned int> lostEdgeIds; // 消失的边的ID
    if (bottomFace.IsEqual(topFace)) // 说明是360度旋转,底面和顶面无效
    {
        // 和旋转轴垂直的侧面有遗漏需要找出来
        TopTools_IndexedMapOfShape idxMapOfFace;
        TopExp::MapShapes(makeSweep.Shape(), TopAbs_ShapeEnum::TopAbs_FACE, idxMapOfFace);
        for (int i = 1; i <= idxMapOfFace.Extent(); ++i)
        {
            TopoDS_Face face = TopoDS::Face(idxMapOfFace(i));
            if (!topoNaming.contains(face))
            {
                unnamedFaces.emplace_back(face);
            }
        }

        // 所有边的集合
        TopoShapeSet edgeSet;
        TopTools_IndexedMapOfShape idxMapOfEdge;
        TopExp::MapShapes(makeSweep.Shape(), TopAbs_ShapeEnum::TopAbs_EDGE, idxMapOfEdge);
        for (int i = 1; i <= idxMapOfEdge.Extent(); ++i)
        {
            edgeSet.insert(idxMapOfEdge(i));
        }

        // 建立底面所有边的拓扑命名
        // 底面边: 草图曲线ID
        TopTools_IndexedMapOfShape idxMapOfEdge0;
        TopExp::MapShapes(originalFace, TopAbs_ShapeEnum::TopAbs_EDGE, idxMapOfEdge0);
        TopTools_IndexedMapOfShape idxMapOfEdge1;
        TopExp::MapShapes(bottomFace, TopAbs_ShapeEnum::TopAbs_EDGE, idxMapOfEdge1);
        int countEdges = idxMapOfEdge0.Extent();
        if (countEdges == idxMapOfEdge1.Extent() &&
            static_cast<size_t>(countEdges) == edgeNameInfos.size())
        {
            size_t idx(0);
            for (int i = 1; i <= countEdges; ++i, ++idx)
            {
                TopoDS_Edge edge0 = TopoDS::Edge(idxMapOfEdge0(i));
                TopoDS_Edge edge1 = TopoDS::Edge(idxMapOfEdge1(i));
                assert(edge0 == edgeNameInfos[idx].edge);
                const TopoUtil::EdgeNamingInfo& edgeInfo = edgeNameInfos[idx];
                if (edgeSet.find(edge1) == edgeSet.cend()) // added by wangyao 2025.05.24 在最终的实际形体中,边不存在
                {
                    lostEdgeIds.insert(edgeInfo.id);
                    continue;
                }
                else
                {
                    topoNaming.setName(edge1, TopoNameBuilder().id(edgeInfo.id).build()); // 底面边: 草图曲线ID
                }
            }
        }
        else
        {
            assert(false);
        }
    }
    else // 拉伸以及非360度旋转,底面和顶面有效
    {
        // 记录底面&顶面的拓扑名称
        {
            TopoNameBuilder bottomBuilder;
            bottomBuilder.id(elemIdValue);
            if (profileIndex > 0) { bottomBuilder.index(profileIndex); }
            topoNaming.setName(bottomFace, bottomBuilder.build());
        }
        {
            TopoNameBuilder topBuilder;
            topBuilder.id(elemIdValue).id(elemIdValue);
            if (profileIndex > 0) { topBuilder.index(profileIndex); }
            topoNaming.setName(topFace, topBuilder.build());
        }

        // 建立底面和顶面所有边的拓扑命名
        // 底面边: 草图曲线ID
        // 顶面边: 草图曲线ID + 特征ID
        TopTools_IndexedMapOfShape idxMapOfEdge0;
        TopExp::MapShapes(originalFace, TopAbs_ShapeEnum::TopAbs_EDGE, idxMapOfEdge0);
        TopTools_IndexedMapOfShape idxMapOfEdge1;
        TopExp::MapShapes(bottomFace, TopAbs_ShapeEnum::TopAbs_EDGE, idxMapOfEdge1);
        TopTools_IndexedMapOfShape idxMapOfEdge2;
        TopExp::MapShapes(topFace, TopAbs_ShapeEnum::TopAbs_EDGE, idxMapOfEdge2);
        int countEdges = idxMapOfEdge0.Extent();
        if (countEdges == idxMapOfEdge1.Extent() &&
            countEdges == idxMapOfEdge2.Extent() &&
            static_cast<size_t>(countEdges) == edgeNameInfos.size())
        {
            size_t idx(0);
            for (int i = 1; i <= countEdges; ++i, ++idx)
            {
                TopoDS_Edge edge0 = TopoDS::Edge(idxMapOfEdge0(i));
                TopoDS_Edge edge1 = TopoDS::Edge(idxMapOfEdge1(i));
                TopoDS_Edge edge2 = TopoDS::Edge(idxMapOfEdge2(i));
                assert(edge0 == edgeNameInfos[idx].edge);
                const TopoUtil::EdgeNamingInfo& edgeInfo = edgeNameInfos[idx];
                topoNaming.setName(edge1, TopoNameBuilder().id(edgeInfo.id).build()); // 底面边: 草图曲线ID
                if (!edge2.IsEqual(edge1)) // 旋转轴与边重合,旋转角度不是360度比如270度时,与旋转轴重合的边在顶面和底面是共用的
                {
                    topoNaming.setName(edge2, TopoNameBuilder().id(edgeInfo.id).id(elemIdValue).build()); // 顶面边: 草图曲线ID + 特征ID
                }
            }
        }
        else
        {
            assert(false);
        }
    }

    // 记录侧边的拓扑命名
    auto recordSideEdgeName = [&makeSweep, &topoNaming](const TopoDS_Vertex& vertex,
        unsigned int id1, unsigned int id2)
    {
        const TopTools_ListOfShape& generated = makeSweep.Generated(vertex);
        assert(generated.Extent() == 1 || generated.Extent() == 0); // 当旋转轴于边重合时,此时边的顶点的侧边不存在
        for (TopTools_ListIteratorOfListOfShape iter(generated); iter.More(); iter.Next())
        {
            const TopoDS_Shape& shape = iter.Value();
            assert(!shape.IsNull());
            assert(shape.ShapeType() == TopAbs_ShapeEnum::TopAbs_EDGE);
            topoNaming.setName(shape, TopoNameBuilder().id(id1).id(id2).build());
        }
    };

    // 建立侧边的拓扑命名
    // 格式: 草图曲线ID + 草图曲线ID
    size_t num = edgeNameInfos.size();
    TopoShapeSet visitedVertices;
    for (size_t i = 0; i < num; ++i)
    {
        // 边和下一条相邻的边
        const TopoUtil::EdgeNamingInfo& edgeNameInfo = edgeNameInfos[i];
        const TopoUtil::EdgeNamingInfo& edgeNameInfoNext = edgeNameInfo.sibling == size_t(-1) ?
            edgeNameInfos[i + 1] : edgeNameInfos[edgeNameInfo.sibling];

        // 提取边1的顶点
        TopoDS_Vertex v1_first, v1_last;
        TopExp::Vertices(edgeNameInfo.edge, v1_first, v1_last);
        // 提取边2的顶点
        TopoDS_Vertex v2_first, v2_last;
        TopExp::Vertices(edgeNameInfoNext.edge, v2_first, v2_last);

        // 对共有的顶点求生成的侧边
        if (v1_last.IsPartner(v2_first) || v1_last.IsPartner(v2_last))
        {
            // 对于圆弧边和直线段边构成的轮廓(只有两条边),有可能两次循环访问的都是同一个顶点,所以需要先判断下是否已经访问过.
            if (visitedVertices.find(v1_last) == visitedVertices.cend())
            {
                visitedVertices.insert(v1_last);
                recordSideEdgeName(v1_last, edgeNameInfo.id, edgeNameInfoNext.id);
                continue;
            }
        }
        if (v1_first.IsPartner(v2_first) || v1_first.IsPartner(v2_last))
        {
            // 对于圆弧边和直线段边构成的轮廓(只有两条边),有可能两次循环访问的都是同一个顶点,所以需要先判断下是否已经访问过.
            if (visitedVertices.find(v1_first) == visitedVertices.cend())
            {
                visitedVertices.insert(v1_first);
                recordSideEdgeName(v1_first, edgeNameInfo.id, edgeNameInfoNext.id);
                continue;
            }
        }
    }

    // 命名遗漏的面
    for (const TopoDS_Face& face : unnamedFaces)
    {
        TopTools_IndexedMapOfShape idxMapOfEdge;
        TopExp::MapShapes(face, TopAbs_ShapeEnum::TopAbs_EDGE, idxMapOfEdge);
        int numEdges = idxMapOfEdge.Extent();
        if (1 == numEdges) // 对矩形做旋转,旋转轴和矩形的一条边重合,侧边的面就只有一条边(圆)
        {
            TopoName edgeName;
            std::vector<std::uint32_t> edgeIds;
            if (!topoNaming.getName(idxMapOfEdge(1), edgeName) ||
                !TopoNameCodec::extractIds(edgeName, edgeIds) || edgeIds.size() != 2)
            {
                assert(false);
                continue;
            }
            // 用消失的边的ID命名
            if (lostEdgeIds.find(edgeIds[0]) != lostEdgeIds.cend())
            {
                topoNaming.setName(face, TopoNameBuilder().id(edgeIds[0]).build());
            }
            else if (lostEdgeIds.find(edgeIds[1]) != lostEdgeIds.cend())
            {
                topoNaming.setName(face, TopoNameBuilder().id(edgeIds[1]).build());
            }
            else
            {
                assert(false);
            }
        }
        else if (2 == numEdges)
        {
            TopoName edgeName1;
            TopoName edgeName2;
            std::vector<std::uint32_t> edgeIds1;
            std::vector<std::uint32_t> edgeIds2;
            if (!topoNaming.getName(idxMapOfEdge(1), edgeName1) ||
                !TopoNameCodec::extractIds(edgeName1, edgeIds1) || edgeIds1.size() != 2 ||
                !topoNaming.getName(idxMapOfEdge(2), edgeName2) ||
                !TopoNameCodec::extractIds(edgeName2, edgeIds2) || edgeIds2.size() != 2)
            {
                assert(false);
                continue;
            }
            // 用共有的边的ID命名
            // 也可以用消失的边的ID命名;这两者应该是等价的
            if (edgeIds1[0] == edgeIds2[0] || edgeIds1[0] == edgeIds2[1])
            {
                topoNaming.setName(face, TopoNameBuilder().id(edgeIds1[0]).build());
            }
            else if (edgeIds1[1] == edgeIds2[0] || edgeIds1[1] == edgeIds2[1])
            {
                topoNaming.setName(face, TopoNameBuilder().id(edgeIds1[1]).build());
            }
            else
            {
                assert(false);
            }
        }
        else
        {
            assert(false);
        }
    }

    return true;
}

bool TopoNamingUtil::naming(
    const TopoDS_Face& face,
    const std::vector<TopoUtil::EdgeNamingInfo>& edgeNameInfos,
    unsigned int elemIdValue,
    TopoNaming& topoNaming,
    unsigned int profileIndex)
{
    {
        TopoNameBuilder faceBuilder;
        faceBuilder.id(elemIdValue);
        if (profileIndex > 0) { faceBuilder.index(profileIndex); }
        topoNaming.setName(face, faceBuilder.build());
    }

    for (const TopoUtil::EdgeNamingInfo& edgeNameInfo : edgeNameInfos)
    {
        topoNaming.setName(edgeNameInfo.edge, TopoNameBuilder().id(edgeNameInfo.id).build());
    }

    return true;
}

bool TopoNamingUtil::naming(
    const TopoDS_Wire& originalWire,
    BRepPrimAPI_MakeSweep& makeSweep,
    const std::vector<TopoUtil::EdgeNamingInfo>& edgeNameInfos,
    unsigned int elemIdValue,
    TopoNaming& topoNaming)
{
    if (edgeNameInfos.empty())
    {
        assert(false);
        return false;
    }

    // 建立所有侧面的拓扑命名
    // 格式: 草图曲线的ID
    for (const TopoUtil::EdgeNamingInfo& edgeNameInfo : edgeNameInfos)
    {
        const TopTools_ListOfShape& generated = makeSweep.Generated(edgeNameInfo.edge);
        assert(generated.Extent() == 1 || generated.Extent() == 0); // 对矩形轮廓做360度旋转,有两条边没有生成面
        for (TopTools_ListIteratorOfListOfShape iter(generated); iter.More(); iter.Next())
        {
            const TopoDS_Shape& shape = iter.Value();
            assert(!shape.IsNull());
            assert(shape.ShapeType() == TopAbs_ShapeEnum::TopAbs_FACE);
            topoNaming.setName(shape, TopoNameBuilder().id(edgeNameInfo.id).build());
        }
    }

    // 建立底面和顶面的拓扑命名
    TopoDS_Wire bottomWire = TopoDS::Wire(makeSweep.FirstShape());
    assert(!bottomWire.IsNull());
    TopoDS_Wire topWire = TopoDS::Wire(makeSweep.LastShape());
    assert(!topWire.IsNull());
    std::set<unsigned int> lostEdgeIds; // 消失的边的ID
    if (bottomWire.IsEqual(topWire)) // 说明是360度旋转,底面和顶面无效
    {
        // 所有边的集合
        TopoShapeSet edgeSet;
        TopTools_IndexedMapOfShape idxMapOfEdge;
        TopExp::MapShapes(makeSweep.Shape(), TopAbs_ShapeEnum::TopAbs_EDGE, idxMapOfEdge);
        for (int i = 1; i <= idxMapOfEdge.Extent(); ++i)
            edgeSet.insert(idxMapOfEdge(i));

        // 建立底面所有边的拓扑命名
        // 底面边: 草图曲线ID
        TopTools_IndexedMapOfShape idxMapOfEdge0;
        TopExp::MapShapes(originalWire, TopAbs_ShapeEnum::TopAbs_EDGE, idxMapOfEdge0);
        TopTools_IndexedMapOfShape idxMapOfEdge1;
        TopExp::MapShapes(bottomWire, TopAbs_ShapeEnum::TopAbs_EDGE, idxMapOfEdge1);
        int countEdges = idxMapOfEdge0.Extent();
        if (countEdges == idxMapOfEdge1.Extent() &&
            static_cast<size_t>(countEdges) == edgeNameInfos.size())
        {
            size_t idx(0);
            for (int i = 1; i <= countEdges; ++i, ++idx)
            {
                TopoDS_Edge edge0 = TopoDS::Edge(idxMapOfEdge0(i));
                TopoDS_Edge edge1 = TopoDS::Edge(idxMapOfEdge1(i));
                assert(edge0 == edgeNameInfos[idx].edge);
                const TopoUtil::EdgeNamingInfo& edgeInfo = edgeNameInfos[idx];
                if (edgeSet.find(edge1) == edgeSet.cend()) // 在最终的实际形体中,边不存在
                {
                    lostEdgeIds.insert(edgeInfo.id);
                    continue;
                }
                else
                {
                    topoNaming.setName(edge1, TopoNameBuilder().id(edgeInfo.id).build()); // 底面边: 草图曲线ID
                }
            }
        }
        else
        {
            assert(false);
        }
    }
    else // 拉伸以及非360度旋转,底面和顶面有效
    {
        // 建立底面和顶面所有边的拓扑命名
        // 底面边: 草图曲线ID
        // 顶面边: 草图曲线ID + 特征ID
        TopTools_IndexedMapOfShape idxMapOfEdge0;
        TopExp::MapShapes(originalWire, TopAbs_ShapeEnum::TopAbs_EDGE, idxMapOfEdge0);
        TopTools_IndexedMapOfShape idxMapOfEdge1;
        TopExp::MapShapes(bottomWire, TopAbs_ShapeEnum::TopAbs_EDGE, idxMapOfEdge1);
        TopTools_IndexedMapOfShape idxMapOfEdge2;
        TopExp::MapShapes(topWire, TopAbs_ShapeEnum::TopAbs_EDGE, idxMapOfEdge2);
        int countEdges = idxMapOfEdge0.Extent();
        if (countEdges == idxMapOfEdge1.Extent() &&
            countEdges == idxMapOfEdge2.Extent() &&
            static_cast<size_t>(countEdges) == edgeNameInfos.size())
        {
            size_t idx(0);
            for (int i = 1; i <= countEdges; ++i, ++idx)
            {
                TopoDS_Edge edge0 = TopoDS::Edge(idxMapOfEdge0(i));
                TopoDS_Edge edge1 = TopoDS::Edge(idxMapOfEdge1(i));
                TopoDS_Edge edge2 = TopoDS::Edge(idxMapOfEdge2(i));
                assert(edge0 == edgeNameInfos[idx].edge);
                const TopoUtil::EdgeNamingInfo& edgeInfo = edgeNameInfos[idx];
                topoNaming.setName(edge1, TopoNameBuilder().id(edgeInfo.id).build()); // 底面边: 草图曲线ID
                if (!edge2.IsEqual(edge1)) // 旋转轴与边重合,旋转角度不是360度比如270度时,与旋转轴重合的边在顶面和底面是共用的
                {
                    topoNaming.setName(edge2, TopoNameBuilder().id(edgeInfo.id).id(elemIdValue).build()); // 顶面边: 草图曲线ID + 特征ID
                }
            }
        }
        else
        {
            assert(false);
        }
    }

    // 记录侧边的拓扑命名
    auto recordSideEdgeName = [&makeSweep, &topoNaming](const TopoDS_Vertex& vertex,
        unsigned int id1, unsigned int id2)
    {
        const TopTools_ListOfShape& generated = makeSweep.Generated(vertex);
        assert(generated.Extent() == 1 || generated.Extent() == 0); // 当旋转轴于边重合时,此时边的顶点的侧边不存在
        for (TopTools_ListIteratorOfListOfShape iter(generated); iter.More(); iter.Next())
        {
            const TopoDS_Shape& shape = iter.Value();
            assert(!shape.IsNull());
            assert(shape.ShapeType() == TopAbs_ShapeEnum::TopAbs_EDGE);
            topoNaming.setName(shape, TopoNameBuilder().id(id1).id(id2).build());
        }
    };

    // 建立侧边的拓扑命名
    // 格式: 草图曲线ID + 草图曲线ID
    size_t num = edgeNameInfos.size();
    if (num == 1)
    {
        // 单条曲线：两个顶点的生成边，用序号 1/2 区分
        const TopoUtil::EdgeNamingInfo& edgeInfo = edgeNameInfos[0];
        TopoDS_Vertex v1, v2;
        TopExp::Vertices(edgeInfo.edge, v1, v2);
        auto nameSideEdge = [&](const TopoDS_Vertex& v, unsigned int seq)
        {
            const TopTools_ListOfShape& generated = makeSweep.Generated(v);
            for (TopTools_ListIteratorOfListOfShape iter(generated); iter.More(); iter.Next())
            {
                const TopoDS_Shape& shape = iter.Value();
                assert(shape.ShapeType() == TopAbs_ShapeEnum::TopAbs_EDGE);
                topoNaming.setName(shape,
                    TopoNameBuilder().id(edgeInfo.id).id(edgeInfo.id).index(seq).build());
            }
        };
        nameSideEdge(v1, 1);
        nameSideEdge(v2, 2);
    }
    else
    {
        TopoShapeSet visitedVertices;
        for (size_t i = 0; i < num; ++i)
        {
            // 边和下一条相邻的边
            const TopoUtil::EdgeNamingInfo& edgeNameInfo = edgeNameInfos[i];
            const TopoUtil::EdgeNamingInfo& edgeNameInfoNext = edgeNameInfo.sibling == size_t(-1) ?
                edgeNameInfos[(i + 1) % num] : edgeNameInfos[edgeNameInfo.sibling];

            // 提取边1的顶点
            TopoDS_Vertex v1_first, v1_last;
            TopExp::Vertices(edgeNameInfo.edge, v1_first, v1_last);
            // 提取边2的顶点
            TopoDS_Vertex v2_first, v2_last;
            TopExp::Vertices(edgeNameInfoNext.edge, v2_first, v2_last);

            // 对共有的顶点求生成的侧边
            if (v1_last.IsPartner(v2_first) || v1_last.IsPartner(v2_last))
            {
                // 对于圆弧边和直线段边构成的轮廓(只有两条边),有可能两次循环访问的都是同一个顶点,所以需要先判断下是否已经访问过.
                if (visitedVertices.find(v1_last) == visitedVertices.cend())
                {
                    visitedVertices.insert(v1_last);
                    recordSideEdgeName(v1_last, edgeNameInfo.id, edgeNameInfoNext.id);
                    continue;
                }
            }
            if (v1_first.IsPartner(v2_first) || v1_first.IsPartner(v2_last))
            {
                // 对于圆弧边和直线段边构成的轮廓(只有两条边),有可能两次循环访问的都是同一个顶点,所以需要先判断下是否已经访问过.
                if (visitedVertices.find(v1_first) == visitedVertices.cend())
                {
                    visitedVertices.insert(v1_first);
                    recordSideEdgeName(v1_first, edgeNameInfo.id, edgeNameInfoNext.id);
                    continue;
                }
            }
        }

        // Name side edges at free endpoints of open chains.
        // For closed loops every vertex is already in visitedVertices, so this is a no-op.
        const TopoUtil::EdgeNamingInfo* edgeInfoPtrs[2] = {
            &edgeNameInfos.front(),
            &edgeNameInfos.back()
        };
        for (const TopoUtil::EdgeNamingInfo* pEdgeNameInfo : edgeInfoPtrs)
        {
            const TopoUtil::EdgeNamingInfo& edgeInfo = *pEdgeNameInfo;
            TopoDS_Vertex v1, v2;
            TopExp::Vertices(edgeInfo.edge, v1, v2);
            if (!v1.IsNull() && visitedVertices.find(v1) == visitedVertices.cend())
                recordSideEdgeName(v1, edgeInfo.id, edgeInfo.id);
            if (!v2.IsNull() && visitedVertices.find(v2) == visitedVertices.cend())
                recordSideEdgeName(v2, edgeInfo.id, edgeInfo.id);
        }
    }

    return true;
}

bool TopoNamingUtil::naming(
    const TopoDS_Wire& pathWire,
    const TopoDS_Wire& profileWire,
    BRepOffsetAPI_MakePipeShell& makePipeShell,
    const std::vector<TopoUtil::EdgeNamingInfo>& pathEdgeNameInfos,
    const std::vector<TopoUtil::EdgeNamingInfo>& profileEdgeNameInfos,
    unsigned int elemIdValue,
    TopoNaming& topoNaming)
{
    //-----------------------------------------------------
    // 建立所有侧面的拓扑命名
    // 格式: 轮廓曲线ID + 路径曲线ID
    //-----------------------------------------------------
    std::unordered_map<TopoDS_Shape, std::pair<unsigned int, unsigned int>, ShapeHasher, ShapeEqual> sideFaceNames;
    for (const TopoUtil::EdgeNamingInfo& edgeNameInfo : profileEdgeNameInfos)
    {
        const TopTools_ListOfShape& generated = makePipeShell.Generated(edgeNameInfo.edge);
        assert(generated.Extent() == pathEdgeNameInfos.size());
        for (TopTools_ListIteratorOfListOfShape iter(generated); iter.More(); iter.Next())
        {
            const TopoDS_Shape& shape = iter.Value();
            assert(!shape.IsNull());
            assert(shape.ShapeType() == TopAbs_ShapeEnum::TopAbs_FACE);
            sideFaceNames[shape] = std::pair<unsigned int, unsigned int>(edgeNameInfo.id, 0);
        }
    }
    for (const TopoUtil::EdgeNamingInfo& edgeNameInfo : pathEdgeNameInfos)
    {
        const TopTools_ListOfShape& generated = makePipeShell.Generated(edgeNameInfo.edge);
        assert(generated.Extent() == profileEdgeNameInfos.size());
        for (TopTools_ListIteratorOfListOfShape iter(generated); iter.More(); iter.Next())
        {
            const TopoDS_Shape& shape = iter.Value();
            assert(!shape.IsNull());
            assert(shape.ShapeType() == TopAbs_ShapeEnum::TopAbs_FACE);
            auto iterSideFaceName = sideFaceNames.find(shape);
            if (iterSideFaceName != sideFaceNames.cend())
            {
                iterSideFaceName->second.second = edgeNameInfo.id;
            }
            else
            {
                assert(false);
            }
        }
    }
    for (const auto& kvp : sideFaceNames)
    {
        if (kvp.second.first != 0 && kvp.second.second != 0)
        {
            topoNaming.setName(kvp.first, TopoNameBuilder().id(kvp.second.first).id(kvp.second.second).build());
        }
        else
        {
            assert(false);
        }
    }

    //-----------------------------------------------------
    // 建立底面的拓扑命名
    // 格式: 特征ID
    //-----------------------------------------------------
    TopoDS_Shape firstShape = makePipeShell.FirstShape();
    TopoDS_Face bottomFace;
    if (!firstShape.IsNull()) // 如果路径草图是封闭的并且曲线数量>1则firstShape为空
    {
        TopAbs_ShapeEnum type = firstShape.ShapeType();
        if (TopAbs_ShapeEnum::TopAbs_FACE == type)
        {
            bottomFace = TopoDS::Face(firstShape);
            assert(!bottomFace.IsNull());
            topoNaming.setName(bottomFace, TopoNameBuilder().id(elemIdValue).build());
        }
        // 如果路径草图是闭合的曲线比如圆,则会进入此分支
        else if (TopAbs_ShapeEnum::TopAbs_WIRE == type)
        {
            TopTools_IndexedMapOfShape idxMapOfEdge0;
            TopExp::MapShapes(profileWire, TopAbs_ShapeEnum::TopAbs_EDGE, idxMapOfEdge0);
            TopTools_IndexedMapOfShape idxMapOfEdge1;
            TopExp::MapShapes(firstShape, TopAbs_ShapeEnum::TopAbs_EDGE, idxMapOfEdge1);
            int countEdges = idxMapOfEdge0.Extent();
            if (countEdges == idxMapOfEdge1.Extent() &&
                static_cast<size_t>(countEdges) == profileEdgeNameInfos.size())
            {
                size_t idx(0);
                for (int i = 1; i <= countEdges; ++i, ++idx)
                {
                    TopoDS_Edge edge0 = TopoDS::Edge(idxMapOfEdge0(i));
                    TopoDS_Edge edge1 = TopoDS::Edge(idxMapOfEdge1(i));
                    assert(edge0 == profileEdgeNameInfos[idx].edge);
                    const TopoUtil::EdgeNamingInfo& edgeInfo = profileEdgeNameInfos[idx];
                    topoNaming.setName(edge1, TopoNameBuilder().id(edgeInfo.id).build()); // 底面边: 草图曲线ID
                }
            }
            else
            {
                assert(false);
            }
        }
        else
        {
            assert(false);
        }
    }

    //-----------------------------------------------------
    // 建立顶面的拓扑命名
    // 格式: 特征ID + 特征ID
    //-----------------------------------------------------
    TopoDS_Shape lastShape = makePipeShell.LastShape();
    TopoDS_Face topFace;
    // 如果路径草图是封闭的并且曲线数量>1则firstShape为空
    // 如果路径草图是闭合的曲线比如圆, 则lastShape的类型为TopAbs_WIRE
    if (!lastShape.IsNull() && TopAbs_ShapeEnum::TopAbs_FACE == lastShape.ShapeType())
    {
        topFace = TopoDS::Face(lastShape);
        assert(!topFace.IsNull());
        topoNaming.setName(topFace, TopoNameBuilder().id(elemIdValue).id(elemIdValue).build());
    }

    //-----------------------------------------------------
    // 建立底面和顶面所有边的拓扑命名
    // 底面边: 草图曲线ID
    // 顶面边: 草图曲线ID + 特征ID
    //-----------------------------------------------------
    if (!bottomFace.IsNull() && !topFace.IsNull())
    {
        TopTools_IndexedMapOfShape idxMapOfEdge0;
        TopExp::MapShapes(profileWire, TopAbs_ShapeEnum::TopAbs_EDGE, idxMapOfEdge0);
        TopTools_IndexedMapOfShape idxMapOfEdge1;
        TopExp::MapShapes(bottomFace, TopAbs_ShapeEnum::TopAbs_EDGE, idxMapOfEdge1);
        TopTools_IndexedMapOfShape idxMapOfEdge2;
        TopExp::MapShapes(topFace, TopAbs_ShapeEnum::TopAbs_EDGE, idxMapOfEdge2);
        int countEdges = idxMapOfEdge0.Extent();
        assert(countEdges == idxMapOfEdge1.Extent());
        assert(countEdges == idxMapOfEdge2.Extent());
        assert(static_cast<size_t>(countEdges) == profileEdgeNameInfos.size());
        if (countEdges == idxMapOfEdge1.Extent() &&
            countEdges == idxMapOfEdge2.Extent() &&
            static_cast<size_t>(countEdges) == profileEdgeNameInfos.size())
        {
            size_t idx(0);
            for (int i = 1; i <= countEdges; ++i, ++idx)
            {
                TopoDS_Edge edge0 = TopoDS::Edge(idxMapOfEdge0(i));
                TopoDS_Edge edge1 = TopoDS::Edge(idxMapOfEdge1(i));
                TopoDS_Edge edge2 = TopoDS::Edge(idxMapOfEdge2(i));
                assert(edge0 == profileEdgeNameInfos[idx].edge);
                const TopoUtil::EdgeNamingInfo& edgeInfo = profileEdgeNameInfos[idx];
                topoNaming.setName(edge1, TopoNameBuilder().id(edgeInfo.id).build()); // 底面边: 草图曲线ID
                topoNaming.setName(edge2, TopoNameBuilder().id(edgeInfo.id).id(elemIdValue).build()); // 顶面边: 草图曲线ID + 特征ID
            }
        }
    }

    //-----------------------------------------------------
    // 建立侧边的拓扑命名
    // 格式: 轮廓草图曲线ID + 轮廓草图曲线ID + 路径曲线ID
    // 记录侧边的拓扑命名
    //-----------------------------------------------------
    auto recordSideEdgeName = [&makePipeShell, &topoNaming, &pathEdgeNameInfos](const TopoDS_Vertex& vertex,
        unsigned int id1, unsigned int id2)
    {
        const TopTools_ListOfShape& generated = makePipeShell.Generated(vertex);
        if (generated.Extent() != pathEdgeNameInfos.size())
        {
            assert(false);
            return;
        }
        unsigned int i(0);
        for (TopTools_ListIteratorOfListOfShape iter(generated); iter.More(); iter.Next(), ++i)
        {
            const TopoDS_Shape& shape = iter.Value();
            assert(!shape.IsNull());
            assert(shape.ShapeType() == TopAbs_ShapeEnum::TopAbs_EDGE);
            topoNaming.setName(shape, TopoNameBuilder().id(id1).id(id2).id(pathEdgeNameInfos[i].id).build());
        }
    };
    size_t num = profileEdgeNameInfos.size();
    TopoShapeSet visitedVertices;
    for (size_t i = 0; i < num; ++i)
    {
        // 边和下一条相邻的边
        const TopoUtil::EdgeNamingInfo& edgeNameInfo = profileEdgeNameInfos[i];
        const TopoUtil::EdgeNamingInfo& edgeNameInfoNext = edgeNameInfo.sibling == size_t(-1) ?
            profileEdgeNameInfos[i + 1] : profileEdgeNameInfos[edgeNameInfo.sibling];

        // 提取边1的顶点
        TopoDS_Vertex v1_first, v1_last;
        TopExp::Vertices(edgeNameInfo.edge, v1_first, v1_last);
        // 提取边2的顶点
        TopoDS_Vertex v2_first, v2_last;
        TopExp::Vertices(edgeNameInfoNext.edge, v2_first, v2_last);

        // 对共有的顶点求生成的侧边
        if (v1_last.IsPartner(v2_first) || v1_last.IsPartner(v2_last))
        {
            // 对于圆弧边和直线段边构成的轮廓(只有两条边),有可能两次循环访问的都是同一个顶点,所以需要先判断下是否已经访问过.
            if (visitedVertices.find(v1_last) == visitedVertices.cend())
            {
                visitedVertices.insert(v1_last);
                recordSideEdgeName(v1_last, edgeNameInfo.id, edgeNameInfoNext.id);
                continue;
            }
        }
        if (v1_first.IsPartner(v2_first) || v1_first.IsPartner(v2_last))
        {
            // 对于圆弧边和直线段边构成的轮廓(只有两条边),有可能两次循环访问的都是同一个顶点,所以需要先判断下是否已经访问过.
            if (visitedVertices.find(v1_first) == visitedVertices.cend())
            {
                visitedVertices.insert(v1_first);
                recordSideEdgeName(v1_first, edgeNameInfo.id, edgeNameInfoNext.id);
                continue;
            }
        }
    }

    //-----------------------------------------------------
    // 建立结合面边的拓扑命名
    // 格式: 路径草图曲线ID1 + 路径草图曲线ID2 + 轮廓草图曲线ID or 序号
    //-----------------------------------------------------
    auto recordJointEdgeName = [&makePipeShell, &topoNaming, &profileEdgeNameInfos](const TopoDS_Vertex& vertex,
        unsigned int id1, unsigned int id2)
    {
        const TopTools_ListOfShape& generated = makePipeShell.Generated(vertex);
        if (generated.Extent() == profileEdgeNameInfos.size())
        {
            unsigned int i(0);
            for (TopTools_ListIteratorOfListOfShape iter(generated); iter.More(); iter.Next())
            {
                const TopoDS_Shape& shape = iter.Value();
                assert(!shape.IsNull());
                assert(shape.ShapeType() == TopAbs_ShapeEnum::TopAbs_EDGE);
                topoNaming.setName(shape, TopoNameBuilder().id(id1).id(id2).id(profileEdgeNameInfos[i++].id).build());
            }
        }
        // 当轮廓是闭合曲线比如圆时,结合面边的数量可能>1;
        // 当轮廓由多段圆弧+直线段构成时;会存在一段圆弧对应多条结合面的边的情况;通过拓扑分析应该能够区分出来但感觉没必要,这个复杂度太大;适度简化直接以序号来命名;
        else 
        {
            unsigned int i(0);
            for (TopTools_ListIteratorOfListOfShape iter(generated); iter.More(); iter.Next())
            {
                const TopoDS_Shape& shape = iter.Value();
                assert(!shape.IsNull());
                assert(shape.ShapeType() == TopAbs_ShapeEnum::TopAbs_EDGE);
                topoNaming.setName(shape, TopoNameBuilder().id(id1).id(id2).index(++i).build());
            }
        }
    };
    num = pathEdgeNameInfos.size();
    visitedVertices.clear();
    for (size_t i = 0; i < num; ++i)
    {
        if (i == num - 1) continue;

        // 边和下一条相邻的边
        const TopoUtil::EdgeNamingInfo& edgeNameInfo = pathEdgeNameInfos[i];
        const TopoUtil::EdgeNamingInfo& edgeNameInfoNext = edgeNameInfo.sibling == size_t(-1) ?
            pathEdgeNameInfos[i + 1] : pathEdgeNameInfos[edgeNameInfo.sibling];

        // 提取边1的顶点
        TopoDS_Vertex v1_first, v1_last;
        TopExp::Vertices(edgeNameInfo.edge, v1_first, v1_last);
        // 提取边2的顶点
        TopoDS_Vertex v2_first, v2_last;
        TopExp::Vertices(edgeNameInfoNext.edge, v2_first, v2_last);

        // 对共有的顶点求生成的侧边
        if (v1_last.IsPartner(v2_first) || v1_last.IsPartner(v2_last))
        {
            // 对于圆弧边和直线段边构成的轮廓(只有两条边),有可能两次循环访问的都是同一个顶点,所以需要先判断下是否已经访问过.
            if (visitedVertices.find(v1_last) == visitedVertices.cend())
            {
                visitedVertices.insert(v1_last);
                recordJointEdgeName(v1_last, edgeNameInfo.id, edgeNameInfoNext.id);
                continue;
            }
        }
        if (v1_first.IsPartner(v2_first) || v1_first.IsPartner(v2_last))
        {
            // 对于圆弧边和直线段边构成的轮廓(只有两条边),有可能两次循环访问的都是同一个顶点,所以需要先判断下是否已经访问过.
            if (visitedVertices.find(v1_first) == visitedVertices.cend())
            {
                visitedVertices.insert(v1_first);
                recordJointEdgeName(v1_first, edgeNameInfo.id, edgeNameInfoNext.id);
                continue;
            }
        }
    }

    return true;
}

bool TopoNamingUtil::naming(
    const std::vector<TopoUtil::WireInfo>& profileWireInfos,
    BRepOffsetAPI_ThruSections& makeLoft,
    unsigned int elemIdValue,
    TopoNaming& topoNaming)
{
    if (profileWireInfos.size() < 2)
    {
        assert(false);
        return false;
    }

    //-----------------------------------------------------
    // 建立所有侧面的拓扑命名
    // 格式: 草图曲线的ID
    //-----------------------------------------------------
    const std::vector<TopoUtil::EdgeNamingInfo>* pEdgeNameInfos(nullptr);
    if (!profileWireInfos.front().edgeNameInfos.empty()) // 收尾轮廓可能为空
    {
        pEdgeNameInfos = &(profileWireInfos.front().edgeNameInfos);
    }
    else if (!profileWireInfos.back().edgeNameInfos.empty()) // 收尾轮廓可能为空
    {
        pEdgeNameInfos = &(profileWireInfos.back().edgeNameInfos);
    }
    else if (profileWireInfos.size() > 2)
    {
        pEdgeNameInfos = &(profileWireInfos[1].edgeNameInfos);
        if (pEdgeNameInfos->empty())
        {
            assert(false);
            return false;
        }
    }
    if (!pEdgeNameInfos)
    {
        assert(false);
        return false;
    }
    for (const TopoUtil::EdgeNamingInfo& edgeNameInfo : *pEdgeNameInfos)
    {
        const TopTools_ListOfShape& generated = makeLoft.Generated(edgeNameInfo.edge); 
        if (generated.Extent() == 1)
        {
            const TopoDS_Shape& shape = generated.First();
            assert(!shape.IsNull());
            assert(shape.ShapeType() == TopAbs_ShapeEnum::TopAbs_FACE);
            topoNaming.setName(shape, TopoNameBuilder().id(edgeNameInfo.id).build());
        }
        else
        {
            // 经实践:当轮廓是闭合曲线时,会生成两个侧面
            assert(generated.Extent() == 2);
            unsigned int index(0);
            for (TopTools_ListIteratorOfListOfShape iter(generated); iter.More(); iter.Next())
            {
                const TopoDS_Shape& shape = iter.Value();
                assert(!shape.IsNull());
                assert(shape.ShapeType() == TopAbs_ShapeEnum::TopAbs_FACE);
                topoNaming.setName(shape, TopoNameBuilder().id(edgeNameInfo.id).index(++index).build());
            }
        }
    }

    //-----------------------------------------------------
    // 建立底面的拓扑命名: 特征ID
    //-----------------------------------------------------
    TopoDS_Shape firstShape = makeLoft.FirstShape();
    if (!firstShape.IsNull() && firstShape.ShapeType() == TopAbs_ShapeEnum::TopAbs_FACE)
    {
        TopoDS_Face bottomFace = TopoDS::Face(firstShape);
        topoNaming.setName(bottomFace, TopoNameBuilder().id(elemIdValue).build());

        // 建立底面所有边的拓扑命名
        // 底面边: 草图曲线ID
        TopTools_IndexedMapOfShape idxMapOfEdge;
        TopExp::MapShapes(bottomFace, TopAbs_ShapeEnum::TopAbs_EDGE, idxMapOfEdge);
        int countEdges = idxMapOfEdge.Extent();
        const std::vector<TopoUtil::EdgeNamingInfo>& edgeNameInfos = profileWireInfos.front().edgeNameInfos;
        if (static_cast<size_t>(countEdges) == edgeNameInfos.size())
        {
            size_t idx(0);
            for (int i = 1; i <= countEdges; ++i, ++idx)
            {
                TopoDS_Edge edge = TopoDS::Edge(idxMapOfEdge(i));
                const TopoUtil::EdgeNamingInfo& edgeInfo = edgeNameInfos[idx];
                topoNaming.setName(edge, TopoNameBuilder().id(edgeInfo.id).build()); // 底面边: 草图曲线ID
            }
        }
        else if (edgeNameInfos.size() == 1 && countEdges > 1)
        {
            // 经实践:当底面轮廓是闭合曲线时,会生成两条边
            assert(countEdges == 2);
            const TopoUtil::EdgeNamingInfo& edgeInfo = edgeNameInfos.front();
            unsigned int index(0);
            for (int i = 1; i <= countEdges; ++i)
            {
                TopoDS_Edge edge = TopoDS::Edge(idxMapOfEdge(i));
                topoNaming.setName(edge, TopoNameBuilder().id(edgeInfo.id).index(++index).build()); // 底面边: 草图曲线ID, 序号
            }
        }
        else
        {
            assert(false);
        }
    }

    //-----------------------------------------------------
    // 建立顶面的拓扑命名: 特征ID + 特征ID
    //-----------------------------------------------------
    TopoDS_Shape lastShape = makeLoft.LastShape();
    if (!lastShape.IsNull() && lastShape.ShapeType() == TopAbs_ShapeEnum::TopAbs_FACE)
    {
        TopoDS_Face topFace = TopoDS::Face(lastShape);
        topoNaming.setName(topFace, TopoNameBuilder().id(elemIdValue).id(elemIdValue).build());

        // 建立顶面所有边的拓扑命名
        // 底面边: 草图曲线ID
        TopTools_IndexedMapOfShape idxMapOfEdge;
        TopExp::MapShapes(topFace, TopAbs_ShapeEnum::TopAbs_EDGE, idxMapOfEdge);
        int countEdges = idxMapOfEdge.Extent();
        const std::vector<TopoUtil::EdgeNamingInfo>& edgeNameInfos = profileWireInfos.back().edgeNameInfos;
        if (static_cast<size_t>(countEdges) == edgeNameInfos.size())
        {
            size_t idx(0);
            for (int i = 1; i <= countEdges; ++i, ++idx)
            {
                TopoDS_Edge edge = TopoDS::Edge(idxMapOfEdge(i));
                const TopoUtil::EdgeNamingInfo& edgeInfo = edgeNameInfos[idx];
                topoNaming.setName(edge, TopoNameBuilder().id(edgeInfo.id).build()); // 顶面边: 草图曲线ID
            }
        }
        else if (edgeNameInfos.size() == 1 && countEdges > 1)
        {
            // 经实践:当底面轮廓是闭合曲线时,会生成两条边
            assert(countEdges == 2);
            const TopoUtil::EdgeNamingInfo& edgeInfo = edgeNameInfos.front();
            unsigned int index(0);
            for (int i = 1; i <= countEdges; ++i)
            {
                TopoDS_Edge edge = TopoDS::Edge(idxMapOfEdge(i));
                topoNaming.setName(edge, TopoNameBuilder().id(edgeInfo.id).index(++index).build()); // 顶面边: 草图曲线ID, 序号
            }
        }
        else
        {
            assert(false);
        }
    }

    //-----------------------------------------------------
    // 建立侧边的拓扑命名
    // 格式: 草图曲线ID + 草图曲线ID
    //-----------------------------------------------------
    // 记录侧边的拓扑命名
    auto recordSideEdgeName = [&makeLoft, &topoNaming](const TopoDS_Vertex& vertex,
        unsigned int id1, unsigned int id2)
    {
        const TopTools_ListOfShape& generated = makeLoft.Generated(vertex);
        assert(generated.Extent() == 1 || generated.Extent() == 0); // 当旋转轴于边重合时,此时边的顶点的侧边不存在
        for (TopTools_ListIteratorOfListOfShape iter(generated); iter.More(); iter.Next())
        {
            const TopoDS_Shape& shape = iter.Value();
            assert(!shape.IsNull());
            assert(shape.ShapeType() == TopAbs_ShapeEnum::TopAbs_EDGE);
            topoNaming.setName(shape, TopoNameBuilder().id(id1).id(id2).build());
        }
    };

    const std::vector<TopoUtil::EdgeNamingInfo>& edgeNameInfos = *pEdgeNameInfos;
    size_t num = edgeNameInfos.size();
    TopoShapeSet visitedVertices;
    for (size_t i = 0; i < num; ++i)
    {
        // 边和下一条相邻的边
        const TopoUtil::EdgeNamingInfo& edgeNameInfo = edgeNameInfos[i];
        const TopoUtil::EdgeNamingInfo& edgeNameInfoNext = edgeNameInfo.sibling == size_t(-1) ?
            edgeNameInfos[i + 1] : edgeNameInfos[edgeNameInfo.sibling];

        // 提取边1的顶点
        TopoDS_Vertex v1_first, v1_last;
        TopExp::Vertices(edgeNameInfo.edge, v1_first, v1_last);
        // 提取边2的顶点
        TopoDS_Vertex v2_first, v2_last;
        TopExp::Vertices(edgeNameInfoNext.edge, v2_first, v2_last);

        // 对共有的顶点求生成的侧边
        if (v1_last.IsPartner(v2_first) || v1_last.IsPartner(v2_last))
        {
            // 对于圆弧边和直线段边构成的轮廓(只有两条边),有可能两次循环访问的都是同一个顶点,所以需要先判断下是否已经访问过.
            if (visitedVertices.find(v1_last) == visitedVertices.cend())
            {
                visitedVertices.insert(v1_last);
                recordSideEdgeName(v1_last, edgeNameInfo.id, edgeNameInfoNext.id);
                continue;
            }
        }
        if (v1_first.IsPartner(v2_first) || v1_first.IsPartner(v2_last))
        {
            // 对于圆弧边和直线段边构成的轮廓(只有两条边),有可能两次循环访问的都是同一个顶点,所以需要先判断下是否已经访问过.
            if (visitedVertices.find(v1_first) == visitedVertices.cend())
            {
                visitedVertices.insert(v1_first);
                recordSideEdgeName(v1_first, edgeNameInfo.id, edgeNameInfoNext.id);
                continue;
            }
        }
    }

    //-----------------------------------------------------
    // 遗漏的边
    //-----------------------------------------------------
    const TopoDS_Shape& shape = makeLoft.Shape();

    // 1.遍历找寻有无遗漏的边
    std::vector<TopoDS_Edge> omittedEdges;
    omittedEdges.reserve(10);
    TopTools_IndexedMapOfShape idxMapOfEdge;
    TopExp::MapShapes(shape, TopAbs_ShapeEnum::TopAbs_EDGE, idxMapOfEdge);
    for (int i = 1; i <= idxMapOfEdge.Extent(); ++i)
    {
        TopoDS_Edge edge = TopoDS::Edge(idxMapOfEdge(i));
        if (topoNaming.contains(edge)) continue;
        omittedEdges.emplace_back(edge);
    }
    if (omittedEdges.empty())
    {
        return true;
    }

    // 2.建立放样体中:边<>面的映射
    TopTools_IndexedDataMapOfShapeListOfShape newEdgeToFacesMap;
    TopExp::MapShapesAndAncestors(shape, TopAbs_EDGE, TopAbs_FACE, newEdgeToFacesMap);

    // 3.对遗漏的边命名
    unsigned int index(0);
    TopoName names1st, names2nd;
    for (const TopoDS_Edge& edge : omittedEdges)
    {
        const TopTools_ListOfShape& faceList = newEdgeToFacesMap.FindFromKey(edge);
        if (faceList.IsEmpty())
        {
            assert(false);
            continue;
        }

        if (faceList.Extent() == 2)
        {
            names1st.clear();
            const TopoDS_Shape& face1 = faceList.First();
            if (!topoNaming.getName(face1, names1st))
            {
                assert(false);
                continue;
            }

            names2nd.clear();
            const TopoDS_Shape& face2 = faceList.Last();
            if (!topoNaming.getName(face2, names2nd))
            {
                assert(false);
                continue;
            }

            if (names1st > names2nd)
            {
                std::swap(names1st, names2nd);
            }
            topoNaming.setName(edge, TopoNameBuilder(names1st).source(names2nd).build());
        }
        else // 超出预期外的边: 直接用特征ID+序号来命名
        {
            assert(false);
            topoNaming.setName(edge, TopoNameBuilder().id(elemIdValue).index(++index).build());
        }
    }

    return true;
}

bool TopoNamingUtil::primitiveNaming(
    const TopoDS_Shape& shape,
    unsigned int elemIdValue,
    TopoNaming& topoNaming)
{
    unsigned int index(0);

    // 对面命名
    TopTools_IndexedMapOfShape faceMap;
    TopExp::MapShapes(shape, TopAbs_ShapeEnum::TopAbs_FACE, faceMap);
    for (int i = 1; i <= faceMap.Extent(); ++i)
    {
        topoNaming.setName(faceMap(i), TopoNameBuilder().id(elemIdValue).index(++index).build());
    }

    // 对边命名
    TopTools_IndexedMapOfShape edgeMap;
    TopExp::MapShapes(shape, TopAbs_ShapeEnum::TopAbs_EDGE, edgeMap);
    for (int i = 1; i <= edgeMap.Extent(); ++i)
    {
        topoNaming.setName(edgeMap(i), TopoNameBuilder().id(elemIdValue).index(++index).build());
    }

    return true;
}

bool TopoNamingUtil::naming(
    const TopoDS_Shape& sourceShape,
    const TopoNaming& sourceNaming,
    BRepOffsetAPI_MakeOffsetShape& mkOffset,
    unsigned int elemIdValue,
    TopoNaming& topoNaming,
    std::uint32_t index)
{
    // name offset faces from source faces: v1:<srcFace>+@offsetId
    TopTools_IndexedMapOfShape sourceFaces;
    TopExp::MapShapes(sourceShape, TopAbs_FACE, sourceFaces);
    for (int i = 1; i <= sourceFaces.Extent(); ++i)
    {
        const TopoDS_Shape& sourceFace = sourceFaces(i);
        const TopTools_ListOfShape& generated = mkOffset.Generated(sourceFace);
        if (generated.IsEmpty()) continue;
        assert(generated.Size() == 1);
        const TopoDS_Shape& offsetFace = generated.First();

        TopoName sourceName;
        if (!sourceNaming.getName(sourceFace, sourceName) || sourceName.empty())
        {
            assert(false);
            continue;
        }

        TopoNameBuilder builder(sourceName);
        builder.generated(elemIdValue);
        if (index > 0) builder.index(index);
        topoNaming.setName(offsetFace, builder.build());
    }

    // name offset edges from source edges: v1:<srcEdge>+@offsetId
    TopTools_IndexedMapOfShape sourceEdges;
    TopExp::MapShapes(sourceShape, TopAbs_EDGE, sourceEdges);
    for (int i = 1; i <= sourceEdges.Extent(); ++i)
    {
        const TopoDS_Shape& sourceEdge = sourceEdges(i);
        const TopTools_ListOfShape& generated = mkOffset.Generated(sourceEdge);
        if (generated.IsEmpty()) continue;
        assert(generated.Size() == 1);
        const TopoDS_Shape& offsetEdge = generated.First();

        TopoName sourceName;
        if (!sourceNaming.getName(sourceEdge, sourceName) || sourceName.empty())
        {
            assert(false);
            continue;
        }

        TopoNameBuilder builder(sourceName);
        builder.generated(elemIdValue);
        if (index > 0) builder.index(index);
        topoNaming.setName(offsetEdge, builder.build());
    }

    return true;
}

bool TopoNamingUtil::patternNaming(
    const TopoDS_Shape& sourceShape,
    const wy3d::TopoNaming& sourceNaming,
    const std::vector<unsigned int>& suffix,
    BRepBuilderAPI_Transform& transform,
    TopoNaming& topoNaming)
{
    if (suffix.empty())
    {
        assert(false);
        return false;
    }

    TopTools_IndexedMapOfShape sourceFaces;
    TopExp::MapShapes(sourceShape, TopAbs_FACE, sourceFaces);
    for (int i = 1; i <= sourceFaces.Extent(); ++i)
    {
        const TopoDS_Shape& sourceFace = sourceFaces(i);
        TopoDS_Shape modifiedShape = transform.ModifiedShape(sourceFace);
        assert(!modifiedShape.IsNull());

        TopoName sourceName;
        if (!sourceNaming.getName(sourceFace, sourceName) || sourceName.empty())
        {
            assert(false);
            continue;
        }
        TopoNameBuilder builder(sourceName);
        builder.generated(suffix.front());
        for (size_t suffixIndex = 1; suffixIndex < suffix.size(); ++suffixIndex)
        {
            builder.index(suffix[suffixIndex]);
        }
        topoNaming.setName(modifiedShape, builder.build());
    }

    TopTools_IndexedMapOfShape sourceEdges;
    TopExp::MapShapes(sourceShape, TopAbs_EDGE, sourceEdges);
    for (int i = 1; i <= sourceEdges.Extent(); ++i)
    {
        const TopoDS_Shape& sourceEdge = sourceEdges(i);
        TopoDS_Shape modifiedShape = transform.ModifiedShape(sourceEdge);
        assert(!modifiedShape.IsNull());

        TopoName sourceName;
        if (!sourceNaming.getName(sourceEdge, sourceName) || sourceName.empty())
        {
            assert(false);
            continue;
        }
        TopoNameBuilder builder(sourceName);
        builder.generated(suffix.front());
        for (size_t suffixIndex = 1; suffixIndex < suffix.size(); ++suffixIndex)
        {
            builder.index(suffix[suffixIndex]);
        }
        topoNaming.setName(modifiedShape, builder.build());
    }

    return true;
}

TopoNameList TopoNamingUtil::computeNewFaces(const ShapeDelta& faceDelta, const TopoNaming& topoNaming)
{
    TopoNameList newFaceNames;

    // 没有生成新面
    size_t numNewFaces = faceDelta.addedSingle.size() + faceDelta.addedDouble.size() + faceDelta.addedMulti.size();
    if (0 == numNewFaces)
    {
        return newFaceNames;
    }

    newFaceNames.reserve(numNewFaces);
    auto assemblyFaceName = [&newFaceNames, &topoNaming](const TopoDS_Shape& newFace)
    {
        TopoName faceName;
        if (topoNaming.getName(newFace, faceName) && !faceName.empty())
        {
            newFaceNames.emplace_back(std::move(faceName));
        }
        else
        {
            assert(false);
        }
    };
    for (const auto& kvp : faceDelta.addedSingle)
    {
        assemblyFaceName(kvp.first);
    }
    for (const auto& kvp : faceDelta.addedDouble)
    {
        assemblyFaceName(kvp.first);
    }
    for (const auto& kvp : faceDelta.addedMulti)
    {
        assemblyFaceName(kvp.first);
    }

    return newFaceNames;
}

TopoNameList TopoNamingUtil::computeFacesFromShape(
    const ShapeDelta& faceDelta,
    const TopoNaming& topoNaming,
    const TopoDS_Shape& shape)
{
    TopoNameList faceNames;

    TopoShapeSet faceSet;
    TopTools_IndexedMapOfShape faceMap;
    TopExp::MapShapes(shape, TopAbs_ShapeEnum::TopAbs_FACE, faceMap);
    for (int i = 1; i <= faceMap.Extent(); ++i)
    {
        const TopoDS_Shape& oldShape = faceMap(i);
        faceSet.insert(oldShape);
    }

    std::vector<TopoDS_Shape> facesFromShape;
    facesFromShape.reserve(20);
    for (int i = 1; i <= faceMap.Extent(); ++i)
    {
        const TopoDS_Shape& oldShape = faceMap(i);

        auto iterKept = faceDelta.kept.find(oldShape);
        if (iterKept != faceDelta.kept.cend())
        {
            facesFromShape.emplace_back(*iterKept);
            continue;
        }

        auto iterModified = faceDelta.modified.find(oldShape);
        if (iterModified != faceDelta.modified.cend())
        {
            facesFromShape.emplace_back(iterModified->second);
            continue;
        }
    }
    for (const auto& kvp : faceDelta.addedSingle)
    {
        if (faceSet.find(kvp.second.source) != faceSet.cend())
        {
            facesFromShape.emplace_back(kvp.first);
        }
    }

    faceNames.reserve(facesFromShape.size());
    auto assemblyFaceName = [&faceNames, &topoNaming](const TopoDS_Shape& newFace)
    {
        TopoName faceName;
        if (topoNaming.getName(newFace, faceName) && !faceName.empty())
        {
            faceNames.emplace_back(std::move(faceName));
        }
        else
        {
            assert(false);
        }
    };
    for (const TopoDS_Shape& face : facesFromShape)
    {
        assemblyFaceName(face);
    }

    return faceNames;
}

NS_WY3D_END
