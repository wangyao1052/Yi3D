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

#include "SolidModificationElementNode.h"

#include <osg/MatrixTransform>
#include <osg/CullFace>
#include <osg/BlendFunc>
#include <osg/PolygonOffset>
#include <osg/LineWidth>
#include <OsgUtils.h>

#include <wy3dSolid.h>
#include <wy3dSolid.h>
#include <wy3dSolidModification.h>
#include <wy3dSolidModification.h>
#include "scene/Scene.h"
#include "scene/nodes/SolidElementNode.h"
#include "scene/nodes/SheetElementNode.h"
#include "scene/RenderConst.h"
#include "scene/Colors.h"

namespace
{

// 实体和片体的场景节点各有一套渲染数据, 这里收集成同一份, 供形体修改元素使用.
struct HostRenderData
{
    osg::ref_ptr<osg::Vec3Array> vertices;
    osg::ref_ptr<osg::Vec3Array> normals;
    osg::ref_ptr<osg::UIntArray> triangleIndices;
    osg::ref_ptr<osg::UIntArray> lineIndices;
    // 借用宿主节点的面/边信息, 不拷贝; 生命周期同宿主节点
    const std::vector<ElementNode::FaceInfo>* faceInfos = nullptr;
    const std::vector<ElementNode::EdgeInfo>* edgeInfos = nullptr;
};

wydb::ElementId findHostId(const wydb::Element* pElem)
{
    if (const wy3d::SolidModification* pSolidMod = wy3d::SolidModification::cast(pElem))
    {
        return pSolidMod->getParent();
    }
    if (const wy3d::Solid* pSolid = wy3d::Solid::cast(pElem))
    {
        return pSolid->getParent();
    }
    return wydb::ElementId::kNull;
}

template<typename HostNode>
void collectHostRenderDataFrom(const HostNode* pHostNode, HostRenderData& data)
{
    data.vertices = pHostNode->getVertices();
    data.normals = pHostNode->getNormals();
    data.triangleIndices = pHostNode->getTriangleIndices();
    data.lineIndices = pHostNode->getLineIndices();
    data.faceInfos = &pHostNode->getFaceInfos();
    data.edgeInfos = &pHostNode->getEdgeInfos();
}

// 宿主既可能是实体也可能是片体
bool collectHostRenderData(const ElementNode* pHostNode, HostRenderData& data)
{
    if (const SolidElementNode* pSolidNode = dynamic_cast<const SolidElementNode*>(pHostNode))
    {
        collectHostRenderDataFrom(pSolidNode, data);
        return true;
    }
    if (const SheetElementNode* pSheetNode = dynamic_cast<const SheetElementNode*>(pHostNode))
    {
        collectHostRenderDataFrom(pSheetNode, data);
        return true;
    }
    return false;
}

bool findHostMatrix(const ElementNode* pHostNode, osg::Matrix& matrix)
{
    if (const SolidElementNode* pSolidNode = dynamic_cast<const SolidElementNode*>(pHostNode))
    {
        matrix = pSolidNode->getMatrix();
        return true;
    }
    if (const SheetElementNode* pSheetNode = dynamic_cast<const SheetElementNode*>(pHostNode))
    {
        matrix = pSheetNode->getMatrix();
        return true;
    }
    return false;
}

} // namespace

bool SolidModificationElementNode::transform(wydb::Database* pDb)
{
    // 由于当前Solid::Transform-->Solid::Shape,所以该接口直接不做任何操作.
    return true;
}

void SolidModificationElementNode::generateRenderObjectImpl(Scene* pScene, const wydb::Element* pElem)
{
    assert(pScene);
    assert(pElem);
    assert(_vertices);
    assert(_normals);
    assert(_triangleIndices);
    assert(_lineIndices);

    osg::Matrix hostMatrix;
    if (!findHostMatrix(pScene->getElementNode(findHostId(pElem)), hostMatrix))
    {
        assert(false);
        return;
    }

    // 设置包围盒始终为空
    _boundBox = osg::BoundingBox();

    /* 不渲染三角面片
    // 面片
    if (!_triangleIndices->empty())
    {
        // batch
        _shapeGeom = this->generateShapeGeom(pElem->getId());
        osg::ref_ptr<osg::MatrixTransform> pMatrixTransform = new osg::MatrixTransform();
        pMatrixTransform->setMatrix(hostMatrix);
        pMatrixTransform->addChild(_shapeGeom);
        _shapeNode = pMatrixTransform;
        _osgNode->addChild(_shapeNode);
    }
    */

    // 边
    if (!_lineIndices->empty())
    {
        _edgeGeom = this->generateEdgeGeom(pElem->getId());
        osg::ref_ptr<osg::MatrixTransform> pMatrixTransform = new osg::MatrixTransform();
        pMatrixTransform->setMatrix(hostMatrix);
        pMatrixTransform->addChild(_edgeGeom);
        _edgeNode = pMatrixTransform;
        _edgeNode->setNodeMask(0); // 默认不显示只在高亮和预览的时候显示
        _osgNode->addChild(_edgeNode);
    }
}

osg::ref_ptr<osg::Geometry> SolidModificationElementNode::generateShapeGeom(const wydb::ElementId& id) const
{
    if (_triangleIndices->empty())
    {
        assert(false);
        return nullptr;
    }

    osg::ref_ptr<osg::Geometry> shapeGeom = new osg::Geometry();
    shapeGeom->setNodeMask(static_cast<unsigned int>(this->getNodeType()));
    shapeGeom->setUseDisplayList(false);
    shapeGeom->setUseVertexBufferObjects(true);
    shapeGeom->getOrCreateStateSet()->setMode(GL_CULL_FACE, osg::StateAttribute::ON);
    shapeGeom->getOrCreateStateSet()->setAttribute(new osg::CullFace(osg::CullFace::BACK));
    shapeGeom->setVertexArray(_vertices);
    shapeGeom->setNormalArray(_normals, osg::Array::Binding::BIND_PER_VERTEX);
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    colors->push_back(Colors::kSolidFace);
    shapeGeom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);
    shapeGeom->addPrimitiveSet(new osg::DrawElementsUInt(GL_TRIANGLES, _triangleIndices->begin(), _triangleIndices->end()));
    shapeGeom->setUserValue("ElementId", static_cast<unsigned int>(id.value()));

    return shapeGeom;
}

osg::ref_ptr<osg::Geometry> SolidModificationElementNode::generateEdgeGeom(const wydb::ElementId& id) const
{
    if (_lineIndices->empty())
    {
        assert(false);
        return nullptr;
    }

    osg::ref_ptr<osg::Geometry> edgeGeom = new osg::Geometry();
    edgeGeom->setNodeMask(static_cast<unsigned int>(this->getNodeType()));
    edgeGeom->setUseDisplayList(false);
    edgeGeom->setUseVertexBufferObjects(true);
    edgeGeom->setVertexArray(_vertices);
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    colors->push_back(Colors::kSolidEdge);
    edgeGeom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);
    edgeGeom->addPrimitiveSet(new osg::DrawElementsUInt(GL_LINES, _lineIndices->begin(), _lineIndices->end()));
    edgeGeom->setUserValue("ElementId", static_cast<unsigned int>(id.value()));
    edgeGeom->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    edgeGeom->getOrCreateStateSet()->setAttribute(new osg::LineWidth(2.0));

    return edgeGeom;
}

ElementNode::GenRenderDataRet SolidModificationElementNode::generateRenderDataImpl(Scene* pScene, const wydb::Element* pElement)
{
    assert(pScene);
    assert(pElement);

    _triangleIndices = new osg::UIntArray();
    _lineIndices = new osg::UIntArray();

    // 实体修改元素
    std::vector<unsigned int> newFaceIndexVec;
    if (const wy3d::SolidModification* pSolidMod = wy3d::SolidModification::cast(pElement))
    {
        newFaceIndexVec = pSolidMod->getNewFaceIndices();
    }
    else if (const wy3d::Solid* pSolid = wy3d::Solid::cast(pElement))
    {
        newFaceIndexVec = pSolid->getNewFaceIndices();
    }
    else
    {
        assert(false);
        return GenRenderDataRet::Ok_Empty;
    }
    const wydb::ElementId ownerId = findHostId(pElement);
    assert(!ownerId.isNull());

    // 实体修改元素新生成的面为空
    if (newFaceIndexVec.empty())
    {
        // 当倒角特征出错时会导致newFaceIndices为空
        return GenRenderDataRet::Ok_Empty;
    }

    // 关联的宿主元素节点(实体或片体)
    HostRenderData hostData;
    if (!collectHostRenderData(pScene->getElementNode(ownerId), hostData))
    {
        assert(false);
        return GenRenderDataRet::Ok_Empty;
    }
    _vertices = hostData.vertices;
    if (!_vertices)
    {
        assert(false);
        return GenRenderDataRet::Ok_Empty;
    }
    _normals = hostData.normals;
    if (!_normals)
    {
        assert(false);
        return GenRenderDataRet::Ok_Empty;
    }
    if (!hostData.triangleIndices || !hostData.lineIndices ||
        !hostData.faceInfos || !hostData.edgeInfos)
    {
        assert(false);
        return GenRenderDataRet::Ok_Empty;
    }
    const std::vector<ElementNode::FaceInfo>& faceInfos = *hostData.faceInfos;
    const std::vector<ElementNode::EdgeInfo>& edgeInfos = *hostData.edgeInfos;

    // 新生成的面的索引
    std::set<unsigned int> newFaceIndices;
    newFaceIndices.insert(newFaceIndexVec.cbegin(), newFaceIndexVec.cend());
    std::set<unsigned int>::const_iterator iterNewFaceIndex = newFaceIndices.cbegin();
    assert(iterNewFaceIndex != newFaceIndices.cend());

    // 算出总的三角形索引数量 + 边的索引
    size_t totalTriIndices(0);
    std::set<unsigned int> edgeIndexSet;
    for (size_t i = 0; i < faceInfos.size(); ++i)
    {
        if (i == *iterNewFaceIndex)
        {
            totalTriIndices += static_cast<size_t>(faceInfos[i].numTriangles) * 3;
            edgeIndexSet.insert(faceInfos[i].edgeIndices.cbegin(), faceInfos[i].edgeIndices.cend());

            ++iterNewFaceIndex;
            if (iterNewFaceIndex == newFaceIndices.cend())
            {
                break;
            }
        }
    }
    // 不渲染三角面
    //_triangleIndices->reserve(totalTriIndices);

    // 填充三角形索引
    if (0) // 实体修改元素不渲染面只渲染边
    {
        auto iterBeg = hostData.triangleIndices->begin();
        auto iterEnd = iterBeg;
        iterNewFaceIndex = newFaceIndices.cbegin();
        for (size_t i = 0; i < faceInfos.size(); ++i)
        {
            iterEnd += static_cast<size_t>(faceInfos[i].numTriangles) * 3;
            if (i == *iterNewFaceIndex)
            {
                _triangleIndices->insert(_triangleIndices->end(), iterBeg, iterEnd);

                ++iterNewFaceIndex;
                if (iterNewFaceIndex == newFaceIndices.cend())
                {
                    break;
                }
            }
            iterBeg = iterEnd;
        }
    }

    // 填充边的索引
    if (!edgeIndexSet.empty())
    {
        auto iterBeg = hostData.lineIndices->begin();
        auto iterEnd = iterBeg;
        auto iterEdgeIndex = edgeIndexSet.cbegin();
        for (size_t i = 0; i < edgeInfos.size(); ++i)
        {
            assert(hostData.lineIndices->end() - iterEnd >= static_cast<size_t>(edgeInfos[i].numLines) * 2);
            iterEnd += static_cast<size_t>(edgeInfos[i].numLines) * 2;
            if (i == *iterEdgeIndex)
            {
                _lineIndices->insert(_lineIndices->end(), iterBeg, iterEnd);

                ++iterEdgeIndex;
                if (iterEdgeIndex == edgeIndexSet.cend())
                {
                    break;
                }
            }
            iterBeg = iterEnd;
        }
    }

    return GenRenderDataRet::Ok;
}

void SolidModificationElementNode::highlightImpl(bool flag)
{
    this->updateColorAndTransparent();
}

void SolidModificationElementNode::previewImpl(bool flag)
{
    if (this->isHighlighted())
    {
        assert(false);
        return;
    }
    
    /* 不渲染面
    // 面
    if (_shapeNode)
    {
        OsgUtils::setNodeColor(_shapeNode, flag ? Colors::kSolidFace_Preview : Colors::kSolidFace);
    }
    */

    // 边
    if (_edgeNode)
    {
        OsgUtils::setNodeColor(_edgeNode, flag ? Colors::kEdge_Preview : Colors::kSolidEdge);

        if (flag) // 预览时关闭深度测试使边线始终可见
        {
            _edgeNode->setNodeMask(~PICK_MASK);
            _edgeNode->getOrCreateStateSet()->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
            _edgeNode->getOrCreateStateSet()->setRenderBinDetails(RenderBinNumers::Preview, "RenderBin");
        }
        else // 隐藏
        {
            _edgeNode->setNodeMask(0);
            _edgeNode->getOrCreateStateSet()->setMode(GL_DEPTH_TEST, osg::StateAttribute::ON);
            _edgeNode->getOrCreateStateSet()->setRenderBinDetails(0, "RenderBin");
        }
    }
}

void SolidModificationElementNode::setActiveImpl(bool flag)
{
    this->updateColorAndTransparent();
}

void SolidModificationElementNode::updateColorAndTransparent()
{
    /* 不渲染面
    // 面
    if (_shapeNode)
    {
        if (this->isActive())
        {
            OsgUtils::setNodeColor(_shapeNode, this->isHighlighted() ? Colors::kSolidFace_Highlight : Colors::kSolidFace);
        }
        else
        {
            OsgUtils::setNodeColor(_shapeNode, this->isHighlighted() ? Colors::kTransparent : Colors::kSolidFace);
        }

        if (_shapeGeom)
        {
            if (this->isActive())
            {
                // remove transparent
                _shapeGeom->getOrCreateStateSet()->setRenderingHint(osg::StateSet::DEFAULT_BIN);
                _shapeGeom->getOrCreateStateSet()->removeAttribute(osg::StateAttribute::Type::BLENDFUNC);
                // remove z-fighting
                _shapeGeom->getOrCreateStateSet()->removeAttribute(osg::StateAttribute::Type::POLYGONOFFSET);
            }
            else
            {
                // transparent
                _shapeGeom->getOrCreateStateSet()->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
                _shapeGeom->getOrCreateStateSet()->setAttributeAndModes(new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
                // z-fighting
                osg::ref_ptr<osg::PolygonOffset> polyOffset = new osg::PolygonOffset(-1.0f, 1.0f);
                _shapeGeom->getOrCreateStateSet()->setAttributeAndModes(polyOffset, osg::StateAttribute::OVERRIDE | osg::StateAttribute::ON);
            }
        }
    }
    */

    // 边
    if (_edgeNode)
    {
        if (this->isActive())
        {
            OsgUtils::setNodeColor(_edgeNode, this->isHighlighted() ? Colors::kEdge_Highlight : Colors::kSolidEdge);
        }
        else
        {
            OsgUtils::setNodeColor(_edgeNode, this->isHighlighted() ? Colors::kTransparent : Colors::kSolidEdge);
        }

        // 高亮时关闭深度测试使边线始终可见
        if (this->isActive() && this->isHighlighted())
        {
            _edgeNode->getOrCreateStateSet()->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
            _edgeNode->getOrCreateStateSet()->setRenderBinDetails(RenderBinNumers::Highlight, "RenderBin");
        }
        else
        {
            _edgeNode->getOrCreateStateSet()->setMode(GL_DEPTH_TEST, osg::StateAttribute::ON);
            _edgeNode->getOrCreateStateSet()->setRenderBinDetails(0, "RenderBin");
        }

        if (this->isHighlighted()) // 显示
            _edgeNode->setNodeMask(~PICK_MASK);
        else // 隐藏
            _edgeNode->setNodeMask(0);
    }
}

bool SolidModificationElementNode::computeWhetherActive(const wydb::Element* pCurElem) const
{
    // always return true
    return true;
    /*
    assert(pCurElem);
    const wydb::Database* pDb = pCurElem->getDatabase();
    assert(pDb);
    const wydb::Element* pOwnerElem = pDb->getElement(pCurElem->getParent());
    if (!pOwnerElem)
    {
        assert(false);
        return true;
    }
    return pOwnerElem->getParent().isNull();
    */
}