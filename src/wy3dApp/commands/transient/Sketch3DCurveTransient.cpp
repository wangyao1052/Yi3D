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

#include "Sketch3DCurveTransient.h"

#include <cassert>
#include <vector>

#include <osg/LineWidth>
#include <wyVector3.h>
#include <wy3dSketchCircle3D.h>

#include "scene/RenderConst.h"

// 采样段数(圆/弧足够圆滑,直线多点无碍)
static const int kSampleSegments = 64;

Sketch3DCurveTransient::Sketch3DCurveTransient(const wy3d::SketchCurve3D* pCurve)
    : GuiCmdTransient(), _id(wydb::ElementId::kNull)
{
    assert(pCurve);
    if (!pCurve)
    {
        return;
    }
    _id = pCurve->getId();

    // 采样折线化
    std::vector<wy::Vector3> points;
    points.reserve(kSampleSegments + 1);
    for (int i = 0; i <= kSampleSegments; ++i)
    {
        const double t = static_cast<double>(i) / kSampleSegments;
        points.emplace_back(pCurve->getPointAt(t));
    }

    // 圆为闭合曲线,补尾首段
    const bool closed = (wy3d::SketchCircle3D::cast(pCurve) != nullptr);
    this->initGeom(points, closed);
}

Sketch3DCurveTransient::~Sketch3DCurveTransient()
{
}

void Sketch3DCurveTransient::initGeom(const std::vector<wy::Vector3>& points, bool closed)
{
    if (points.size() < 2)
    {
        return;
    }

    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
    geom->setUseDisplayList(false);
    geom->setUseVertexBufferObjects(true);
    geom->setNodeMask(~PICK_MASK); // 不可拾取

    // 顶点数组
    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
    vertices->reserve(points.size());
    for (const wy::Vector3& pnt : points)
    {
        vertices->push_back(osg::Vec3(pnt.x(), pnt.y(), pnt.z()));
    }
    geom->setVertexArray(vertices);

    // 颜色:橙色(与2D草图参考高亮一致)
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    colors->push_back(osg::Vec4(1.0f, 0.392f, 0.039f, 1.0f));
    geom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);

    // 相邻点组成GL_LINES线段,闭合曲线补尾首段
    std::vector<unsigned int> indices;
    indices.reserve(points.size() * 2);
    for (size_t i = 0; i + 1 < points.size(); ++i)
    {
        indices.push_back(static_cast<unsigned int>(i));
        indices.push_back(static_cast<unsigned int>(i + 1));
    }
    if (closed)
    {
        indices.push_back(static_cast<unsigned int>(points.size() - 1));
        indices.push_back(0);
    }
    geom->addPrimitiveSet(new osg::DrawElementsUInt(GL_LINES, indices.cbegin(), indices.cend()));

    // 加粗
    geom->getOrCreateStateSet()->setAttribute(new osg::LineWidth(3.0));

    _root->addChild(geom.get());
}