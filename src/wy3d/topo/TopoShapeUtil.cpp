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
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepFill_Filling.hxx>

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

namespace
{
    class EdgeVertexTable
    {
    public:
        ErrorCode build(const std::vector<TopoDS_Edge>& edges)
        {
            _edgeVertexIndices.resize(edges.size() * 2);
            for (size_t i = 0; i < edges.size(); ++i)
            {
                TopoDS_Vertex v1, v2;
                TopExp::Vertices(edges[i], v1, v2);
                if (v1.IsNull() || v2.IsNull())
                {
                    return ErrorCode::PLANARSHEET_InvalidData;
                }
                // A closed edge reports the same vertex twice: it carries both of its own
                // ends and passes the degree check below on its own
                const int vertexIndices[2] = { addOrFind(v1), addOrFind(v2) };
                for (int end = 0; end < 2; ++end)
                {
                    _edgeVertexIndices[i * 2 + end] = vertexIndices[end];
                    _adjacentEdges[vertexIndices[end]].emplace_back(static_cast<int>(i));
                }
            }
            return ErrorCode::NoError;
        }

        int edgeVertexIndex(size_t edgeIndex, int end) const
        {
            return _edgeVertexIndices[edgeIndex * 2 + end];
        }

        const std::vector<std::vector<int>>& adjacentEdges() const
        {
            return _adjacentEdges;
        }

        const std::vector<int>& adjacentEdges(int vertexIndex) const
        {
            return _adjacentEdges[static_cast<size_t>(vertexIndex)];
        }

    private:
        int addOrFind(const TopoDS_Vertex& v)
        {
            for (int i = 0; i < static_cast<int>(_vertices.size()); ++i)
            {
                if (_vertices[i].IsPartner(v)) return i;
                const Standard_Real dist = BRep_Tool::Pnt(_vertices[i]).Distance(BRep_Tool::Pnt(v));
                if (dist < BRep_Tool::Tolerance(_vertices[i]) || dist < BRep_Tool::Tolerance(v))
                {
                    return i;
                }
            }
            _vertices.emplace_back(v);
            _adjacentEdges.emplace_back();
            return static_cast<int>(_vertices.size()) - 1;
        }

    private:
        std::vector<TopoDS_Vertex> _vertices;
        std::vector<std::vector<int>> _adjacentEdges;
        // two vertex indices per edge, in the order the edge runs
        std::vector<int> _edgeVertexIndices;
    };
}

ErrorCode TopoShapeUtil::makeWireFromEdges(
    const std::vector<TopoDS_Edge>& edges,
    TopoDS_Wire& outWire)
{
    outWire = TopoDS_Wire();
    if (edges.empty())
    {
        return ErrorCode::warnTOPOSHAPE_NullShape;
    }

    // A malformed edge (a child that is not a vertex, a vertex without point geometry)
    // makes the OCCT accessors below raise; that is invalid input, not an open loop
    try
    {
        EdgeVertexTable vertexTable;
        ErrorCode errorCode = vertexTable.build(edges);
        if (ErrorCode::NoError != errorCode)
        {
            return errorCode;
        }

        // Every vertex must be shared by exactly two edges, which rules out open chains,
        // branches and self-touching loops; with the connectivity check below it leaves
        // exactly one simple closed loop
        for (const std::vector<int>& adjEdges : vertexTable.adjacentEdges())
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
            const size_t edgeIndex = stack.back();
            stack.pop_back();
            for (int end = 0; end < 2; ++end)
            {
                const int vertexIndex = vertexTable.edgeVertexIndex(edgeIndex, end);
                for (int neighborEdge : vertexTable.adjacentEdges(vertexIndex))
                {
                    if (!visited[static_cast<size_t>(neighborEdge)])
                    {
                        visited[static_cast<size_t>(neighborEdge)] = true;
                        stack.emplace_back(static_cast<size_t>(neighborEdge));
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
            size_t nextEdge = edges.size();
            for (int end = 0; end < 2; ++end)
            {
                const int vertexIndex = vertexTable.edgeVertexIndex(curEdge, end);
                for (int neighborEdge : vertexTable.adjacentEdges(vertexIndex))
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

        outWire = wire;
        return ErrorCode::NoError;
    }
    catch (const Standard_Failure&)
    {
        outWire = TopoDS_Wire();
        return ErrorCode::PLANARSHEET_InvalidData;
    }
}

ErrorCode TopoShapeUtil::makePlanarFaceFromEdges(
    const std::vector<TopoDS_Edge>& edges,
    TopoDS_Face& outFace)
{
    outFace = TopoDS_Face();

    TopoDS_Wire wire;
    ErrorCode errorCode = makeWireFromEdges(edges, wire);
    if (ErrorCode::NoError != errorCode)
    {
        return errorCode;
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

ErrorCode TopoShapeUtil::makeFilledFaceFromEdges(
    const std::vector<TopoDS_Edge>& edges,
    TopoDS_Face& outFace)
{
    outFace = TopoDS_Face();

    TopoDS_Wire wire;
    ErrorCode errorCode = makeWireFromEdges(edges, wire);
    if (ErrorCode::NoError != errorCode)
    {
        return errorCode;
    }

    try
    {
        BRepBuilderAPI_MakeFace planarMakeFace(wire, Standard_True);
        if (planarMakeFace.IsDone())
        {
            outFace = planarMakeFace.Face();
        }
    }
    catch (const Standard_Failure&)
    {
        outFace = TopoDS_Face();
    }

    if (outFace.IsNull())
    {
        // Fill with the wire's own edges: BRepFill requires a continuous constraint
        // sequence while the picked edges come in arbitrary order and orientation
        try
        {
            BRepFill_Filling filling;
            for (TopExp_Explorer exp(wire, TopAbs_ShapeEnum::TopAbs_EDGE); exp.More(); exp.Next())
            {
                filling.Add(TopoDS::Edge(exp.Current()), GeomAbs_C0, Standard_True);
            }
            filling.Build();
            if (filling.IsDone())
            {
                outFace = filling.Face();
            }
        }
        catch (const Standard_Failure&)
        {
            outFace = TopoDS_Face();
        }
    }
    if (outFace.IsNull())
    {
        return ErrorCode::FILLEDSHEET_GenerateError;
    }

    return ErrorCode::NoError;
}

NS_WY3D_END
