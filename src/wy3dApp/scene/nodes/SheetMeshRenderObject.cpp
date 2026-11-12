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

#include "scene/nodes/SheetMeshRenderObject.h"

#include <cassert>

#include <osg/BlendFunc>
#include <osg/Geometry>
#include <osg/MatrixTransform>
#include <osg/PolygonOffset>
#include <osg/StateAttribute>
#include <osg/StateSet>

#include <wy3dSheet.h>

#include <OsgUtils.h>

#include <wy3dApp/Colors.h>
#include "scene/RenderConst.h"

namespace wy3dApp {

SheetMeshRenderObject::SheetMeshRenderObject(const wydb::ElementId& id,
    ElementNodeType nodeType,
    MeshRenderData& data,
    const wy3d::Sheet* pSheet)
    : MeshRenderObject(id, nodeType, data, /*doubleSided=*/true, Colors::sheetMesh)
{
    _faceColor = _palette.element.normal.face;
    assert(pSheet);
    if (pSheet)
    {
        this->updateStyle(*pSheet);
    }
}

void SheetMeshRenderObject::updateStyle(const wy3d::Sheet& sheet)
{
    _faceColor = OsgUtils::toOsgColor(sheet.getColor());
}

osg::Vec4 SheetMeshRenderObject::getFaceDefaultColor() const
{
    return _faceColor;
}

osg::Vec4 SheetMeshRenderObject::getEdgeDefaultColor() const
{
    return _palette.element.normal.edge;
}

void SheetMeshRenderObject::updateAppearance(
    const DisplayState& dispState,
    const wydb::Element* pElement)
{
    if (pElement)
    {
        if (const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pElement))
        {
            this->updateStyle(*pSheet);
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
                stateSet->setRenderBinDetails(0, "RenderBin");
                // Push faces slightly behind coplanar edge geometry to keep outlines crisp.
                osg::ref_ptr<osg::PolygonOffset> polyOffset = new osg::PolygonOffset(1.0f, 1.0f);
                stateSet->setAttributeAndModes(polyOffset,
                    osg::StateAttribute::OVERRIDE | osg::StateAttribute::ON);

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
            edgeColor = _wireframe ? _palette.edge.preview : _palette.element.preview.edge;
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
        else
        {
            stateSet->setMode(GL_DEPTH_TEST, osg::StateAttribute::ON);
            stateSet->setRenderBinDetails(0, "RenderBin");
        }
    }
}

} // namespace wy3dApp
