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

#include "SketchEntityElementNode.h"

#include <cassert>

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
#include <wy3dSketchLine.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>
#include <wy3dSketchPoint.h>

#include <osg/BlendFunc>
#include <osg/Material>
#include <osg/PolygonOffset>
#include <osg/CullFace>
#include <osg/LineWidth>
#include <osg/LineStipple>
#include <osg/Point>

#include "scene/SketchEntityLinearization.h"
#include "scene/RenderConst.h"
#include "scene/Colors.h"
#include "scene/nodes/LineRenderObject.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

SketchEntityElementNode::SketchEntityElementNode(const wydb::ElementId& id) : ElementNode(id)
{
}

std::unique_ptr<RenderObject> SketchEntityElementNode::createRenderObject(
    Scene* pScene, const wydb::Element* pElem)
{
    (void)pScene;
    assert(pElem);
    const wy3d::SketchEntity* pSketchEntity = wy3d::SketchEntity::cast(pElem);
    if (!pSketchEntity)
    {
        assert(false);
        return nullptr;
    }

    bool isConstruction(false);
    bool isCenterLine(false);
    if (const wy3d::SketchCurve* pSketchCurve = wy3d::SketchCurve::cast(pSketchEntity))
    {
        isConstruction = pSketchCurve->isConstruction();
        const wy3d::SketchCenterLine* pCenterLine = wy3d::SketchCenterLine::cast(pSketchCurve);
        if (pCenterLine) isCenterLine = true;
    }
    // Construction and centre lines share the curve colour, only thinner and dashed.
    const bool isDashed = (isConstruction || isCenterLine);
    const float lineWidth = isDashed ? 1.5f : 2.0f;

    LineRenderData* data = this->renderData();
    if (!data || !data->vertices || !data->lineIndices)
    {
        assert(false);
        return nullptr;
    }

    LineRenderStyle style;
    style.nodeType = this->getNodeType();
    style.color = Colors::kSketchEntity;
    style.highlightColor = Colors::kSketchEntity_Highlight;
    style.previewColor = Colors::kSketchEntity_Preview;
    style.lineWidth = lineWidth;
    style.highlightLineWidth = lineWidth;
    style.changeLineWidthOnHighlight = false;
    style.depthTest = false;
    style.highlightDepthTest = false;
    style.changeDepthTestOnHighlight = false;
    style.lighting = false;
    style.applyStateToRoot = true;
    style.renderBin = RenderBinNumers::SketchEntity;
    style.stripFromSegments = true;
    style.stipple = isDashed;
    style.stippleFactor = CENTER_LINE_STIPPLE_FACTOR;
    style.stipplePattern = CENTER_LINE_STIPPLE_PATTERN;
    style.pointsWhenNoLines = true;
    style.pointSize = 8.0f;
    style.highlightPointSize = 8.0f;
    style.changePointSizeOnHighlight = false;

    auto renderObject = std::make_unique<LineRenderObject>(
        pElem->getId(), this->getNodeType(), *data, *data->lineIndices, style);
    renderObject->build();
    return renderObject;
}

LineRenderObject* SketchEntityElementNode::renderObject() const
{
    return this->getRenderObjectAs<LineRenderObject>();
}

std::unique_ptr<RenderData> SketchEntityElementNode::generateRenderData(
    Scene* pScene, const wydb::Element* pElement) noexcept
{
    (void)pScene;

    try
    {
        auto data = std::make_unique<LineRenderData>();

        assert(pElement);
        const wy3d::SketchEntity* pSketchEntity = wy3d::SketchEntity::cast(pElement);
        if (!pSketchEntity)
        {
            assert(false);
            return nullptr;
        }

        const wydb::Database* pDb = pElement->getDatabase();
        assert(pDb);
        wydb::ElementId sketchId = pSketchEntity->getParent();
        const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(sketchId));
        if (!pSketch)
        {
            assert(false);
            return nullptr;
        }
        const wy3d::Plane& sketchPlane = pSketch->getPlane();

        if (const wy3d::SketchCurve* pSketchCurve = wy3d::SketchCurve::cast(pSketchEntity))
        {
            unsigned int totalNumVertices(0);
            unsigned int totalNumIndices(0);
            std::shared_ptr<SketchEntityLinearization> pEntityLinear = std::make_shared<SketchEntityLinearization>(pSketchEntity);
            totalNumVertices += pEntityLinear->getVertices().size();
            totalNumIndices += pEntityLinear->getIndices().size();

            data->vertices->reserve(totalNumVertices);
            data->lineIndices->reserve(totalNumIndices);
            unsigned int baseIndex = data->vertices->size();
            for (const wy::Vector2& pnt2d : pEntityLinear->getVertices())
            {
                wy::Vector3 pnt3d = sketchPlane.value(pnt2d.x(), pnt2d.y());
                data->vertices->push_back(osg::Vec3(pnt3d.x(), pnt3d.y(), pnt3d.z()));
            }
            for (unsigned int index : pEntityLinear->getIndices())
            {
                data->lineIndices->push_back(baseIndex + index);
            }
        }
        else if (const wy3d::SketchPoint* pSketchPoint = wy3d::SketchPoint::cast(pSketchEntity))
        {
            wy::Vector3 pnt3d = sketchPlane.value(pSketchPoint->getPosition());
            data->vertices->push_back(osg::Vec3(pnt3d.x(), pnt3d.y(), pnt3d.z()));
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

} // namespace wy3dApp
