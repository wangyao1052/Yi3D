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

#include "scene/nodes/BodyModificationRenderObject.h"

#include <cassert>

#include <osg/Array>
#include <osg/Geometry>
#include <osg/LineWidth>
#include <osg/MatrixTransform>
#include <osg/PrimitiveSet>

#include <OsgUtils.h>

#include "scene/RenderConst.h"

namespace wy3dApp {

BodyModificationRenderObject::BodyModificationRenderObject(const wydb::ElementId& id,
    ElementNodeType nodeType,
    LineRenderData& data,
    const BodyModificationRenderStyle& style)
    : _id(id),
    _nodeType(nodeType),
    _data(&data),
    _style(style)
{
    assert(_data);
}

BodyModificationRenderObject::~BodyModificationRenderObject() = default;

void BodyModificationRenderObject::build()
{
    this->clear();
    _edgeNode = nullptr;
    _edgeGeom = nullptr;

    if (!_data || !_data->lineIndices || _data->lineIndices->empty())
    {
        return;
    }

    _edgeGeom = new osg::Geometry();
    _edgeGeom->setNodeMask(_nodeType);
    _edgeGeom->setUseDisplayList(false);
    _edgeGeom->setUseVertexBufferObjects(true);
    _edgeGeom->setVertexArray(_data->vertices);
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    colors->push_back(_style.edgeColor);
    _edgeGeom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);
    _edgeGeom->addPrimitiveSet(new osg::DrawElementsUInt(GL_LINES,
        _data->lineIndices->begin(), _data->lineIndices->end()));
    _edgeGeom->setUserValue("ElementId", static_cast<unsigned int>(_id.value()));
    _edgeGeom->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    _edgeGeom->getOrCreateStateSet()->setAttribute(new osg::LineWidth(2.0));

    _edgeNode = new osg::MatrixTransform(_data->matrix);
    _edgeNode->addChild(_edgeGeom);
    _edgeNode->setNodeMask(0); // 默认不显示只在高亮和预览的时候显示
    _node->addChild(_edgeNode);
}

void BodyModificationRenderObject::updateAppearance(
    const DisplayState& dispState,
    const wydb::Element* pElement)
{
    (void)pElement;
    //_displayState = dispState;

    if (!_edgeNode)
    {
        return;
    }

    osg::Vec4 color = _style.edgeColor;
    if (dispState.highlighted)
    {
        color = dispState.active ? _style.highlightColor : _style.inactiveColor;
    }
    else if (dispState.previewed)
    {
        color = _style.previewColor;
    }
    OsgUtils::setNodeColor(_edgeNode, color);

    osg::StateSet* stateSet = _edgeNode->getOrCreateStateSet();
    if (dispState.active && dispState.highlighted)
    {
        // 高亮时关闭深度测试使边线始终可见
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

    // 高亮/预览时显示叠加边线
    _edgeNode->setNodeMask(
        (dispState.highlighted || dispState.previewed) ? ~ElementNodeType::All : 0);
}

} // namespace wy3dApp
