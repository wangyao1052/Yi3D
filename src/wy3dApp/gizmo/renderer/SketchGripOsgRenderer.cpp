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

#include "SketchGripOsgRenderer.h"

#include "gizmo/BaseGizmo.h"
#include "gizmo/element/SketchEntityGripGizmo.h"
#include "OsgUtils.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

SketchGripOsgRenderer::SketchGripOsgRenderer(BaseGizmo* pGizmo)
    : OsgGizmoRenderer(pGizmo)
{
    _geom = this->generateGeometry();
    _at = new osg::AutoTransform();
    _at->setAutoRotateMode(osg::AutoTransform::ROTATE_TO_SCREEN);
    _at->setAutoScaleToScreen(true);
    _at->addChild(_geom);
    this->getOsgNode()->addChild(_at.get());
}

SketchGripOsgRenderer::~SketchGripOsgRenderer()
{
}

osg::ref_ptr<osg::Geometry> SketchGripOsgRenderer::generateGeometry()
{
    // 几何
    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
    geom->setUseDisplayList(false);
    geom->setUseVertexBufferObjects(true);

    // 顶点数组
    float size(4);
    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
    vertices->resize(5);
    (*vertices)[0] = osg::Vec3(-size, -size, 0.0f);
    (*vertices)[1] = osg::Vec3( size, -size, 0.0f);
    (*vertices)[2] = osg::Vec3( size,  size, 0.0f);
    (*vertices)[3] = osg::Vec3(-size,  size, 0.0f);
    geom->setVertexArray(vertices);

    // 法向数组
    osg::ref_ptr<osg::Vec3Array> normals = new osg::Vec3Array();
    normals->push_back(osg::Vec3(0.0f, 0.0f, 1.0f));
    geom->setNormalArray(normals, osg::Array::Binding::BIND_OVERALL);

    // 颜色数组
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    colors->push_back(OsgGizmoNode::SKETCH_ENTITY_COLOR);
    geom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);

    // 索引数组
    osg::ref_ptr<osg::UShortArray> indices = new osg::UShortArray();
    indices->resize(6);
    (*indices)[0] = 0;
    (*indices)[1] = 1;
    (*indices)[2] = 2;
    (*indices)[3] = 2;
    (*indices)[4] = 3;
    (*indices)[5] = 0;

    // 绘制三角面片
    geom->addPrimitiveSet(new osg::DrawElementsUShort(GL_TRIANGLES, indices->begin(), indices->end()));

    // 关闭光照
    geom->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);

    return geom;
}

void SketchGripOsgRenderer::onActivate()
{
    OsgUtils::setNodeColor(_geom.get(), OsgGizmoNode::RED_COLOR);
}

void SketchGripOsgRenderer::onDeactivate()
{
    OsgUtils::setNodeColor(_geom.get(), OsgGizmoNode::SKETCH_ENTITY_COLOR);
}

void SketchGripOsgRenderer::setPosition(const wy::Vector3& pos)
{
    _at->setPosition(osg::Vec3d(pos.x(), pos.y(), pos.z()));
}

void SketchGripOsgRenderer::refresh()
{
    auto* pGizmo = static_cast<SketchEntityGripGizmo*>(_pGizmo);
    wy::Vector3 pos = pGizmo->_sketchPlane.value(pGizmo->getGripPosition());
    this->setPosition(pos);
}

}
