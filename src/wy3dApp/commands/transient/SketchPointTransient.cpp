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

#include "SketchPointTransient.h"

#include <cassert>
#include <osg/Point>
#include <wyVector2.h>
#include <wydbDatabase.h>
#include <wy3dSketch.h>
#include <wy3dSketchCurve.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>
#include <wy3dSketchEllipse.h>
#include <wy3dSketchEllipseArc.h>

#include "utils/MathUtils.h"
#include "scene/SketchEntityLinearization.h"
#include <wy3dApp/Application.h>
#include "scene/RenderConst.h"

SketchPointTransient::SketchPointTransient(const wy3d::Plane& sketchPlane, const wy::Vector2& point,
    float pointSize, const osg::Vec4& color) : _sketchPlane(sketchPlane)
{
    assert(pointSize > 0.0);

    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
    geom->setDataVariance(osg::Object::DYNAMIC);
    geom->setUseDisplayList(false);
    geom->setUseVertexBufferObjects(true);
    // 顶点数组
    _vertices = new osg::Vec3Array();
    _vertices->push_back(MathUtils::toVec3(sketchPlane.value(point)));
    geom->setVertexArray(_vertices);
    // 法向数组
    osg::ref_ptr<osg::Vec3Array> normals = new osg::Vec3Array();
    normals->push_back(osg::Vec3(0.0f, 0.0f, 1.0f));
    geom->setNormalArray(normals, osg::Array::Binding::BIND_OVERALL);
    // 颜色数组
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    colors->push_back(color);
    geom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);
    // 绘制点
    geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::POINTS, 0, 1));
    // 点大小
    geom->getOrCreateStateSet()->setAttribute(new osg::Point(pointSize));
    // 添加到根节点
    _root->addChild(geom.get());
    _geom = geom;
}

void SketchPointTransient::update(const wy::Vector2& point)
{
    assert(_vertices);
    (*_vertices)[0] = MathUtils::toVec3(_sketchPlane.value(point));
    _vertices->dirty();
    _geom->dirtyBound();
}