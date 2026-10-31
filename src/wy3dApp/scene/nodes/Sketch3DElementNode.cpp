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
#include <string>

#include <Standard_Failure.hxx>
#include <osg/LineWidth>
#include <OsgUtils.h>

#include <wydbDatabase.h>
#include <wy3dSketch3D.h>
#include <wy3dSketchEntity3D.h>

#include "scene/SketchEntity3DLinearization.h"
#include <wy3dApp/ElementNodeType.h>
#include "scene/RenderConst.h"
#include "scene/Colors.h"
#include "scene/nodes/CurveRenderObject.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

std::unique_ptr<RenderObject> Sketch3DElementNode::createRenderObject(
    Scene* pScene, const wydb::Element* pElem)
{
    (void)pScene;
    assert(pElem);
    const wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pElem);
    if (!pSketch3D)
    {
        assert(false);
        return nullptr;
    }

    CurveRenderData* data = this->renderData();
    if (!data || !data->vertices || !data->lineIndices)
    {
        assert(false);
        return nullptr;
    }

    CurveRenderStyle style;
    style.nodeType = this->getNodeType();
    style.color = Colors::kSketch;
    style.highlightColor = Colors::kSketch_Highlight;
    style.previewColor = Colors::kSketch_Preview;
    style.lineWidth = 1.2f;
    style.highlightLineWidth = 2.0f;
    style.changeLineWidthOnHighlight = true;
    style.depthTest = true;
    style.highlightDepthTest = false;
    style.changeDepthTestOnHighlight = true;
    style.lighting = false;
    style.applyStateToRoot = false;
    style.renderBin = RenderBinNumers::SketchElement;
    style.highlightOverlayColor = Colors::kEdge_Highlight;
    style.previewOverlayColor = Colors::kEdge_Preview;

    auto renderObject = std::make_unique<CurveRenderObject>(
        pElem->getId(), this->getNodeType(), *data, *data->lineIndices, style);
    renderObject->build();
    return renderObject;
}

CurveRenderObject* Sketch3DElementNode::renderObject() const
{
    return this->getRenderObjectAs<CurveRenderObject>();
}

std::unique_ptr<RenderData> Sketch3DElementNode::generateRenderData(
    Scene* pScene, const wydb::Element* pElement) noexcept
{
    (void)pScene;

    try
    {
        auto data = std::make_unique<CurveRenderData>();

        assert(pElement);
        const wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pElement);
        if (!pSketch3D)
        {
            assert(false);
            return nullptr;
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
        data->vertices->reserve(10);
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

        data->vertices->reserve(data->vertices->size() + totalNumVertices);
        data->lineIndices->reserve(totalNumIndices);
        data->curveInfos.reserve(entityLinears.size());
        for (const EntityLinearInfo& info : entityLinears)
        {
            assert(info.pLinear);
            unsigned int baseIndex = data->vertices->size();
            for (const wy::Vector3& pnt : info.pLinear->getVertices())
            {
                data->vertices->push_back(osg::Vec3(pnt.x(), pnt.y(), pnt.z()));
            }
            const std::vector<unsigned int>& indices = info.pLinear->getIndices();
            CurveRenderInfo curveInfo;
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
            data->curveInfos.emplace_back(curveInfo);
            for (unsigned int index : indices)
            {
                data->lineIndices->push_back(baseIndex + index);
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

bool Sketch3DElementNode::computeWhetherActive(const wydb::Element* pCurElem) const
{
    assert(pCurElem);
    return pCurElem->getParent().isNull();
}

unsigned int Sketch3DElementNode::getCurveId(unsigned int primitiveIndex) const
{
    const CurveRenderData* data = this->renderData();
    if (!data || data->curveInfos.empty()) return 0;

    unsigned int count(0);
    for (const CurveRenderInfo& info : data->curveInfos)
    {
        count += info.numLines;
        if (primitiveIndex < count)
        {
            return info.id;
        }
    }

    return 0;
}

wyap::Selection Sketch3DElementNode::createSelection(
    osg::Drawable* pDrawable,
    unsigned int primitiveIndex,
    wy3d::SelectionType selType) const
{
    (void)pDrawable;

    if (wy3d::SelectionTypeUtil::HasValue(selType, wy3d::SelectionType::SketchCurve3D))
    {
        unsigned int curveId = this->getCurveId(primitiveIndex);
        if (0 == curveId)
        {
            assert(false);
            return wyap::Selection(wydb::ElementId::kNull);
        }

        return wyap::Selection(static_cast<unsigned int>(wy3d::SelectionType::SketchCurve3D),
            this->getElementId(), std::to_string(curveId));
    }
    else if (wy3d::SelectionTypeUtil::HasValue(selType, wy3d::SelectionType::Element))
    {
        return wyap::Selection(this->getElementId());
    }
    else
    {
        assert(false);
        return wyap::Selection(wydb::ElementId::kNull);
    }
}

void Sketch3DElementNode::highlightCurveByIndex(unsigned int curveIndex, bool flag)
{
    if (CurveRenderObject* obj = this->renderObject())
    {
        obj->highlightCurveByIndex(curveIndex, flag);
    }
    else
    {
        assert(false);
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
    if (CurveRenderObject* obj = this->renderObject())
    {
        obj->previewCurveByIndex(curveIndex, flag);
    }
    else
    {
        assert(false);
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

unsigned int Sketch3DElementNode::getCurveIndexById(unsigned int id) const
{
    const CurveRenderData* data = this->renderData();
    if (!data) return static_cast<unsigned int>(-1);

    unsigned int index(-1);
    for (const CurveRenderInfo& info : data->curveInfos)
    {
        ++index;
        if (info.id == id)
        {
            return index;
        }
    }
    return static_cast<unsigned int>(-1);
}

} // namespace wy3dApp
