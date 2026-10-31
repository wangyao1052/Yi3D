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

#include "scene/nodes/MeshRenderObject.h"

#include <cassert>

#include <osg/BlendFunc>
#include <osg/CullFace>
#include <osg/Geometry>
#include <osg/Group>
#include <osg/LightModel>
#include <osg/LineWidth>
#include <osg/MatrixTransform>
#include <osg/PolygonOffset>
#include <osg/PrimitiveSet>
#include <osg/Array>

#include <OsgUtils.h>

#include "scene/RenderConst.h"

namespace wy3dApp {

MeshRenderObject::MeshRenderObject(const wydb::ElementId& id,
    ElementNodeType nodeType,
    MeshRenderData& data,
    const osg::Matrix& matrix)
    : _id(id),
    _nodeType(nodeType),
    _data(&data),
    _matrix(matrix)
{
    assert(_data);
}

MeshRenderObject::~MeshRenderObject() = default;

void MeshRenderObject::build(const MeshRenderStyle& style, bool initialNoBatchFace)
{
    assert(_data);
    _style = style;
    _noBatchFace = initialNoBatchFace;

    this->clear();
    _shapeNode = nullptr;
    _shapeGeom = nullptr;
    _nobatchShapeGeom = nullptr;
    _edgeNode = nullptr;
    _edgeWrap = nullptr;
    _edgeGeom = nullptr;
    _edgeGeomHighlight = nullptr;
    _edgeGeomPreview = nullptr;

    // 面片
    if (_data->triangleIndices && !_data->triangleIndices->empty())
    {
        _shapeGeom = this->generateShapeGeom(true);
        if (initialNoBatchFace)
        {
            if (_shapeGeom) _shapeGeom->setNodeMask(0);
            _nobatchShapeGeom = this->generateShapeGeom(false);
        }

        _shapeNode = new osg::MatrixTransform(_matrix);
        if (_shapeGeom) _shapeNode->addChild(_shapeGeom);
        if (_nobatchShapeGeom) _shapeNode->addChild(_nobatchShapeGeom);
        _node->addChild(_shapeNode);
    }

    // 边
    if (_data->lineIndices && !_data->lineIndices->empty())
    {
        _edgeGeom = this->generateEdgeGeom();
        _edgeNode = new osg::MatrixTransform(_matrix);
        _edgeWrap = new osg::Group;
        if (_edgeGeom) _edgeWrap->addChild(_edgeGeom);
        _edgeNode->addChild(_edgeWrap);
        _node->addChild(_edgeNode);
    }

    this->applyAppearance(_style);
}

void MeshRenderObject::applyAppearance(const MeshRenderStyle& style)
{
    _style = style;

    // 面
    if (_shapeNode)
    {
        const osg::Vec4 faceColor = style.active
            ? (style.highlighted ? style.faceHighlightColor : style.faceColor)
            : (style.highlighted ? style.inactiveColor : style.faceColor);
        OsgUtils::setNodeColor(_shapeNode, faceColor);

        if (_shapeGeom)
        {
            osg::StateSet* stateSet = _shapeGeom->getOrCreateStateSet();
            if (style.active)
            {
                if (style.cut)
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
        const osg::Vec4 edgeColor = style.active
            ? (style.highlighted ? style.edgeHighlightColor : style.edgeColor)
            : (style.highlighted ? style.inactiveColor : style.edgeColor);
        OsgUtils::setNodeColor(_edgeGeom, edgeColor);

        osg::StateSet* stateSet = _edgeGeom->getOrCreateStateSet();
        if (style.active)
        {
            if (style.highlighted || style.cut)
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
        else
        {
            if (style.highlighted)
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
}

void MeshRenderObject::setPreview(bool flag)
{
    // 面
    if (_shapeNode)
    {
        OsgUtils::setNodeColor(_shapeNode, flag ? _style.facePreviewColor : _style.faceColor);
    }

    // 边
    if (_edgeGeom)
    {
        // 线框模式下无面可见，边改用亮橙色与黑色线框区分
        OsgUtils::setNodeColor(_edgeGeom,
            flag ? (_wireframe ? _style.edgePreviewWireframeColor : _style.edgePreviewColor)
                 : _style.edgeColor);

        osg::StateSet* stateSet = _edgeGeom->getOrCreateStateSet();
        if (flag)
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

void MeshRenderObject::setWireframe(bool flag)
{
    _wireframe = flag;
    if (_shapeNode)
    {
        _shapeNode->setNodeMask(flag ? 0 : _nodeType);
    }
}

void MeshRenderObject::setEdgesVisible(bool flag)
{
    _showEdges = flag;
    // 隐藏边时在包裹Group上挂回调跳过渲染遍历，拾取遍历不受影响，边仍可选中
    if (_edgeWrap)
    {
        _edgeWrap->setCullCallback(flag ? nullptr : new OsgUtils::SkipRenderCallback());
    }
}

void MeshRenderObject::highlightFace(unsigned int faceIndex, bool flag)
{
    if (!_data || faceIndex >= _data->faceInfos.size())
    {
        assert(false);
        return;
    }

    this->startNoBatchRender_Face();
    if (flag)
    {
        this->setFaceColor(faceIndex, _style.faceHighlightColor);
        _data->faceInfos[faceIndex].addFlag(FaceInfoFlag::Highlight);
    }
    else
    {
        this->setFaceColor(faceIndex, _style.faceColor);
        _data->faceInfos[faceIndex].removeFlag(FaceInfoFlag::Highlight);
    }
}

void MeshRenderObject::highlightFace(unsigned int faceIndex, const osg::Vec4& color)
{
    if (!_data || faceIndex >= _data->faceInfos.size())
    {
        assert(false);
        return;
    }

    this->startNoBatchRender_Face();
    this->setFaceColor(faceIndex, color);
    _data->faceInfos[faceIndex].addFlag(FaceInfoFlag::Highlight);
}

void MeshRenderObject::previewFace(unsigned int faceIndex, bool flag)
{
    if (!_data || faceIndex >= _data->faceInfos.size())
    {
        return;
    }

    this->startNoBatchRender_Face();
    if (flag)
    {
        // 预览态是临时态(没有变量记录是否在该状态下),不能影响当前的高亮态.
        if (!_data->faceInfos[faceIndex].hasFlag(FaceInfoFlag::Highlight))
        {
            this->setFaceColor(faceIndex, _style.facePreviewColor);
        }
    }
    else
    {
        // 预览态是临时态(没有变量记录是否在该状态下),不能影响当前的高亮态.
        if (!_data->faceInfos[faceIndex].hasFlag(FaceInfoFlag::Highlight))
        {
            this->setFaceColor(faceIndex, _style.faceColor);
        }
    }
}

void MeshRenderObject::highlightEdge(unsigned int edgeIndex, bool flag)
{
    if (_edgeGeomHighlight)
    {
        if (_edgeNode) _edgeNode->removeChild(_edgeGeomHighlight);
        _edgeGeomHighlight = nullptr;
    }

    if (!_data || edgeIndex >= _data->edgeInfos.size())
    {
        assert(false);
        return;
    }

    if (flag) _data->edgeInfos[edgeIndex].addFlag(EdgeInfoFlag::Highlight);
    else _data->edgeInfos[edgeIndex].removeFlag(EdgeInfoFlag::Highlight);

    osg::ref_ptr<osg::Geometry> edgeGeomHighlight = this->generateEdgeGeom_Highlight();
    if (edgeGeomHighlight)
    {
        _edgeGeomHighlight = edgeGeomHighlight;
        if (_edgeNode) _edgeNode->addChild(_edgeGeomHighlight);
    }
}

void MeshRenderObject::previewEdge(unsigned int edgeIndex, bool flag)
{
    if (_edgeGeomPreview)
    {
        if (_edgeNode) _edgeNode->removeChild(_edgeGeomPreview);
        _edgeGeomPreview = nullptr;
    }

    if (!_data || edgeIndex >= _data->edgeInfos.size())
    {
        assert(false);
        return;
    }

    if (flag && _data->edgeInfos[edgeIndex].hasFlag(EdgeInfoFlag::Highlight))
        return;

    if (flag)
    {
        osg::ref_ptr<osg::Geometry> edgeGeomPreview = this->generateEdgeGeom_Preview(edgeIndex);
        assert(edgeGeomPreview);
        if (edgeGeomPreview)
        {
            _edgeGeomPreview = edgeGeomPreview;
            if (_edgeNode) _edgeNode->addChild(_edgeGeomPreview);
        }
    }
}

void MeshRenderObject::clearDynamicRenderGeometry()
{
    this->endNoBatchRender_Face();
    if (_edgeGeomHighlight)
    {
        if (_edgeNode) _edgeNode->removeChild(_edgeGeomHighlight);
        _edgeGeomHighlight = nullptr;
    }
    if (_edgeGeomPreview)
    {
        if (_edgeNode) _edgeNode->removeChild(_edgeGeomPreview);
        _edgeGeomPreview = nullptr;
    }
}

osg::ref_ptr<osg::Geometry> MeshRenderObject::generateShapeGeom(bool batch) const
{
    if (!_data || !_data->triangleIndices || _data->triangleIndices->empty() || _data->faceInfos.empty())
    {
        assert(false);
        return nullptr;
    }

    osg::ref_ptr<osg::Geometry> shapeGeom = new osg::Geometry();
    shapeGeom->setNodeMask(_nodeType);
    shapeGeom->setUseDisplayList(false);
    shapeGeom->setUseVertexBufferObjects(true);
    if (_style.doubleSided)
    {
        // Sheet: 双面渲染 + 双面光照
        osg::StateSet* pSS = shapeGeom->getOrCreateStateSet();
        pSS->setMode(GL_CULL_FACE, osg::StateAttribute::OFF);
        osg::ref_ptr<osg::LightModel> lightModel = new osg::LightModel;
        lightModel->setTwoSided(true);
        pSS->setAttributeAndModes(lightModel, osg::StateAttribute::ON);
    }
    else
    {
        shapeGeom->getOrCreateStateSet()->setMode(GL_CULL_FACE, osg::StateAttribute::ON);
        shapeGeom->getOrCreateStateSet()->setAttribute(new osg::CullFace(osg::CullFace::BACK));
    }
    shapeGeom->setVertexArray(_data->vertices);
    shapeGeom->setNormalArray(_data->normals, osg::Array::Binding::BIND_PER_VERTEX);
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    if (batch)
    {
        colors->push_back(_style.faceColor);
        shapeGeom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);
        shapeGeom->addPrimitiveSet(new osg::DrawElementsUInt(GL_TRIANGLES, _data->triangleIndices->begin(), _data->triangleIndices->end()));
    }
    else
    {
        size_t numFaces = _data->faceInfos.size();
        colors->resize(numFaces, _style.faceColor);
        shapeGeom->setColorArray(colors, osg::Array::Binding::BIND_PER_PRIMITIVE_SET);

        auto iterBeg = _data->triangleIndices->begin();
        auto iterEnd = iterBeg;
        for (size_t i = 0; i < numFaces; ++i)
        {
            iterEnd += static_cast<size_t>(_data->faceInfos[i].numTriangles) * 3;
            shapeGeom->addPrimitiveSet(new osg::DrawElementsUInt(GL_TRIANGLES, iterBeg, iterEnd));
            iterBeg = iterEnd;
        }
    }

    shapeGeom->setUserValue("ElementId", static_cast<unsigned int>(_id.value()));
    return shapeGeom;
}

osg::ref_ptr<osg::Geometry> MeshRenderObject::generateEdgeGeom() const
{
    if (!_data || !_data->lineIndices || _data->lineIndices->empty() || _data->edgeInfos.empty())
    {
        assert(false);
        return nullptr;
    }

    osg::ref_ptr<osg::Geometry> edgeGeom = new osg::Geometry();
    edgeGeom->setNodeMask(_nodeType);
    edgeGeom->setUseDisplayList(false);
    edgeGeom->setUseVertexBufferObjects(true);
    edgeGeom->setVertexArray(_data->vertices);
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    colors->push_back(_style.edgeColor);
    edgeGeom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);
    edgeGeom->addPrimitiveSet(new osg::DrawElementsUInt(GL_LINES, _data->lineIndices->begin(), _data->lineIndices->end()));
    edgeGeom->setUserValue("ElementId", static_cast<unsigned int>(_id.value()));
    osg::StateSet* stateSet = edgeGeom->getOrCreateStateSet();
    stateSet->setAttributeAndModes(new osg::LineWidth(1.5f), osg::StateAttribute::ON);
    stateSet->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    return edgeGeom;
}

osg::ref_ptr<osg::Geometry> MeshRenderObject::generateEdgeGeom_Highlight() const
{
    if (!_data || !_data->lineIndices || _data->lineIndices->empty() || _data->edgeInfos.empty())
    {
        assert(false);
        return nullptr;
    }

    // 索引数组
    osg::ref_ptr<osg::UIntArray> indices = new osg::UIntArray();
    size_t totalNumIndices(0);
    for (const EdgeInfo& edgeInfo : _data->edgeInfos)
    {
        if (edgeInfo.hasFlag(EdgeInfoFlag::Highlight))
        {
            totalNumIndices += static_cast<size_t>(edgeInfo.numLines) * 2;
        }
    }
    if (totalNumIndices == 0) // 没有高亮的边
    {
        return nullptr;
    }
    indices->reserve(totalNumIndices);

    // 填充索引数组
    size_t startIndex = 0;
    for (const EdgeInfo& edgeInfo : _data->edgeInfos)
    {
        size_t numIndices = static_cast<size_t>(edgeInfo.numLines) * 2;
        if (edgeInfo.hasFlag(EdgeInfoFlag::Highlight))
        {
            indices->insert(indices->end(),
                _data->lineIndices->begin() + startIndex,
                _data->lineIndices->begin() + startIndex + numIndices);
        }
        startIndex += numIndices;
    }

    // 创建几何
    osg::ref_ptr<osg::Geometry> edgeGeom = new osg::Geometry();
    edgeGeom->setNodeMask(~ElementNodeType::All); // 不可拾取
    edgeGeom->setUseDisplayList(false);
    edgeGeom->setUseVertexBufferObjects(true);
    edgeGeom->setVertexArray(_data->vertices);
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    colors->push_back(_style.edgeHighlightOverlayColor);
    edgeGeom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);
    edgeGeom->addPrimitiveSet(new osg::DrawElementsUInt(GL_LINES, indices->begin(), indices->end()));
    edgeGeom->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    // 宽度
    edgeGeom->getOrCreateStateSet()->setAttribute(new osg::LineWidth(3.0f));
    // 关闭深度测试始终可见
    edgeGeom->getOrCreateStateSet()->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
    edgeGeom->getOrCreateStateSet()->setRenderBinDetails(RenderBinNumers::Highlight, "RenderBin");

    return edgeGeom;
}

osg::ref_ptr<osg::Geometry> MeshRenderObject::generateEdgeGeom_Preview(unsigned int edgeIndex) const
{
    if (!_data || !_data->lineIndices || _data->lineIndices->empty() || _data->edgeInfos.empty())
    {
        assert(false);
        return nullptr;
    }
    if (edgeIndex >= _data->edgeInfos.size())
    {
        assert(false);
        return nullptr;
    }

    // 索引数组
    osg::ref_ptr<osg::UIntArray> indices = new osg::UIntArray();
    size_t numIndices = static_cast<size_t>(_data->edgeInfos[edgeIndex].numLines) * 2;
    indices->reserve(numIndices);

    // 填充索引数组
    size_t startIndex = 0;
    for (size_t i = 0; i < _data->edgeInfos.size(); ++i)
    {
        const EdgeInfo& edgeInfo = _data->edgeInfos[i];
        numIndices = static_cast<size_t>(edgeInfo.numLines) * 2;
        if (i == edgeIndex)
        {
            indices->insert(indices->end(),
                _data->lineIndices->begin() + startIndex,
                _data->lineIndices->begin() + startIndex + numIndices);
            break;
        }
        startIndex += numIndices;
    }

    // 创建几何
    osg::ref_ptr<osg::Geometry> edgeGeom = new osg::Geometry();
    edgeGeom->setNodeMask(~ElementNodeType::All); // 不可拾取
    edgeGeom->setUseDisplayList(false);
    edgeGeom->setUseVertexBufferObjects(true);
    edgeGeom->setVertexArray(_data->vertices);
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    colors->push_back(_style.edgePreviewOverlayColor);
    edgeGeom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);
    edgeGeom->addPrimitiveSet(new osg::DrawElementsUInt(GL_LINES, indices->begin(), indices->end()));
    edgeGeom->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    // 宽度
    edgeGeom->getOrCreateStateSet()->setAttribute(new osg::LineWidth(3.0f));
    // 关闭深度测试始终可见
    edgeGeom->getOrCreateStateSet()->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
    edgeGeom->getOrCreateStateSet()->setRenderBinDetails(RenderBinNumers::Highlight, "RenderBin");

    return edgeGeom;
}

void MeshRenderObject::startNoBatchRender_Face()
{
    if (_noBatchFace)
    {
        return;
    }

    assert(_shapeGeom);
    if (_shapeGeom) _shapeGeom->setNodeMask(0);
    assert(!_nobatchShapeGeom);
    _nobatchShapeGeom = this->generateShapeGeom(false);
    if (_nobatchShapeGeom)
    {
        assert(_shapeNode);
        if (_shapeNode) _shapeNode->addChild(_nobatchShapeGeom);
        this->applyAppearance(_style);
    }
    else
    {
        assert(false);
    }
    _noBatchFace = true;
}

void MeshRenderObject::endNoBatchRender_Face()
{
    if (!_noBatchFace)
    {
        return;
    }

    assert(_shapeGeom);
    if (_shapeGeom) _shapeGeom->setNodeMask(_nodeType);
    if (_nobatchShapeGeom)
    {
        assert(_shapeNode);
        if (_shapeNode) _shapeNode->removeChild(_nobatchShapeGeom);
        _nobatchShapeGeom = nullptr;
    }
    _noBatchFace = false;
}

bool MeshRenderObject::setFaceColor(unsigned int faceIndex, const osg::Vec4& color)
{
    if (!_noBatchFace)
    {
        return false;
    }
    if (!_data || faceIndex >= _data->faceInfos.size())
    {
        return false;
    }
    assert(_nobatchShapeGeom);
    if (!_nobatchShapeGeom) return false;

    osg::Vec4Array* pColorVec4Arr = dynamic_cast<osg::Vec4Array*>(_nobatchShapeGeom->getColorArray());
    if (pColorVec4Arr)
    {
        assert(pColorVec4Arr->size() == _data->faceInfos.size());
        if (faceIndex < pColorVec4Arr->size())
        {
            (*pColorVec4Arr)[faceIndex] = color;
            _nobatchShapeGeom->dirtyGLObjects();
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        return false;
    }
}

} // namespace wy3dApp
