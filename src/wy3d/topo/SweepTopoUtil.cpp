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
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <TopoDS.hxx>
#include <BRep_Tool.hxx>
#include <Geom_Curve.hxx>
#include <BRepLib.hxx>

#include <wyVector2.h>
#include <wyVector3.h>
#include <wy3dImpl.h>
#include <wy3dSketch.h>
#include <wy3dCurve.h>
#include <wy3dSketchPath.h>
#include <wy3dErrorCode.h>

#include "topo/SketchTopoBuilder.h"
#include "topo/TopoNamingUtil.h"
#include "topo/SweepTopoUtil.h"

NS_WY3D_BEG

ErrorCode SweepTopoUtil::createPathWire(
    const wy3d::Sketch& pathSketch,
    TopoUtil::WireInfo& pathWireInfo,
    wy::Vector3& pathStartPos,
    wy::Vector3& pathStartDir)
{
    SketchTopoBuilder sketchTopoBuilder(&pathSketch, true);

    SketchPath sketchPath(&pathSketch);
    if (!sketchPath.check())
    {
        std::shared_ptr<SketchError> pError = sketchPath.getError();
        if (pError) return pError->type;
        else return ErrorCode::PATH_InvalidPath;
    }

    const std::vector<BiCurve>& pathCurves = sketchPath.getPath();
    if (pathCurves.empty()) { assert(false); return ErrorCode::PATH_NoCurves; }

    BRepBuilderAPI_MakeWire makeWire;
    for (const BiCurve& curve : pathCurves)
    {
        const SketchCurve* pCurve = curve.curve;
        assert(pCurve);
        TopoDS_Edge edge = sketchTopoBuilder.makeEdge(pCurve);
        if (edge.IsNull()) { assert(false); continue; }
        if (curve.orient)
        {
            edge = TopoDS::Edge(edge.Reversed());
        }
        makeWire.Add(edge);
    }
    if (!makeWire.IsDone() || makeWire.Wire().IsNull()) { assert(false); return ErrorCode::TOPOSHAPE_GenerateShapeError; }
    pathWireInfo.wire = makeWire.Wire();

    assert(!pathCurves.empty());
    const BiCurve& startPathCurve = pathCurves[0];
    const wy3d::SketchCurve* pSweeptartCurve = startPathCurve.curve;
    assert(pSweeptartCurve);
    const wy3d::SketchPlane& pathPlane = pathSketch.getPlane();
    if (startPathCurve.orient)
    {
        pathStartPos = pathPlane.value(pSweeptartCurve->getStartPoint());
        wy::Vector2 dir2d = pSweeptartCurve->getDirectionAt(0.0);
        pathStartDir = pathPlane.value(dir2d) - pathPlane.value(wy::Vector2::kZero);
        pathStartDir.normalize();
    }
    else
    {
        pathStartPos = pathPlane.value(pSweeptartCurve->getEndPoint());
        wy::Vector2 dir2d = pSweeptartCurve->getDirectionAt(1.0);
        pathStartDir = pathPlane.value(dir2d) - pathPlane.value(wy::Vector2::kZero);
        pathStartDir = -pathStartDir;
        pathStartDir.normalize();
    }

    const std::map<Handle(Geom_Curve), unsigned int>& curve2Id = sketchTopoBuilder.getCurve2IdMap();
    TopoUtil::recordEdgeNamesOfWire_AppendedMode(pathWireInfo.wire, curve2Id, pathWireInfo.edgeNameInfos);

    return ErrorCode::NoError;
}

ErrorCode SweepTopoUtil::createPathWire(
    const wy3d::Curve& pathCurve,
    TopoUtil::WireInfo& pathWireInfo,
    wy::Vector3& pathStartPos,
    wy::Vector3& pathStartDir)
{
    BRepBuilderAPI_MakeWire makeWire;
    TopoDS_Edge edge = pathCurve.getEdge();
    if (!edge.IsNull()) makeWire.Add(edge);
    if (!makeWire.IsDone() || makeWire.Wire().IsNull()) { assert(false); return ErrorCode::TOPOSHAPE_GenerateShapeError; }
    pathWireInfo.wire = makeWire.Wire();

    TopoUtil::EdgeNamingInfo edgeNameInfo;
    edgeNameInfo.edge = edge;
    edgeNameInfo.id = pathCurve.getId().value();
    edgeNameInfo.sibling = size_t(-1);
    pathWireInfo.edgeNameInfos.emplace_back(edgeNameInfo);

    BRepLib::BuildCurve3d(edge, wy3d::TOL);
    Standard_Real first(0.0), last(0.0);
    Handle(Geom_Curve) geomCurve = BRep_Tool::Curve(edge, first, last);
    if (geomCurve.IsNull()) { assert(false); return ErrorCode::PATH_InvalidPath; }
    gp_Pnt startPnt;
    gp_Vec tangent;
    geomCurve->D1(first, startPnt, tangent);
    if (tangent.Magnitude() <= wy3d::TOL) { assert(false); return ErrorCode::PATH_InvalidPath; }
    gp_Dir dir(tangent);
    pathStartPos.set(startPnt.X(), startPnt.Y(), startPnt.Z());
    pathStartDir.set(dir.X(), dir.Y(), dir.Z());

    return ErrorCode::NoError;
}

ErrorCode SweepTopoUtil::makePipeShell(
    unsigned int elemIdValue,
    const TopoUtil::WireInfo& pathWireInfo,
    const TopoUtil::WireInfo& profileWireInfo,
    bool bMakeSolid,
    TopoDS_Shape& resultShape,
    TopoNaming& topoNaming)
{
    BRepOffsetAPI_MakePipeShell pipeShellMaker(pathWireInfo.wire);
    pipeShellMaker.SetMode(false);
    pipeShellMaker.SetTransitionMode(BRepBuilderAPI_RightCorner);
    pipeShellMaker.Add(profileWireInfo.wire);
    pipeShellMaker.Build();
    if (Standard_False == pipeShellMaker.IsDone()) return ErrorCode::TOPOSHAPE_GenerateShapeError;
    if (bMakeSolid && Standard_False == pipeShellMaker.MakeSolid()) return ErrorCode::TOPOSHAPE_GenerateShapeError;

    TopoNamingUtil::naming(pathWireInfo.wire, profileWireInfo.wire, pipeShellMaker,
        pathWireInfo.edgeNameInfos, profileWireInfo.edgeNameInfos, elemIdValue, topoNaming,
        bMakeSolid);

    resultShape = pipeShellMaker.Shape();
    return ErrorCode::NoError;
}

NS_WY3D_END
