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

#include "scene/nodes/DatumPlaneRenderObject.h"

#include <cassert>

#include <osg/Array>
#include <osg/BlendFunc>
#include <osg/CullFace>
#include <osg/Depth>
#include <osg/Geometry>
#include <osg/LineWidth>
#include <osg/PolygonOffset>
#include <osg/PrimitiveSet>

#include <OsgUtils.h>

#include "scene/RenderConst.h"

namespace wy3dApp {

DatumPlaneRenderObject::DatumPlaneRenderObject(const wydb::ElementId& id,
    ElementNodeType nodeType,
    MeshRenderData& data,
    const DatumPlaneRenderStyle& style)
    : _id(id),
    _nodeType(nodeType),
    _data(&data),
    _style(style)
{
    assert(_data);
}

DatumPlaneRenderObject::~DatumPlaneRenderObject() = default;

void DatumPlaneRenderObject::build()
{
    this->clear();
    _planeGeom = nullptr;
    _curvesGeom = nullptr;

    if (!_data)
    {
        assert(false);
        return;
    }

    // 面
    if (_data->triangleIndices && !_data->triangleIndices->empty())
    {
        _planeGeom = new osg::Geometry();
        _planeGeom->setNodeMask(~ElementNodeType::All); // 不可Pick
        _planeGeom->setUseDisplayList(false);
        _planeGeom->setUseVertexBufferObjects(true);
        _planeGeom->setDataVariance(osg::Object::DYNAMIC);
        _planeGeom->setVertexArray(_data->vertices);
        _planeGeom->setNormalArray(_data->normals, osg::Array::Binding::BIND_OVERALL);
        osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
        colors->push_back(_style.faceColor);
        _planeGeom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);
        _planeGeom->addPrimitiveSet(new osg::DrawElementsUInt(GL_TRIANGLES,
            _data->triangleIndices->begin(), _data->triangleIndices->end()));

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
            RenderBinNumers::DATUM_PLANE,
            RenderBinNames::DepthSortedBin);

        // 6.禁用光照计算
        stateset->setMode(GL_LIGHTING, osg::StateAttribute::OFF);

        // 附加ID
        _planeGeom->setUserValue("ElementId", static_cast<unsigned int>(_id.value()));

        _node->addChild(_planeGeom);
    }

    // 边
    if (_data->lineIndices && !_data->lineIndices->empty())
    {
        _curvesGeom = new osg::Geometry();
        _curvesGeom->setNodeMask(_nodeType);
        _curvesGeom->setUseDisplayList(false);
        _curvesGeom->setUseVertexBufferObjects(true);
        _curvesGeom->setDataVariance(osg::Object::DYNAMIC);
        _curvesGeom->setVertexArray(_data->vertices);
        osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
        colors->push_back(_style.edgeColor);
        _curvesGeom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);
        _curvesGeom->addPrimitiveSet(new osg::DrawElementsUInt(GL_LINES,
            _data->lineIndices->begin(), _data->lineIndices->end()));

        osg::StateSet* stateset = _curvesGeom->getOrCreateStateSet();

        // 1.确保边线在透明面上方渲染
        stateset->setRenderBinDetails(
            RenderBinNumers::DATUM_PLANE_LINE,
            RenderBinNames::RenderBin);

        // 2.线宽与抗锯齿
        stateset->setAttribute(new osg::LineWidth(1.2f));
        stateset->setMode(GL_LINE_SMOOTH, osg::StateAttribute::ON);

        // 3.禁用深度测试(始终可见)
        stateset->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);

        // 4.禁用光照
        stateset->setMode(GL_LIGHTING, osg::StateAttribute::OFF);

        // 附加ID
        _curvesGeom->setUserValue("ElementId", static_cast<unsigned int>(_id.value()));

        _node->addChild(_curvesGeom);
    }
}

void DatumPlaneRenderObject::setHighlighted(bool flag)
{
    if (_planeGeom)
    {
        OsgUtils::setNodeColor(_planeGeom, flag ? _style.faceHighlightColor : _style.faceColor);

        _planeGeom->getOrCreateStateSet()->setRenderBinDetails(
            flag ? RenderBinNumers::DATUM_PLANE + 1 : RenderBinNumers::DATUM_PLANE,
            RenderBinNames::DepthSortedBin);
    }

    if (_curvesGeom)
    {
        OsgUtils::setNodeColor(_curvesGeom, flag ? _style.edgeHighlightColor : _style.edgeColor);

        _curvesGeom->getOrCreateStateSet()->setAttribute(new osg::LineWidth(flag ? 2.0f : 1.2f));
        _curvesGeom->getOrCreateStateSet()->setRenderBinDetails(
            flag ? RenderBinNumers::DATUM_PLANE_LINE + 1 : RenderBinNumers::DATUM_PLANE_LINE,
            RenderBinNames::RenderBin);
    }
}

void DatumPlaneRenderObject::setPreviewed(bool flag)
{
    if (_planeGeom)
    {
        OsgUtils::setNodeColor(_planeGeom, flag ? _style.facePreviewColor : _style.faceColor);

        _planeGeom->getOrCreateStateSet()->setRenderBinDetails(
            flag ? RenderBinNumers::DATUM_PLANE + 1 : RenderBinNumers::DATUM_PLANE,
            RenderBinNames::DepthSortedBin);
    }

    if (_curvesGeom)
    {
        OsgUtils::setNodeColor(_curvesGeom, flag ? _style.edgePreviewColor : _style.edgeColor);

        _curvesGeom->getOrCreateStateSet()->setAttribute(new osg::LineWidth(flag ? 2.0f : 1.2f));
        _curvesGeom->getOrCreateStateSet()->setRenderBinDetails(
            flag ? RenderBinNumers::DATUM_PLANE_LINE + 1 : RenderBinNumers::DATUM_PLANE_LINE,
            RenderBinNames::RenderBin);
    }
}

void DatumPlaneRenderObject::dirtyGeometryBound()
{
    if (_planeGeom) _planeGeom->dirtyBound();
    if (_curvesGeom) _curvesGeom->dirtyBound();
}

} // namespace wy3dApp
