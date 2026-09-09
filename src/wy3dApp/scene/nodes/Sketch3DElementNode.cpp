///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 Wang Yao <wangyao1052@163.com>
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

#include "Sketch3DElementNode.h"

#include <cassert>

#include <osg/LineWidth>
#include <OsgUtils.h>

#include <wydbDatabase.h>
#include <wy3dSketch3D.h>
#include <wy3dSketchEntity3D.h>

#include "scene/SketchEntity3DLinearization.h"
#include "scene/RenderConst.h"
#include "scene/Colors.h"

bool Sketch3DElementNode::pickByNormalBoxImpl(osg::Polytope& polytope) const
{
    // 是否包含所有点
    for (const osg::Vec3& vertex : *_vertices)
    {
        if (!polytope.contains(vertex))
        {
            return false;
        }
    }

    return true;
}

void Sketch3DElementNode::generateRenderObjectImpl(Scene* pScene, const wydb::Element* pElem)
{
    assert(pElem);
    const wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pElem);
    if (!pSketch3D)
    {
        assert(false);
        return;
    }

    assert(_vertices);
    assert(_lineIndices);

    // 生成包围盒
    _boundBox = this->computeBoundingBox(*_vertices);

    // 生成渲染对象:普通线
    _curveNode = new osg::Group();
    if (!_lineIndices->empty())
    {
        _curveGeom = new osg::Geometry();
        _curveGeom->setNodeMask(static_cast<unsigned int>(this->getNodeType()));
        _curveGeom->getOrCreateStateSet()->setRenderBinDetails(RenderBinNumers::SketchElement, "RenderBin");
        {
            _curveGeom->setUseDisplayList(false);
            _curveGeom->setUseVertexBufferObjects(true);
            _curveGeom->setVertexArray(_vertices);
            osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
            colors->push_back(Colors::kSketch);
            _curveGeom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);
            _curveGeom->addPrimitiveSet(new osg::DrawElementsUInt(GL_LINES, _lineIndices->begin(), _lineIndices->end()));
            _curveGeom->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
            _curveGeom->setUserValue("ElementId", static_cast<unsigned int>(pElem->getId().value()));
            _curveGeom->getStateSet()->setAttribute(new osg::LineWidth(1.2f));
        }
        _curveNode->addChild(_curveGeom);
    }
    _osgNode->addChild(_curveNode);
}

ElementNode::GenRenderDataRet Sketch3DElementNode::generateRenderDataImpl(Scene* pScene, const wydb::Element* pElement)
{
    _lineIndices = new osg::UIntArray();

    assert(pElement);
    const wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pElement);
    if (!pSketch3D)
    {
        assert(false);
        return GenRenderDataRet::Ok_Empty;
    }
    const wydb::Database* pDb = pElement->getDatabase();
    assert(pDb);

    unsigned int totalNumVertices(0);
    unsigned int totalNumIndices(0);
    struct EntityLinearInfo
    {
        wydb::ElementId id;
        std::shared_ptr<SketchEntity3DLinearization> pLinear;
        EntityLinearInfo() : id(wydb::ElementId::kNull), pLinear(nullptr) {}
    };
    std::list<EntityLinearInfo> entityLinears;
    _vertices->reserve(10);
    for (auto iter = pSketch3D->createIterator(); !iter.isDone(); iter.moveNext())
    {
        wydb::ElementId id = iter.current();
        if (id.isNull()) continue;
        const wydb::Element* pElem = pDb->getElement(id);
        const wy3d::SketchEntity3D* pEntity = wy3d::SketchEntity3D::cast(pElem);
        if (!pEntity)
        {
            assert(false);
            continue;
        }
        std::shared_ptr<SketchEntity3DLinearization> pEntityLinear = std::make_shared<SketchEntity3DLinearization>(pEntity);
        totalNumVertices += pEntityLinear->getVertices().size();
        totalNumIndices += pEntityLinear->getIndices().size();
        EntityLinearInfo info;
        info.id = pEntity->getId();
        info.pLinear = pEntityLinear;
        entityLinears.emplace_back(info);
    }

    _vertices->reserve(_vertices->size() + totalNumVertices);
    _lineIndices->reserve(totalNumIndices);
    _curveInfos.reserve(entityLinears.size());
    for (const EntityLinearInfo& info : entityLinears)
    {
        assert(info.pLinear);
        unsigned int baseIndex = _vertices->size();
        for (const wy::Vector3& pnt : info.pLinear->getVertices())
        {
            _vertices->push_back(osg::Vec3(pnt.x(), pnt.y(), pnt.z()));
        }
        const std::vector<unsigned int>& indices = info.pLinear->getIndices();
        CurveInfo curveInfo;
        curveInfo.id = info.id.value();
        size_t numIndices = indices.size();
        if (numIndices % 2 == 0) // 偶数
        {
            curveInfo.numLines = numIndices / 2;
        }
        else
        {
            assert(false);
            curveInfo.numLines = 0;
        }
        _curveInfos.emplace_back(curveInfo);
        for (unsigned int index : indices)
        {
            _lineIndices->push_back(baseIndex + index);
        }
    }

    return GenRenderDataRet::Ok;
}

void Sketch3DElementNode::highlightImpl(bool flag)
{
    this->highlightGeom(_curveGeom, flag, Colors::kSketch_Highlight);
}

void Sketch3DElementNode::previewImpl(bool flag)
{
    if (this->isHighlighted())
    {
        assert(false);
        return;
    }

    this->highlightGeom(_curveGeom, flag, Colors::kSketch_Preview);
}

void Sketch3DElementNode::highlightGeom(const osg::ref_ptr<osg::Geometry>& geom, bool flag, const osg::Vec4& highlightColor)
{
    if (!geom) return;

    OsgUtils::setNodeColor(geom, flag ? highlightColor : Colors::kSketch);
    if (flag)
    {
        // 高亮时加大线宽
        geom->getOrCreateStateSet()->setAttribute(new osg::LineWidth(2.0f));
        // 高亮时禁用深度测试确保不被遮挡住
        geom->getOrCreateStateSet()->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
    }
    else
    {
        // 取消高亮时恢复线宽
        geom->getOrCreateStateSet()->setAttribute(new osg::LineWidth(1.2f));
        // 取消高亮时启用深度测试
        geom->getOrCreateStateSet()->setMode(GL_DEPTH_TEST, osg::StateAttribute::ON);
    }
}

void Sketch3DElementNode::setActiveImpl(bool flag)
{
}

bool Sketch3DElementNode::computeWhetherActive(const wydb::Element* pCurElem) const
{
    assert(pCurElem);
    return pCurElem->getParent().isNull();
}

unsigned int Sketch3DElementNode::getCurveId(unsigned int primitiveIndex) const
{
    if (_curveInfos.empty()) return 0;

    unsigned int count(0);
    for (const CurveInfo& info : _curveInfos)
    {
        count += info.numLines;
        if (primitiveIndex < count)
        {
            return info.id;
        }
    }

    return 0;
}

void Sketch3DElementNode::highlightCurveByIndex(unsigned int curveIndex, bool flag)
{
    if (_curveGeomHighlight)
    {
        if (_curveNode) _curveNode->removeChild(_curveGeomHighlight);
        _curveGeomHighlight = nullptr;
    }

    if (curveIndex >= _curveInfos.size())
    {
        assert(false);
        return;
    }

    if (flag) _curveInfos[curveIndex].addFlag(CurveInfoFlag::Highlight);
    else _curveInfos[curveIndex].removeFlag(CurveInfoFlag::Highlight);

    osg::ref_ptr<osg::Geometry> curveGeomHighlight = this->generateCurveGeom_Highlight();
    if (curveGeomHighlight)
    {
        _curveGeomHighlight = curveGeomHighlight;
        if (_curveNode) _curveNode->addChild(_curveGeomHighlight);
    }
}

void Sketch3DElementNode::highlightCurveById(unsigned int id, bool flag)
{
    unsigned int index = this->getCurveIndexById(id);
    if (index != static_cast<unsigned int>(-1))
    {
        this->highlightCurveByIndex(index, flag);
    }
}

void Sketch3DElementNode::previewCurveByIndex(unsigned int curveIndex, bool flag)
{
    if (_curveGeomPreview)
    {
        if (_curveNode) _curveNode->removeChild(_curveGeomPreview);
        _curveGeomPreview = nullptr;
    }

    if (curveIndex >= _curveInfos.size())
    {
        assert(false);
        return;
    }

    if (flag)
    {
        osg::ref_ptr<osg::Geometry> curveGeomPreview = this->generateCurveGeom_Preview(curveIndex);
        assert(curveGeomPreview);
        if (curveGeomPreview)
        {
            _curveGeomPreview = curveGeomPreview;
            if (_curveNode) _curveNode->addChild(_curveGeomPreview);
        }
    }
}

void Sketch3DElementNode::previewCurveById(unsigned int id, bool flag)
{
    unsigned int index = this->getCurveIndexById(id);
    if (index != static_cast<unsigned int>(-1))
    {
        this->previewCurveByIndex(index, flag);
    }
}

void Sketch3DElementNode::clearDynamicRenderGeometry()
{
    if (_curveGeomHighlight)
    {
        if (_curveNode) _curveNode->removeChild(_curveGeomHighlight);
        _curveGeomHighlight = nullptr;
    }
    if (_curveGeomPreview)
    {
        if (_curveNode) _curveNode->removeChild(_curveGeomPreview);
        _curveGeomPreview = nullptr;
    }
    for (CurveInfo& info : _curveInfos)
    {
        info.removeFlag(CurveInfoFlag::Highlight);
    }
}

osg::ref_ptr<osg::Geometry> Sketch3DElementNode::generateCurveGeom_Highlight()
{
    if (!_lineIndices || _lineIndices->empty())
    {
        return nullptr;
    }

    // 统计高亮索引总数
    size_t totalNumIndices(0);
    for (const CurveInfo& curveInfo : _curveInfos)
    {
        if (curveInfo.hasFlag(CurveInfoFlag::Highlight))
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
    for (const CurveInfo& curveInfo : _curveInfos)
    {
        size_t numIndices = static_cast<size_t>(curveInfo.numLines) * 2;
        if (curveInfo.hasFlag(CurveInfoFlag::Highlight))
        {
            indices->insert(indices->end(),
                _lineIndices->begin() + startIndex,
                _lineIndices->begin() + startIndex + numIndices);
        }
        startIndex += numIndices;
    }

    // 创建几何
    osg::ref_ptr<osg::Geometry> edgeGeom = new osg::Geometry();
    edgeGeom->setNodeMask(~PICK_MASK); // 不可拾取
    edgeGeom->setUseDisplayList(false);
    edgeGeom->setUseVertexBufferObjects(true);
    edgeGeom->setVertexArray(_vertices);
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    colors->push_back(Colors::kEdge_Highlight);
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

osg::ref_ptr<osg::Geometry> Sketch3DElementNode::generateCurveGeom_Preview(unsigned int curveIndex)
{
    if (_lineIndices->empty() || _curveInfos.empty())
    {
        assert(false);
        return nullptr;
    }
    if (curveIndex >= _curveInfos.size())
    {
        assert(false);
        return nullptr;
    }

    // 索引数组
    osg::ref_ptr<osg::UIntArray> indices = new osg::UIntArray();
    size_t numIndices = static_cast<size_t>(_curveInfos[curveIndex].numLines) * 2;
    indices->reserve(numIndices);

    // 填充索引数组
    size_t startIndex = 0;
    for (size_t i = 0; i < _curveInfos.size(); ++i)
    {
        const CurveInfo& curveInfo = _curveInfos[i];
        numIndices = static_cast<size_t>(curveInfo.numLines) * 2;
        if (i == curveIndex)
        {
            indices->insert(indices->end(),
                _lineIndices->begin() + startIndex,
                _lineIndices->begin() + startIndex + numIndices);
            break;
        }
        startIndex += numIndices;
    }

    // 创建几何
    osg::ref_ptr<osg::Geometry> edgeGeom = new osg::Geometry();
    edgeGeom->setNodeMask(~PICK_MASK); // 不可拾取
    edgeGeom->setUseDisplayList(false);
    edgeGeom->setUseVertexBufferObjects(true);
    edgeGeom->setVertexArray(_vertices);
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    colors->push_back(Colors::kEdge_Preview);
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

unsigned int Sketch3DElementNode::getCurveIndexById(unsigned int id) const
{
    unsigned int index(-1);
    for (const CurveInfo& info : _curveInfos)
    {
        ++index;
        if (info.id == id)
        {
            return index;
        }
    }
    return static_cast<unsigned int>(-1);
}
