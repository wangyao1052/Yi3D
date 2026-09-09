///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 Wang Yao <wangyao1052@163.com>
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

#include "Sketch3DTopoBuilder.h"

#include <cassert>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRep_Tool.hxx>
#include <Geom_Line.hxx>
#include <Geom_Circle.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <gp_Ax2.hxx>

#include <wy3dSketchEntity3D.h>
#include <wy3dSketchLine3D.h>
#include <wy3dSketchCircle3D.h>

#include "utils/OccUtil.h"

NS_WY3D_BEG

Sketch3DTopoBuilder::Sketch3DTopoBuilder(bool recordTopoHistory)
    : _recordTopoHistory(recordTopoHistory)
{
}

TopoDS_Edge Sketch3DTopoBuilder::makeEdge(const wy3d::SketchEntity3D* pEntity)
{
    assert(pEntity);
    if (const wy3d::SketchLine3D* pLine = wy3d::SketchLine3D::cast(pEntity))
    {
        return this->makeEdge(pLine);
    }
    else if (const wy3d::SketchCircle3D* pCircle = wy3d::SketchCircle3D::cast(pEntity))
    {
        return this->makeEdge(pCircle);
    }
    else
    {
        assert(false);
        return TopoDS_Edge();
    }
}

TopoDS_Edge Sketch3DTopoBuilder::makeEdge(const wy3d::SketchLine3D* pLine)
{
    assert(pLine);

    const wy::Vector3 startPnt = pLine->getStartPoint();
    const wy::Vector3 endPnt = pLine->getEndPoint();
    wy::Vector3 dir = endPnt - startPnt;
    if (dir.length() < 1e-7) // degenerate line
    {
        return TopoDS_Edge();
    }

    Handle(Geom_Line) line = new Geom_Line(OccUtil::toPnt(startPnt), OccUtil::toDir(dir));
    Handle(Geom_TrimmedCurve) geomCurve = new Geom_TrimmedCurve(line, 0.0, pLine->getLength());
    return this->makeEdgeFromCurve(geomCurve, pLine->getId().value());
}

TopoDS_Edge Sketch3DTopoBuilder::makeEdge(const wy3d::SketchCircle3D* pCircle)
{
    assert(pCircle);

    if (pCircle->getRadius() < 1e-7)
    {
        assert(false);
        return TopoDS_Edge();
    }

    const wy::Vector3& normal = pCircle->getNormal();
    if (normal.length() < 0.5)
    {
        assert(false);
        return TopoDS_Edge();
    }

    // Orthogonalize xDir to the circle plane (fallback to an arbitrary axis)
    wy::Vector3 xDir = pCircle->getXDir() - normal * pCircle->getXDir().dot(normal);
    if (xDir.length() < 0.5)
    {
        wy::Vector3 ref = (std::fabs(normal.z()) < 0.9) ? wy::Vector3::kZAxis : wy::Vector3::kXAxis;
        xDir = ref - normal * ref.dot(normal);
    }
    if (xDir.length() < 0.5)
    {
        assert(false);
        return TopoDS_Edge();
    }
    xDir.normalize();

    gp_Ax2 ax2(OccUtil::toPnt(pCircle->getCenter()), OccUtil::toDir(normal), OccUtil::toDir(xDir));
    Handle(Geom_Circle) circle = new Geom_Circle(ax2, pCircle->getRadius());
    return this->makeEdgeFromCurve(circle, pCircle->getId().value());
}

TopoDS_Edge Sketch3DTopoBuilder::makeEdgeFromCurve(const Handle(Geom_Curve)& geomCurve, unsigned int entityId)
{
    BRepBuilderAPI_MakeEdge makeEdge(geomCurve);
    if (makeEdge.IsDone())
    {
        if (_recordTopoHistory)
        {
            double first(0.0), last(0.0);
            Handle(Geom_Curve) curve = BRep_Tool::Curve(makeEdge.Edge(), first, last);
            assert(!curve.IsNull());
            _curve2Id[curve] = entityId;
        }
        return makeEdge.Edge();
    }
    else
    {
        assert(false);
        return TopoDS_Edge();
    }
}

NS_WY3D_END
