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

#include "SketchCurveTransient.h"

#include <cassert>
#include <osg/LineWidth>
#include <wyVector2.h>
#include <wydbDatabase.h>
#include <wy3dSketch.h>
#include <wy3dSketchCurve.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>
#include <wy3dSketchEllipse.h>
#include <wy3dSketchEllipseArc.h>
#include <wy3dSketchSpline.h>
#include <wy3dSketchSpline.h>

#include "utils/MathUtils.h"
#include "scene/SketchEntityLinearization.h"
#include <wy3dApp/Application.h>
#include "scene/RenderConst.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

SketchCurveTransient::SketchCurveTransient(const wydb::ElementId& id, double startParam, double endParam)
    : _id(id), _startParam(startParam), _endParam(endParam)
{
    assert(!_id.isNull());
    const wy3d::SketchCurve* pCurve = this->getCurve(_id);
    assert(pCurve);
    if (pCurve)
    {
        this->init(pCurve, startParam, endParam);
    }
}

SketchCurveTransient::SketchCurveTransient(const wy3d::SketchCurve* pCurve, double startParam, double endParam)
    : _id(wydb::ElementId::kNull), _startParam(startParam), _endParam(endParam)
{
    assert(pCurve);
    _id = pCurve->getId();
    this->init(pCurve, startParam, endParam);
}

SketchCurveTransient::SketchCurveTransient(const wy3d::Plane& sketchPlane,
    const wy::Vector2& center, double radius, double startAngle, double endAngle)
    : _id(wydb::ElementId::kNull), _startParam(0.0), _endParam(1.0)
{
    this->init(sketchPlane, center, radius, startAngle, endAngle);
}

SketchCurveTransient::SketchCurveTransient(const wy3d::Plane& sketchPlane,
    const wy::Vector2& startPnt, const wy::Vector2& endPnt,
    osg::ref_ptr<osg::LineStipple> lineStipple,
    osg::ref_ptr<osg::LineWidth> lineWidth)
    : _id(wydb::ElementId::kNull), _startParam(0.0), _endParam(1.0), _lineStipple(lineStipple), _lineWidth(lineWidth)
{
    this->init(sketchPlane, startPnt, endPnt, 0.0, 1.0);
}

bool SketchCurveTransient::init(const wy3d::SketchCurve* pCurve, double startParam, double endParam)
{
    assert(pCurve);
    const wydb::Database* pDb = pCurve->getDatabase();
    assert(pDb);
    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(pCurve->getParent()));
    assert(pSketch);
    if (!pSketch) return false;
    const wy3d::Plane& sketchPlane = pSketch->getPlane();

    std::shared_ptr<SketchEntityLinearization> pLinearization;
    if (const wy3d::SketchLine* pLine = wy3d::SketchLine::cast(pCurve))
    {
        wy::Vector2 lineStartPnt = pLine->getStartPoint();
        wy::Vector2 lineVec = pLine->getEndPoint() - lineStartPnt;
        wy::Vector2 startPnt = lineStartPnt + lineVec * startParam;
        wy::Vector2 endPnt = lineStartPnt + lineVec * endParam;
        pLinearization = std::make_shared<SketchEntityLinearization>(startPnt, endPnt);
    }
    else if (const wy3d::SketchCenterLine* pCenterLine = wy3d::SketchCenterLine::cast(pCurve))
    {
        wy::Vector2 lineStartPnt = pCenterLine->getStartPoint();
        wy::Vector2 lineVec = pCenterLine->getEndPoint() - lineStartPnt;
        wy::Vector2 startPnt = lineStartPnt + lineVec * startParam;
        wy::Vector2 endPnt = lineStartPnt + lineVec * endParam;
        pLinearization = std::make_shared<SketchEntityLinearization>(startPnt, endPnt);
        _lineStipple = new osg::LineStipple(CENTER_LINE_STIPPLE_FACTOR, CENTER_LINE_STIPPLE_PATTERN);
    }
    else if (const wy3d::SketchCircle* pCircle = wy3d::SketchCircle::cast(pCurve))
    {
        if (startParam == 0.0 && endParam == 1.0)
        {
            pLinearization = std::make_shared<SketchEntityLinearization>(pCircle->getCenter(), pCircle->getRadius());
        }
        else
        {
            double startAngle = wy3d::normalizeRadian(startParam * wy3d::TWO_PI);
            double endAngle = wy3d::normalizeRadian(endParam * wy3d::TWO_PI);
            if (endAngle < startAngle) endAngle += wy3d::TWO_PI;
            pLinearization = std::make_shared<SketchEntityLinearization>(pCircle->getCenter(), pCircle->getRadius(), startAngle, endAngle);
        }
    }
    else if (const wy3d::SketchArc* pArc = wy3d::SketchArc::cast(pCurve))
    {
            double startAngle = wy3d::normalizeRadian(pArc->getStartAngle());
            double totalAngle = pArc->getTotalAngle();
            pLinearization = std::make_shared<SketchEntityLinearization>(pArc->getCenter(), pArc->getRadius(),
                startAngle + startParam * totalAngle, startAngle + endParam * totalAngle);
    }
    else if (const wy3d::SketchEllipse* pEllipse = wy3d::SketchEllipse::cast(pCurve))
    {
        if (startParam == 0.0 && endParam == 1.0)
        {
            pLinearization = std::make_shared<SketchEntityLinearization>(pEllipse->getCenter(), pEllipse->getMajorAxis(), pEllipse->getRadiusRatio());
        }
        else
        {
            pLinearization = std::make_shared<SketchEntityLinearization>(pEllipse->getCenter(), pEllipse->getMajorAxis(), pEllipse->getRadiusRatio(),
                startParam * wy3d::TWO_PI, endParam * wy3d::TWO_PI);
        }
    }
    else if (const wy3d::SketchEllipseArc* pEllipseArc = wy3d::SketchEllipseArc::cast(pCurve))
    {
        double startAngle = wy3d::normalizeRadian(pEllipseArc->getStartAngle());
        double totalAngle = pEllipseArc->getTotalAngle();
        pLinearization = std::make_shared<SketchEntityLinearization>(pEllipseArc->getCenter(), pEllipseArc->getMajorAxis(), pEllipseArc->getRadiusRatio(),
            startAngle + startParam * totalAngle, startAngle + endParam * totalAngle);
    }
    else if (const wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pCurve))
    {        pLinearization = std::make_shared<SketchEntityLinearization>(pSpline->getOccSpline());
    }
    else
    {
        assert(false);
        return false;
    }

    if (!pLinearization) return false;
    this->initGeom(sketchPlane, pLinearization.get());
    return true;
}

bool SketchCurveTransient::init(const wy3d::Plane& sketchPlane, const wy::Vector2& startPnt, const wy::Vector2& endPnt, double startParam, double endParam)
{
    wy::Vector2 lineVec = endPnt - startPnt;
    std::shared_ptr<SketchEntityLinearization> pLinearization = std::make_shared<SketchEntityLinearization>(
        startPnt + lineVec * startParam, startPnt + lineVec * endParam);
    this->initGeom(sketchPlane, pLinearization.get());
    return true;
}

bool SketchCurveTransient::init(const wy3d::Plane& sketchPlane, const wy::Vector2& center, double radius, double startAngle, double endAngle)
{
    startAngle = wy3d::normalizeRadian(startAngle);
    endAngle = wy3d::normalizeRadian(endAngle);
    if (endAngle < startAngle) endAngle += wy3d::TWO_PI;
    std::shared_ptr<SketchEntityLinearization> pLinearization = std::make_shared<SketchEntityLinearization>(center, radius, startAngle, endAngle);
    this->initGeom(sketchPlane, pLinearization.get());
    return true;
}

void SketchCurveTransient::initGeom(const wy3d::Plane& sketchPlane, SketchEntityLinearization* pLinearization)
{
    if (!pLinearization) return;
    const std::vector<wy::Vector2>& vertices2 = pLinearization->getVertices();
    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
    vertices->reserve(vertices2.size());
    for (const wy::Vector2& uv : vertices2)
    {
        vertices->push_back(MathUtils::toVec3(sketchPlane.value(uv)));
    }
    this->initGeom(vertices, pLinearization->getIndices());
    return;
}

void SketchCurveTransient::initGeom(osg::ref_ptr<osg::Vec3Array> vertices, const std::vector<unsigned int>& indices)
{
    if (!vertices) return;
    if (vertices->empty() || indices.empty()) return;

    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
    geom->setUseDisplayList(false);
    geom->setUseVertexBufferObjects(true);
    geom->setNodeMask(~PICK_MASK); // 不可拾取
    // 顶点数组
    geom->setVertexArray(vertices);
    // 法向数组
    osg::ref_ptr<osg::Vec3Array> normals = new osg::Vec3Array();
    normals->push_back(osg::Vec3(0.0f, 0.0f, 1.0f));
    geom->setNormalArray(normals, osg::Array::Binding::BIND_OVERALL);
    // 颜色数组
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    colors->push_back(osg::Vec4(1.0f, 0.392f, 0.039f, 1.0f));
    geom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);
    // GL_LINES
    geom->addPrimitiveSet(new osg::DrawElementsUShort(GL_LINES, indices.cbegin(), indices.cend()));
    // 加粗
    geom->getOrCreateStateSet()->setAttribute(_lineWidth.get() ? _lineWidth.get() : new osg::LineWidth(3.0));
    // 线样式
    if (_lineStipple)
    {
        geom->getOrCreateStateSet()->setAttributeAndModes(_lineStipple, osg::StateAttribute::ON);
    }
    // 添加到根节点
    _root->addChild(geom.get());
}

const wy3d::SketchCurve* SketchCurveTransient::getCurve(const wydb::ElementId& id)
{
    const wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return nullptr;
    const wy3d::SketchCurve* pCurve = wy3d::SketchCurve::cast(pDb->getElement(id));
    return pCurve;
}

SketchDotLineTransient::SketchDotLineTransient(
    const wy3d::Plane& sketchPlane, const wy::Vector2& startPnt, const wy::Vector2& endPnt)
    : _sketchPlane(sketchPlane)
{
    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
    geom->setDataVariance(osg::Object::DYNAMIC);
    geom->setUseDisplayList(false);
    geom->setUseVertexBufferObjects(true);
    // 顶点数组
    _vertices = new osg::Vec3Array();
    _vertices->resize(2);
    (*_vertices)[0] = MathUtils::toVec3(sketchPlane.value(startPnt));
    (*_vertices)[1] = MathUtils::toVec3(sketchPlane.value(endPnt));
    geom->setVertexArray(_vertices);
    // 法向数组
    osg::ref_ptr<osg::Vec3Array> normals = new osg::Vec3Array();
    normals->push_back(osg::Vec3(0.0f, 0.0f, 1.0f));
    geom->setNormalArray(normals, osg::Array::Binding::BIND_OVERALL);
    // 颜色数组
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    colors->push_back(osg::Vec4(1.0f, 0.392f, 0.039f, 1.0f));
    geom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);
    // 索引数组
    osg::ref_ptr<osg::UShortArray> indices = new osg::UShortArray();
    indices->resize(2);
    (*indices)[0] = 0;
    (*indices)[1] = 1;
    // 绘制线
    geom->addPrimitiveSet(new osg::DrawElementsUShort(GL_LINES, indices->begin(), indices->end()));
    // 点划线
    geom->getOrCreateStateSet()->setAttributeAndModes(
        new osg::LineStipple(DOT_LINE_STIPPLE_FACTOR, DOT_LINE_STIPPLE_PATTERN), osg::StateAttribute::ON);
    // 添加到根节点
    _root->addChild(geom.get());
    _geom = geom;
}

void SketchDotLineTransient::update(const wy::Vector2& startPnt, const wy::Vector2& endPnt)
{
    (*_vertices)[0] = MathUtils::toVec3(_sketchPlane.value(startPnt));
    (*_vertices)[1] = MathUtils::toVec3(_sketchPlane.value(endPnt));
    _vertices->dirty();
    _geom->dirtyBound();
}

}
