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

#include "BodyModificationElementNode.h"

#include <Standard_Failure.hxx>
#include <osg/MatrixTransform>
#include <osg/CullFace>
#include <osg/BlendFunc>
#include <osg/PolygonOffset>
#include <osg/LineWidth>
#include <OsgUtils.h>

#include <wy3dSolid.h>
#include <wy3dBodyModification.h>
#include <wy3dApp/Scene.h>
#include "scene/nodes/SolidElementNode.h"
#include "scene/nodes/SheetElementNode.h"
#include "scene/nodes/ElementMeshInfo.h"
#include "scene/nodes/ElementNodeImpl.h"
#include <wy3dApp/ElementNodeType.h>
#include "scene/RenderConst.h"
#include "scene/Colors.h"
#include "scene/nodes/BodyModificationRenderObject.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

BodyModificationElementNode::BodyModificationElementNode(const wydb::ElementId& id)
    : ElementNode(id)
{
    // 该节点的包围盒始终为空(不进入 RTree/fit view/框选),不参与基类的自动计算
    _pImpl->autoComputeBoundingBox = false;
}

wyap::Selection BodyModificationElementNode::createSelection(
    osg::Drawable* pDrawable,
    unsigned int primitiveIndex,
    wy3d::SelectionType selType) const
{
    (void)pDrawable;
    (void)primitiveIndex;
    (void)selType;

    assert(false);
    return wyap::Selection(wydb::ElementId::kNull);
}

namespace
{

// 实体和片体的场景节点各有一套渲染数据, 这里收集成同一份, 供形体修改元素使用.
struct HostRenderData
{
    osg::ref_ptr<osg::Vec3Array> vertices;
    osg::ref_ptr<osg::UIntArray> lineIndices;
    // 借用宿主节点的面/边信息, 不拷贝; 生命周期同宿主节点
    const std::vector<FaceInfo>* faceInfos = nullptr;
    const std::vector<EdgeInfo>* edgeInfos = nullptr;
};

wydb::ElementId findHostId(const wydb::Element* pElem)
{
    if (const wy3d::BodyModification* pSolidMod = wy3d::BodyModification::cast(pElem))
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

std::unique_ptr<RenderObject> BodyModificationElementNode::createRenderObject(
    Scene* pScene, const wydb::Element* pElem)
{
    assert(pScene);
    assert(pElem);
    LineRenderData* data = this->renderData();
    if (!data || !data->vertices || !data->lineIndices)
    {
        assert(false);
        return nullptr;
    }

    osg::Matrix hostMatrix;
    if (!findHostMatrix(pScene->getElementNode(findHostId(pElem)), hostMatrix))
    {
        assert(false);
        return nullptr;
    }

    // 设置包围盒始终为空
    _boundBox = osg::BoundingBox();

    BodyModificationRenderStyle style;
    style.edgeColor = Colors::kSolidEdge;
    style.highlightColor = Colors::kEdge_Highlight;
    style.previewColor = Colors::kEdge_Preview;
    style.inactiveColor = Colors::kTransparent;

    auto renderObject = std::make_unique<BodyModificationRenderObject>(
        pElem->getId(), this->getNodeType(), *data, hostMatrix, style);
    renderObject->build();
    return renderObject;
}

BodyModificationRenderObject* BodyModificationElementNode::renderObject() const
{
    return this->getRenderObjectAs<BodyModificationRenderObject>();
}

std::unique_ptr<RenderData> BodyModificationElementNode::generateRenderData(
    Scene* pScene, const wydb::Element* pElement) noexcept
{
    try
    {
        auto data = std::make_unique<LineRenderData>();

        assert(pScene);
        assert(pElement);

        // 实体修改元素
        std::vector<unsigned int> newFaceIndexVec;
        if (const wy3d::BodyModification* pSolidMod = wy3d::BodyModification::cast(pElement))
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
            return nullptr;
        }
        const wydb::ElementId ownerId = findHostId(pElement);
        assert(!ownerId.isNull());

        // 实体修改元素新生成的面为空
        if (newFaceIndexVec.empty())
        {
            // 当倒角特征出错时会导致newFaceIndices为空
            return nullptr;
        }

        // 关联的宿主元素节点(实体或片体)
        HostRenderData hostData;
        if (!collectHostRenderData(pScene->getElementNode(ownerId), hostData))
        {
            assert(false);
            return nullptr;
        }
        // 宿主当前可能没有形体(例如布尔结果暂时为空),此时不生成修改边
        data->vertices = hostData.vertices;
        if (!data->vertices || !hostData.lineIndices ||
            !hostData.faceInfos || !hostData.edgeInfos)
        {
            return nullptr;
        }
        const std::vector<FaceInfo>& faceInfos = *hostData.faceInfos;
        const std::vector<EdgeInfo>& edgeInfos = *hostData.edgeInfos;

        // 新生成的面的索引
        std::set<unsigned int> newFaceIndices;
        newFaceIndices.insert(newFaceIndexVec.cbegin(), newFaceIndexVec.cend());
        std::set<unsigned int>::const_iterator iterNewFaceIndex = newFaceIndices.cbegin();
        assert(iterNewFaceIndex != newFaceIndices.cend());

        // 计算新面的所有边索引(实体修改元素只渲染边)
        std::set<unsigned int> edgeIndexSet;
        for (size_t i = 0; i < faceInfos.size(); ++i)
        {
            if (i == *iterNewFaceIndex)
            {
                edgeIndexSet.insert(faceInfos[i].edgeIndices.cbegin(), faceInfos[i].edgeIndices.cend());

                ++iterNewFaceIndex;
                if (iterNewFaceIndex == newFaceIndices.cend())
                {
                    break;
                }
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
                    data->lineIndices->insert(data->lineIndices->end(), iterBeg, iterEnd);

                    ++iterEdgeIndex;
                    if (iterEdgeIndex == edgeIndexSet.cend())
                    {
                        break;
                    }
                }
                iterBeg = iterEnd;
            }
        }

        return data;
    }
    catch (const Standard_Failure&)
    {
        assert(false);
        return nullptr;
    }
    catch (...)
    {
        assert(false);
        return nullptr;
    }
}

bool BodyModificationElementNode::computeWhetherActive(const wydb::Element* pCurElem) const
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

} // namespace wy3dApp
