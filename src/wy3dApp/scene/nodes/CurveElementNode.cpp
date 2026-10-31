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

#include "CurveElementNode.h"

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

#include <wy3dMath.h>
#include <wydbDatabase.h>
#include <wy3dFeature.h>
#include <wy3dBox.h>
#include <wy3dCylinder.h>
#include <wy3dSphere.h>
#include <wy3dSketch.h>
#include <wy3dCurve.h>

#include <osg/BlendFunc>
#include <osg/Material>
#include <osg/PolygonOffset>
#include <osg/CullFace>
#include <osg/LineWidth>
#include <osg/Point>

#include "scene/SketchEntityLinearization.h"
#include "scene/RenderConst.h"
#include "scene/Colors.h"
#include "scene/nodes/LineRenderObject.h"
#include "utils/MathUtils.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

std::unique_ptr<RenderObject> CurveElementNode::createRenderObject(
    Scene* pScene, const wydb::Element* pElem)
{
    (void)pScene;
    assert(pElem);
    const wy3d::Curve* pCurve = wy3d::Curve::cast(pElem);
    if (!pCurve)
    {
        assert(false);
        return nullptr;
    }

    LineRenderData* data = this->renderData();
    if (!data || !data->vertices || !data->lineIndices)
    {
        assert(false);
        return nullptr;
    }

    LineRenderStyle style;
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

    auto renderObject = std::make_unique<LineRenderObject>(
        pElem->getId(), this->getNodeType(), *data, *data->lineIndices, style);
    renderObject->build();
    return renderObject;
}

LineRenderObject* CurveElementNode::renderObject() const
{
    return this->getRenderObjectAs<LineRenderObject>();
}

std::unique_ptr<RenderData> CurveElementNode::generateRenderData(
    Scene* pScene, const wydb::Element* pElement) noexcept
{
    (void)pScene;

    try
    {
        auto data = std::make_unique<LineRenderData>();

        assert(pElement);
        const wy3d::Curve* pCurve = wy3d::Curve::cast(pElement);
        if (!pCurve)
        {
            assert(false);
            return nullptr;
        }

        TopoDS_Edge edge = pCurve->getEdge();
        if (edge.IsNull())
        {
            assert(false);
            return nullptr;
        }

        // 由包围盒确定三角网格化精度
        Bnd_Box bounds;
        BRepBndLib::Add(edge, bounds);
        bounds.SetGap(0.0);
        Standard_Real xMin, yMin, zMin, xMax, yMax, zMax;
        bounds.Get(xMin, yMin, zMin, xMax, yMax, zMax);
        Standard_Real deflection = ((xMax - xMin) + (yMax - yMin) + (zMax - zMin)) / 2400.0;
        if (deflection < gp::Resolution())
        {
            deflection = Precision::Confusion();
        }
        deflection = std::min(deflection, 20.0);
        Standard_Real AngDeflectionRads = 0.5; // 弧度

        // 离散化
        BRepMesh_IncrementalMesh incrementalMesh(edge, deflection, Standard_False, AngDeflectionRads, Standard_True);

        TopLoc_Location aLoc;
        Handle(Poly_Polygon3D) aPoly = BRep_Tool::Polygon3D(edge, aLoc);
        if (aPoly.IsNull())
        {
            assert(false);
            return nullptr;
        }

        Standard_Boolean identity = true;
        gp_Trsf myTransf;
        if (!aLoc.IsIdentity())
        {
            identity = false;
            myTransf = aLoc.Transformation();
        }

        // 顶点
        const TColgp_Array1OfPnt& aNodes = aPoly->Nodes();
        int nbNodesInEdge = aPoly->NbNodes();
        data->vertices->reserve(nbNodesInEdge);
        gp_Pnt pnt;
        for (Standard_Integer j = 1; j <= nbNodesInEdge; j++)
        {
            pnt = aNodes(j);
            if (!identity) pnt.Transform(myTransf);
            data->vertices->push_back(osg::Vec3(pnt.X(), pnt.Y(), pnt.Z()));
        }

        // 索引
        if (data->vertices->size() <= 1)
        {
            assert(false);
            return nullptr;
        }
        data->lineIndices->reserve((data->vertices->size() - 1) * 2);
        for (size_t i = 0; i < data->vertices->size() - 1; ++i)
        {
            data->lineIndices->push_back(i);
            data->lineIndices->push_back(i + 1);
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

bool CurveElementNode::computeWhetherActive(const wydb::Element* pCurElem) const
{
    assert(pCurElem);
    return pCurElem->getParent().isNull();
}

} // namespace wy3dApp
