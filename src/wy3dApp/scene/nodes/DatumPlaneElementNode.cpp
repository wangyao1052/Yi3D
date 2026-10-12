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

#include "DatumPlaneElementNode.h"

#include <cassert>

#include <gp_Quaternion.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Face.hxx>
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <Precision.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopExp.hxx>
#include <BRep_Tool.hxx>
#include <TopoDS.hxx>
#include <TopExp_Explorer.hxx>
#include <TColgp_Array1OfDir.hxx>
#include <Poly_Connect.hxx>
#include <GeomLib.hxx>

#include <osg/MatrixTransform>
#include <osg/LineStipple>
#include <OsgUtils.h>
#include <osg/Depth>

#include <wyVector2.h>
#include <wy3dMath.h>
#include <wydbDatabase.h>
#include <wy3dFeature.h>
#include <wy3dBox.h>
#include <wy3dCylinder.h>
#include <wy3dSphere.h>
#include <wy3dSketch.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>

#include <wy3dDatumPlane.h>

#include <osg/BlendFunc>
#include <osg/Material>
#include <osg/PolygonOffset>
#include <osg/CullFace>
#include <osg/LineWidth>

#include "scene/SketchEntityLinearization.h"
#include "scene/RenderConst.h"
#include "scene/Colors.h"
#include "utils/MathUtils.h"
#include "wy3dAppDefs.h"

#define SIDE 100.0

namespace wy3dApp {

DatumPlaneElementNode::DatumPlaneElementNode(const wydb::ElementId& id)
    : ElementNode(id), _min(-SIDE, -SIDE), _max(SIDE, SIDE)
{}

inline void updateMinMax(double& uMin, double& vMin, double& uMax, double& vMax,
    const wy3d::Plane& plane, wy::Vector2& uv, double x, double y, double z)
{
    uv = plane.uv(x, y, z);
    uMin = std::min(uMin, uv.x());
    vMin = std::min(vMin, uv.y());
    uMax = std::max(uMax, uv.x());
    vMax = std::max(vMax, uv.y());
}

void DatumPlaneElementNode::update(const wydb::Database* pDb, const osg::BoundingBox& bbox)
{
    if (!pDb)
    {
        assert(false);
        return;
    }

    if (!bbox.valid())
    {
        assert(false);
        return;
    }
    assert(!std::isnan(bbox.xMin()));
    assert(!std::isnan(bbox.yMin()));
    assert(!std::isnan(bbox.zMin()));
    assert(!std::isnan(bbox.xMax()));
    assert(!std::isnan(bbox.yMax()));
    assert(!std::isnan(bbox.zMax()));

    // 基准平面
    const wy3d::DatumPlane* pDatumPlane = wy3d::DatumPlane::cast(pDb->getElement(this->getElementId()));
    if (!pDatumPlane)
    {
        assert(false);
        return;
    }
    const wy3d::Plane& plane = pDatumPlane->getPlane();

    // 计算UV范围
    double uMin(DBL_MAX), vMin(DBL_MAX), uMax(-DBL_MAX), vMax(-DBL_MAX);

    // 空的包围盒使用默认UV大小
    if (bbox.radius() <= 1e-7)
    {
        uMin = -SIDE;
        vMin = -SIDE;
        uMax = SIDE;
        vMax = SIDE;
    }
    else // 计算UV
    {
        wy::Vector2 uv;
        updateMinMax(uMin, vMin, uMax, vMax, plane, uv,
            bbox.xMin(), bbox.yMin(), bbox.zMin());
        updateMinMax(uMin, vMin, uMax, vMax, plane, uv,
            bbox.xMin(), bbox.yMin(), bbox.zMax());
        updateMinMax(uMin, vMin, uMax, vMax, plane, uv,
            bbox.xMin(), bbox.yMax(), bbox.zMin());
        updateMinMax(uMin, vMin, uMax, vMax, plane, uv,
            bbox.xMin(), bbox.yMax(), bbox.zMax());
        updateMinMax(uMin, vMin, uMax, vMax, plane, uv,
            bbox.xMax(), bbox.yMin(), bbox.zMin());
        updateMinMax(uMin, vMin, uMax, vMax, plane, uv,
            bbox.xMax(), bbox.yMin(), bbox.zMax());
        updateMinMax(uMin, vMin, uMax, vMax, plane, uv,
            bbox.xMax(), bbox.yMax(), bbox.zMin());
        updateMinMax(uMin, vMin, uMax, vMax, plane, uv,
            bbox.xMax(), bbox.yMax(), bbox.zMax());

        // 更新UV范围
        double uRange = uMax - uMin;
        double uSpacing = 0.1 * uRange;
        uMin -= uSpacing;
        uMax += uSpacing;
        double vRange = vMax - vMin;
        double vSpacing = 0.1 * vRange;
        vMin -= vSpacing;
        vMax += vSpacing;

        // 特殊情况处理
        if (std::fabs(uRange) <= 1e-7)
        {
            double base = uMin;
            uMin = base - (vMax - vMin) / 2;
            uMax = base + (vMax - vMin) / 2;
        }
        else if (std::fabs(vRange) <= 1e-7)
        {
            double base = vMin;
            vMin = base - (uMax - uMin) / 2;
            vMax = base + (uMax - uMin) / 2;
        }
    }
    _min.set(uMin, vMin);
    _max.set(uMax, vMax);

    // 更新顶点坐标
    if (_vertices && _vertices->size() == 4)
    {
        (*_vertices)[0] = MathUtils::toVec3(plane.value(uMin, vMin));
        (*_vertices)[1] = MathUtils::toVec3(plane.value(uMax, vMin));
        (*_vertices)[2] = MathUtils::toVec3(plane.value(uMax, vMax));
        (*_vertices)[3] = MathUtils::toVec3(plane.value(uMin, vMax));
        _vertices->dirty();

        // 更新包围盒
        _boundBox = this->computeBoundingBox(*_vertices);
    }
    else
    {
        assert(false);
    }

    // 刷新渲染对象
    if (_planeGeom) _planeGeom->dirtyBound();
    if (_curvesGeom) _curvesGeom->dirtyBound();
}

bool DatumPlaneElementNode::pickByNormalBoxImpl(osg::Polytope& polytope) const
{
    // 是否包含所有点
    for (const osg::Vec3& vertex : *_vertices)
    {
        if (!polytope.contains(vertex))
        {
            return false;
        }
    }

    return true;
}

void DatumPlaneElementNode::generateRenderObjectImpl(Scene* pScene, const wydb::Element* pElem)
{
    assert(pElem);
    const wy3d::DatumPlane* pDatumPlane = wy3d::DatumPlane::cast(pElem);
    if (!pDatumPlane)
    {
        assert(false);
        return;
    }
    wydb::ElementId id = pElem->getId();

    assert(_vertices);
    assert(_normals);
    assert(_triangleIndices);
    assert(_lineIndices);

    // 生成包围盒
    _boundBox = this->computeBoundingBox(*_vertices);

    if (!_triangleIndices->empty())
    {
        _planeGeom = new osg::Geometry();
        _planeGeom->setNodeMask(~PICK_MASK); // 不可Pick
        _planeGeom->setUseDisplayList(false);
        _planeGeom->setUseVertexBufferObjects(true);
        _planeGeom->setDataVariance(osg::Object::DYNAMIC);
        _planeGeom->setVertexArray(_vertices);
        _planeGeom->setNormalArray(_normals, osg::Array::BIND_OVERALL);
        osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
        colors->push_back(Colors::kDatumPlaneFace);
        _planeGeom->setColorArray(colors, osg::Array::BIND_OVERALL);
        _planeGeom->addPrimitiveSet(new osg::DrawElementsUInt(GL_TRIANGLES,
            _triangleIndices->begin(), _triangleIndices->end()));

        osg::StateSet* stateset = _planeGeom->getOrCreateStateSet();

        // 1.混合与透明设置
        stateset->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
        stateset->setAttributeAndModes(
            new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA),
            osg::StateAttribute::ON);

        // 2.深度缓冲控制以避免透明面片互相覆盖
        stateset->setMode(GL_DEPTH_TEST, osg::StateAttribute::ON);
        stateset->setAttribute(new osg::Depth(
            osg::Depth::LESS,
            0.0, 1.0,
            false // 关闭深度写入!关键点!
        ));

        // 3.双面渲染(禁用背面剔除)
        stateset->setAttributeAndModes(
            new osg::CullFace(osg::CullFace::BACK),
            osg::StateAttribute::OFF);

        // 4.多边形偏移(解决Z-Fighting)
        osg::ref_ptr<osg::PolygonOffset> polyOffset = new osg::PolygonOffset();
        polyOffset->setFactor(-2.5f);  // 经验值：Creo 风格偏移
        polyOffset->setUnits(1.5f);
        stateset->setAttributeAndModes(polyOffset, osg::StateAttribute::ON);

        // 5.渲染排序（确保透明面最后绘制）
        stateset->setRenderBinDetails(
            /*20*/RenderBinNumers::DATUM_PLANE,  // 高于普通几何体的渲染顺序
            RenderBinNames::DepthSortedBin);

        // 6.禁用光照计算
        stateset->setMode(GL_LIGHTING, osg::StateAttribute::OFF);

        // 附加ID
        _planeGeom->setUserValue("ElementId", static_cast<unsigned int>(id.value()));

        _osgNode->addChild(_planeGeom);
    }

    if (!_lineIndices->empty())
    {
        _curvesGeom = new osg::Geometry();
        _curvesGeom->setNodeMask(static_cast<unsigned int>(this->getNodeType()));
        _curvesGeom->setUseDisplayList(false);
        _curvesGeom->setUseVertexBufferObjects(true);
        _curvesGeom->setDataVariance(osg::Object::DYNAMIC);
        _curvesGeom->setVertexArray(_vertices);
        osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
        colors->push_back(Colors::kDatumPlaneEdge);
        _curvesGeom->setColorArray(colors, osg::Array::BIND_OVERALL);
        _curvesGeom->addPrimitiveSet(new osg::DrawElementsUInt(GL_LINES,
            _lineIndices->begin(), _lineIndices->end()));

        osg::StateSet* stateset = _curvesGeom->getOrCreateStateSet();

        // 1.确保边线在透明面上方渲染
        //stateset->setRenderBinDetails(
        //    /*25*/RenderBinNumers::DATUM_PLANE_LINE,  // 比面片更高的渲染顺序
        //    "DepthSortedBin");
        stateset->setRenderBinDetails(
            /*25*/RenderBinNumers::DATUM_PLANE_LINE,  // 比面片更高的渲染顺序
            RenderBinNames::RenderBin);

        // 2.线宽与抗锯齿
        stateset->setAttribute(new osg::LineWidth(1.2f));
        stateset->setMode(GL_LINE_SMOOTH, osg::StateAttribute::ON);

        // 3. 禁用深度写入(避免影响后续渲染)
        //stateset->setAttribute(new osg::Depth(
        //    osg::Depth::LESS,
        //    0.0, 1.0,
        //    false));
        // 3.禁用深度测试(始终可见)
        stateset->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);

        // 4.禁用光照
        stateset->setMode(GL_LIGHTING, osg::StateAttribute::OFF);

        // 附加ID
        _curvesGeom->setUserValue("ElementId", static_cast<unsigned int>(id.value()));

        _osgNode->addChild(_curvesGeom);
    }
}

ElementNode::GenRenderDataRet DatumPlaneElementNode::generateRenderDataImpl(Scene* pScene, const wydb::Element* pElement)
{
    _normals = new osg::Vec3Array();
    _triangleIndices = new osg::UIntArray();
    _lineIndices = new osg::UIntArray();

    assert(pElement);
    const wy3d::DatumPlane* pDatumPlane = wy3d::DatumPlane::cast(pElement);
    if (!pDatumPlane)
    {
        assert(false);
        return GenRenderDataRet::Ok_Empty;
    }
    const wy3d::Plane& sketchPlane = pDatumPlane->getPlane();

    _vertices->reserve(4);
    _vertices->push_back(MathUtils::toVec3(sketchPlane.value(_min.x(), _min.y())));
    _vertices->push_back(MathUtils::toVec3(sketchPlane.value(_max.x(), _min.y())));
    _vertices->push_back(MathUtils::toVec3(sketchPlane.value(_max.x(), _max.y())));
    _vertices->push_back(MathUtils::toVec3(sketchPlane.value(_min.x(), _max.y())));

    _normals->reserve(1);
    _normals->push_back(MathUtils::toVec3(sketchPlane.getNormal()));

    _triangleIndices->reserve(6);
    _triangleIndices->push_back(0);
    _triangleIndices->push_back(1);
    _triangleIndices->push_back(2);
    _triangleIndices->push_back(2);
    _triangleIndices->push_back(3);
    _triangleIndices->push_back(0);

    _lineIndices->reserve(8);
    _lineIndices->push_back(0);
    _lineIndices->push_back(1);
    _lineIndices->push_back(1);
    _lineIndices->push_back(2);
    _lineIndices->push_back(2);
    _lineIndices->push_back(3);
    _lineIndices->push_back(3);
    _lineIndices->push_back(0);

    return GenRenderDataRet::Ok;
}

void DatumPlaneElementNode::highlightImpl(bool flag)
{
    if (_planeGeom)
    {
        OsgUtils::setNodeColor(_planeGeom, flag ? Colors::kDatumPlaneFace_Highlight : Colors::kDatumPlaneFace);

        _planeGeom->getOrCreateStateSet()->setRenderBinDetails(
            flag ? RenderBinNumers::DATUM_PLANE + 1 : RenderBinNumers::DATUM_PLANE,
            RenderBinNames::DepthSortedBin);
    }

    if (_curvesGeom)
    {
        OsgUtils::setNodeColor(_curvesGeom, flag ? Colors::kDatumPlaneEdge_Highlight : Colors::kDatumPlaneEdge);

        _curvesGeom->getOrCreateStateSet()->setAttribute(new osg::LineWidth(flag ? 2.0f : 1.2f));
        _curvesGeom->getOrCreateStateSet()->setRenderBinDetails(
            flag ? RenderBinNumers::DATUM_PLANE_LINE + 1 : RenderBinNumers::DATUM_PLANE_LINE,
            RenderBinNames::RenderBin);
    }
}

void DatumPlaneElementNode::previewImpl(bool flag)
{
    if (_planeGeom)
    {
        OsgUtils::setNodeColor(_planeGeom, flag ? Colors::kDatumPlaneFace_Preview : Colors::kDatumPlaneFace);

        _planeGeom->getOrCreateStateSet()->setRenderBinDetails(
            flag ? RenderBinNumers::DATUM_PLANE + 1 : RenderBinNumers::DATUM_PLANE,
            RenderBinNames::DepthSortedBin);
    }

    if (_curvesGeom)
    {
        OsgUtils::setNodeColor(_curvesGeom, flag ? Colors::kDatumPlaneEdge_Preview : Colors::kDatumPlaneEdge);

        _curvesGeom->getOrCreateStateSet()->setAttribute(new osg::LineWidth(flag ? 2.0f : 1.2f));
        _curvesGeom->getOrCreateStateSet()->setRenderBinDetails(
            flag ? RenderBinNumers::DATUM_PLANE_LINE + 1 : RenderBinNumers::DATUM_PLANE_LINE,
            RenderBinNames::RenderBin);
    }
}

void DatumPlaneElementNode::setActiveImpl(bool flag)
{
}

}
