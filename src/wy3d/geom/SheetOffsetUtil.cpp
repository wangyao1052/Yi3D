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

#include <cassert>
#include <cmath>
#include <vector>

#include <BRep_Builder.hxx>
#include <BRepOffsetAPI_MakeOffsetShape.hxx>
#include <BRepOffset_Mode.hxx>
#include <GeomAbs_JoinType.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopTools_ListIteratorOfListOfShape.hxx>

#include <wy3dImpl.h>
#include <wy3dTopoShapeMap.h>
#include <utils/wy3dSheetOffsetUtil.h>

NS_WY3D_BEG

bool SheetOffsetUtil::collectFaceByIndices(
    const TopoDS_Shape& shape,
    const std::vector<unsigned int>& faceIndices,
    std::vector<TopoDS_Face>& faces)
{
    TopTools_IndexedMapOfShape faceMap;
    TopExp::MapShapes(shape, TopAbs_ShapeEnum::TopAbs_FACE, faceMap);
    faces.reserve(faceIndices.size());
    for (unsigned int faceIndex : faceIndices)
    {
        if (faceIndex >= static_cast<unsigned int>(faceMap.Extent())) return false;
        faces.emplace_back(TopoDS::Face(faceMap(static_cast<int>(faceIndex) + 1)));
    }
    return true;
}

bool SheetOffsetUtil::findFaceOccurrences(const TopoDS_Shape& shape, std::vector<TopoDS_Face>& faces)
{
    TopTools_IndexedMapOfShape faceMap;
    TopExp::MapShapes(shape, TopAbs_ShapeEnum::TopAbs_FACE, faceMap);

    std::vector<TopoDS_Face> occurrences;
    occurrences.reserve(faces.size());
    for (const TopoDS_Face& face : faces)
    {
        const int index = faceMap.FindIndex(face);
        if (index <= 0)
        {
            assert(false);
            return false;
        }
        occurrences.emplace_back(TopoDS::Face(faceMap(index)));
    }
    faces.swap(occurrences);
    return true;
}

bool SheetOffsetUtil::collectShellRegions(const TopoDS_Shape& shape, std::vector<TopoDS_Shape>& regions)
{
    regions.clear();

    const TopAbs_ShapeEnum shapeType = shape.ShapeType();
    if (TopAbs_ShapeEnum::TopAbs_SHELL == shapeType)
    {
        regions.emplace_back(shape);
        return true;
    }
    else if (TopAbs_ShapeEnum::TopAbs_COMPOUND == shapeType)
    {
        for (TopExp_Explorer ex(shape, TopAbs_ShapeEnum::TopAbs_SHELL); ex.More(); ex.Next())
        {
            regions.emplace_back(ex.Current());
        }
        return !regions.empty();
    }
    else
    {
        assert(false);
        return false;
    }
}

bool SheetOffsetUtil::collectFaceRegions(
    const TopoDS_Shape& shape,
    const std::vector<TopoDS_Face>& targetFaces,
    std::vector<TopoDS_Shape>& regions)
{
    regions.clear();
    if (targetFaces.empty()) return false;

    TopoShapeSet targetFaceSet;
    for (const TopoDS_Face& targetFace : targetFaces)
    {
        targetFaceSet.insert(targetFace);
    }
    TopTools_IndexedDataMapOfShapeListOfShape edge2Faces;
    TopExp::MapShapesAndAncestors(shape, TopAbs_ShapeEnum::TopAbs_EDGE,
        TopAbs_ShapeEnum::TopAbs_FACE, edge2Faces);

    TopoShapeSet visited;
    std::vector<TopoDS_Face> todo;
    for (const TopoDS_Face& startFace : targetFaces)
    {
        if (visited.find(startFace) != visited.cend()) continue;

        std::vector<TopoDS_Face> group;
        group.reserve(5);
        todo.clear();
        todo.emplace_back(startFace);
        visited.insert(startFace);
        while (!todo.empty())
        {
            const TopoDS_Face face = todo.back();
            todo.pop_back();
            group.emplace_back(face);

            TopTools_IndexedMapOfShape faceEdgeMap;
            TopExp::MapShapes(face, TopAbs_ShapeEnum::TopAbs_EDGE, faceEdgeMap);
            for (int i = 1; i <= faceEdgeMap.Extent(); ++i)
            {
                const TopoDS_Shape& edge = faceEdgeMap(i);
                if (!edge2Faces.Contains(edge)) continue;

                const TopTools_ListOfShape& faceList = edge2Faces.FindFromKey(edge);
                for (TopTools_ListIteratorOfListOfShape faceIter(faceList); faceIter.More(); faceIter.Next())
                {
                    const TopoDS_Face neighbour = TopoDS::Face(faceIter.Value());
                    if (targetFaceSet.find(neighbour) == targetFaceSet.cend()) continue;
                    if (visited.find(neighbour) != visited.cend()) continue;
                    visited.insert(neighbour);
                    todo.emplace_back(neighbour);
                }
            }
        }

        TopoDS_Shell groupShell;
        BRep_Builder builder;
        builder.MakeShell(groupShell);
        for (const TopoDS_Face& face : group)
        {
            builder.Add(groupShell, face);
        }
        regions.emplace_back(groupShell);
    }

    return !regions.empty();
}

bool SheetOffsetUtil::offsetRegion(
    const TopoDS_Shape& region, double offset,
    BRepOffsetAPI_MakeOffsetShape& offsetAlgo,
    TopoDS_Shape& offsetShape)
{
    offsetAlgo.PerformByJoin(region, offset, wy3d::TOL * 10,
        BRepOffset_Skin, Standard_False, Standard_False, GeomAbs_Intersection);
    if (!offsetAlgo.IsDone() || offsetAlgo.Shape().IsNull() ||
        TopAbs_ShapeEnum::TopAbs_SHELL != offsetAlgo.Shape().ShapeType())
    {
        return false;
    }
    offsetShape = offsetAlgo.Shape();
    return true;
}

ErrorCode SheetOffsetUtil::makeOffsetShape(
    const TopoDS_Shape& sourceShape,
    const std::vector<std::uint32_t>& faceIndices,
    double offset,
    TopoDS_Shape& offsetShape)
{
    offsetShape = TopoDS_Shape();

    if (sourceShape.IsNull()) return ErrorCode::OFFSETSHEET_InvalidData;
    if (!std::isfinite(offset) || std::fabs(offset) > wy3d::kMaxValue)
    {
        return ErrorCode::OFFSETSHEET_InvalidOffset;
    }
    if (faceIndices.empty()) return ErrorCode::OFFSETSHEET_NoFaceSelected;

    try
    {
        std::vector<TopoDS_Face> targetFaces;
        if (!collectFaceByIndices(sourceShape, faceIndices, targetFaces))
        {
            return ErrorCode::OFFSETSHEET_FaceNotExists;
        }

        std::vector<TopoDS_Shape> regions;
        if (!collectFaceRegions(sourceShape, targetFaces, regions) || regions.empty())
        {
            return ErrorCode::OFFSETSHEET_GenerateError;
        }

        // A distance below the modelling tolerance means no offsetting: the picked faces are copied as they are
        const bool isCopy = std::fabs(offset) < wy3d::kMinValue;

        std::vector<TopoDS_Shape> offsetShapes;
        offsetShapes.reserve(regions.size());
        for (const TopoDS_Shape& region : regions)
        {
            if (isCopy)
            {
                offsetShapes.emplace_back(region);
                continue;
            }

            BRepOffsetAPI_MakeOffsetShape offsetAlgo;
            TopoDS_Shape regionOffset;
            if (!offsetRegion(region, offset, offsetAlgo, regionOffset))
            {
                return ErrorCode::OFFSETSHEET_GenerateError;
            }
            offsetShapes.emplace_back(regionOffset);
        }

        // 单个连通分量就是一壳, 多个分量只能拼成复合体: 与特征的追加语义不同, 这里不带源形体
        if (1 == offsetShapes.size())
        {
            offsetShape = offsetShapes.front();
        }
        else
        {
            TopoDS_Compound compound;
            BRep_Builder builder;
            builder.MakeCompound(compound);
            for (const TopoDS_Shape& regionOffset : offsetShapes)
            {
                builder.Add(compound, regionOffset);
            }
            offsetShape = compound;
        }
        return ErrorCode::NoError;
    }
    catch (const Standard_Failure&)
    {
        assert(false);
    }

    offsetShape = TopoDS_Shape();
    return ErrorCode::OFFSETSHEET_GenerateError;
}

NS_WY3D_END
