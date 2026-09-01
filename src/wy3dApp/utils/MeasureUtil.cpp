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

#include "MeasureUtil.h"

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>

#include "TopoShapeUtil.h"

std::pair<bool, double> MeasureUtil::edgeLength(const TopoDS_Shape& shape, unsigned int edgeIndex)
{
    const std::pair<bool, TopoDS_Edge> result = TopoShapeUtil::getEdge(shape, edgeIndex);
    if (!result.first || result.second.IsNull())
        return { false, 0.0 };

    GProp_GProps lProps;
    BRepGProp::LinearProperties(result.second, lProps);
    return { true, lProps.Mass() };
}

std::pair<bool, double> MeasureUtil::faceArea(const TopoDS_Shape& shape, unsigned int faceIndex)
{
    const std::pair<bool, TopoDS_Face> result = TopoShapeUtil::getFace(shape, faceIndex);
    if (!result.first || result.second.IsNull())
        return { false, 0.0 };

    GProp_GProps sProps;
    BRepGProp::SurfaceProperties(result.second, sProps);
    return { true, sProps.Mass() };
}

std::pair<bool, double> MeasureUtil::facePerimeter(const TopoDS_Shape& shape, unsigned int faceIndex)
{
    const std::pair<bool, TopoDS_Face> result = TopoShapeUtil::getFace(shape, faceIndex);
    if (!result.first || result.second.IsNull())
        return { false, 0.0 };

    double sum = 0.0;
    for (TopExp_Explorer wireExplorer(result.second, TopAbs_WIRE); wireExplorer.More(); wireExplorer.Next())
    {
        GProp_GProps lProps;
        BRepGProp::LinearProperties(wireExplorer.Current(), lProps);
        sum += lProps.Mass();
    }
    return { true, sum };
}

std::pair<bool, double> MeasureUtil::bodyVolume(const TopoDS_Shape& shape)
{
    if (shape.IsNull())
        return { false, 0.0 };

    GProp_GProps vProps;
    BRepGProp::VolumeProperties(shape, vProps);
    return { true, vProps.Mass() };
}

std::pair<bool, double> MeasureUtil::bodySurfaceArea(const TopoDS_Shape& shape)
{
    if (shape.IsNull())
        return { false, 0.0 };

    GProp_GProps sProps;
    BRepGProp::SurfaceProperties(shape, sProps);
    return { true, sProps.Mass() };
}
