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

#include "scene/nodes/CurveRenderObject.h"

#include <cassert>

#include <osg/Array>
#include <osg/Geometry>
#include <osg/LineWidth>
#include <osg/PrimitiveSet>

#include "scene/RenderConst.h"

namespace wy3dApp {

CurveRenderObject::CurveRenderObject(const wydb::ElementId& id,
    ElementNodeType nodeType,
    CurveRenderData& data,
    osg::UIntArray& indices,
    const CurveRenderStyle& style)
    : _id(id),
    _nodeType(nodeType),
    _data(&data),
    _indices(&indices),
    _style(style)
{
    assert(_data);
}

CurveRenderObject::~CurveRenderObject() = default;

void CurveRenderObject::build()
{
    assert(_data);

    _line = std::make_unique<LineRenderObject>(_id, _nodeType, *_data, *_indices, _style);
    _line->build();
    _node->addChild(_line->getContentRoot());
}

void CurveRenderObject::setHighlighted(bool flag)
{
    if (_line) _line->setHighlighted(flag);
}

void CurveRenderObject::setPreviewed(bool flag)
{
    if (_line) _line->setPreviewed(flag);
}

void CurveRenderObject::highlightCurveByIndex(unsigned int curveIndex, bool flag)
{
    if (_highlightGeom)
    {
        _node->removeChild(_highlightGeom);
        _highlightGeom = nullptr;
    }

    if (!_data || curveIndex >= _data->curveInfos.size())
    {
        assert(false);
        return;
    }

    if (flag) _data->curveInfos[curveIndex].addFlag(CurveRenderInfoFlag::Highlight);
    else _data->curveInfos[curveIndex].removeFlag(CurveRenderInfoFlag::Highlight);

    osg::ref_ptr<osg::Geometry> highlightGeom = this->generateHighlightGeom();
    if (highlightGeom)
    {
        _highlightGeom = highlightGeom;
        _node->addChild(_highlightGeom);
    }
}

void CurveRenderObject::previewCurveByIndex(unsigned int curveIndex, bool flag)
{
    if (_previewGeom)
    {
        _node->removeChild(_previewGeom);
        _previewGeom = nullptr;
    }

    if (!_data || curveIndex >= _data->curveInfos.size())
    {
        assert(false);
        return;
    }

    if (flag)
    {
        osg::ref_ptr<osg::Geometry> previewGeom = this->generatePreviewGeom(curveIndex);
        assert(previewGeom);
        if (previewGeom)
        {
            _previewGeom = previewGeom;
            _node->addChild(_previewGeom);
        }
    }
}

void CurveRenderObject::clearDynamicRenderGeometry()
{
    if (_highlightGeom)
    {
        _node->removeChild(_highlightGeom);
        _highlightGeom = nullptr;
    }
    if (_previewGeom)
    {
        _node->removeChild(_previewGeom);
        _previewGeom = nullptr;
    }
    if (_data)
    {
        for (CurveRenderInfo& info : _data->curveInfos)
        {
            info.removeFlag(CurveRenderInfoFlag::Highlight);
        }
    }
}

osg::Geometry* CurveRenderObject::getGeometry() const
{
    return _line ? _line->getGeometry() : nullptr;
}

osg::ref_ptr<osg::Geometry> CurveRenderObject::generateHighlightGeom() const
{
    if (!_data || !_indices || _indices->empty())
    {
        return nullptr;
    }

    // 统计高亮索引总数
    size_t totalNumIndices(0);
    for (const CurveRenderInfo& curveInfo : _data->curveInfos)
    {
        if (curveInfo.hasFlag(CurveRenderInfoFlag::Highlight))
            totalNumIndices += static_cast<size_t>(curveInfo.numLines) * 2;
    }
    if (totalNumIndices == 0)
    {
        return nullptr;
    }

    // 填充索引数组
    osg::ref_ptr<osg::UIntArray> indices = new osg::UIntArray();
    indices->reserve(totalNumIndices);

    size_t startIndex = 0;
    for (const CurveRenderInfo& curveInfo : _data->curveInfos)
    {
        size_t numIndices = static_cast<size_t>(curveInfo.numLines) * 2;
        if (curveInfo.hasFlag(CurveRenderInfoFlag::Highlight))
        {
            indices->insert(indices->end(),
                _indices->begin() + startIndex,
                _indices->begin() + startIndex + numIndices);
        }
        startIndex += numIndices;
    }

    // 创建几何
    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
    geom->setNodeMask(~ElementNodeType::All); // 不可拾取
    geom->setUseDisplayList(false);
    geom->setUseVertexBufferObjects(true);
    geom->setVertexArray(_data->vertices);
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    colors->push_back(_style.highlightOverlayColor);
    geom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);
    geom->addPrimitiveSet(new osg::DrawElementsUInt(GL_LINES, indices->begin(), indices->end()));
    geom->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    // 宽度
    geom->getOrCreateStateSet()->setAttribute(new osg::LineWidth(3.0f));
    // 关闭深度测试始终可见
    geom->getOrCreateStateSet()->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
    geom->getOrCreateStateSet()->setRenderBinDetails(RenderBinNumers::Highlight, "RenderBin");

    return geom;
}

osg::ref_ptr<osg::Geometry> CurveRenderObject::generatePreviewGeom(unsigned int curveIndex) const
{
    if (!_data || !_indices || _indices->empty() || _data->curveInfos.empty())
    {
        assert(false);
        return nullptr;
    }
    if (curveIndex >= _data->curveInfos.size())
    {
        assert(false);
        return nullptr;
    }

    // 索引数组
    osg::ref_ptr<osg::UIntArray> indices = new osg::UIntArray();
    size_t numIndices = static_cast<size_t>(_data->curveInfos[curveIndex].numLines) * 2;
    indices->reserve(numIndices);

    // 填充索引数组
    size_t startIndex = 0;
    for (size_t i = 0; i < _data->curveInfos.size(); ++i)
    {
        const CurveRenderInfo& curveInfo = _data->curveInfos[i];
        numIndices = static_cast<size_t>(curveInfo.numLines) * 2;
        if (i == curveIndex)
        {
            indices->insert(indices->end(),
                _indices->begin() + startIndex,
                _indices->begin() + startIndex + numIndices);
            break;
        }
        startIndex += numIndices;
    }

    // 创建几何
    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
    geom->setNodeMask(~ElementNodeType::All); // 不可拾取
    geom->setUseDisplayList(false);
    geom->setUseVertexBufferObjects(true);
    geom->setVertexArray(_data->vertices);
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    colors->push_back(_style.previewOverlayColor);
    geom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);
    geom->addPrimitiveSet(new osg::DrawElementsUInt(GL_LINES, indices->begin(), indices->end()));
    geom->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    // 宽度
    geom->getOrCreateStateSet()->setAttribute(new osg::LineWidth(3.0f));
    // 关闭深度测试始终可见
    geom->getOrCreateStateSet()->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
    geom->getOrCreateStateSet()->setRenderBinDetails(RenderBinNumers::Highlight, "RenderBin");

    return geom;
}

} // namespace wy3dApp
