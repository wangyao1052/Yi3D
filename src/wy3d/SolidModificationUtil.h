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

#ifndef WY3D_SOLID_MODIFICATION_UTIL_H
#define WY3D_SOLID_MODIFICATION_UTIL_H

#include <vector>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <wydbDatabase.h>
#include <wy3dDefs.h>
#include <wy3dErrorCode.h>
#include <wy3dTopoNaming.h>

NS_WY3D_BEG

class SolidModificationUtil
{
public:
    // 由新生成面的拓扑名称--->新生成面的索引
    static std::vector<unsigned int> computeNewFaceIndices(
        wydb::Database* pDb,
        const TopoNameList& newFaces,
        const wydb::ElementId& ownerId);

    template<ErrorCode InvalidData, ErrorCode EdgeNotExists, ErrorCode FaceNotExists>
    static ErrorCode getTopoEdgesByTopoNamings(
        const TopoNaming& topoNaming,
        const TopoNameList& edgeNames,
        const TopoNameList& faceNames,
        std::vector<TopoDS_Edge>& topoEdges)
    {
        if (edgeNames.empty() && faceNames.empty())
        {
            return InvalidData;
        }

        // 根据边的拓扑名查找拓扑边
        topoEdges.reserve(edgeNames.size() < 10 ? 10 : edgeNames.size());
        for (const TopoName& edge : edgeNames)
        {
            TopoDS_Edge topoEdge = TopoDS::Edge(topoNaming.smartFind(TopAbs_ShapeEnum::TopAbs_EDGE, edge));
            if (topoEdge.IsNull())
            {
                return EdgeNotExists;
            }
            else
            {
                topoEdges.emplace_back(topoEdge);
            }
        }

        // 根据面的拓扑名查找拓扑面-->拓扑边
        for (const TopoName& face : faceNames)
        {
            TopoDS_Face topoFace = TopoDS::Face(topoNaming.smartFind(TopAbs_ShapeEnum::TopAbs_FACE, face));
            if (topoFace.IsNull())
            {
                return FaceNotExists;
            }
            else
            {
                TopTools_IndexedMapOfShape edgeMap;
                TopExp::MapShapes(topoFace, TopAbs_EDGE, edgeMap);
                for (int i = 1; i <= edgeMap.Extent(); ++i)
                {
                    topoEdges.emplace_back(TopoDS::Edge(edgeMap(i)));
                }
            }
        }

        return ErrorCode::NoError;
    }

    template<ErrorCode InvalidData, ErrorCode FaceNotExists>
    static ErrorCode getTopoFacesByTopoNamings(
        const TopoNaming& topoNaming,
        const TopoNameList& faceNames,
        std::vector<TopoDS_Face>& topoFaces)
    {
        if (faceNames.empty())
        {
            return InvalidData;
        }

        // 根据面的拓扑名查找拓扑面
        for (const TopoName& face : faceNames)
        {
            TopoDS_Face topoFace = TopoDS::Face(topoNaming.smartFind(TopAbs_ShapeEnum::TopAbs_FACE, face));
            if (topoFace.IsNull())
            {
                return FaceNotExists;
            }
            else
            {
                topoFaces.emplace_back(topoFace);
            }
        }

        return ErrorCode::NoError;
    }
};

NS_WY3D_END

#endif // WY3D_SOLID_MODIFICATION_UTIL_H
