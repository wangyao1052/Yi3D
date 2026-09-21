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
#include <wy3dImpl.h>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <Geom_Plane.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS_Shell.hxx>
#include <TopAbs.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <BRepAdaptor_CompCurve.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepLib_FindSurface.hxx>
#include <BRepFill_Filling.hxx>
#include <GCPnts_QuasiUniformDeflection.hxx>
#include <GeomAdaptor_Surface.hxx>
#include <Precision.hxx>
#include <gp_Pln.hxx>
#include <cmath>

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
    // BRepLib_FindSurface arguments: Tol = -1 asks for the max of the shape's own edge tolerances
    // instead of a fixed value, and OnlyPlane keeps the fitted surface to a plane so that no
    // cylinder fit can pass as "coplanar"
    const double kUseShapeTolerance = -1.0;
    const Standard_Boolean kOnlyPlane = Standard_True;

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

    // Builds one closed wire per connected component of the edges. Every vertex has to be
    // shared by exactly two edges, which rules out open chains, branches and self-touching
    // loops; that leaves every component a simple closed loop of its own
    ErrorCode _splitEdgeLoops(const std::vector<TopoDS_Edge>& edges, std::vector<TopoDS_Wire>& outWires)
    {
        outWires.clear();

        EdgeVertexTable vertexTable;
        ErrorCode errorCode = vertexTable.build(edges);
        if (ErrorCode::NoError != errorCode) return errorCode;

        for (const std::vector<int>& adjEdges : vertexTable.adjacentEdges())
        {
            if (adjEdges.size() != 2) return ErrorCode::PLANARSHEET_EdgesNotClosed;
        }

        // Walk each component from its first unused edge: with every vertex of degree two the
        // walk runs around the loop and stops when it comes back to where it started
        std::vector<bool> used(edges.size(), false);
        for (size_t seed = 0; seed < edges.size(); ++seed)
        {
            if (used[seed]) continue;

            std::vector<size_t> orderedEdgeIndices;
            orderedEdgeIndices.reserve(edges.size());
            size_t curEdge = seed;
            for (;;)
            {
                orderedEdgeIndices.emplace_back(curEdge);
                used[curEdge] = true;

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
                    if (nextEdge != edges.size()) break;
                }
                if (nextEdge == edges.size()) break;
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
            outWires.emplace_back(wire);
        }

        return ErrorCode::NoError;
    }

    struct _PlanarLoop
    {
        TopoDS_Wire wire;
        // Face with this loop as its only wire: what the nesting tests classify against
        TopoDS_Face face;
        // Points along the loop, used both for the winding and for the nesting tests
        std::vector<gp_Pnt> samples;
        Bnd_Box box;
        double signedArea = 0.0;
        int depth = 0;
        int parent = -1;
    };

    // Points along the loop, thinned to what the nesting tests need: enough of them to notice
    // that a loop leaves another one, few enough to keep the pairwise tests cheap. The
    // deflection follows the box the caller has already taken, so it stays one formula
    ErrorCode _sampleLoop(const TopoDS_Wire& wire, const Bnd_Box& box, std::vector<gp_Pnt>& outSamples)
    {
        try
        {
            if (box.IsVoid()) return ErrorCode::PLANARSHEET_InvalidData;

            Standard_Real xMin(0), yMin(0), zMin(0), xMax(0), yMax(0), zMax(0);
            box.Get(xMin, yMin, zMin, xMax, yMax, zMax);
            const double dx = xMax - xMin, dy = yMax - yMin, dz = zMax - zMin;
            const double diagonal = std::sqrt(dx * dx + dy * dy + dz * dz);

            BRepAdaptor_CompCurve compCurve(wire);
            GCPnts_QuasiUniformDeflection sampler(compCurve, std::max(wy3d::TOL, 1.0e-4 * diagonal));
            if (!sampler.IsDone()) return ErrorCode::PLANARSHEET_InvalidData;

            std::vector<gp_Pnt> points;
            points.reserve(static_cast<size_t>(sampler.NbPoints()));
            for (Standard_Integer i = 1; i <= sampler.NbPoints(); ++i)
            {
                points.emplace_back(compCurve.Value(sampler.Parameter(i)));
            }
            if (points.size() < 3) return ErrorCode::PLANARSHEET_InvalidData;

            const size_t kMaxSamples = 64;
            if (points.size() > kMaxSamples)
            {
                std::vector<gp_Pnt> thinned;
                thinned.reserve(kMaxSamples);
                for (size_t i = 0; i < kMaxSamples; ++i)
                {
                    thinned.emplace_back(points[i * points.size() / kMaxSamples]);
                }
                points.swap(thinned);
            }
            outSamples.swap(points);

            return ErrorCode::NoError;
        }
        catch (const Standard_Failure&)
        {
            return ErrorCode::PLANARSHEET_InvalidData;
        }
    }

    // Samples the loop and measures the area it encloses as seen from the plane normal: a
    // positive area means the loop runs counter clockwise and suits an outer wire
    ErrorCode _measureLoop(_PlanarLoop& loop, const gp_Pln& plane)
    {
        try
        {
            BRepBndLib::Add(loop.wire, loop.box);
            ErrorCode errorCode = _sampleLoop(loop.wire, loop.box, loop.samples);
            if (ErrorCode::NoError != errorCode) return errorCode;

            // Newell's area vector of the closed polygon, projected on the plane normal
            gp_XYZ sum(0.0, 0.0, 0.0);
            for (size_t i = 0; i < loop.samples.size(); ++i)
            {
                const gp_Pnt& a = loop.samples[i];
                const gp_Pnt& b = loop.samples[(i + 1) % loop.samples.size()];
                sum += a.XYZ().Crossed(b.XYZ());
            }
            loop.signedArea = 0.5 * gp_Vec(sum).Dot(gp_Vec(plane.Axis().Direction()));

            return ErrorCode::NoError;
        }
        catch (const Standard_Failure&)
        {
            return ErrorCode::PLANARSHEET_InvalidData;
        }
    }

    // Where the samples of one loop sit in another loop's face: all inside means the face
    // contains the loop, all outside means the two do not interact, and anything else means
    // the loops cross or touch each other
    ErrorCode _testLoopInside(const std::vector<gp_Pnt>& samples, const TopoDS_Face& face, bool& outInside)
    {
        outInside = false;

        size_t insideCount = 0;
        for (const gp_Pnt& sample : samples)
        {
            BRepClass_FaceClassifier classifier(face, sample, Precision::Confusion());
            const TopAbs_State state = classifier.State();
            if (TopAbs_IN == state)
            {
                ++insideCount;
            }
            else if (TopAbs_OUT != state)
            {
                return ErrorCode::PLANARSHEET_LoopsNotNested;
            }
        }

        if (0 == insideCount) return ErrorCode::NoError;
        if (insideCount == samples.size())
        {
            outInside = true;
            return ErrorCode::NoError;
        }
        return ErrorCode::PLANARSHEET_LoopsNotNested;
    }

    // Nesting depth of every loop: how many other loops contain it. Even depths are outer
    // wires, odd depths are the holes of the directly enclosing loop
    ErrorCode _resolveNesting(std::vector<_PlanarLoop>& loops)
    {
        const size_t count = loops.size();
        std::vector<std::vector<bool>> contains(count, std::vector<bool>(count, false));
        for (size_t i = 0; i < count; ++i)
        {
            for (size_t j = 0; j < count; ++j)
            {
                if (i == j) continue;
                if (loops[i].box.IsOut(loops[j].box)) continue;

                bool inside = false;
                ErrorCode errorCode = _testLoopInside(loops[i].samples, loops[j].face, inside);
                if (ErrorCode::NoError != errorCode) return errorCode;
                contains[j][i] = inside;
                if (inside) ++loops[i].depth;
            }
        }

        for (size_t i = 0; i < count; ++i)
        {
            if (0 == loops[i].depth % 2) continue;

            // A hole belongs to the one loop that contains it directly
            int parent = -1;
            for (size_t j = 0; j < count; ++j)
            {
                if (!contains[j][i] || loops[j].depth != loops[i].depth - 1) continue;
                if (parent >= 0) return ErrorCode::PLANARSHEET_LoopsNotNested;
                parent = static_cast<int>(j);
            }
            if (parent < 0) return ErrorCode::PLANARSHEET_LoopsNotNested;
            loops[i].parent = parent;
        }

        return ErrorCode::NoError;
    }

    // One face per even depth loop, with the odd depth loops it directly contains as holes
    ErrorCode _makeFacesWithHoles(const std::vector<_PlanarLoop>& loops, const gp_Pln& plane,
        std::vector<TopoDS_Face>& outFaces)
    {
        for (size_t i = 0; i < loops.size(); ++i)
        {
            if (0 != loops[i].depth % 2) continue;

            // The outer wire has to run counter clockwise seen from the plane normal, the
            // holes the other way round: swapping them turns a hole into a second plate
            TopoDS_Wire outerWire = loops[i].wire;
            if (loops[i].signedArea < 0.0) outerWire = TopoDS::Wire(outerWire.Reversed());

            BRepBuilderAPI_MakeFace makeFace(plane, outerWire);
            if (!makeFace.IsDone() || makeFace.Face().IsNull()) return ErrorCode::PLANARSHEET_InvalidData;

            for (size_t k = 0; k < loops.size(); ++k)
            {
                if (loops[k].parent != static_cast<int>(i)) continue;

                TopoDS_Wire holeWire = loops[k].wire;
                if (loops[k].signedArea > 0.0) holeWire = TopoDS::Wire(holeWire.Reversed());
                makeFace.Add(holeWire);
            }

            makeFace.Build();
            if (!makeFace.IsDone() || makeFace.Face().IsNull()) return ErrorCode::PLANARSHEET_InvalidData;
            outFaces.emplace_back(makeFace.Face());
        }

        if (outFaces.empty()) return ErrorCode::PLANARSHEET_InvalidData;
        return ErrorCode::NoError;
    }

    // One loop into one face: the planar shortcut first, the plate filler for everything a
    // plane cannot take. Shared by the single loop entry point and the per loop patches of
    // makeFilledSheetFromEdges
    ErrorCode _fillLoop(const TopoDS_Wire& wire, TopoDS_Face& outFace)
    {
        outFace = TopoDS_Face();

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

    // The shape a sheet arrives in: one shell per face inside a compound, which is what a
    // sketch profile makes and what the planar path above produces
    void _compoundOfShells(const std::vector<TopoDS_Face>& faces, TopoDS_Shape& outShape)
    {
        BRep_Builder brepBuilder;
        TopoDS_Compound compound;
        brepBuilder.MakeCompound(compound);
        for (const TopoDS_Face& face : faces)
        {
            TopoDS_Shell shell;
            brepBuilder.MakeShell(shell);
            brepBuilder.Add(shell, face);
            brepBuilder.Add(compound, shell);
        }
        outShape = compound;
    }

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
        std::vector<TopoDS_Wire> wires;
        ErrorCode errorCode = _splitEdgeLoops(edges, wires);
        if (ErrorCode::NoError != errorCode)
        {
            return errorCode;
        }
        // This entry point keeps its single loop contract: the filled sheet builds one face
        // per wire and has no use for a hole
        if (1 != wires.size())
        {
            return ErrorCode::PLANARSHEET_EdgesNotClosed;
        }

        outWire = wires.front();
        return ErrorCode::NoError;
    }
    catch (const Standard_Failure&)
    {
        outWire = TopoDS_Wire();
        return ErrorCode::PLANARSHEET_InvalidData;
    }
}

ErrorCode TopoShapeUtil::makePlanarSheetFromEdges(
    const std::vector<TopoDS_Edge>& edges,
    TopoDS_Shape& outShape)
{
    outShape = TopoDS_Shape();
    if (edges.empty())
    {
        return ErrorCode::warnTOPOSHAPE_NullShape;
    }

    try
    {
        std::vector<TopoDS_Wire> wires;
        ErrorCode errorCode = _splitEdgeLoops(edges, wires);
        if (ErrorCode::NoError != errorCode)
        {
            return errorCode;
        }
        if (wires.empty())
        {
            return ErrorCode::PLANARSHEET_EdgesNotClosed;
        }

        // One lookup for the whole set: it answers whether the loops share a plane at all and
        // hands the plane back. FindSurface does not attach that plane to the edges it is
        // given (measured on the bundled OCCT 7.7), so the picked edges can go in as they are
        BRep_Builder brepBuilder;
        TopoDS_Compound compound;
        brepBuilder.MakeCompound(compound);
        for (const TopoDS_Wire& wire : wires)
        {
            brepBuilder.Add(compound, wire);
        }

        BRepLib_FindSurface planeFinder(compound, kUseShapeTolerance, kOnlyPlane);
        if (!planeFinder.Found())
        {
            return ErrorCode::PLANARSHEET_EdgesNotCoplanar;
        }
        const gp_Pln plane = GeomAdaptor_Surface(planeFinder.Surface()).Plane();

        std::vector<_PlanarLoop> loops(wires.size());
        for (size_t i = 0; i < wires.size(); ++i)
        {
            loops[i].wire = wires[i];
            errorCode = _measureLoop(loops[i], plane);
            if (ErrorCode::NoError != errorCode)
            {
                return errorCode;
            }

            // The loop on its own, to classify the samples of the other loops against
            BRepBuilderAPI_MakeFace makeFace(plane, loops[i].wire);
            if (!makeFace.IsDone() || makeFace.Face().IsNull())
            {
                return ErrorCode::PLANARSHEET_InvalidData;
            }
            loops[i].face = makeFace.Face();
        }

        errorCode = _resolveNesting(loops);
        if (ErrorCode::NoError != errorCode)
        {
            return errorCode;
        }

        std::vector<TopoDS_Face> faces;
        errorCode = _makeFacesWithHoles(loops, plane, faces);
        if (ErrorCode::NoError != errorCode)
        {
            return errorCode;
        }

        // The shape the sketch profile path produces: one shell per face inside a compound
        _compoundOfShells(faces, outShape);
        return ErrorCode::NoError;
    }
    catch (const Standard_Failure&)
    {
        outShape = TopoDS_Shape();
        return ErrorCode::PLANARSHEET_InvalidData;
    }
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

    return _fillLoop(wire, outFace);
}

ErrorCode TopoShapeUtil::makeFilledSheetFromEdges(
    const std::vector<TopoDS_Edge>& edges,
    TopoDS_Shape& outShape)
{
    outShape = TopoDS_Shape();
    if (edges.empty())
    {
        return ErrorCode::warnTOPOSHAPE_NullShape;
    }

    try
    {
        std::vector<TopoDS_Wire> wires;
        ErrorCode errorCode = _splitEdgeLoops(edges, wires);
        if (ErrorCode::NoError != errorCode)
        {
            return errorCode;
        }
        if (wires.empty())
        {
            return ErrorCode::PLANARSHEET_EdgesNotClosed;
        }

        // One loop is filled on its own, with a plane of its own or without one
        if (1 == wires.size())
        {
            TopoDS_Face face;
            errorCode = _fillLoop(wires.front(), face);
            if (ErrorCode::NoError != errorCode)
            {
                return errorCode;
            }

            const std::vector<TopoDS_Face> faces{ face };
            _compoundOfShells(faces, outShape);
            return ErrorCode::NoError;
        }

        // Several loops have to share a plane, and the planar path owns that verdict along with
        // the hole rules. A second region comes back as a second face, which is where this
        // entry point's one face ends
        TopoDS_Shape planarShape;
        errorCode = makePlanarSheetFromEdges(edges, planarShape);
        if (ErrorCode::NoError != errorCode)
        {
            return errorCode;
        }

        size_t faceCount = 0;
        for (TopExp_Explorer faceExp(planarShape, TopAbs_ShapeEnum::TopAbs_FACE); faceExp.More(); faceExp.Next())
        {
            ++faceCount;
        }
        // One region is exactly one face: a second one is the refusal, and so is none at all
        if (1 != faceCount)
        {
            return ErrorCode::FILLEDSHEET_EdgesNotSingleRegion;
        }

        outShape = planarShape;
        return ErrorCode::NoError;
    }
    catch (const Standard_Failure&)
    {
        outShape = TopoDS_Shape();
        return ErrorCode::PLANARSHEET_InvalidData;
    }
}

NS_WY3D_END
