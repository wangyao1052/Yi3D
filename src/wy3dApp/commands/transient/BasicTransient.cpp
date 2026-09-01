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

#include <wyVector2.h>
#include <wyVector3.h>
#include "BasicTransient.h"
#include <osg/Point>
#include <osg/AutoTransform>
#include "snap/SnapConsts.h"
#include "scene/Colors.h"
#include "utils/MathUtils.h"

LineTransient::LineTransient(
    osg::ref_ptr<osg::LineStipple> lineStipple,
    osg::ref_ptr<osg::LineWidth> lineWidth,
    const osg::Vec4& color) : _lineStipple(lineStipple), _lineWidth(lineWidth)
{
    _geom = new osg::Geometry();
    _geom->setDataVariance(osg::Object::DYNAMIC);
    _geom->setUseDisplayList(false);
    _geom->setUseVertexBufferObjects(true);
    // 顶点数组
    _vertices = new osg::Vec3Array();
    _vertices->resize(2);
    (*_vertices)[0] = osg::Vec3(0.0f, 0.0f, 0.0f);
    (*_vertices)[1] = osg::Vec3(1.0f, 0.0f, 0.0f);
    _geom->setVertexArray(_vertices);
    // 法向数组
    osg::ref_ptr<osg::Vec3Array> normals = new osg::Vec3Array();
    normals->push_back(osg::Vec3(0.0f, 0.0f, 1.0f));
    _geom->setNormalArray(normals, osg::Array::Binding::BIND_OVERALL);
    // 颜色数组
    _colors = new osg::Vec4Array();
    _colors->push_back(color);
    _geom->setColorArray(_colors, osg::Array::Binding::BIND_OVERALL);
    // 索引数组
    osg::ref_ptr<osg::UShortArray> indices = new osg::UShortArray();
    indices->resize(2);
    (*indices)[0] = 0;
    (*indices)[1] = 1;
    // 绘制线
    _geom->addPrimitiveSet(new osg::DrawElementsUShort(GL_LINES, indices->begin(), indices->end()));
    // 线宽度
    if (_lineWidth)
    {
        _geom->getOrCreateStateSet()->setAttribute(_lineWidth);
    }
    // 线样式
    if (_lineStipple)
    {
        _geom->getOrCreateStateSet()->setAttributeAndModes(_lineStipple, osg::StateAttribute::ON);
    }
    // 添加到根节点
    _root->addChild(_geom.get());
}

LineTransient::~LineTransient()
{
}

void LineTransient::update(const wy3d::SketchPlane& plane,
    const wy::Vector2& startPnt2d, const wy::Vector2& endPnt2d)
{
    wy::Vector3 startPnt = plane.value(startPnt2d);
    wy::Vector3 endPnt = plane.value(endPnt2d);
    (*_vertices)[0].set(startPnt.x(), startPnt.y(), startPnt.z());
    (*_vertices)[1].set(endPnt.x(), endPnt.y(), endPnt.z());
    _vertices->dirty();
    _geom->dirtyBound();
}

void LineTransient::update(const wy::Vector3& pnt1, const wy::Vector3& pnt2)
{
    (*_vertices)[0].set(pnt1.x(), pnt1.y(), pnt1.z());
    (*_vertices)[1].set(pnt2.x(), pnt2.y(), pnt2.z());
    _vertices->dirty();
    _geom->dirtyBound();
}

void LineTransient::setColor(const osg::Vec4& color)
{
    _colors->assign(_colors->size(), color);
    _colors->dirty();
}

PointTransient::PointTransient(const wy::Vector3& pnt, const osg::Vec4& color, float pointSize)
{
    _geom = new osg::Geometry();
    _geom->setDataVariance(osg::Object::DYNAMIC);
    _geom->setUseDisplayList(false);
    _geom->setUseVertexBufferObjects(true);
    // 顶点数组
    _vertices = new osg::Vec3Array();
    _vertices->resize(1);
    (*_vertices)[0] = osg::Vec3(pnt.x(), pnt.y(), pnt.z());
    _geom->setVertexArray(_vertices);
    // 颜色数组
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    colors->push_back(color);
    _geom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);
    // 绘制点
    _geom->addPrimitiveSet(new osg::DrawArrays(GL_POINTS, 0, 1));
    // 点大小
    _geom->getOrCreateStateSet()->setAttribute(new osg::Point(pointSize));
    // 添加到根节点
    _root->addChild(_geom.get());
}

PointTransient::~PointTransient()
{
}

CenterPointTransient::CenterPointTransient() : _len(8.0f)
{
    _pat = new osg::PositionAttitudeTransform();
    {
        osg::ref_ptr<osg::AutoTransform> at = new osg::AutoTransform();
        at->setAutoRotateMode(osg::AutoTransform::NO_ROTATION);
        at->setAutoScaleToScreen(true);
        _pat->addChild(at);

        osg::ref_ptr<osg::Geometry>  geom = new osg::Geometry();
        geom->setUseDisplayList(false);
        geom->setUseVertexBufferObjects(true);
        at->addChild(geom);
        // 顶点数组
        osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
        vertices->resize(4);
        (*vertices)[0] = osg::Vec3(-_len, 0.0f, 0.0f);
        (*vertices)[1] = osg::Vec3(_len, 0.0f, 0.0f);
        (*vertices)[2] = osg::Vec3(0.0f, -_len, 0.0f);
        (*vertices)[3] = osg::Vec3(0.0f, _len, 0.0f);
        geom->setVertexArray(vertices);
        // 颜色数组
        osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
        colors->push_back(osg::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
        geom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);
        // 索引数组
        osg::ref_ptr<osg::UShortArray> indices = new osg::UShortArray();
        indices->resize(4);
        (*indices)[0] = 0;
        (*indices)[1] = 1;
        (*indices)[2] = 2;
        (*indices)[3] = 3;
        // 绘制线
        geom->addPrimitiveSet(new osg::DrawElementsUShort(GL_LINES, indices->begin(), indices->end()));
    }
    // 添加到根节点
    _root->addChild(_pat);
}

CenterPointTransient::~CenterPointTransient()
{
}

void CenterPointTransient::update(const wy3d::SketchPlane& plane, const wy::Vector2& position)
{
    assert(_pat);
    _pat->setPosition(MathUtils::toVec3(plane.value(position)));
    _pat->setAttitude(MathUtils::computeQuat(plane));
}