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

#include "SketchPolygonCircleOsgRenderer.h"

#include <cassert>
#include <cmath>
#include <vector>

#include <osg/Geometry>
#include <osg/LineStipple>
#include <osg/LineWidth>
#include <wyMath.h>
#include <wyVector2.h>
#include <wy3dSketch.h>

#include "gizmo/element/sketchpolygon/SketchPolygonCircleGhostGizmo.h"
#include "scene/Colors.h"
#include "scene/RenderConst.h"
#include "utils/MathUtils.h"
#include "utils/SketchClosedLoop.h"
#include "utils/SketchPolygonGeometry.h"
#include <wy3dApp/Application.h>
#include "application/ApplicationImpl.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

namespace {

// Fine enough that the circle reads as smooth at any zoom level
constexpr int kCircleSegments = 128;

} // namespace

SketchPolygonCircleOsgRenderer::SketchPolygonCircleOsgRenderer(
    Gizmo* pGizmo)
    : GhostOsgRenderer(pGizmo)
{
    assert(pGizmo);
}

SketchPolygonCircleOsgRenderer::~SketchPolygonCircleOsgRenderer()
{
}

void SketchPolygonCircleOsgRenderer::refresh()
{
    assert(_osgNode);
    _osgNode->removeChildren(0, _osgNode->getNumChildren());

    const wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    if (!pDb) return;

    const SketchPolygonCircleGhostGizmo* pGhostGizmo =
        dynamic_cast<const SketchPolygonCircleGhostGizmo*>(_pGizmo);
    if (!pGhostGizmo)
    {
        assert(false);
        return;
    }
    const SketchPolygonInfo& polygon = pGhostGizmo->getPolygonInfo();

    std::vector<wy::Vector2> corners;
    if (!readSketchClosedLoopCorners(
            pDb, polygon.lineIds, corners, nullptr, polygon.distTol))
    {
        return; // Lines deleted or no longer a closed loop: hide the circle
    }

    const PolygonRegularity regularity =
        validateRegularPolygon(corners, polygon.angleTol, polygon.distTol);
    if (!regularity.isValid) return;

    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(polygon.sketchId));
    if (!pSketch || !pSketch->getPlane().isValid()) return;
    const wy3d::Plane& plane = pSketch->getPlane();

    // The circle mirrors the dimension label: tangent to the edges or through
    // the vertices
    const double radius =
        SketchPolygonDimension::CircumscribedRadius == pGhostGizmo->getDimension()
        ? regularity.circumRadius
        : regularity.inRadius;

    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
    vertices->reserve(kCircleSegments);
    for (int i = 0; i < kCircleSegments; ++i)
    {
        const double angle = wy::TWO_PI * i / kCircleSegments;
        const wy::Vector2 uv = regularity.center
            + radius * wy::Vector2(std::cos(angle), std::sin(angle));
        vertices->push_back(MathUtils::toVec3(plane.value(uv)));
    }

    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    colors->push_back(Colors::kGhostGizmo);

    osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry();
    geometry->setUseDisplayList(false);
    geometry->setUseVertexBufferObjects(true);
    geometry->setVertexArray(vertices);
    geometry->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);
    geometry->addPrimitiveSet(new osg::DrawArrays(GL_LINE_LOOP, 0, kCircleSegments));

    osg::StateSet* pStateSet = geometry->getOrCreateStateSet();
    pStateSet->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    pStateSet->setAttributeAndModes(new osg::LineWidth(1.0f), osg::StateAttribute::ON);
    pStateSet->setAttributeAndModes(
        new osg::LineStipple(CENTER_LINE_STIPPLE_FACTOR, CENTER_LINE_STIPPLE_PATTERN),
        osg::StateAttribute::ON);

    _osgNode->addChild(geometry);
}

} // namespace wy3dApp
