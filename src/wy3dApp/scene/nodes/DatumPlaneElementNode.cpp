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

#include "DatumPlaneElementNode.h"
#include "scene/nodes/ElementBounds.h"

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
#include <osg/LineStipple>
#include <OsgUtils.h>
#include <osg/Depth>

#include <wyVector2.h>
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

#include <wy3dDatumPlane.h>

#include <osg/BlendFunc>
#include <osg/Material>
#include <osg/PolygonOffset>
#include <osg/CullFace>
#include <osg/LineWidth>

#include "scene/SketchEntityLinearization.h"
#include <wy3dApp/ElementNodeType.h>
#include "scene/RenderConst.h"
#include "scene/Colors.h"
#include "scene/nodes/DatumPlaneRenderObject.h"
#include "utils/MathUtils.h"
#include <wy3dApp/wy3dAppDefs.h>

#define SIDE 100.0

namespace wy3dApp {

DatumPlaneElementNode::DatumPlaneElementNode(const wydb::ElementId& id)
    : DatumElementNode(id), _min(-SIDE, -SIDE), _max(SIDE, SIDE)
{}

inline void updateMinMax(double& uMin, double& vMin, double& uMax, double& vMax,
    const wy3d::Plane& plane, wy::Vector2& uv, double x, double y, double z)
{
    uv = plane.uv(x, y, z);
    uMin = std::min(uMin, uv.x());
    vMin = std::min(vMin, uv.y());
    uMax = std::max(uMax, uv.x());
    vMax = std::max(vMax, uv.y());
}

void DatumPlaneElementNode::updateVisualSize(const wydb::Database* pDb, const osg::BoundingBox& bbox)
{
    if (!pDb)
    {
        assert(false);
        return;
    }

    if (!bbox.valid())
    {
        assert(false);
        return;
    }
    assert(!std::isnan(bbox.xMin()));
    assert(!std::isnan(bbox.yMin()));
    assert(!std::isnan(bbox.zMin()));
    assert(!std::isnan(bbox.xMax()));
    assert(!std::isnan(bbox.yMax()));
    assert(!std::isnan(bbox.zMax()));

    // 基准平面
    const wy3d::DatumPlane* pDatumPlane = wy3d::DatumPlane::cast(pDb->getElement(this->getElementId()));
    if (!pDatumPlane)
    {
        assert(false);
        return;
    }
    const wy3d::Plane& plane = pDatumPlane->getPlane();

    // 计算UV范围
    double uMin(DBL_MAX), vMin(DBL_MAX), uMax(-DBL_MAX), vMax(-DBL_MAX);

    // 空的包围盒使用默认UV大小
    if (bbox.radius() <= 1e-7)
    {
        uMin = -SIDE;
        vMin = -SIDE;
        uMax = SIDE;
        vMax = SIDE;
    }
    else // 计算UV
    {
        wy::Vector2 uv;
        updateMinMax(uMin, vMin, uMax, vMax, plane, uv,
            bbox.xMin(), bbox.yMin(), bbox.zMin());
        updateMinMax(uMin, vMin, uMax, vMax, plane, uv,
            bbox.xMin(), bbox.yMin(), bbox.zMax());
        updateMinMax(uMin, vMin, uMax, vMax, plane, uv,
            bbox.xMin(), bbox.yMax(), bbox.zMin());
        updateMinMax(uMin, vMin, uMax, vMax, plane, uv,
            bbox.xMin(), bbox.yMax(), bbox.zMax());
        updateMinMax(uMin, vMin, uMax, vMax, plane, uv,
            bbox.xMax(), bbox.yMin(), bbox.zMin());
        updateMinMax(uMin, vMin, uMax, vMax, plane, uv,
            bbox.xMax(), bbox.yMin(), bbox.zMax());
        updateMinMax(uMin, vMin, uMax, vMax, plane, uv,
            bbox.xMax(), bbox.yMax(), bbox.zMin());
        updateMinMax(uMin, vMin, uMax, vMax, plane, uv,
            bbox.xMax(), bbox.yMax(), bbox.zMax());

        // 更新UV范围
        double uRange = uMax - uMin;
        double uSpacing = 0.1 * uRange;
        uMin -= uSpacing;
        uMax += uSpacing;
        double vRange = vMax - vMin;
        double vSpacing = 0.1 * vRange;
        vMin -= vSpacing;
        vMax += vSpacing;

        // 特殊情况处理
        if (std::fabs(uRange) <= 1e-7)
        {
            double base = uMin;
            uMin = base - (vMax - vMin) / 2;
            uMax = base + (vMax - vMin) / 2;
        }
        else if (std::fabs(vRange) <= 1e-7)
        {
            double base = vMin;
            vMin = base - (uMax - uMin) / 2;
            vMax = base + (uMax - uMin) / 2;
        }
    }
    _min.set(uMin, vMin);
    _max.set(uMax, vMax);

    // 更新顶点坐标
    osg::ref_ptr<osg::Vec3Array> vertices = this->getVertices();
    if (vertices && vertices->size() == 4)
    {
        (*vertices)[0] = MathUtils::toVec3(plane.value(uMin, vMin));
        (*vertices)[1] = MathUtils::toVec3(plane.value(uMax, vMin));
        (*vertices)[2] = MathUtils::toVec3(plane.value(uMax, vMax));
        (*vertices)[3] = MathUtils::toVec3(plane.value(uMin, vMax));
        vertices->dirty();

        // 更新包围盒
        _boundBox = ElementBounds::compute(*vertices);
    }
    else
    {
        assert(false);
    }

    // 刷新渲染对象
    if (DatumPlaneRenderObject* obj = this->renderObject())
    {
        obj->dirtyGeometryBound();
    }
}

std::unique_ptr<RenderObject> DatumPlaneElementNode::createRenderObject(
    Scene* pScene, const wydb::Element* pElem)
{
    (void)pScene;
    assert(pElem);
    const wy3d::DatumPlane* pDatumPlane = wy3d::DatumPlane::cast(pElem);
    if (!pDatumPlane)
    {
        assert(false);
        return nullptr;
    }

    MeshRenderData* data = this->renderData();
    if (!data || !data->vertices || !data->normals || !data->triangleIndices || !data->lineIndices)
    {
        assert(false);
        return nullptr;
    }

    DatumPlaneRenderStyle style;
    style.faceColor = Colors::kDatumPlaneFace;
    style.edgeColor = Colors::kDatumPlaneEdge;
    style.faceHighlightColor = Colors::kDatumPlaneFace_Highlight;
    style.edgeHighlightColor = Colors::kDatumPlaneEdge_Highlight;
    style.facePreviewColor = Colors::kDatumPlaneFace_Preview;
    style.edgePreviewColor = Colors::kDatumPlaneEdge_Preview;

    auto renderObject = std::make_unique<DatumPlaneRenderObject>(
        pElem->getId(), this->getNodeType(), *data, style);
    renderObject->build();
    return renderObject;
}

DatumPlaneRenderObject* DatumPlaneElementNode::renderObject() const
{
    return this->getRenderObjectAs<DatumPlaneRenderObject>();
}

std::unique_ptr<RenderData> DatumPlaneElementNode::generateRenderData(
    Scene* pScene, const wydb::Element* pElement) noexcept
{
    (void)pScene;

    try
    {
        auto data = std::make_unique<MeshRenderData>();

        assert(pElement);
        const wy3d::DatumPlane* pDatumPlane = wy3d::DatumPlane::cast(pElement);
        if (!pDatumPlane)
        {
            assert(false);
            return nullptr;
        }
        const wy3d::Plane& sketchPlane = pDatumPlane->getPlane();

        data->vertices->reserve(4);
        data->vertices->push_back(MathUtils::toVec3(sketchPlane.value(_min.x(), _min.y())));
        data->vertices->push_back(MathUtils::toVec3(sketchPlane.value(_max.x(), _min.y())));
        data->vertices->push_back(MathUtils::toVec3(sketchPlane.value(_max.x(), _max.y())));
        data->vertices->push_back(MathUtils::toVec3(sketchPlane.value(_min.x(), _max.y())));

        data->normals->reserve(1);
        data->normals->push_back(MathUtils::toVec3(sketchPlane.getNormal()));

        data->triangleIndices->reserve(6);
        data->triangleIndices->push_back(0);
        data->triangleIndices->push_back(1);
        data->triangleIndices->push_back(2);
        data->triangleIndices->push_back(2);
        data->triangleIndices->push_back(3);
        data->triangleIndices->push_back(0);

        data->lineIndices->reserve(8);
        data->lineIndices->push_back(0);
        data->lineIndices->push_back(1);
        data->lineIndices->push_back(1);
        data->lineIndices->push_back(2);
        data->lineIndices->push_back(2);
        data->lineIndices->push_back(3);
        data->lineIndices->push_back(3);
        data->lineIndices->push_back(0);

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
