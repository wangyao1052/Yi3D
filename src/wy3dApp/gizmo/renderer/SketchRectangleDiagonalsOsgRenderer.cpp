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

#include "SketchRectangleDiagonalsOsgRenderer.h"

#include <array>
#include <cassert>

#include <osg/Geometry>
#include <osg/LineStipple>
#include <osg/LineWidth>
#include <wyVector2.h>
#include <wy3dSketch.h>

#include "gizmo/element/sketchrectangle/SketchRectangleDiagonalsGhostGizmo.h"
#include "scene/Colors.h"
#include "scene/RenderConst.h"
#include "utils/MathUtils.h"
#include "utils/SketchRectangleRecognizer.h"
#include <wy3dApp/Application.h>
#include "application/ApplicationImpl.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

SketchRectangleDiagonalsOsgRenderer::SketchRectangleDiagonalsOsgRenderer(BaseGizmo* pGizmo)
    : GhostOsgRenderer(pGizmo)
{
    assert(pGizmo);
}

SketchRectangleDiagonalsOsgRenderer::~SketchRectangleDiagonalsOsgRenderer()
{
}

void SketchRectangleDiagonalsOsgRenderer::refresh()
{
    assert(_osgNode);
    _osgNode->removeChildren(0, _osgNode->getNumChildren());

    const wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    if (!pDb) return;

    const SketchRectangleDiagonalsGhostGizmo* pGhostGizmo =
        dynamic_cast<const SketchRectangleDiagonalsGhostGizmo*>(_pGizmo);
    if (!pGhostGizmo)
    {
        assert(false);
        return;
    }
    const SketchRectangleInfo& rect = pGhostGizmo->getRectangleInfo();

    std::array<wy::Vector2, 4> corners{};
    if (!readSketchRectangleCorners(pDb, rect.lineIds, corners))
    {
        return; // 线被删除/不闭合：不显示对角线
    }

    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(rect.sketchId));
    if (!pSketch || !pSketch->getPlane().isValid()) return;
    const wy3d::Plane& plane = pSketch->getPlane();

    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
    vertices->reserve(4);
    vertices->push_back(MathUtils::toVec3(plane.value(corners[0])));
    vertices->push_back(MathUtils::toVec3(plane.value(corners[2])));
    vertices->push_back(MathUtils::toVec3(plane.value(corners[1])));
    vertices->push_back(MathUtils::toVec3(plane.value(corners[3])));

    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    colors->push_back(Colors::kGhostGizmo);

    osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry();
    geometry->setUseDisplayList(false);
    geometry->setUseVertexBufferObjects(true);
    geometry->setVertexArray(vertices);
    geometry->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);
    geometry->addPrimitiveSet(new osg::DrawArrays(GL_LINES, 0, 4));

    osg::StateSet* pStateSet = geometry->getOrCreateStateSet();
    pStateSet->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    pStateSet->setAttributeAndModes(new osg::LineWidth(1.0f), osg::StateAttribute::ON);
    pStateSet->setAttributeAndModes(
        new osg::LineStipple(CENTER_LINE_STIPPLE_FACTOR, CENTER_LINE_STIPPLE_PATTERN),
        osg::StateAttribute::ON);

    _osgNode->addChild(geometry);
}

} // namespace wy3dApp
