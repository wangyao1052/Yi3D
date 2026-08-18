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

#include <wyVector3.h>
#include "topo/TopoShapeUtil.h"
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <Geom_Plane.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <TopExp.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>

NS_WY3D_BEG

TopoDS_Compound TopoShapeUtil::makeCompound(const TopoDS_Shape& shape1, const TopoDS_Shape& shape2)
{
    BRep_Builder brepBuilder;
    TopoDS_Compound compound;
    brepBuilder.MakeCompound(compound);
    brepBuilder.Add(compound, shape1);
    brepBuilder.Add(compound, shape2);
    return compound;
}

static inline wy::Vector3 _toVector3(const gp_Pnt& pnt)
{
    return wy::Vector3(pnt.X(), pnt.Y(), pnt.Z());
}
static inline wy::Vector3 _toVector3(const gp_Dir& dir)
{
    return wy::Vector3(dir.X(), dir.Y(), dir.Z());
}

bool TopoShapeUtil::getFacePlane(const TopoDS_Face& face, wy3d::SketchPlane& plane)
{
    if (face.IsNull()) return false;
    Handle(Geom_Surface) surface = BRep_Tool::Surface(face);
    if (surface.IsNull()) return false;
    Handle(Geom_Plane) planeSurf = Handle(Geom_Plane)::DownCast(surface);
    if (planeSurf.IsNull()) return false;

    const gp_Ax3& ax3 = planeSurf->Position();
    const gp_Pnt& origin = ax3.Location();
    const gp_Dir& xAxis = ax3.XDirection();
    const gp_Dir& zAxis = ax3.Direction();

    TopAbs_Orientation orient = face.Orientation();
    if (orient == TopAbs_REVERSED)
    {
        plane = wy3d::SketchPlane(
            _toVector3(origin),
            _toVector3(zAxis.Reversed()),
            _toVector3(xAxis.Reversed()));
    }
    else
    {
        plane = wy3d::SketchPlane(
            _toVector3(origin),
            _toVector3(zAxis),
            _toVector3(xAxis));
    }
    if (plane.isValid()) return true;
    else return false;
}

ErrorCode TopoShapeUtil::makePlanarFaceFromEdges(
    const std::vector<TopoDS_Edge>& edges,
    TopoDS_Face& outFace)
{
    outFace = TopoDS_Face();
    if (edges.empty())
    {
        return ErrorCode::warnTOPOSHAPE_NullShape;
    }

    std::vector<TopoDS_Vertex> vertices;
    std::vector<std::vector<int>> vertexAdjEdges;
    auto findOrAddVertex = [&vertices, &vertexAdjEdges](const TopoDS_Vertex& v) -> int
    {
        for (int i = 0; i < static_cast<int>(vertices.size()); ++i)
        {
            if (vertices[i].IsPartner(v)) return i;
            const Standard_Real dist = BRep_Tool::Pnt(vertices[i]).Distance(BRep_Tool::Pnt(v));
            if (dist < BRep_Tool::Tolerance(vertices[i]) || dist < BRep_Tool::Tolerance(v))
            {
                return i;
            }
        }
        vertices.emplace_back(v);
        vertexAdjEdges.emplace_back();
        return static_cast<int>(vertices.size()) - 1;
    };
    for (size_t i = 0; i < edges.size(); ++i)
    {
        TopoDS_Vertex v1, v2;
        TopExp::Vertices(edges[i], v1, v2);
        if (v1.IsNull() || v2.IsNull())
        {
            return ErrorCode::PLANARSHEET_InvalidData;
        }
        int idx1 = findOrAddVertex(v1);
        int idx2 = findOrAddVertex(v2);
        vertexAdjEdges[idx1].emplace_back(static_cast<int>(i));
        vertexAdjEdges[idx2].emplace_back(static_cast<int>(i));
    }

    for (const std::vector<int>& adjEdges : vertexAdjEdges)
    {
        if (adjEdges.size() != 2)
        {
            return ErrorCode::PLANARSHEET_EdgesNotClosed;
        }
    }

    std::vector<bool> visited(edges.size(), false);
    std::vector<size_t> stack{0};
    visited[0] = true;
    while (!stack.empty())
    {
        size_t edgeIdx = stack.back();
        stack.pop_back();
        TopoDS_Vertex v1, v2;
        TopExp::Vertices(edges[edgeIdx], v1, v2);
        const TopoDS_Vertex edgeVertices[2] = { v1, v2 };
        for (const TopoDS_Vertex& v : edgeVertices)
        {
            int vi = findOrAddVertex(v);
            for (int neighborEdge : vertexAdjEdges[vi])
            {
                if (!visited[neighborEdge])
                {
                    visited[neighborEdge] = true;
                    stack.emplace_back(neighborEdge);
                }
            }
        }
    }
    for (bool b : visited)
    {
        if (!b)
        {
            return ErrorCode::PLANARSHEET_EdgesNotClosed;
        }
    }

    std::vector<size_t> orderedEdgeIndices;
    orderedEdgeIndices.reserve(edges.size());
    std::vector<bool> used(edges.size(), false);
    size_t curEdge = 0;
    for (size_t k = 0; k < edges.size(); ++k)
    {
        orderedEdgeIndices.emplace_back(curEdge);
        used[curEdge] = true;
        if (k + 1 == edges.size())
        {
            break;
        }
        TopoDS_Vertex v1, v2;
        TopExp::Vertices(edges[curEdge], v1, v2);
        const int vertexIndices[2] = { findOrAddVertex(v1), findOrAddVertex(v2) };
        size_t nextEdge = edges.size();
        for (int vi : vertexIndices)
        {
            for (int neighborEdge : vertexAdjEdges[vi])
            {
                if (!used[static_cast<size_t>(neighborEdge)])
                {
                    nextEdge = static_cast<size_t>(neighborEdge);
                    break;
                }
            }
            if (nextEdge != edges.size())
            {
                break;
            }
        }
        if (nextEdge == edges.size())
        {
            return ErrorCode::PLANARSHEET_EdgesNotClosed;
        }
        curEdge = nextEdge;
    }

    TopoDS_Wire wire;
    try
    {
        BRepBuilderAPI_MakeWire makeWire;
        for (size_t edgeIndex : orderedEdgeIndices)
        {
            makeWire.Add(edges[edgeIndex]);
        }
        wire = makeWire.Wire();
    }
    catch (const Standard_Failure&)
    {
        return ErrorCode::PLANARSHEET_EdgesNotClosed;
    }
    if (wire.IsNull() || !wire.Closed())
    {
        return ErrorCode::PLANARSHEET_EdgesNotClosed;
    }

    try
    {
        BRepBuilderAPI_MakeFace makeFace(wire, Standard_True);
        outFace = makeFace.Face();
    }
    catch (const Standard_Failure&)
    {
        return ErrorCode::PLANARSHEET_InvalidData;
    }
    if (outFace.IsNull())
    {
        return ErrorCode::PLANARSHEET_EdgesNotCoplanar;
    }

    return ErrorCode::NoError;
}

NS_WY3D_END
