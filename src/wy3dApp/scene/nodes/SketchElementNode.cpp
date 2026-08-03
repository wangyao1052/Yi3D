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
#include "scene/RenderConst.h"
#include "scene/Colors.h"
#include "utils/MathUtils.h"

bool SketchElementNode::pickByNormalBoxImpl(osg::Polytope& polytope) const
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

void SketchElementNode::generateRenderObjectImpl(Scene* pScene, const wydb::Element* pElem)
{
    assert(pElem);
    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pElem);
    if (!pSketch)
    {
        assert(false);
        return;
    }

    assert(_vertices);
    assert(_lineIndices);
    assert(_centerLineIndices);
    assert(_pointIndices);

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

    // 生成渲染对象:中心线
    if (!_centerLineIndices->empty())
    {
        _centerLinesGeom = new osg::Geometry();
        _centerLinesGeom->setNodeMask(static_cast<unsigned int>(this->getNodeType()));
        _centerLinesGeom->getOrCreateStateSet()->setRenderBinDetails(RenderBinNumers::SketchElement, "RenderBin");
        {
            _centerLinesGeom->setUseDisplayList(false);
            _centerLinesGeom->setUseVertexBufferObjects(true);
            _centerLinesGeom->setVertexArray(_vertices);
            osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
            colors->push_back(Colors::kSketch);
            _centerLinesGeom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);
            _centerLinesGeom->addPrimitiveSet(new osg::DrawElementsUInt(GL_LINES, _centerLineIndices->begin(), _centerLineIndices->end()));
            _centerLinesGeom->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
            _centerLinesGeom->setUserValue("ElementId", static_cast<unsigned int>(pElem->getId().value()));
            _centerLinesGeom->getStateSet()->setAttribute(new osg::LineWidth(1.2f));
            // 线样式
            {
                _centerLinesGeom->getOrCreateStateSet()->setAttributeAndModes(new osg::LineStipple(
                    CENTER_LINE_STIPPLE_FACTOR, CENTER_LINE_STIPPLE_PATTERN), osg::StateAttribute::ON);
            }
        }
        _osgNode->addChild(_centerLinesGeom);
    }

    // 生成渲染对象:点
    if (!_pointIndices->empty())
    {
        _pointsGeom = new osg::Geometry();
        _pointsGeom->setNodeMask(static_cast<unsigned int>(this->getNodeType()));
        _pointsGeom->getOrCreateStateSet()->setRenderBinDetails(RenderBinNumers::SketchElement, "RenderBin");
        {
            _pointsGeom->setUseDisplayList(false);
            _pointsGeom->setUseVertexBufferObjects(true);
            _pointsGeom->setVertexArray(_vertices);
            osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
            colors->push_back(Colors::kSketch);
            _pointsGeom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);
            _pointsGeom->addPrimitiveSet(new osg::DrawElementsUInt(GL_POINTS, _pointIndices->begin(), _pointIndices->end()));
            _pointsGeom->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
            _pointsGeom->setUserValue("ElementId", static_cast<unsigned int>(pElem->getId().value()));
            _pointsGeom->getStateSet()->setAttribute(new osg::Point(5.0f));
        }
        _osgNode->addChild(_pointsGeom);
    }
}

ElementNode::GenRenderDataRet SketchElementNode::generateRenderDataImpl(Scene* pScene, const wydb::Element* pElement)
{
    _lineIndices = new osg::UIntArray();
    _centerLineIndices = new osg::UIntArray();
    _pointIndices = new osg::UIntArray();

    assert(pElement);
    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pElement);
    if (!pSketch)
    {
        assert(false);
        return GenRenderDataRet::Ok_Empty;
    }
    const wydb::Database* pDb = pElement->getDatabase();
    assert(pDb);

    const wy3d::SketchPlane& sketchPlane = pSketch->getPlane();
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
    _vertices->reserve(10);
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
            _vertices->push_back(MathUtils::toVec3d(sketchPlane.value(pPoint->getPosition())));
        }
    }

    // added by wangyao 2025.06.17 {
    // 草图点
    if (!_vertices->empty())
    {
        _pointIndices->reserve(_vertices->size());
        for (unsigned int i = 0; i < _vertices->size(); ++i)
        {
            _pointIndices->push_back(i);
        }
    }
    // }
    _vertices->reserve(_vertices->size() + totalNumVertices);
    _lineIndices->reserve(totalNumIndices);
    _centerLineIndices->reserve(totalNumIndicesOfCenterLine);
    _curveInfos.reserve(entityLinears.size());
    for (const EntityLinearInfo& info : entityLinears)
    {
        assert(info.pLinear);
        unsigned int baseIndex = _vertices->size();
        for (const wy::Vector2& pnt2d : info.pLinear->getVertices())
        {
            wy::Vector3 pnt3d = sketchPlane.value(pnt2d.x(), pnt2d.y());
            _vertices->push_back(osg::Vec3(pnt3d.x(), pnt3d.y(), pnt3d.z()));
        }
        if (info.isCenterLine)
        {
            const std::vector<unsigned int>& indices = info.pLinear->getIndices();
            CurveInfo curveInfo;
            curveInfo.id = info.id.value();
            size_t numIndices = indices.size();
            if (numIndices % 2 == 0)
                curveInfo.numLines = numIndices / 2;
            else
                curveInfo.numLines = 0;
            _centerLineInfos.emplace_back(curveInfo);
            for (unsigned int index : indices)
            {
                _centerLineIndices->push_back(baseIndex + index);
            }
        }
        else
        {
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
    }

    return GenRenderDataRet::Ok;
}

void SketchElementNode::highlightImpl(bool flag)
{
    this->highlightGeom(_curveGeom, flag, Colors::kSketch_Highlight);
    this->highlightGeom(_centerLinesGeom, flag, Colors::kSketch_Highlight);
    if (_pointsGeom) this->highlightGeom(_pointsGeom, flag, Colors::kSketch_Highlight);
}

void SketchElementNode::previewImpl(bool flag)
{
    if (this->isHighlighted())
    {
        assert(false);
        return;
    }

    this->highlightGeom(_curveGeom, flag, Colors::kSketch_Preview);
    this->highlightGeom(_centerLinesGeom, flag, Colors::kSketch_Preview);
    if (_pointsGeom) this->highlightGeom(_pointsGeom, flag, Colors::kSketch_Preview);
}

void SketchElementNode::highlightGeom(const osg::ref_ptr<osg::Geometry>& geom, bool flag, const osg::Vec4& highlightColor)
{
    if (!geom) return;

    OsgUtils::setNodeColor(geom, flag ? highlightColor : Colors::kSketch);
    if (flag)
    {
        // 草图高亮时加大点的大小
        if (_pointsGeom)
        {
            _pointsGeom->getOrCreateStateSet()->setAttribute(new osg::Point(7.0f));
        }
        // 草图高亮时加大线宽
        geom->getOrCreateStateSet()->setAttribute(new osg::LineWidth(2.0f));
        // 草图高亮时禁用深度测试确保不被遮挡住
        geom->getOrCreateStateSet()->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
    }
    else
    {
        // 草图取消高亮时恢复点的大小
        if (_pointsGeom)
        {
            _pointsGeom->getOrCreateStateSet()->setAttribute(new osg::Point(5.0f));
        }
        // 草图取消高亮时恢复线宽
        geom->getOrCreateStateSet()->setAttribute(new osg::LineWidth(1.2f));
        // 草图取消高亮时启用深度测试
        geom->getOrCreateStateSet()->setMode(GL_DEPTH_TEST, osg::StateAttribute::ON);
    }
}

void SketchElementNode::setActiveImpl(bool flag)
{
}

bool SketchElementNode::computeWhetherActive(const wydb::Element* pCurElem) const
{
    assert(pCurElem);
    return pCurElem->getParent().isNull();
}

unsigned int SketchElementNode::getCurveId(unsigned int primitiveIndex) const
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

unsigned int SketchElementNode::getCenterLineCurveId(unsigned int primitiveIndex) const
{
    if (_centerLineInfos.empty()) return 0;

    unsigned int count(0);
    for (const CurveInfo& info : _centerLineInfos)
    {
        count += info.numLines;
        if (primitiveIndex < count)
        {
            return info.id;
        }
    }

    return 0;
}

void SketchElementNode::highlightCurveByIndex(unsigned int curveIndex, bool flag)
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

void SketchElementNode::highlightCurveById(unsigned int id, bool flag)
{
    unsigned int index = static_cast<unsigned int>(-1);
    index = this->getCurveIndexById(id);
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

void SketchElementNode::previewCurveById(unsigned int id, bool flag)
{
    unsigned int index = static_cast<unsigned int>(-1);
    index = this->getCurveIndexById(id);
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

void SketchElementNode::clearDynamicRenderGeometry()
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
    for (CurveInfo& info : _centerLineInfos)
    {
        info.removeFlag(CurveInfoFlag::Highlight);
    }
}

osg::ref_ptr<osg::Geometry> SketchElementNode::generateCurveGeom_Highlight()
{
    if ((!_lineIndices || _lineIndices->empty()) && (!_centerLineIndices || _centerLineIndices->empty()))
    {
        return nullptr;
    }

    // 统计高亮索引总数（普通线 + 中心线）
    size_t totalNumIndices(0);
    for (const CurveInfo& curveInfo : _curveInfos)
    {
        if (curveInfo.hasFlag(CurveInfoFlag::Highlight))
            totalNumIndices += static_cast<size_t>(curveInfo.numLines) * 2;
    }
    for (const CurveInfo& curveInfo : _centerLineInfos)
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
    startIndex = 0;
    for (const CurveInfo& curveInfo : _centerLineInfos)
    {
        size_t numIndices = static_cast<size_t>(curveInfo.numLines) * 2;
        if (curveInfo.hasFlag(CurveInfoFlag::Highlight))
        {
            indices->insert(indices->end(),
                _centerLineIndices->begin() + startIndex,
                _centerLineIndices->begin() + startIndex + numIndices);
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

osg::ref_ptr<osg::Geometry> SketchElementNode::generateCurveGeom_Preview(unsigned int curveIndex)
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

unsigned int SketchElementNode::getCurveIndexById(unsigned int id) const
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

unsigned int SketchElementNode::getCenterLineCurveIndexById(unsigned int id) const
{
    unsigned int index(-1);
    for (const CurveInfo& info : _centerLineInfos)
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
    if (_curveGeomHighlight)
    {
        if (_curveNode) _curveNode->removeChild(_curveGeomHighlight);
        _curveGeomHighlight = nullptr;
    }

    if (curveIndex >= _centerLineInfos.size())
    {
        assert(false);
        return;
    }

    if (flag) _centerLineInfos[curveIndex].addFlag(CurveInfoFlag::Highlight);
    else _centerLineInfos[curveIndex].removeFlag(CurveInfoFlag::Highlight);

    osg::ref_ptr<osg::Geometry> curveGeomHighlight = this->generateCurveGeom_Highlight();
    if (curveGeomHighlight)
    {
        _curveGeomHighlight = curveGeomHighlight;
        if (_curveNode) _curveNode->addChild(_curveGeomHighlight);
    }
}

void SketchElementNode::previewCenterLineByIndex(unsigned int curveIndex, bool flag)
{
    if (_curveGeomPreview)
    {
        if (_curveNode) _curveNode->removeChild(_curveGeomPreview);
        _curveGeomPreview = nullptr;
    }

    if (curveIndex >= _centerLineInfos.size())
    {
        assert(false);
        return;
    }

    if (flag)
    {
        osg::ref_ptr<osg::Geometry> curveGeomPreview = this->generateCenterLineGeom_Preview(curveIndex);
        assert(curveGeomPreview);
        if (curveGeomPreview)
        {
            _curveGeomPreview = curveGeomPreview;
            if (_curveNode) _curveNode->addChild(_curveGeomPreview);
        }
    }
}

osg::ref_ptr<osg::Geometry> SketchElementNode::generateCenterLineGeom_Preview(unsigned int curveIndex)
{
    if (!_centerLineIndices || _centerLineIndices->empty() || _centerLineInfos.empty())
    {
        assert(false);
        return nullptr;
    }
    if (curveIndex >= _centerLineInfos.size())
    {
        assert(false);
        return nullptr;
    }

    osg::ref_ptr<osg::UIntArray> indices = new osg::UIntArray();
    size_t numIndices = static_cast<size_t>(_centerLineInfos[curveIndex].numLines) * 2;
    indices->reserve(numIndices);

    size_t startIndex = 0;
    for (size_t i = 0; i < _centerLineInfos.size(); ++i)
    {
        const CurveInfo& curveInfo = _centerLineInfos[i];
        numIndices = static_cast<size_t>(curveInfo.numLines) * 2;
        if (i == curveIndex)
        {
            indices->insert(indices->end(),
                _centerLineIndices->begin() + startIndex,
                _centerLineIndices->begin() + startIndex + numIndices);
            break;
        }
        startIndex += numIndices;
    }

    osg::ref_ptr<osg::Geometry> edgeGeom = new osg::Geometry();
    edgeGeom->setNodeMask(~PICK_MASK);
    edgeGeom->setUseDisplayList(false);
    edgeGeom->setUseVertexBufferObjects(true);
    edgeGeom->setVertexArray(_vertices);
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    colors->push_back(Colors::kEdge_Preview);
    edgeGeom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);
    edgeGeom->addPrimitiveSet(new osg::DrawElementsUInt(GL_LINES, indices->begin(), indices->end()));
    edgeGeom->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    edgeGeom->getOrCreateStateSet()->setAttribute(new osg::LineWidth(3.0f));
    edgeGeom->getOrCreateStateSet()->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
    edgeGeom->getOrCreateStateSet()->setRenderBinDetails(RenderBinNumers::Highlight, "RenderBin");

    return edgeGeom;
}