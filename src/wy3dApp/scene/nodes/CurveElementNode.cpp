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
#include "utils/MathUtils.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

void CurveElementNode::generateRenderObjectImpl(Scene* pScene, const wydb::Element* pElem)
{
    assert(pElem);
    const wy3d::Curve* pCurve = wy3d::Curve::cast(pElem);
    if (!pCurve)
    {
        assert(false);
        return;
    }

    if (!_vertices || !_lineIndices)
    {
        assert(false);
        return;
    }

    // 生成包围盒
    _boundBox = this->computeBoundingBox(*_vertices);

    // 生成渲染对象
    if (!_lineIndices->empty())
    {
        _curvesGeom = new osg::Geometry();
        _curvesGeom->setNodeMask(this->getNodeType());
        _curvesGeom->getOrCreateStateSet()->setRenderBinDetails(RenderBinNumers::SketchElement, "RenderBin");
        {
            _curvesGeom->setUseDisplayList(false);
            _curvesGeom->setUseVertexBufferObjects(true);
            _curvesGeom->setVertexArray(_vertices);
            osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
            colors->push_back(Colors::kSketch);
            _curvesGeom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);
            _curvesGeom->addPrimitiveSet(new osg::DrawElementsUInt(GL_LINES, _lineIndices->begin(), _lineIndices->end()));
            _curvesGeom->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
            _curvesGeom->setUserValue("ElementId", static_cast<unsigned int>(pElem->getId().value()));
            _curvesGeom->getStateSet()->setAttribute(new osg::LineWidth(1.2f));
        }
        _osgNode->addChild(_curvesGeom);
    }
}

ElementNode::GenRenderDataRet CurveElementNode::generateRenderDataImpl(Scene* pScene, const wydb::Element* pElement)
{
    _vertices = new osg::Vec3Array();
    _lineIndices = new osg::UIntArray();

    assert(pElement);
    const wy3d::Curve* pCurve = wy3d::Curve::cast(pElement);
    if (!pCurve)
    {
        assert(false);
        return GenRenderDataRet::Ok_Empty;
    }    if (!pCurve)
    {
        assert(false);
        return GenRenderDataRet::Ok_Empty;
    }

    TopoDS_Edge edge = pCurve->getEdge();
    if (edge.IsNull())
    {
        assert(false);
        return GenRenderDataRet::Ok_Empty;
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
        return GenRenderDataRet::Ok_Empty;
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
    _vertices->reserve(nbNodesInEdge);
    gp_Pnt pnt;
    for (Standard_Integer j = 1; j <= nbNodesInEdge; j++)
    {
        pnt = aNodes(j);
        if (!identity) pnt.Transform(myTransf);
        (*_vertices).push_back(osg::Vec3(pnt.X(), pnt.Y(), pnt.Z()));
    }

    // 索引
    if (_vertices->size() <= 1)
    {
        assert(false);
        return GenRenderDataRet::Ok_Empty;
    }
    _lineIndices->reserve((_vertices->size() - 1) * 2);
    for (size_t i = 0; i < _vertices->size() - 1; ++i)
    {
        (*_lineIndices).push_back(i);
        (*_lineIndices).push_back(i + 1);
    }

    return GenRenderDataRet::Ok;
}

void CurveElementNode::highlightImpl(bool flag)
{
    this->highlightGeom(_curvesGeom, flag, Colors::kSketch_Highlight);
}

void CurveElementNode::previewImpl(bool flag)
{
    if (this->isHighlighted())
    {
        assert(false);
        return;
    }

    this->highlightGeom(_curvesGeom, flag, Colors::kSketch_Preview);
}

void CurveElementNode::highlightGeom(const osg::ref_ptr<osg::Geometry>& geom, bool flag, const osg::Vec4& highlightColor)
{
    if (!geom) return;

    OsgUtils::setNodeColor(geom, flag ? highlightColor : Colors::kSketch);
    if (flag)
    {
        // 草图高亮时加大线宽
        geom->getOrCreateStateSet()->setAttribute(new osg::LineWidth(2.0f));
        // 草图高亮时禁用深度测试确保不被遮挡住
        geom->getOrCreateStateSet()->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
    }
    else
    {
        // 草图取消高亮时恢复线宽
        geom->getOrCreateStateSet()->setAttribute(new osg::LineWidth(1.2f));
        // 草图取消高亮时启用深度测试
        geom->getOrCreateStateSet()->setMode(GL_DEPTH_TEST, osg::StateAttribute::ON);
    }
}

void CurveElementNode::setActiveImpl(bool flag)
{
}

bool CurveElementNode::computeWhetherActive(const wydb::Element* pCurElem) const
{
    assert(pCurElem);
    return pCurElem->getParent().isNull();
}

} // namespace wy3dApp
