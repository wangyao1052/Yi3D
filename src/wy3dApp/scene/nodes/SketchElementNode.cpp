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

#include "SketchElementNode.h"

#include <cassert>
#include <string>

#include <Standard_Failure.hxx>
#include <gp_Quaternion.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Face.hxx>
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <Precision.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopExp.hxx>
#include <BRep_Tool.hxx>
#include <TopoDS.hxx>
#include <TopExp_Explorer.hxx>
#include <TColgp_Array1OfDir.hxx>
#include <Poly_Connect.hxx>
#include <GeomLib.hxx>

#include <osg/MatrixTransform>
#include <osg/LineStipple>
#include <OsgUtils.h>

#include <wyVector2.h>
#include <wyVector3.h>
#include <wy3dMath.h>
#include <wydbDatabase.h>
#include <wy3dFeature.h>
#include <wy3dBox.h>
#include <wy3dCylinder.h>
#include <wy3dSphere.h>
#include <wy3dSketch.h>
#include <wy3dSketchPoint.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>

#include <osg/BlendFunc>
#include <osg/Material>
#include <osg/PolygonOffset>
#include <osg/CullFace>
#include <osg/LineWidth>
#include <osg/Point>

#include "scene/SketchEntityLinearization.h"
#include <wy3dApp/ElementNodeType.h>
#include "scene/RenderConst.h"
#include "scene/Colors.h"
#include "scene/nodes/SketchRenderObject.h"
#include "utils/MathUtils.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

std::unique_ptr<RenderObject> SketchElementNode::createRenderObject(
    Scene* pScene, const wydb::Element* pElem)
{
    (void)pScene;
    assert(pElem);
    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pElem);
    if (!pSketch)
    {
        assert(false);
        return nullptr;
    }

    SketchRenderData* data = this->renderData();
    if (!data || !data->vertices || !data->lineIndices ||
        !data->centerLineIndices || !data->pointIndices)
    {
        assert(false);
        return nullptr;
    }

    const ElementNodeType nodeType = this->getNodeType();

    SketchRenderStyle style;
    style.curveStyle.nodeType = nodeType;
    style.curveStyle.color = Colors::kSketch;
    style.curveStyle.highlightColor = Colors::kSketch_Highlight;
    style.curveStyle.previewColor = Colors::kSketch_Preview;
    style.curveStyle.lineWidth = 1.2f;
    style.curveStyle.highlightLineWidth = 2.0f;
    style.curveStyle.depthTest = true;
    style.curveStyle.highlightDepthTest = false;
    style.curveStyle.lighting = false;
    style.curveStyle.renderBin = RenderBinNumers::SketchElement;

    style.centerLineStyle = style.curveStyle;
    style.centerLineStyle.stipple = true;
    style.centerLineStyle.stippleFactor = CENTER_LINE_STIPPLE_FACTOR;
    style.centerLineStyle.stipplePattern = CENTER_LINE_STIPPLE_PATTERN;

    style.pointStyle = style.curveStyle;
    style.pointStyle.indicesAsPoints = true;
    style.pointStyle.pointSize = 5.0f;
    style.pointStyle.highlightPointSize = 7.0f;
    style.pointStyle.changePointSizeOnHighlight = true;

    style.highlightOverlayColor = Colors::kEdge_Highlight;
    style.previewOverlayColor = Colors::kEdge_Preview;

    auto renderObject = std::make_unique<SketchRenderObject>(pElem->getId(), *data, style);
    renderObject->build();
    return renderObject;
}

SketchRenderObject* SketchElementNode::renderObject() const
{
    return this->getRenderObjectAs<SketchRenderObject>();
}

osg::Geometry* SketchElementNode::getCenterLinesGeom() const
{
    SketchRenderObject* obj = this->renderObject();
    return obj ? obj->getCenterLinesGeom() : nullptr;
}

std::unique_ptr<RenderData> SketchElementNode::generateRenderData(
    Scene* pScene, const wydb::Element* pElement) noexcept
{
    (void)pScene;

    try
    {
        auto data = std::make_unique<SketchRenderData>();

        assert(pElement);
        const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pElement);
        if (!pSketch)
        {
            assert(false);
            return nullptr;
        }
        const wydb::Database* pDb = pElement->getDatabase();
        assert(pDb);

        const wy3d::Plane& sketchPlane = pSketch->getPlane();
        unsigned int totalNumVertices(0);
        unsigned int totalNumIndices(0);
        unsigned int totalNumIndicesOfCenterLine(0);
        struct EntityLinearInfo
        {
            wydb::ElementId id;
            std::shared_ptr<SketchEntityLinearization> pLinear;
            bool isCenterLine;
            EntityLinearInfo() : id(wydb::ElementId::kNull), pLinear(nullptr), isCenterLine(false) {}
        };
        std::list<EntityLinearInfo> entityLinears;
        data->vertices->reserve(10);
        for (auto iter = pSketch->createIterator(); !iter.isDone(); iter.moveNext())
        {
            wydb::ElementId id = iter.current();
            if (id.isNull()) continue;
            const wydb::Element* pElem = pDb->getElement(id);
            const wy3d::SketchEntity* pEntity = wy3d::SketchEntity::cast(pElem);
            if (!pEntity)
            {
                assert(false);
                continue;
            }
            if (const wy3d::SketchCurve* pCurve = wy3d::SketchCurve::cast(pEntity))
            {
                if (pCurve->isConstruction()) continue; // 过滤掉构造线
                const wy3d::SketchCenterLine* pCenterLine = wy3d::SketchCenterLine::cast(pEntity);
                std::shared_ptr<SketchEntityLinearization> pEntityLinear = std::make_shared<SketchEntityLinearization>(pEntity);
                totalNumVertices += pEntityLinear->getVertices().size();
                if (pCenterLine) totalNumIndicesOfCenterLine += pEntityLinear->getIndices().size();
                else totalNumIndices += pEntityLinear->getIndices().size();
                EntityLinearInfo info;
                info.id = pCurve->getId();
                info.pLinear = pEntityLinear;
                info.isCenterLine = pCenterLine ? true : false;
                entityLinears.emplace_back(info);
            }
            else if (const wy3d::SketchPoint* pPoint = wy3d::SketchPoint::cast(pEntity))
            {
                data->vertices->push_back(MathUtils::toVec3d(sketchPlane.value(pPoint->getPosition())));
            }
        }

        // added by wangyao 2025.06.17 {
        // 草图点
        if (!data->vertices->empty())
        {
            data->pointIndices->reserve(data->vertices->size());
            for (unsigned int i = 0; i < data->vertices->size(); ++i)
            {
                data->pointIndices->push_back(i);
            }
        }
        // }
        data->vertices->reserve(data->vertices->size() + totalNumVertices);
        data->lineIndices->reserve(totalNumIndices);
        data->centerLineIndices->reserve(totalNumIndicesOfCenterLine);
        data->curveInfos.reserve(entityLinears.size());
        for (const EntityLinearInfo& info : entityLinears)
        {
            assert(info.pLinear);
            unsigned int baseIndex = data->vertices->size();
            for (const wy::Vector2& pnt2d : info.pLinear->getVertices())
            {
                wy::Vector3 pnt3d = sketchPlane.value(pnt2d.x(), pnt2d.y());
                data->vertices->push_back(osg::Vec3(pnt3d.x(), pnt3d.y(), pnt3d.z()));
            }
            if (info.isCenterLine)
            {
                const std::vector<unsigned int>& indices = info.pLinear->getIndices();
                CurveRenderInfo curveInfo;
                curveInfo.id = info.id.value();
                size_t numIndices = indices.size();
                if (numIndices % 2 == 0)
                    curveInfo.numLines = numIndices / 2;
                else
                    curveInfo.numLines = 0;
                data->centerLineInfos.emplace_back(curveInfo);
                for (unsigned int index : indices)
                {
                    data->centerLineIndices->push_back(baseIndex + index);
                }
            }
            else
            {
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

bool SketchElementNode::computeWhetherActive(const wydb::Element* pCurElem) const
{
    assert(pCurElem);
    return pCurElem->getParent().isNull();
}

unsigned int SketchElementNode::getCurveId(unsigned int primitiveIndex) const
{
    const SketchRenderData* data = this->renderData();
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

unsigned int SketchElementNode::getCenterLineCurveId(unsigned int primitiveIndex) const
{
    const SketchRenderData* data = this->renderData();
    if (!data || data->centerLineInfos.empty()) return 0;

    unsigned int count(0);
    for (const CurveRenderInfo& info : data->centerLineInfos)
    {
        count += info.numLines;
        if (primitiveIndex < count)
        {
            return info.id;
        }
    }

    return 0;
}

wyap::Selection SketchElementNode::createSelection(
    osg::Drawable* pDrawable,
    unsigned int primitiveIndex,
    wy3d::SelectionType selType) const
{
    if (wy3d::SelectionTypeUtil::HasValue(selType, wy3d::SelectionType::SketchCurve))
    {
        unsigned int curveId(0);
        if (pDrawable == this->getCenterLinesGeom())
        {
            curveId = this->getCenterLineCurveId(primitiveIndex);
        }
        else
        {
            curveId = this->getCurveId(primitiveIndex);
        }
        if (0 == curveId)
        {
            assert(false);
            return wyap::Selection(wydb::ElementId::kNull);
        }

        return wyap::Selection(static_cast<unsigned int>(wy3d::SelectionType::SketchCurve),
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

void SketchElementNode::highlightCurveByIndex(unsigned int curveIndex, bool flag)
{
    if (SketchRenderObject* obj = this->renderObject())
    {
        obj->highlightCurveByIndex(curveIndex, flag);
    }
    else
    {
        assert(false);
    }
}

void SketchElementNode::highlightCurveById(unsigned int id, bool flag)
{
    unsigned int index = this->getCurveIndexById(id);
    if (index != static_cast<unsigned int>(-1))
    {
        this->highlightCurveByIndex(index, flag);
        return;
    }
    index = this->getCenterLineCurveIndexById(id);
    if (index != static_cast<unsigned int>(-1))
    {
        this->highlightCenterLineByIndex(index, flag);
    }
}

void SketchElementNode::previewCurveByIndex(unsigned int curveIndex, bool flag)
{
    if (SketchRenderObject* obj = this->renderObject())
    {
        obj->previewCurveByIndex(curveIndex, flag);
    }
    else
    {
        assert(false);
    }
}

void SketchElementNode::previewCurveById(unsigned int id, bool flag)
{
    unsigned int index = this->getCurveIndexById(id);
    if (index != static_cast<unsigned int>(-1))
    {
        this->previewCurveByIndex(index, flag);
        return;
    }
    index = this->getCenterLineCurveIndexById(id);
    if (index != static_cast<unsigned int>(-1))
    {
        this->previewCenterLineByIndex(index, flag);
    }
}

unsigned int SketchElementNode::getCurveIndexById(unsigned int id) const
{
    const SketchRenderData* data = this->renderData();
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

unsigned int SketchElementNode::getCenterLineCurveIndexById(unsigned int id) const
{
    const SketchRenderData* data = this->renderData();
    if (!data) return static_cast<unsigned int>(-1);

    unsigned int index(-1);
    for (const CurveRenderInfo& info : data->centerLineInfos)
    {
        ++index;
        if (info.id == id)
        {
            return index;
        }
    }
    return static_cast<unsigned int>(-1);
}

void SketchElementNode::highlightCenterLineByIndex(unsigned int curveIndex, bool flag)
{
    if (SketchRenderObject* obj = this->renderObject())
    {
        obj->highlightCenterLineByIndex(curveIndex, flag);
    }
    else
    {
        assert(false);
    }
}

void SketchElementNode::previewCenterLineByIndex(unsigned int curveIndex, bool flag)
{
    if (SketchRenderObject* obj = this->renderObject())
    {
        obj->previewCenterLineByIndex(curveIndex, flag);
    }
    else
    {
        assert(false);
    }
}

} // namespace wy3dApp
