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

#include "SketchSplineCtrolPointsPathOsgRenderer.h"
#include <cassert>
#include <osg/LineStipple>
#include <wyVector2.h>
#include <wy3dSketch.h>
#include <wy3dSketchSpline.h>
#include "scene/RenderConst.h"
#include "scene/Colors.h"
#include "gizmo/BaseGizmo.h"
#include "utils/MathUtils.h"
#include <wy3dApp/Application.h>
#include "application/ApplicationImpl.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

void SketchSplineCtrolPointsPathOsgRenderer::refresh()
{
    wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    if (!pDb) return;
    const wydb::Element* pElem = pDb->getElement(_pGizmo->getModifiedElement());
    if (!pElem) return;

    assert(_osgNode);
    _osgNode->removeChildren(0, _osgNode->getNumChildren());
    osg::ref_ptr<osg::Geometry> geom = this->generateGeometry(pElem);
    if (geom)
        _osgNode->addChild(geom);
}

osg::ref_ptr<osg::Geometry> SketchSplineCtrolPointsPathOsgRenderer::generateGeometry(const wydb::Element* pElem)
{
    const wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pElem);
    if (!pSpline)
    {
        assert(false);
        return nullptr;
    }

    if (wy3d::SplineMode::ControlPoints != pSpline->getMode())
    {
        assert(false);
        return nullptr;
    }
    const std::vector<wy::Vector2>& points = pSpline->getPoints();
    if (points.empty())
    {
        assert(false);
        return nullptr;
    }

    const wydb::Database* pDb = pSpline->getDatabase();
    assert(pDb);
    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(pSpline->getParent()));
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
    vertices2 = points;
    size_t numVertices = vertices2.size();
    if (0 == numVertices)
    {
        assert(false);
        return nullptr;
    }

    std::vector<unsigned int> indices;
    indices.reserve(2 * (numVertices - 1));
    for (size_t i = 0; i < numVertices - 1; ++i)
    {
        indices.push_back(i);
        indices.push_back(i + 1);
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
        // 中心线
        geometry->getOrCreateStateSet()->setAttributeAndModes(
            new osg::LineStipple(CENTER_LINE_STIPPLE_FACTOR, CENTER_LINE_STIPPLE_PATTERN),
            osg::StateAttribute::ON);
    }
    return geometry;
}

} // namespace wy3dApp
