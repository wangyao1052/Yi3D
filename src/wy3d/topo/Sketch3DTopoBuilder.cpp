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
#include <Geom_Ellipse.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <gp_Ax2.hxx>
#include <ElCLib.hxx>

#include <wy3dMath.h>
#include <wy3dSketchEntity3D.h>
#include <wy3dSketchLine3D.h>
#include <wy3dSketchCircle3D.h>
#include <wy3dSketchArc3D.h>
#include <wy3dSketchEllipse3D.h>
#include <wy3dSketchEllipseArc3D.h>

#include "utils/OccUtil.h"

NS_WY3D_BEG

// 椭圆/椭圆弧共用的定位基(长轴方向 = xDir)
static bool buildEllipseAx2(const wy::Vector3& center, const wy::Vector3& normal,
    const wy::Vector3& xDirIn, gp_Ax2& ax2)
{
    if (normal.length() < 0.5)
    {
        assert(false);
        return false;
    }

    // Orthogonalize xDir to the ellipse plane (fallback to an arbitrary axis)
    wy::Vector3 xDir = xDirIn - normal * xDirIn.dot(normal);
    if (xDir.length() < 0.5)
    {
        wy::Vector3 ref = (std::fabs(normal.z()) < 0.9) ? wy::Vector3::kZAxis : wy::Vector3::kXAxis;
        xDir = ref - normal * ref.dot(normal);
    }
    if (xDir.length() < 0.5)
    {
        assert(false);
        return false;
    }
    xDir.normalize();

    ax2 = gp_Ax2(OccUtil::toPnt(center), OccUtil::toDir(normal), OccUtil::toDir(xDir));
    return true;
}

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
    else if (const wy3d::SketchArc3D* pArc = wy3d::SketchArc3D::cast(pEntity))
    {
        return this->makeEdge(pArc);
    }
    else if (const wy3d::SketchEllipse3D* pEllipse = wy3d::SketchEllipse3D::cast(pEntity))
    {
        return this->makeEdge(pEllipse);
    }
    else if (const wy3d::SketchEllipseArc3D* pEllipseArc = wy3d::SketchEllipseArc3D::cast(pEntity))
    {
        return this->makeEdge(pEllipseArc);
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

TopoDS_Edge Sketch3DTopoBuilder::makeEdge(const wy3d::SketchArc3D* pArc)
{
    assert(pArc);

    if (pArc->getRadius() < 1e-7) // unreachable: setRadius rejects below kMinValue
    {
        assert(false);
        return TopoDS_Edge();
    }
    if (pArc->getTotalAngle() < 1e-7) // degenerate arc; a full circle is a SketchCircle3D
    {
        return TopoDS_Edge();
    }

    const wy::Vector3& normal = pArc->getNormal();
    if (normal.length() < 0.5)
    {
        assert(false);
        return TopoDS_Edge();
    }

    // Orthogonalize xDir to the arc plane (fallback to an arbitrary axis)
    wy::Vector3 xDir = pArc->getXDir() - normal * pArc->getXDir().dot(normal);
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

    gp_Ax2 ax2(OccUtil::toPnt(pArc->getCenter()), OccUtil::toDir(normal), OccUtil::toDir(xDir));
    Handle(Geom_Circle) circle = new Geom_Circle(ax2, pArc->getRadius());
    double startAngle = ElCLib::InPeriod(pArc->getStartAngle(), 0.0, wy3d::TWO_PI);
    double endAngle = startAngle + pArc->getTotalAngle();
    Handle(Geom_TrimmedCurve) geomCurve = new Geom_TrimmedCurve(circle, startAngle, endAngle);
    return this->makeEdgeFromCurve(geomCurve, pArc->getId().value());
}

TopoDS_Edge Sketch3DTopoBuilder::makeEdge(const wy3d::SketchEllipse3D* pEllipse)
{
    assert(pEllipse);

    if (pEllipse->getMajorRadius() < 1e-7 || pEllipse->getMinorRadius() < 1e-7) // unreachable: setters reject below kMinValue
    {
        assert(false);
        return TopoDS_Edge();
    }

    gp_Ax2 ax2;
    if (!buildEllipseAx2(pEllipse->getCenter(), pEllipse->getNormal(), pEllipse->getXDir(), ax2))
    {
        return TopoDS_Edge();
    }

    Handle(Geom_Ellipse) ellipse = new Geom_Ellipse(ax2, pEllipse->getMajorRadius(), pEllipse->getMinorRadius());
    return this->makeEdgeFromCurve(ellipse, pEllipse->getId().value());
}

TopoDS_Edge Sketch3DTopoBuilder::makeEdge(const wy3d::SketchEllipseArc3D* pEllipseArc)
{
    assert(pEllipseArc);

    if (pEllipseArc->getMajorRadius() < 1e-7 || pEllipseArc->getMinorRadius() < 1e-7) // unreachable: setters reject below kMinValue
    {
        assert(false);
        return TopoDS_Edge();
    }
    if (pEllipseArc->getTotalAngle() < 1e-7) // degenerate arc; a full ellipse is a SketchEllipse3D
    {
        return TopoDS_Edge();
    }

    gp_Ax2 ax2;
    if (!buildEllipseAx2(pEllipseArc->getCenter(), pEllipseArc->getNormal(), pEllipseArc->getXDir(), ax2))
    {
        return TopoDS_Edge();
    }

    Handle(Geom_Ellipse) ellipse = new Geom_Ellipse(ax2, pEllipseArc->getMajorRadius(), pEllipseArc->getMinorRadius());
    // Geom_TrimmedCurve 的参数是参数角(偏近点角),而实体存的是极角,须先把区间长度换到参数角
    // (极角到参数角是保向的一一映射,区间长度直接对应;用"起点+长度"可避免跨 2π 回卷)
    const double majorRadius = pEllipseArc->getMajorRadius();
    const double minorRadius = pEllipseArc->getMinorRadius();
    const double startPolar = wy3d::normalizeRadian(pEllipseArc->getStartAngle());
    const double startAngle = wy3d::ellipsePolarAngleToParametricAngle(startPolar, majorRadius, minorRadius);
    double totalAngle = wy3d::ellipsePolarAngleToParametricAngle(startPolar + pEllipseArc->getTotalAngle(), majorRadius, minorRadius)
        - startAngle;
    while (totalAngle < 0.0)
    {
        totalAngle += wy3d::TWO_PI;
    }
    Handle(Geom_TrimmedCurve) geomCurve = new Geom_TrimmedCurve(ellipse, startAngle, startAngle + totalAngle);
    return this->makeEdgeFromCurve(geomCurve, pEllipseArc->getId().value());
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
