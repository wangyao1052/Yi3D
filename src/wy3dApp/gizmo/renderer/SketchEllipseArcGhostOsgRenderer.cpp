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

#include "SketchEllipseArcGhostOsgRenderer.h"
#include <cassert>
#include <wyVector2.h>
#include <wy3dSketch.h>
#include <wy3dSketchEllipseArc.h>
#include "scene/SketchEntityLinearization.h"
#include "scene/Colors.h"
#include "gizmo/BaseGizmo.h"
#include "utils/MathUtils.h"
#include <wy3dApp/Application.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

void SketchEllipseArcGhostOsgRenderer::refresh()
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return;
    assert(_pGizmo->getElementIds().size() == 1);
    const wydb::Element* pElem = pDb->getElement(_pGizmo->getFirstElementId());
    if (!pElem) return;

    assert(_osgNode);
    _osgNode->removeChildren(0, _osgNode->getNumChildren());
    osg::ref_ptr<osg::Geometry> geom = this->generateGeometry(pElem);
    if (geom)
        _osgNode->addChild(geom);
}

osg::ref_ptr<osg::Geometry> SketchEllipseArcGhostOsgRenderer::generateGeometry(const wydb::Element* pElem)
{
    const wy3d::SketchEllipseArc* pEllipseArc = wy3d::SketchEllipseArc::cast(pElem);
    if (!pEllipseArc)
    {
        assert(false);
        return nullptr;
    }

    const wydb::Database* pDb = pEllipseArc->getDatabase();
    assert(pDb);
    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(pEllipseArc->getParent()));
    if (!pSketch)
    {
        assert(false);
        return nullptr;
    }
    const wy3d::Plane& sketchPlane = pSketch->getPlane();
    if (!sketchPlane.isValid())
    {
        assert(false);
        return nullptr;
    }

    std::vector<wy::Vector2> vertices2;
    std::vector<unsigned int> indices;
    {
        wy::Vector2 center = pEllipseArc->getCenter();
        double majorRadius = pEllipseArc->getMajorRadius();
        double minorRadius = pEllipseArc->getMinorRadius();

        // 椭圆弧从实际椭圆弧的终点开始绘制到起点(绘制的是其余部分从而组成完整的圆弧)
        double twoPI = wy3d::PI * 2;
        double totalAngle = pEllipseArc->getTotalAngle();
        assert(totalAngle >= 0 && totalAngle < twoPI);
        double drawStartAngle = MathUtils::normalizeRadian(pEllipseArc->getStartAngle() + totalAngle);
        double drawTotalAngle = std::fabs(twoPI - totalAngle);
        double drawEndAngle = drawStartAngle + drawTotalAngle;

        // 转换为参数角度
        drawStartAngle = wy3d::ellipsePolarAngleToParametricAngle(drawStartAngle, majorRadius, minorRadius);
        drawEndAngle = wy3d::ellipsePolarAngleToParametricAngle(drawEndAngle, majorRadius, minorRadius);
        if (drawEndAngle < drawStartAngle) drawEndAngle += twoPI;
        drawTotalAngle = drawEndAngle - drawStartAngle;
        assert(totalAngle >= 0 && totalAngle < twoPI);

        // 离散点数量
        size_t numVertices = (drawTotalAngle / twoPI) * 200;
        if (numVertices < 100) numVertices = 100;
        if (numVertices > 200) numVertices = 200;

        // 计算长轴与X轴的夹角
        wy::Vector2 majorAxis = pEllipseArc->getMajorAxis();
        double angle = std::atan2(majorAxis.y(), majorAxis.x());
        double cosAngle = std::cos(angle);
        double sinAngle = std::sin(angle);

        // 计算离散点
        vertices2.reserve(numVertices);
        double delta = drawTotalAngle / (numVertices - 1);
        for (size_t i = 0; i < numVertices; ++i)
        {
            // 计算椭圆的每个点（长轴和短轴）
            double x = std::cos(i * delta + drawStartAngle) * majorRadius;
            double y = std::sin(i * delta + drawStartAngle) * minorRadius;

            // 使用旋转矩阵旋转点
            double xRot = x * cosAngle - y * sinAngle;
            double yRot = x * sinAngle + y * cosAngle;

            // 计算旋转后的点的位置并添加到顶点数组
            vertices2.emplace_back(wy::Vector2(xRot + center.x(), yRot + center.y()));
        }

        indices.reserve(2 * (numVertices - 1));
        for (size_t i = 0; i < numVertices - 1; ++i)
        {
            indices.push_back(i);
            indices.push_back(i + 1);
        }
    }
    if (vertices2.empty() || indices.empty())
    {
        assert(false);
        return nullptr;
    }

    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
    vertices->reserve(vertices2.size());
    for (const wy::Vector2& uv : vertices2)
    {
        vertices->push_back(MathUtils::toVec3(sketchPlane.value(uv)));
    }

    osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry();
    {
        geometry->setUseDisplayList(false);
        geometry->setUseVertexBufferObjects(true);
        geometry->setVertexArray(vertices);
        osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
        colors->push_back(Colors::kGhostGizmo);
        geometry->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);
        geometry->addPrimitiveSet(new osg::DrawElementsUInt(GL_LINES, indices.begin(), indices.end()));
        geometry->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    }
    return geometry;
}

} // namespace wy3dApp
