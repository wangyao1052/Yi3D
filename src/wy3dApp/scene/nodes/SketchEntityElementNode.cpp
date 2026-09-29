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

#include "SketchEntityElementNode.h"

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
#include <OsgUtils.h>

#include <wyVector2.h>
#include <wyVector3.h>
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
#include <wy3dSketchPoint.h>

#include <osg/BlendFunc>
#include <osg/Material>
#include <osg/PolygonOffset>
#include <osg/CullFace>
#include <osg/LineWidth>
#include <osg/LineStipple>
#include <osg/Point>

#include "scene/SketchEntityLinearization.h"
#include "scene/RenderConst.h"
#include "scene/Colors.h"

// GL_LINES resets the stipple counter before every segment, while GL_LINE_STRIP keeps it running.
// Drawing the tessellated curves as a strip is what makes their dashes continuous.
static osg::ref_ptr<osg::UIntArray> makeStripIndices(const osg::UIntArray& lineIndices)
{
    assert(lineIndices.size() % 2 == 0);

    osg::ref_ptr<osg::UIntArray> stripIndices = new osg::UIntArray();
    stripIndices->reserve(lineIndices.size() / 2 + 1);
    for (size_t i = 0; i + 1 < lineIndices.size(); i += 2)
    {
        // The linearizations emit chained pairs (0,1)(1,2)..., so each pair only adds its end point.
        assert(0 == i || lineIndices[i - 1] == lineIndices[i]);
        if (0 == i) stripIndices->push_back(lineIndices[i]);
        stripIndices->push_back(lineIndices[i + 1]);
    }

    return stripIndices;
}

SketchEntityElementNode::SketchEntityElementNode(const wydb::ElementId& id) : ElementNode(id)
{
    // added by wangyao 2025.02.24 {
    // 设置草图图元渲染在前以防被遮挡
    // 关闭光照
    _osgNode->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    // 关闭深度测试
    _osgNode->getOrCreateStateSet()->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
    // 设置RenderBin
    _osgNode->getOrCreateStateSet()->setRenderBinDetails(RenderBinNumers::SketchEntity, "RenderBin");
    // }
}

bool SketchEntityElementNode::pickByNormalBoxImpl(osg::Polytope& polytope) const
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

void SketchEntityElementNode::generateRenderObjectImpl(Scene* pScene, const wydb::Element* pElem)
{
    assert(pElem);
    const wy3d::SketchEntity* pSketchEntity = wy3d::SketchEntity::cast(pElem);
    if (!pSketchEntity)
    {
        assert(false);
        return;
    }

    bool isConstruction(false);
    bool isCenterLine(false);
    if (const wy3d::SketchCurve* pSketchCurve = wy3d::SketchCurve::cast(pSketchEntity))
    {
        isConstruction = pSketchCurve->isConstruction();
        const wy3d::SketchCenterLine* pCenterLine = wy3d::SketchCenterLine::cast(pSketchCurve);
        if (pCenterLine) isCenterLine = true;
    }
    // Construction and centre lines share the curve colour, only thinner and dashed.
    const bool isDashed = (isConstruction || isCenterLine);
    float lineWidth = isDashed ? 1.5f : 2.0f;
    osg::Vec4 color = Colors::kSketchEntity;

    assert(_vertices);
    assert(_lineIndices);

    // 生成包围盒
    _boundBox = this->computeBoundingBox(*_vertices);

    // 生成渲染对象
    if (!_lineIndices->empty())
    {
        _curvesGeom = new osg::Geometry();
        _curvesGeom->setNodeMask(static_cast<unsigned int>(this->getNodeType()));
        {
            _curvesGeom->setUseDisplayList(false);
            _curvesGeom->setUseVertexBufferObjects(true);
            _curvesGeom->setVertexArray(_vertices);
            osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
            colors->push_back(color);
            _curvesGeom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);
            osg::ref_ptr<osg::UIntArray> stripIndices = makeStripIndices(*_lineIndices);
            _curvesGeom->addPrimitiveSet(new osg::DrawElementsUInt(GL_LINE_STRIP, stripIndices->begin(), stripIndices->end()));
            _curvesGeom->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
            _curvesGeom->setUserValue("ElementId", static_cast<unsigned int>(pElem->getId().value()));
            _curvesGeom->getOrCreateStateSet()->setAttribute(new osg::LineWidth(lineWidth));

            if (isDashed)
            {
                _curvesGeom->getOrCreateStateSet()->setAttributeAndModes(new osg::LineStipple(
                    CENTER_LINE_STIPPLE_FACTOR, CENTER_LINE_STIPPLE_PATTERN), osg::StateAttribute::ON);
            }
        }
        _osgNode->addChild(_curvesGeom);
    }
    else if (!_vertices->empty()) // 草图点
    {
        _curvesGeom = new osg::Geometry();
        _curvesGeom->setNodeMask(static_cast<unsigned int>(this->getNodeType()));
        {
            _curvesGeom->setUseDisplayList(false);
            _curvesGeom->setUseVertexBufferObjects(true);
            _curvesGeom->setVertexArray(_vertices);
            osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
            colors->push_back(color);
            _curvesGeom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);
            _curvesGeom->addPrimitiveSet(new osg::DrawArrays(GL_POINTS, 0, 1));
            _curvesGeom->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
            _curvesGeom->setUserValue("ElementId", static_cast<unsigned int>(pElem->getId().value()));
            _curvesGeom->getOrCreateStateSet()->setAttribute(new osg::Point(8.0f));
        }
        _osgNode->addChild(_curvesGeom);
    }
}

ElementNode::GenRenderDataRet SketchEntityElementNode::generateRenderDataImpl(Scene* pScene, const wydb::Element* pElement)
{
    _lineIndices = new osg::UIntArray();

    assert(pElement);
    const wy3d::SketchEntity* pSketchEntity = wy3d::SketchEntity::cast(pElement);
    if (!pSketchEntity)
    {
        assert(false);
        return GenRenderDataRet::Ok_Empty;
    }

    const wydb::Database* pDb = pElement->getDatabase();
    assert(pDb);
    wydb::ElementId sketchId = pSketchEntity->getParent();
    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(sketchId));
    if (!pSketch)
    {
        assert(false);
        return GenRenderDataRet::Ok_Empty;
    }
    const wy3d::SketchPlane& sketchPlane = pSketch->getPlane();

    if (const wy3d::SketchCurve* pSketchCurve = wy3d::SketchCurve::cast(pSketchEntity))
    {
        unsigned int totalNumVertices(0);
        unsigned int totalNumIndices(0);
        std::shared_ptr<SketchEntityLinearization> pEntityLinear = std::make_shared<SketchEntityLinearization>(pSketchEntity);
        totalNumVertices += pEntityLinear->getVertices().size();
        totalNumIndices += pEntityLinear->getIndices().size();

        _vertices->reserve(totalNumVertices);
        _lineIndices->reserve(totalNumIndices);
        unsigned int baseIndex = _vertices->size();
        for (const wy::Vector2& pnt2d : pEntityLinear->getVertices())
        {
            wy::Vector3 pnt3d = sketchPlane.value(pnt2d.x(), pnt2d.y());
            _vertices->push_back(osg::Vec3(pnt3d.x(), pnt3d.y(), pnt3d.z()));
        }
        for (unsigned int index : pEntityLinear->getIndices())
        {
            _lineIndices->push_back(baseIndex + index);
        }
    }
    else if (const wy3d::SketchPoint* pSketchPoint = wy3d::SketchPoint::cast(pSketchEntity))
    {
        wy::Vector3 pnt3d = sketchPlane.value(pSketchPoint->getPosition());
        _vertices->push_back(osg::Vec3(pnt3d.x(), pnt3d.y(), pnt3d.z()));
    }

    return GenRenderDataRet::Ok;
}

void SketchEntityElementNode::highlightImpl(bool flag)
{
    if (_curvesGeom)
        OsgUtils::setNodeColor(_curvesGeom, flag ? Colors::kSketchEntity_Highlight : Colors::kSketchEntity);
}

void SketchEntityElementNode::previewImpl(bool flag)
{
    if (this->isHighlighted())
    {
        assert(false);
        return;
    }
    if (_curvesGeom)
        OsgUtils::setNodeColor(_curvesGeom, flag ? Colors::kSketchEntity_Preview : Colors::kSketchEntity);
}

void SketchEntityElementNode::setActiveImpl(bool flag)
{
}