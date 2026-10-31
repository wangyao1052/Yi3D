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

#include "scene/nodes/LineRenderObject.h"

#include <cassert>

#include <osg/Array>
#include <osg/Geometry>
#include <osg/LineStipple>
#include <osg/LineWidth>
#include <osg/Point>
#include <osg/PrimitiveSet>

#include <OsgUtils.h>

namespace wy3dApp {

namespace {

// GL_LINES resets the stipple counter before every segment, while GL_LINE_STRIP keeps it running.
// Drawing the tessellated curves as a strip is what makes their dashes continuous.
osg::ref_ptr<osg::UIntArray> makeStripIndices(const osg::UIntArray& lineIndices)
{
    assert(lineIndices.size() % 2 == 0);

    osg::ref_ptr<osg::UIntArray> stripIndices = new osg::UIntArray();
    stripIndices->reserve(lineIndices.size() / 2 + 1);
    for (size_t i = 0; i + 1 < lineIndices.size(); i += 2)
    {
        // The linearizations emit chained pairs (0,1)(1,2)..., so each pair only adds its end point.
        assert(0 == i || lineIndices[i - 1] == lineIndices[i]);
        if (0 == i) stripIndices->push_back(lineIndices[i]);
        stripIndices->push_back(lineIndices[i + 1]);
    }

    return stripIndices;
}

} // namespace

LineRenderObject::LineRenderObject(const wydb::ElementId& id,
    ElementNodeType nodeType,
    LineRenderData& data,
    osg::UIntArray& indices,
    const LineRenderStyle& style)
    : _id(id),
    _nodeType(nodeType),
    _data(&data),
    _indices(&indices),
    _style(style)
{
    assert(_data);
}

LineRenderObject::~LineRenderObject() = default;

void LineRenderObject::build()
{
    this->clear();
    _geom = nullptr;
    _isPoints = false;

    if (!_data)
    {
        assert(false);
        return;
    }

    // 根节点状态
    if (_style.applyStateToRoot)
    {
        osg::StateSet* stateSet = _node->getOrCreateStateSet();
        stateSet->setMode(GL_LIGHTING, _style.lighting ? osg::StateAttribute::ON : osg::StateAttribute::OFF);
        stateSet->setMode(GL_DEPTH_TEST, _style.depthTest ? osg::StateAttribute::ON : osg::StateAttribute::OFF);
        stateSet->setRenderBinDetails(_style.renderBin, _style.renderBinName);
    }

    bool isLine = false;
    osg::ref_ptr<osg::UIntArray> stripIndices;
    if (_indices && !_indices->empty())
    {
        if (_style.indicesAsPoints)
        {
            _isPoints = true;
        }
        else
        {
            isLine = true;
            if (_style.stripFromSegments)
            {
                stripIndices = makeStripIndices(*_indices);
            }
        }
    }
    else if (_style.pointsWhenNoLines && _data->vertices && !_data->vertices->empty())
    {
        _isPoints = true;
    }
    else
    {
        return;
    }

    _geom = new osg::Geometry();
    _geom->setNodeMask(_nodeType);
    _geom->setUseDisplayList(false);
    _geom->setUseVertexBufferObjects(true);
    _geom->setVertexArray(_data->vertices);
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    colors->push_back(_style.color);
    _geom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);

    if (isLine)
    {
        if (_style.stripFromSegments)
        {
            _geom->addPrimitiveSet(new osg::DrawElementsUInt(GL_LINE_STRIP, stripIndices->begin(), stripIndices->end()));
        }
        else
        {
            _geom->addPrimitiveSet(new osg::DrawElementsUInt(GL_LINES, _indices->begin(), _indices->end()));
        }
    }
    else
    {
        if (_indices && !_indices->empty())
        {
            _geom->addPrimitiveSet(new osg::DrawElementsUInt(GL_POINTS, _indices->begin(), _indices->end()));
        }
        else
        {
            _geom->addPrimitiveSet(new osg::DrawArrays(GL_POINTS, 0, 1));
        }
    }

    osg::StateSet* stateSet = _geom->getOrCreateStateSet();
    stateSet->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    _geom->setUserValue("ElementId", static_cast<unsigned int>(_id.value()));

    if (!_style.applyStateToRoot)
    {
        stateSet->setRenderBinDetails(_style.renderBin, _style.renderBinName);
    }

    if (isLine)
    {
        stateSet->setAttribute(new osg::LineWidth(_style.lineWidth));
        if (_style.stipple)
        {
            stateSet->setAttributeAndModes(new osg::LineStipple(
                _style.stippleFactor, _style.stipplePattern), osg::StateAttribute::ON);
        }
    }
    else
    {
        stateSet->setAttribute(new osg::Point(_style.pointSize));
    }

    _node->addChild(_geom);
}

void LineRenderObject::setHighlighted(bool flag)
{
    this->applyElementState(flag, _style.highlightColor);
}

void LineRenderObject::setPreviewed(bool flag)
{
    this->applyElementState(flag, _style.previewColor);
}

void LineRenderObject::applyElementState(bool flag, const osg::Vec4& color)
{
    if (!_geom)
    {
        return;
    }

    OsgUtils::setNodeColor(_geom, flag ? color : _style.color);

    osg::StateSet* stateSet = _geom->getOrCreateStateSet();
    if (_style.changeLineWidthOnHighlight)
    {
        const float lineWidth = flag ? _style.highlightLineWidth : _style.lineWidth;
        stateSet->setAttribute(new osg::LineWidth(lineWidth));
    }
    if (_style.changeDepthTestOnHighlight)
    {
        const bool depthTest = flag ? _style.highlightDepthTest : _style.depthTest;
        stateSet->setMode(GL_DEPTH_TEST, depthTest ? osg::StateAttribute::ON : osg::StateAttribute::OFF);
    }
    if (_style.changePointSizeOnHighlight && _isPoints)
    {
        const float pointSize = flag ? _style.highlightPointSize : _style.pointSize;
        stateSet->setAttribute(new osg::Point(pointSize));
    }
}

} // namespace wy3dApp
