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

#include "scene/nodes/SolidMeshRenderObject.h"

#include <cassert>

#include <osg/BlendFunc>
#include <osg/Geometry>
#include <osg/MatrixTransform>
#include <osg/PolygonOffset>
#include <osg/StateAttribute>
#include <osg/StateSet>

#include <wy3dSolid.h>

#include <OsgUtils.h>

#include "scene/Colors.h"
#include "scene/RenderConst.h"

namespace wy3dApp {

namespace {

// cut 体专属颜色，只在本类中使用
const osg::Vec4 kCutFace(0.0f, 0.75f, 1.0f, 1.0f);  // 半透明青蓝色
const osg::Vec4 kCutEdge(0.0f, 0.3f, 0.6f, 1.0f);   // 深青色边框

} // namespace

SolidMeshRenderObject::SolidMeshRenderObject(const wydb::ElementId& id,
    ElementNodeType nodeType,
    MeshRenderData& data,
    const wy3d::Solid* pSolid)
    : MeshRenderObject(id, nodeType, data, /*doubleSided=*/false, Colors::solidMesh)
{
    _faceColor = _palette.element.normal.face;
    assert(pSolid);
    if (pSolid)
    {
        this->updateStyle(*pSolid);
    }
}

void SolidMeshRenderObject::updateStyle(const wy3d::Solid& solid)
{
    _cut = solid.isCut();
    _faceColor = Colors::toOsgColor(solid.getColor());
}

osg::Vec4 SolidMeshRenderObject::getFaceDefaultColor() const
{
    return _cut ? kCutFace : _faceColor;
}

osg::Vec4 SolidMeshRenderObject::getEdgeDefaultColor() const
{
    return _cut ? kCutEdge : _palette.element.normal.edge;
}

void SolidMeshRenderObject::updateAppearance(
    const DisplayState& dispState,
    const wydb::Element* pElement)
{
    if (pElement)
    {
        if (const wy3d::Solid* pSolid = wy3d::Solid::cast(pElement))
        {
            this->updateStyle(*pSolid);
        }
    }

    // 面
    if (_shapeNode)
    {
        osg::Vec4 faceColor = getFaceDefaultColor();
        if (dispState.highlighted)
        {
            faceColor = dispState.active ? _palette.element.highlight.face : _palette.inactive;
        }
        else if (dispState.previewed)
        {
            faceColor = _palette.element.preview.face;
        }
        OsgUtils::setNodeColor(_shapeNode, faceColor);

        if (_shapeGeom)
        {
            osg::StateSet* stateSet = _shapeGeom->getOrCreateStateSet();
            if (dispState.active)
            {
                if (_cut)
                {
                    stateSet->setRenderBinDetails(1, "RenderBin");
                    // z-fighting
                    osg::ref_ptr<osg::PolygonOffset> polyOffset = new osg::PolygonOffset(-1.0f, 1.0f);
                    stateSet->setAttributeAndModes(polyOffset,
                        osg::StateAttribute::OVERRIDE | osg::StateAttribute::ON);
                }
                else
                {
                    stateSet->setRenderBinDetails(0, "RenderBin");
                    // Push faces slightly behind coplanar edge geometry to keep outlines crisp.
                    osg::ref_ptr<osg::PolygonOffset> polyOffset = new osg::PolygonOffset(1.0f, 1.0f);
                    stateSet->setAttributeAndModes(polyOffset,
                        osg::StateAttribute::OVERRIDE | osg::StateAttribute::ON);
                }

                // remove transparent
                stateSet->setRenderingHint(osg::StateSet::DEFAULT_BIN);
                stateSet->removeAttribute(osg::StateAttribute::Type::BLENDFUNC);
            }
            else
            {
                // transparent
                stateSet->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
                stateSet->setAttributeAndModes(new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
                // z-fighting
                osg::ref_ptr<osg::PolygonOffset> polyOffset = new osg::PolygonOffset(-1.0f, 1.0f);
                stateSet->setAttributeAndModes(polyOffset,
                    osg::StateAttribute::OVERRIDE | osg::StateAttribute::ON);
            }
        }
    }

    // 边
    if (_edgeGeom)
    {
        osg::Vec4 edgeColor = getEdgeDefaultColor();
        if (dispState.highlighted)
        {
            edgeColor = dispState.active ? _palette.element.highlight.edge : _palette.inactive;
        }
        else if (dispState.previewed)
        {
            edgeColor = _wireframe ? _palette.element.preview.edgeWireframe : _palette.element.preview.edge;
        }
        OsgUtils::setNodeColor(_edgeGeom, edgeColor);

        osg::StateSet* stateSet = _edgeGeom->getOrCreateStateSet();
        if (dispState.highlighted)
        {
            stateSet->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
            stateSet->setRenderBinDetails(RenderBinNumers::Highlight, "RenderBin");
        }
        else if (dispState.previewed)
        {
            // 预览时关闭深度测试使边线始终可见
            stateSet->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
            stateSet->setRenderBinDetails(RenderBinNumers::Preview, "RenderBin");
        }
        else if (dispState.active && _cut)
        {
            stateSet->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
            stateSet->setRenderBinDetails(RenderBinNumers::Highlight, "RenderBin");
        }
        else
        {
            stateSet->setMode(GL_DEPTH_TEST, osg::StateAttribute::ON);
            stateSet->setRenderBinDetails(0, "RenderBin");
        }
    }
}

} // namespace wy3dApp
