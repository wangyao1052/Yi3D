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

#include "scene/nodes/SketchRenderObject.h"

#include <cassert>

#include <osg/Array>
#include <osg/Geometry>
#include <osg/LineWidth>
#include <osg/PrimitiveSet>

#include "scene/RenderConst.h"

namespace wy3dApp {

SketchRenderObject::SketchRenderObject(const wydb::ElementId& id,
    SketchRenderData& data,
    const SketchRenderStyle& style)
    : _id(id),
    _data(&data),
    _style(style)
{
    assert(_data);
}

SketchRenderObject::~SketchRenderObject() = default;

void SketchRenderObject::build()
{
    assert(_data);

    _curves = std::make_unique<LineRenderObject>(
        _id, _style.curveStyle.nodeType, *_data, *_data->lineIndices, _style.curveStyle);
    _curves->build();
    if (_curves->getGeometry()) _node->addChild(_curves->getContentRoot());

    _centerLines = std::make_unique<LineRenderObject>(
        _id, _style.centerLineStyle.nodeType, *_data, *_data->centerLineIndices, _style.centerLineStyle);
    _centerLines->build();
    if (_centerLines->getGeometry()) _node->addChild(_centerLines->getContentRoot());

    _points = std::make_unique<LineRenderObject>(
        _id, _style.pointStyle.nodeType, *_data, *_data->pointIndices, _style.pointStyle);
    _points->build();
    if (_points->getGeometry()) _node->addChild(_points->getContentRoot());
}

void SketchRenderObject::setHighlighted(bool flag)
{
    if (_curves) _curves->setHighlighted(flag);
    if (_centerLines) _centerLines->setHighlighted(flag);
    if (_points) _points->setHighlighted(flag);
}

void SketchRenderObject::setPreviewed(bool flag)
{
    if (_curves) _curves->setPreviewed(flag);
    if (_centerLines) _centerLines->setPreviewed(flag);
    if (_points) _points->setPreviewed(flag);
}

void SketchRenderObject::highlightCurveByIndex(unsigned int curveIndex, bool flag)
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

void SketchRenderObject::highlightCenterLineByIndex(unsigned int curveIndex, bool flag)
{
    if (_highlightGeom)
    {
        _node->removeChild(_highlightGeom);
        _highlightGeom = nullptr;
    }

    if (!_data || curveIndex >= _data->centerLineInfos.size())
    {
        assert(false);
        return;
    }

    if (flag) _data->centerLineInfos[curveIndex].addFlag(CurveRenderInfoFlag::Highlight);
    else _data->centerLineInfos[curveIndex].removeFlag(CurveRenderInfoFlag::Highlight);

    osg::ref_ptr<osg::Geometry> highlightGeom = this->generateHighlightGeom();
    if (highlightGeom)
    {
        _highlightGeom = highlightGeom;
        _node->addChild(_highlightGeom);
    }
}

void SketchRenderObject::previewCurveByIndex(unsigned int curveIndex, bool flag)
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
        osg::ref_ptr<osg::Geometry> previewGeom = this->generatePreviewGeom(
            *_data->lineIndices, _data->curveInfos, curveIndex);
        assert(previewGeom);
        if (previewGeom)
        {
            _previewGeom = previewGeom;
            _node->addChild(_previewGeom);
        }
    }
}

void SketchRenderObject::previewCenterLineByIndex(unsigned int curveIndex, bool flag)
{
    if (_previewGeom)
    {
        _node->removeChild(_previewGeom);
        _previewGeom = nullptr;
    }

    if (!_data || curveIndex >= _data->centerLineInfos.size())
    {
        assert(false);
        return;
    }

    if (flag)
    {
        osg::ref_ptr<osg::Geometry> previewGeom = this->generatePreviewGeom(
            *_data->centerLineIndices, _data->centerLineInfos, curveIndex);
        assert(previewGeom);
        if (previewGeom)
        {
            _previewGeom = previewGeom;
            _node->addChild(_previewGeom);
        }
    }
}

osg::Geometry* SketchRenderObject::getCenterLinesGeom() const
{
    return _centerLines ? _centerLines->getGeometry() : nullptr;
}

void SketchRenderObject::clearDynamicRenderGeometry()
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
        for (CurveRenderInfo& info : _data->centerLineInfos)
        {
            info.removeFlag(CurveRenderInfoFlag::Highlight);
        }
    }
}

osg::ref_ptr<osg::Geometry> SketchRenderObject::generateHighlightGeom() const
{
    if (!_data || !_data->lineIndices || !_data->centerLineIndices)
    {
        return nullptr;
    }
    if (_data->lineIndices->empty() && _data->centerLineIndices->empty())
    {
        return nullptr;
    }

    // 统计高亮索引总数（普通线 + 中心线）
    size_t totalNumIndices(0);
    for (const CurveRenderInfo& curveInfo : _data->curveInfos)
    {
        if (curveInfo.hasFlag(CurveRenderInfoFlag::Highlight))
            totalNumIndices += static_cast<size_t>(curveInfo.numLines) * 2;
    }
    for (const CurveRenderInfo& curveInfo : _data->centerLineInfos)
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
                _data->lineIndices->begin() + startIndex,
                _data->lineIndices->begin() + startIndex + numIndices);
        }
        startIndex += numIndices;
    }
    startIndex = 0;
    for (const CurveRenderInfo& curveInfo : _data->centerLineInfos)
    {
        size_t numIndices = static_cast<size_t>(curveInfo.numLines) * 2;
        if (curveInfo.hasFlag(CurveRenderInfoFlag::Highlight))
        {
            indices->insert(indices->end(),
                _data->centerLineIndices->begin() + startIndex,
                _data->centerLineIndices->begin() + startIndex + numIndices);
        }
        startIndex += numIndices;
    }

    return this->createOverlayGeom(*indices, _style.highlightOverlayColor);
}

osg::ref_ptr<osg::Geometry> SketchRenderObject::generatePreviewGeom(
    const osg::UIntArray& indices,
    const std::vector<CurveRenderInfo>& infos,
    unsigned int curveIndex) const
{
    if (!_data || indices.empty() || infos.empty())
    {
        assert(false);
        return nullptr;
    }
    if (curveIndex >= infos.size())
    {
        assert(false);
        return nullptr;
    }

    // 索引数组
    osg::ref_ptr<osg::UIntArray> selected = new osg::UIntArray();
    size_t numIndices = static_cast<size_t>(infos[curveIndex].numLines) * 2;
    selected->reserve(numIndices);

    // 填充索引数组
    size_t startIndex = 0;
    for (size_t i = 0; i < infos.size(); ++i)
    {
        const CurveRenderInfo& curveInfo = infos[i];
        numIndices = static_cast<size_t>(curveInfo.numLines) * 2;
        if (i == curveIndex)
        {
            selected->insert(selected->end(),
                indices.begin() + startIndex,
                indices.begin() + startIndex + numIndices);
            break;
        }
        startIndex += numIndices;
    }

    return this->createOverlayGeom(*selected, _style.previewOverlayColor);
}

osg::ref_ptr<osg::Geometry> SketchRenderObject::createOverlayGeom(
    const osg::UIntArray& indices,
    const osg::Vec4& color) const
{
    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
    geom->setNodeMask(~ElementNodeType::All); // 不可拾取
    geom->setUseDisplayList(false);
    geom->setUseVertexBufferObjects(true);
    geom->setVertexArray(_data->vertices);
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    colors->push_back(color);
    geom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);
    geom->addPrimitiveSet(new osg::DrawElementsUInt(GL_LINES, indices.begin(), indices.end()));
    geom->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    // 宽度
    geom->getOrCreateStateSet()->setAttribute(new osg::LineWidth(3.0f));
    // 关闭深度测试始终可见
    geom->getOrCreateStateSet()->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
    geom->getOrCreateStateSet()->setRenderBinDetails(RenderBinNumers::Highlight, "RenderBin");

    return geom;
}

} // namespace wy3dApp
