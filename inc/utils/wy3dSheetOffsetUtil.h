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

#ifndef WY3D_SHEET_OFFSET_UTIL_H
#define WY3D_SHEET_OFFSET_UTIL_H

#include <cstdint>
#include <vector>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <wy3dDefs.h>
#include <wy3dErrorCode.h>

class BRepOffsetAPI_MakeOffsetShape;

NS_WY3D_BEG

class WY3D_EXPORT SheetOffsetUtil
{
public:
    static bool collectFaceByIndices(
        const TopoDS_Shape& shape,
        const std::vector<unsigned int>& faceIndices,
        std::vector<TopoDS_Face>& faces);

    static bool findFaceOccurrences(
        const TopoDS_Shape& shape,
        std::vector<TopoDS_Face>& faces);

    static bool collectShellRegions(
        const TopoDS_Shape& shape,
        std::vector<TopoDS_Shape>& regions);

    static bool collectFaceRegions(
        const TopoDS_Shape& shape,
        const std::vector<TopoDS_Face>& targetFaces,
        std::vector<TopoDS_Shape>& regions);

    static bool offsetRegion(
        const TopoDS_Shape& region,
        double offset,
        BRepOffsetAPI_MakeOffsetShape& offsetAlgo,
        TopoDS_Shape& offsetShape);

    static ErrorCode makeOffsetShape(
        const TopoDS_Shape& sourceShape,
        const std::vector<std::uint32_t>& faceIndices,
        double offset,
        TopoDS_Shape& offsetShape);
};

NS_WY3D_END

#endif // WY3D_SHEET_OFFSET_UTIL_H
