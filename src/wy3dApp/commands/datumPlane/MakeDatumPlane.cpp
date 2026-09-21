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

#include "MakeDatumPlane.h"

#include <Geom_Line.hxx>
#include <Geom_Circle.hxx>
#include <Geom_Ellipse.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <gp_Cylinder.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopExp.hxx>
#include <BRep_Tool.hxx>
#include <BRepLib.hxx>
#include <BRepTools.hxx>
#include <Precision.hxx>
#include <Standard_Failure.hxx>

#include <wyVector2.h>
#include <wyVector3.h>
#include <utils/wy3dCurveIntersectionUtil.h>
#include <wydbTransaction.h>

#include <wy3dSketch.h>
#include <wy3dSketchCurve.h>
#include <wy3dSketchCurve3D.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>
#include <wy3dSketchEllipse.h>
#include <wy3dSketchEllipseArc.h>
#include <wy3dSketchSpline.h>
#include <wy3dSketchSpline.h>
#include <wy3dCurve.h>
#include "wy3d/topo/Sketch3DTopoBuilder.h"

#include "application/Application.h"
#include "scene/Scene.h"
#include "utils/TopoShapeUtil.h"
#include "utils/MathUtils.h"
#include "utils/SplineUtil.h"

void MakeDatumPlane::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pDatumPlane) idSet.insert(_pDatumPlane->getId());
}

bool MakeDatumPlane::create(const wy3d::SketchPlane& sketchPlane)
{
    if (!_pDb || !_pTopTrans || _pDatumPlane || _isFinished)
    {
        return false;
    }
    if (!sketchPlane.isValid())
    {
        assert(false);
        return false;
    }

    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction();
    if (!pTrans) return false;
    wy3d::DatumPlane* pDatumPlane(nullptr);
    if (wy::ErrorStatus::Ok != wy3d::DatumPlane::create(pTrans, sketchPlane, pDatumPlane))
    {
        assert(false);
        _pDb->getTransactionManager()->abortTransaction();
        return false;
    }
    _pDb->getTransactionManager()->endTransaction();
    _pDatumPlane = pDatumPlane;
    // 刷新基准面的显示
    if (Scene* pScene = Application::instance().getActiveScene())
    {
        pScene->updateDatumPlaneVisualSize(_pDb);
    }
    return true;
}

bool MakeDatumPlane::update(const wy3d::SketchPlane& sketchPlane)
{
    if (!_pDb || !_pTopTrans || !_pDatumPlane || _isFinished)
    {
        return false;
    }
    if (!sketchPlane.isValid())
    {
        return false;
    }

    wydb::TransactionManager* pTransMgr = _pDb->getTransactionManager();
    wydb::Transaction* pTrans = pTransMgr->startTransaction();
    if (!pTrans) return false;
    if (wy::ErrorStatus::Ok != _pDatumPlane->upgradeForWrite())
    {
        assert(false);
        pTransMgr->abortTransaction();
        return false;
    }
    if (wy::ErrorStatus::Ok != _pDatumPlane->setPlane(sketchPlane))
    {
        assert(false);
        pTransMgr->abortTransaction();
        return false;
    }
    wy::ErrorStatus error = pTransMgr->endTransaction();
    assert(wy::ErrorStatus::Ok == error);
    pTransMgr->mergeTransaction();
    return true;
}

bool MakeDatumPlane::getSketchPlane(const wyap::Selection& sel, wy3d::SketchPlane& sketchPlane)
{
    const wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return false;
    }

    if (wy3d::UIntToSelectionType(sel.getSelectionType()) == wy3d::SelectionType::Face)
    {
        if (sel.getSubPath().empty())
        {
            assert(false);
            return false;
        }
        unsigned int faceIndex = std::stoul(sel.getSubPath());
        if (faceIndex == -1)
        {
            assert(false);
            return false;
        }
        const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(sel.getElementId()));
        if (!pSolid)
        {
            assert(false);
            return false;
        }
        TopoDS_Shape shape = pSolid->getShape();
        return TopoShapeUtil::getShapeFacePlane(shape, faceIndex, sketchPlane);
    }
    else if (wy3d::UIntToSelectionType(sel.getSelectionType()) == wy3d::SelectionType::Element)
    {
        const wy3d::DatumPlane* pDatumPlane = wy3d::DatumPlane::cast(pDb->getElement(sel.getElementId()));
        if (!pDatumPlane)
        {
            assert(false);
            return false;
        }
        sketchPlane = pDatumPlane->getPlane();
        return true;
    }
    else
    {
        assert(false);
        return false;
    }
}

bool MakeDatumPlane::getSolidEdgeEndPoints(const wyap::Selection& sel, wy::Vector3& startPnt, wy::Vector3& endPnt)
{
    const wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return false;
    }
    if (wy3d::UIntToSelectionType(sel.getSelectionType()) != wy3d::SelectionType::Edge)
    {
        assert(false);
        return false;
    }

    if (sel.getSubPath().empty())
    {
        assert(false);
        return false;
    }
    unsigned int edgeIndex = std::stoul(sel.getSubPath());
    if (edgeIndex == -1)
    {
        assert(false);
        return false;
    }

    const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(sel.getElementId()));
    if (!pSolid)
    {
        assert(false);
        return false;
    }
    TopoDS_Shape shape = pSolid->getShape();
    return TopoShapeUtil::getShapeEdgeEndPoints(shape, edgeIndex, startPnt, endPnt);
}

Handle(Geom_Curve) MakeDatumPlane::getSolidEdgeGeomCurve(const wyap::Selection& sel)
{
    const wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return nullptr;
    }
    if (wy3d::UIntToSelectionType(sel.getSelectionType()) != wy3d::SelectionType::Edge)
    {
        assert(false);
        return nullptr;
    }

    if (sel.getSubPath().empty())
    {
        assert(false);
        return nullptr;
    }
    unsigned int edgeIndex = std::stoul(sel.getSubPath());
    if (static_cast<unsigned int>(-1) == edgeIndex)
    {
        assert(false);
        return nullptr;
    }

    const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(sel.getElementId()));
    if (!pSolid)
    {
        assert(false);
        return nullptr;
    }
    TopoDS_Shape shape = pSolid->getShape();
    return TopoShapeUtil::getShapeEdgeCurve(shape, edgeIndex);
}

Handle(Geom_Curve) MakeDatumPlane::getCurveGeomCurve(const wyap::Selection& sel)
{
    const wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return nullptr;
    }
    if (wy3d::UIntToSelectionType(sel.getSelectionType()) != wy3d::SelectionType::Element)
    {
        assert(false);
        return nullptr;
    }

    if (!sel.getSubPath().empty())
    {
        assert(false);
        return nullptr;
    }

    const wy3d::Curve* pCurve = wy3d::Curve::cast(pDb->getElement(sel.getElementId()));
    if (!pCurve)
    {
        assert(false);
        return nullptr;
    }
    TopoDS_Edge edge = pCurve->getEdge();
    if (edge.IsNull())
    {
        assert(false);
        return nullptr;
    }
    return TopoShapeUtil::getShapeEdgeCurve(edge, 0);
}

Handle(Geom_Curve) MakeDatumPlane::getSketchCurve3DGeomCurve(const wyap::Selection& sel)
{
    const wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return nullptr;
    }
    if (wy3d::UIntToSelectionType(sel.getSelectionType()) != wy3d::SelectionType::SketchCurve3D)
    {
        assert(false);
        return nullptr;
    }

    if (sel.getSubPath().empty())
    {
        assert(false);
        return nullptr;
    }

    try
    {
        unsigned int curveId = std::stoul(sel.getSubPath());
        if (0 == curveId)
        {
            assert(false);
            return nullptr;
        }

        const wydb::Element* pElem = pDb->getElement(wydb::ElementId(curveId));
        const wy3d::SketchCurve3D* pSketchCurve3D = wy3d::SketchCurve3D::cast(pElem);
        if (!pSketchCurve3D)
        {
            assert(false);
            return nullptr;
        }

        wy3d::Sketch3DTopoBuilder sketch3DTopoBuilder;
        TopoDS_Edge edge = sketch3DTopoBuilder.makeEdge(pSketchCurve3D);
        if (edge.IsNull())
        {
            assert(false);
            return nullptr;
        }

        // For entities built by Sketch3DTopoBuilder, BRep_Tool::Curve hands back the basis curve:
        // the trim range lives on the edge only, so it has to be carried over explicitly
        BRepLib::BuildCurve3d(edge, wy3d::TOL);
        double first(0.0), last(0.0);
        Handle(Geom_Curve) geomCurve = BRep_Tool::Curve(edge, first, last);
        if (geomCurve.IsNull() || Precision::IsInfinite(first) || Precision::IsInfinite(last))
        {
            assert(false);
            return nullptr;
        }
        if (geomCurve->IsKind(STANDARD_TYPE(Geom_TrimmedCurve))) return geomCurve;
        return new Geom_TrimmedCurve(geomCurve, first, last);
    }
    catch (const Standard_Failure&)
    {
        assert(false);
        return nullptr;
    }
    catch (...)
    {
        assert(false);
        return nullptr;
    }
}

Handle(Geom_Curve) MakeDatumPlane::getSketchCurveGeomCurve(const wyap::Selection& sel)
{
    const wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return nullptr;
    }
    if (wy3d::UIntToSelectionType(sel.getSelectionType()) != wy3d::SelectionType::SketchCurve)
    {
        assert(false);
        return nullptr;
    }

    if (sel.getSubPath().empty())
    {
        assert(false);
        return nullptr;
    }
    unsigned int curveId = std::stoul(sel.getSubPath());
    if (0 == curveId)
    {
        assert(false);
        return nullptr;
    }

    const wydb::Element* pElem = pDb->getElement(wydb::ElementId(curveId));
    const wy3d::SketchCurve* pSketchCurve = wy3d::SketchCurve::cast(pElem);
    if (!pSketchCurve)
    {
        assert(false);
        return nullptr;
    }

    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(
        pDb->getElement(pSketchCurve->getParent()));
    if (!pSketch)
    {
        assert(false);
        return nullptr;
    }
    const wy3d::SketchPlane& plane = pSketch->getPlane();
    if (!plane.isValid())
    {
        assert(false);
        return nullptr;
    }

    wyrx::ClassInfo* classInfo = pSketchCurve->getClassInfo();
    if (classInfo == wy3d::SketchLine::classInfo())
    {
        const wy3d::SketchLine* pSketchLine = wy3d::SketchLine::cast(pSketchCurve);
        if (!pSketchLine)
        {
            assert(false);
            return nullptr;
        }

        wy::Vector3 startPnt = plane.value(pSketchLine->getStartPoint());
        wy::Vector3 endPnt = plane.value(pSketchLine->getEndPoint());
        if ((sel.getPickPosition() - startPnt).length() > (sel.getPickPosition() - endPnt).length())
        {
            std::swap(startPnt, endPnt);
        }

        wy::Vector3 dir = endPnt - startPnt;
        dir.normalize();
        if (dir.length() < 0.5)
        {
            assert(false);
            return nullptr;
        }

        Handle(Geom_Line) geomLine = new Geom_Line(
            MathUtils::toPnt(startPnt), MathUtils::toDir(dir));
        Handle(Geom_TrimmedCurve) geomLineSeg = new Geom_TrimmedCurve(geomLine, 0.0, (endPnt - startPnt).length());
        return geomLineSeg;
    }
    else if (classInfo == wy3d::SketchCenterLine::classInfo())
    {
        const wy3d::SketchCenterLine* pSketchCenterLine = wy3d::SketchCenterLine::cast(pSketchCurve);
        if (!pSketchCenterLine)
        {
            assert(false);
            return nullptr;
        }

        wy::Vector3 startPnt = plane.value(pSketchCenterLine->getStartPoint());
        wy::Vector3 endPnt = plane.value(pSketchCenterLine->getEndPoint());
        if ((sel.getPickPosition() - startPnt).length() > (sel.getPickPosition() - endPnt).length())
        {
            std::swap(startPnt, endPnt);
        }

        wy::Vector3 dir = endPnt - startPnt;
        dir.normalize();
        if (dir.length() < 0.5)
        {
            assert(false);
            return nullptr;
        }

        Handle(Geom_Line) geomLine = new Geom_Line(
            MathUtils::toPnt(startPnt), MathUtils::toDir(dir));
        Handle(Geom_TrimmedCurve) geomLineSeg = new Geom_TrimmedCurve(geomLine, 0.0, (endPnt - startPnt).length());
        return geomLineSeg;
    }
    else if (classInfo == wy3d::SketchCircle::classInfo())
    {
        const wy3d::SketchCircle* pSketchCircle = wy3d::SketchCircle::cast(pSketchCurve);
        if (!pSketchCircle)
        {
            assert(false);
            return nullptr;
        }

        wy::Vector3 center = plane.value(pSketchCircle->getCenter());
        double radius = pSketchCircle->getRadius();

        gp_Ax2 ax2(MathUtils::toPnt(center), MathUtils::toDir(plane.getNormal()), MathUtils::toDir(plane.getXDir()));
        Handle(Geom_Circle) geomCircle = new Geom_Circle(ax2, radius);
        return geomCircle;
    }
    else if (classInfo == wy3d::SketchArc::classInfo())
    {
        const wy3d::SketchArc* pSketchArc = wy3d::SketchArc::cast(pSketchCurve);
        if (!pSketchArc)
        {
            assert(false);
            return nullptr;
        }

        wy::Vector3 center = plane.value(pSketchArc->getCenter());
        double radius = pSketchArc->getRadius();
        double startAngle = wy3d::normalizeRadian(pSketchArc->getStartAngle());
        double endAngle = wy3d::normalizeRadian(pSketchArc->getEndAngle());
        if (endAngle < startAngle)
        {
            endAngle += wy3d::TWO_PI;
        }

        gp_Ax2 ax2(MathUtils::toPnt(center), MathUtils::toDir(plane.getNormal()), MathUtils::toDir(plane.getXDir()));
        Handle(Geom_Circle) geomCircle = new Geom_Circle(ax2, radius);
        Handle(Geom_TrimmedCurve) geomArc = new Geom_TrimmedCurve(geomCircle, startAngle, endAngle);
        return geomArc;
    }
    else if (classInfo == wy3d::SketchEllipse::classInfo())
    {
        const wy3d::SketchEllipse* pSketchEllipse = wy3d::SketchEllipse::cast(pSketchCurve);
        if (!pSketchEllipse)
        {
            assert(false);
            return nullptr;
        }

        wy::Vector3 center = plane.value(pSketchEllipse->getCenter());
        double majorRadius = pSketchEllipse->getMajorRadius();
        double minorRadius = pSketchEllipse->getMinorRadius();
        if (majorRadius <= 0.0 || minorRadius <= 0.0 || majorRadius < minorRadius)
        {
            assert(false);
            return nullptr;
        }
        wy::Vector3 majorAxis = plane.value(pSketchEllipse->getMajorAxis()) - plane.getOrigin();
        majorAxis.normalize();
        if (majorAxis.length() < 0.5)
        {
            assert(false);
            return nullptr;
        }

        gp_Ax2 ax2(
            MathUtils::toPnt(center),
            MathUtils::toDir(plane.getNormal()),
            MathUtils::toDir(majorAxis)
        );
        Handle(Geom_Ellipse) geomEllipse = new Geom_Ellipse(ax2, majorRadius, minorRadius);
        return geomEllipse;
    }
    else if (classInfo == wy3d::SketchEllipseArc::classInfo())
    {
        const wy3d::SketchEllipseArc* pSketchEllipseArc = wy3d::SketchEllipseArc::cast(pSketchCurve);
        if (!pSketchEllipseArc)
        {
            assert(false);
            return nullptr;
        }

        wy::Vector3 center = plane.value(pSketchEllipseArc->getCenter());
        double majorRadius = pSketchEllipseArc->getMajorRadius();
        double minorRadius = pSketchEllipseArc->getMinorRadius();
        if (majorRadius <= 0.0 || minorRadius <= 0.0 || majorRadius < minorRadius)
        {
            assert(false);
            return nullptr;
        }
        wy::Vector3 majorAxis = plane.value(pSketchEllipseArc->getMajorAxis()) - plane.getOrigin();
        majorAxis.normalize();
        if (majorAxis.length() < 0.5)
        {
            assert(false);
            return nullptr;
        }
        // 几何角度转参数角度
        double startAngle = MathUtils::ellipseGeometricToParametricAngle(pSketchEllipseArc->getStartAngle(), majorRadius, minorRadius);
        double endAngle = MathUtils::ellipseGeometricToParametricAngle(pSketchEllipseArc->getEndAngle(), majorRadius, minorRadius);
        if (endAngle < startAngle)
        {
            endAngle += wy3d::TWO_PI;
        }

        gp_Ax2 ax2(
            MathUtils::toPnt(center),
            MathUtils::toDir(plane.getNormal()),
            MathUtils::toDir(majorAxis)
        );
        Handle(Geom_Ellipse) geomEllipse = new Geom_Ellipse(ax2, majorRadius, minorRadius);
        Handle(Geom_TrimmedCurve) geomEllipseArc = new Geom_TrimmedCurve(geomEllipse, startAngle, endAngle);
        return geomEllipseArc;
    }
    else if (classInfo == wy3d::SketchSpline::classInfo())
    {
        const wy3d::SketchSpline* pSketchSpline = wy3d::SketchSpline::cast(pSketchCurve);
        if (!pSketchSpline)
        {
            assert(false);
            return nullptr;
        }
        Handle(Geom2d_BSplineCurve) pBSpline2d = pSketchSpline->getOccSpline();
        if (pBSpline2d.IsNull())
        {
            assert(false);
            return nullptr;
        }
        return SplineUtil::convertToBSplineCurve3D(pBSpline2d, plane);
    }
    else
    {
        assert(false);
        return nullptr;
    }

    return nullptr;
}

bool MakeDatumPlane::getSolidCylindricalFaceCenterPlane(
    const wyap::Selection& sel,
    wy3d::SketchPlane& plane,
    double& radius)
{
    const wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return false;
    }
    if (wy3d::UIntToSelectionType(sel.getSelectionType()) != wy3d::SelectionType::Face)
    {
        assert(false);
        return false;
    }

    if (sel.getSubPath().empty())
    {
        assert(false);
        return false;
    }
    unsigned int faceIndex = std::stoul(sel.getSubPath());
    if (faceIndex == -1)
    {
        assert(false);
        return false;
    }

    const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(sel.getElementId()));
    if (!pSolid) return false;
    TopoDS_Shape shape = pSolid->getShape();
    if (shape.IsNull()) return false;

    TopTools_IndexedMapOfShape faceMap;
    TopExp::MapShapes(shape, TopAbs_FACE, faceMap);
    faceIndex += 1; // OCC中以1为起始序号
    if (faceMap.Size() < faceIndex) return false;
    const TopoDS_Shape& faceShape = faceMap.FindKey(faceIndex);

    TopoDS_Face face = TopoDS::Face(faceShape);
    if (face.IsNull()) return false;
    Handle(Geom_Surface) surface = BRep_Tool::Surface(face);
    Handle(Geom_CylindricalSurface) cylindricalSurface = Handle(Geom_CylindricalSurface)::DownCast(surface);
    if (cylindricalSurface.IsNull()) return false;

    double uMin, uMax, vMin, vMax;
    BRepTools::UVBounds(face, uMin, uMax, vMin, vMax);
    gp_Cylinder cylinder = cylindricalSurface->Cylinder();
    const gp_Ax3& axisSystem = cylinder.Position();
    const gp_Pnt& axisOrigin = axisSystem.Location();
    const gp_Dir& axisDir = axisSystem.Direction();

    wy::Vector3 origin = MathUtils::toVector3(axisOrigin.Translated((vMin + vMax) / 2.0 * axisDir));
    wy::Vector3 normal = MathUtils::toVector3(axisSystem.Direction());
    wy::Vector3 xDir = MathUtils::toVector3(axisSystem.XDirection());

    // 如有必要反转法向使与全局坐标系的XYZ正向靠拢
    // 在XOY平面内绘制草图圆,再沿Z轴正向创建拉伸体;选中这个拉伸体的圆柱面;axisDir的方向为(0,0,-1);不知道为啥是这样的;
    // 所以以下代码就判断下有无反转法向的必要
    double zPart = std::fabs(axisDir.Z());
    double xPart = std::fabs(axisDir.X());
    double yPart = std::fabs(axisDir.Y());
    if (zPart >= xPart && zPart >= yPart)
    {
        if (axisDir.Z() < 0.0) normal = -normal;
    }
    else if (xPart >= yPart && xPart >= zPart)
    {
        if (axisDir.X() < 0.0) normal = -normal;
    }
    else if (yPart >= xPart && yPart >= zPart)
    {
        if (axisDir.Y() < 0.0) normal = -normal;
    }

    plane = wy3d::SketchPlane(origin, normal, xDir);
    radius = cylinder.Radius();
    assert(radius > 0.0);
    return true;
}

bool MakeDatumPlane::intersect(
    const wy::Vector2& plnMinPnt, const wy::Vector2& plnMaxPnt,
    const wy::Vector2& lineOrigin, const wy::Vector2& lineDir,
    wy::Vector2& intStartPnt, wy::Vector2& intEndPnt,
    double tol)
{
    assert(plnMaxPnt.x() > plnMinPnt.x());
    assert(plnMaxPnt.y() > plnMinPnt.y());
    wy::Vector2 lineOther = lineOrigin + lineDir;
    assert(std::fabs(lineDir.length() - 1.0) <= tol);

    // 自定义比较器
    struct DoubleCompare
    {
    public:
        DoubleCompare(double tol) : _tol(tol) {}
        bool operator()(double a, double b) const
        {
            if (std::fabs(a - b) <= _tol) return false;
            return a < b;
        }

    private:
        double _tol;
    };
    DoubleCompare compare(tol);
    std::set<double, DoubleCompare> ts(compare);

    // 分别求出4个交点
    auto computeIntersectLineLine = [&ts, &lineOrigin, &lineOther, &lineDir](const wy::Vector2& startPnt, const wy::Vector2& endPnt)
    {
        wy::Vector2 intPnt;
        if (wy3d::intersectLineLine(startPnt, endPnt, lineOrigin, lineOther, intPnt))
        {
            ts.insert((intPnt - lineOrigin).dot(lineDir));
        }
    };
    computeIntersectLineLine(plnMinPnt, plnMinPnt + wy::Vector2(1.0, 0.0));
    computeIntersectLineLine(plnMinPnt, plnMinPnt + wy::Vector2(0.0, 1.0));
    computeIntersectLineLine(plnMaxPnt, plnMaxPnt + wy::Vector2(1.0, 0.0));
    computeIntersectLineLine(plnMaxPnt, plnMaxPnt + wy::Vector2(0.0, 1.0));

    // 结果
    size_t num = ts.size();
    if (num == 2 || num == 3)
    {
        auto iter = ts.cbegin();
        double start = *iter;
        ++iter;
        double end = *iter;
        intStartPnt = lineOrigin + lineDir * start;
        intEndPnt = lineOrigin + lineDir * end;
        return true;
    }
    else if (num == 4)
    {
        auto iter = ts.cbegin();
        double start = *iter;
        ++iter;
        double end = *iter;
        intStartPnt = lineOrigin + lineDir * start;
        intEndPnt = lineOrigin + lineDir * end;

        // 如果第2个点在基准面矩形上,说明第3个点也在基准面矩形上;
        if (intEndPnt.x() >= plnMinPnt.x() - tol && intEndPnt.x() <= plnMaxPnt.x() + tol &&
            intEndPnt.y() >= plnMinPnt.y() - tol && intEndPnt.y() <= plnMaxPnt.y() + tol) // 交点在基准面内
        {
            intStartPnt = intEndPnt;
            ++iter;
            end = *iter;
            intEndPnt = lineOrigin + lineDir * end;

            // 如果交线距离过短则适当延长
            double sideLen1 = std::fabs(plnMaxPnt.x() - plnMinPnt.x());
            double sideLen2 = std::fabs(plnMaxPnt.y() - plnMinPnt.y());
            double sideLen = sideLen1 < sideLen2 ? sideLen1 : sideLen2;
            if ((intEndPnt - intStartPnt).length() < sideLen / 10)
            {
                wy::Vector2 center = (intStartPnt + intEndPnt) / 2;
                intStartPnt = center - sideLen / 20 * lineDir;
                intEndPnt = center + sideLen / 20 * lineDir;
            }
        }

        return true;
    }
    else // 0, 1 or > 4
    {
        assert(false);
        return false;
    }
}

bool MakeDatumPlane::intersect(
    const wy3d::SketchPlane& lhs, const wy3d::SketchPlane& rhs,
    wy::Vector3& linePoint, wy::Vector3& lineDir, double tol)
{
    // 检查平面有效性
    if (!lhs.isValid() || !rhs.isValid())
        return false;

    // 获取两个平面的法向量
    const wy::Vector3& n1 = lhs.getNormal();
    const wy::Vector3& n2 = rhs.getNormal();

    // 计算原始叉积向量（未归一化）
    wy::Vector3 rawDir = n1.cross(n2);
    double dirLength = rawDir.length();

    // 检查平面是否平行
    if (dirLength <= tol)
        return false;

    // 归一化交线方向向量（最终输出）
    lineDir = rawDir / dirLength;

    // 计算平面方程的常数项（d = n·p）
    double d1 = n1.dot(lhs.getOrigin());
    double d2 = n2.dot(rhs.getOrigin());

    // 计算正确的辅助向量
    wy::Vector3 u = n2.cross(rawDir);  // n2 × (n1×n2)
    wy::Vector3 v = rawDir.cross(n1);  // (n1×n2) × n1

    // 计算交线上的点
    double sqrLen = dirLength * dirLength;
    linePoint = (d1 * u + d2 * v) / sqrLen;

    return true;
}

bool MakeDatumPlane::computeThrough3PointsPlane(
    const wy::Vector3& pnt1, const wy::Vector3& pnt2, const wy::Vector3& pnt3,
    wy3d::SketchPlane& sketchPlane, double tol)
{
    // 计算两个向量
    wy::Vector3 v1 = pnt2 - pnt1;
    wy::Vector3 v2 = pnt3 - pnt1;

    // 计算法向量（叉积）
    wy::Vector3 normal = v1.cross(v2);

    // 检查三点是否共线（叉积长度接近0）
    double normalLength = normal.length();
    if (normalLength <= tol)
    {
        return false;  // 三点共线，无法确定平面
    }

    // 归一化法向量
    normal /= normalLength;

    // X 轴向量
    wy::Vector3 xDir = v1;
    xDir.normalize();
    if (xDir.length() < 0.5)
    {
        assert(false);
        return false;
    }

    // 结果
    sketchPlane = wy3d::SketchPlane(pnt1, normal, xDir);
    return true;
}