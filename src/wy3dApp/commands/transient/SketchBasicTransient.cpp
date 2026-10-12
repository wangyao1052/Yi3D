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
#include "commands/transient/SketchBasicTransient.h"
#include <osg/AutoTransform>
#include <osg/PositionAttitudeTransform>
#include <osg/LineWidth>
#include <osg/LineStipple>
#include "utils/MathUtils.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

CircleTransient::CircleTransient() : _vertexCnt(100)
{
    _geom = new osg::Geometry();
    _geom->setDataVariance(osg::Object::DYNAMIC);
    _geom->setUseDisplayList(false);
    _geom->setUseVertexBufferObjects(true);
    // 顶点数组
    _vertices = new osg::Vec3Array();
    _vertices->resize(_vertexCnt);
    this->fillVertices(osg::Vec3(0.0f, 0.0f, 0.0f), 1.0f);
    _geom->setVertexArray(_vertices);
    // 法向数组
    osg::ref_ptr<osg::Vec3Array> normals = new osg::Vec3Array();
    normals->push_back(osg::Vec3(0.0f, 0.0f, 1.0f));
    _geom->setNormalArray(normals, osg::Array::Binding::BIND_OVERALL);
    // 颜色数组
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    colors->push_back(osg::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
    _geom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);
    // 索引数组
    osg::ref_ptr<osg::UShortArray> indices = new osg::UShortArray();
    indices->resize(_vertexCnt * 2);
    for (size_t i = 0; i < _vertexCnt; ++i)
    {
        (*indices)[2 * i] = i;
        (*indices)[2 * i + 1] = (i + 1) % _vertexCnt;
    }

    // GL_TRIANGLES
    _geom->addPrimitiveSet(new osg::DrawElementsUShort(GL_LINES, indices->begin(), indices->end()));

    // 添加到根节点
    _root->addChild(_geom.get());
}

CircleTransient::~CircleTransient()
{
}

void CircleTransient::update(const wy::Vector2& pnt1, const wy::Vector2& pnt2, double z)
{
    float radius = (pnt2 - pnt1).length();
    this->fillVertices(osg::Vec3(pnt1.x(), pnt1.y(), z), radius);
    _vertices->dirty();
    _geom->dirtyBound();
}

void CircleTransient::fillVertices(const osg::Vec3& center, float radius)
{
    double delta = (wy3d::PI * 2) / _vertexCnt;
    float x(0.0), y(0.0);
    for (unsigned int i = 0; i < _vertexCnt; ++i)
    {
        x = std::cos(i * delta) * radius;
        y = std::sin(i * delta) * radius;
        (*_vertices)[i].set(center.x() + x, center.y() + y, center.z());
    }
}

SketchCircleTransient::SketchCircleTransient(const wy3d::Plane& sketchPlane)
    : _sketchPlane(sketchPlane), _vertexCnt(100)
{
    if (_sketchPlane.isValid())
    {
        _xDir = _sketchPlane.getXDir();
        _yDir = _sketchPlane.getYDir();
        _origin = _sketchPlane.getOrigin();
    }
    else
    {
        _xDir.set(0.0, 0.0, 0.0);
        _yDir.set(0.0, 0.0, 0.0);
        _origin = _sketchPlane.getOrigin();
    }

    _geom = new osg::Geometry();
    _geom->setDataVariance(osg::Object::DYNAMIC);
    _geom->setUseDisplayList(false);
    _geom->setUseVertexBufferObjects(true);
    // 顶点数组
    _vertices = new osg::Vec3Array();
    _vertices->resize(_vertexCnt);
    this->fillVertices(wy::Vector2(0.0, 0.0), 1.0f);
    _geom->setVertexArray(_vertices);
    // 法向数组
    osg::ref_ptr<osg::Vec3Array> normals = new osg::Vec3Array();
    normals->push_back(osg::Vec3(0.0f, 0.0f, 1.0f));
    _geom->setNormalArray(normals, osg::Array::Binding::BIND_OVERALL);
    // 颜色数组
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    colors->push_back(osg::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
    _geom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);
    // 索引数组
    osg::ref_ptr<osg::UShortArray> indices = new osg::UShortArray();
    indices->resize(_vertexCnt * 2);
    for (size_t i = 0; i < _vertexCnt; ++i)
    {
        (*indices)[2 * i] = i;
        (*indices)[2 * i + 1] = (i + 1) % _vertexCnt;
    }

    // 绘制线
    _geom->addPrimitiveSet(new osg::DrawElementsUShort(GL_LINES, indices->begin(), indices->end()));

    // 添加到根节点
    _root->addChild(_geom.get());
}

SketchCircleTransient::~SketchCircleTransient()
{
}

void SketchCircleTransient::update(const wy::Vector2& center, float radius)
{
    this->fillVertices(center, radius);
    _vertices->dirty();
    _geom->dirtyBound();
}

void SketchCircleTransient::fillVertices(const wy::Vector2& center, float radius)
{
    double delta = (wy3d::PI * 2) / _vertexCnt;
    float u(0.0), v(0.0);
    for (unsigned int i = 0; i < _vertexCnt; ++i)
    {
        u = std::cos(i * delta) * radius + center.x();
        v = std::sin(i * delta) * radius + center.y();
        (*_vertices)[i].set(
            u * _xDir.x() + v * _yDir.x() + _origin.x(),
            u * _xDir.y() + v * _yDir.y() + _origin.y(),
            u * _xDir.z() + v * _yDir.z() + _origin.z());
    }
}

SketchArcTransient::SketchArcTransient(const wy3d::Plane& sketchPlane, const osg::Vec4& color, float lineWidth)
    : _sketchPlane(sketchPlane), _vertexCnt(100)
{
    if (_sketchPlane.isValid())
    {
        _xDir = _sketchPlane.getXDir();
        _yDir = _sketchPlane.getYDir();
        _origin = _sketchPlane.getOrigin();
    }
    else
    {
        _xDir.set(0.0, 0.0, 0.0);
        _yDir.set(0.0, 0.0, 0.0);
        _origin = _sketchPlane.getOrigin();
    }

    // 几何
    _geom = new osg::Geometry();
    _geom->setDataVariance(osg::Object::DYNAMIC);
    _geom->setUseDisplayList(false);
    _geom->setUseVertexBufferObjects(true);
    // 顶点数组
    _vertices = new osg::Vec3Array();
    _vertices->resize(_vertexCnt);
    this->fillVertices(wy::Vector2(0.0, 0.0), 1.0, 0.0, wy3d::TWO_PI);
    _geom->setVertexArray(_vertices);
    // 法向数组
    osg::ref_ptr<osg::Vec3Array> normals = new osg::Vec3Array();
    normals->push_back(osg::Vec3(0.0f, 0.0f, 1.0f));
    _geom->setNormalArray(normals, osg::Array::Binding::BIND_OVERALL);
    // 颜色数组
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    colors->push_back(color);
    _geom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);
    // 索引数组
    osg::ref_ptr<osg::UShortArray> indices = new osg::UShortArray();
    indices->resize((_vertexCnt - 1) * 2);
    for (size_t i = 0; i < _vertexCnt - 1; ++i)
    {
        (*indices)[2 * i] = i;
        (*indices)[2 * i + 1] = i + 1;
    }
    // 绘制线
    _geom->addPrimitiveSet(new osg::DrawElementsUShort(GL_LINES, indices->begin(), indices->end()));
    // 线宽
    _geom->getOrCreateStateSet()->setAttribute(new osg::LineWidth(lineWidth));

    // 添加到根节点
    _root->addChild(_geom.get());
}

SketchArcTransient::~SketchArcTransient()
{
}

void SketchArcTransient::update(const wy::Vector2& center, double radius, double startAngle, double endAngle)
{
    this->fillVertices(center, radius, startAngle, endAngle);
    _vertices->dirty();
    _geom->dirtyBound();
}

void SketchArcTransient::fillVertices(const wy::Vector2& center, float radius, float startAngle, float endAngle)
{
    startAngle = wy3d::normalizeRadian(startAngle);
    endAngle = wy3d::normalizeRadian(endAngle);
    if (endAngle < startAngle) endAngle += wy3d::TWO_PI;
    float totalAngle = endAngle - startAngle;
    assert(totalAngle >= 0 && totalAngle < wy3d::TWO_PI);

    double delta = totalAngle / (_vertexCnt - 1);
    double angle = startAngle;
    float u(0.0), v(0.0);
    for (size_t i = 0; i < _vertexCnt; ++i)
    {
        angle = startAngle + i * delta;
        u = std::cos(angle) * radius + center.x();
        v = std::sin(angle) * radius + center.y();
        (*_vertices)[i].set(
            u * _xDir.x() + v * _yDir.x() + _origin.x(),
            u * _xDir.y() + v * _yDir.y() + _origin.y(),
            u * _xDir.z() + v * _yDir.z() + _origin.z());
    }
}

SketchEllipseTransient::SketchEllipseTransient(const wy3d::Plane& sketchPlane)
    : _sketchPlane(sketchPlane), _vertexCnt(100)
{
    if (_sketchPlane.isValid())
    {
        _xDir = _sketchPlane.getXDir();
        _yDir = _sketchPlane.getYDir();
        _origin = _sketchPlane.getOrigin();
    }
    else
    {
        _xDir.set(0.0, 0.0, 0.0);
        _yDir.set(0.0, 0.0, 0.0);
        _origin = _sketchPlane.getOrigin();
    }

    _geom = new osg::Geometry();
    _geom->setDataVariance(osg::Object::DYNAMIC);
    _geom->setUseDisplayList(false);
    _geom->setUseVertexBufferObjects(true);
    // 顶点数组
    _vertices = new osg::Vec3Array();
    _vertices->resize(_vertexCnt);
    this->fillVertices(wy::Vector2(0.0, 0.0), wy::Vector2(1.0, 0.0), 1.0);
    _geom->setVertexArray(_vertices);
    // 法向数组
    osg::ref_ptr<osg::Vec3Array> normals = new osg::Vec3Array();
    normals->push_back(osg::Vec3(0.0f, 0.0f, 1.0f));
    _geom->setNormalArray(normals, osg::Array::Binding::BIND_OVERALL);
    // 颜色数组
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    colors->push_back(osg::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
    _geom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);
    // 索引数组
    osg::ref_ptr<osg::UShortArray> indices = new osg::UShortArray();
    indices->resize(_vertexCnt * 2);
    for (size_t i = 0; i < _vertexCnt; ++i)
    {
        (*indices)[2 * i] = i;
        (*indices)[2 * i + 1] = (i + 1) % _vertexCnt;
    }
    // 绘制线
    _geom->addPrimitiveSet(new osg::DrawElementsUShort(GL_LINES, indices->begin(), indices->end()));

    // 添加到根节点
    _root->addChild(_geom.get());
}

SketchEllipseTransient::~SketchEllipseTransient()
{
}

void SketchEllipseTransient::update(
    const wy::Vector2& center,
    const wy::Vector2& majorAxis,
    double radiusRatio)
{
    this->fillVertices(center, majorAxis, radiusRatio);
    _vertices->dirty();
    _geom->dirtyBound();
}

void SketchEllipseTransient::fillVertices(
    const wy::Vector2& center,
    const wy::Vector2& majorAxis,
    double radiusRatio)
{
    double majorRadius = majorAxis.length();
    double minorRadius = majorRadius * radiusRatio;

    // 计算长轴与X轴的夹角
    double angle = std::atan2(majorAxis.y(), majorAxis.x());
    double cosAngle = std::cos(angle);
    double sinAngle = std::sin(angle);

    double delta = (wy3d::PI * 2) / _vertexCnt;
    double u(0.0), v(0.0);
    for (unsigned int i = 0; i < _vertexCnt; ++i)
    {
        // 计算椭圆的每个点（长轴和短轴）
        double x = std::cos(i * delta) * majorRadius;
        double y = std::sin(i * delta) * minorRadius;

        // 使用旋转矩阵旋转点
        double xRot = x * cosAngle - y * sinAngle;
        double yRot = x * sinAngle + y * cosAngle;

        // 计算旋转后的点的位置并添加到顶点数组
        u = xRot + center.x();
        v = yRot + center.y();

        (*_vertices)[i].set(
            u * _xDir.x() + v * _yDir.x() + _origin.x(),
            u * _xDir.y() + v * _yDir.y() + _origin.y(),
            u * _xDir.z() + v * _yDir.z() + _origin.z());
    }
}

SketchCsysTransient::SketchCsysTransient(const wy3d::Plane& sketchPlane)
    : _initSketchPlane(sketchPlane)
{
    osg::ref_ptr<osg::AutoTransform> csys = new osg::AutoTransform();
    csys->setAutoRotateMode(osg::AutoTransform::NO_ROTATION);
    csys->setAutoScaleToScreen(true);
    
    // 正向X,Y轴
    {
        osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
        geom->setUseDisplayList(false);
        geom->setUseVertexBufferObjects(true);
        csys->addChild(geom.get());
        // vertex array
        osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
        geom->setVertexArray(vertices.get());
        vertices->reserve(4);
        vertices->push_back(osg::Vec3(0.0, 0.0, 0.0));
        vertices->push_back(osg::Vec3(200.0, 0.0, 0.0));
        vertices->push_back(osg::Vec3(0.0, 0.0, 0.0));
        vertices->push_back(osg::Vec3(0.0, 200.0, 0.0));
        // color array
        osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
        colors->reserve(4);
        float alpha = 0.5f;
        colors->push_back(osg::Vec4(0.7f, 0.0f, 0.0f, alpha));
        colors->push_back(osg::Vec4(0.7f, 0.0f, 0.0f, alpha));
        colors->push_back(osg::Vec4(0.0f, 0.7f, 0.0f, alpha));
        colors->push_back(osg::Vec4(0.0f, 0.7f, 0.0f, alpha));
        geom->setColorArray(colors.get(), osg::Array::Binding::BIND_PER_VERTEX);
        // primitive set
        geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::LINES, 0, 4));
    }
    // 反向X,Y轴
    {
        osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
        geom->setDataVariance(osg::Object::DYNAMIC);
        geom->setUseDisplayList(false);
        geom->setUseVertexBufferObjects(true);
        csys->addChild(geom.get());
        // vertex array
        osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
        geom->setVertexArray(vertices.get());
        vertices->reserve(4);
        vertices->push_back(osg::Vec3(0.0, 0.0, 0.0));
        vertices->push_back(osg::Vec3(-200.0, 0.0, 0.0));
        vertices->push_back(osg::Vec3(0.0, 0.0, 0.0));
        vertices->push_back(osg::Vec3(0.0, -200.0, 0.0));
        // color array
        osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
        colors->reserve(4);
        float alpha = 0.5f;
        colors->push_back(osg::Vec4(0.7f, 0.0f, 0.0f, alpha));
        colors->push_back(osg::Vec4(0.7f, 0.0f, 0.0f, alpha));
        colors->push_back(osg::Vec4(0.0f, 0.7f, 0.0f, alpha));
        colors->push_back(osg::Vec4(0.0f, 0.7f, 0.0f, alpha));
        geom->setColorArray(colors.get(), osg::Array::Binding::BIND_PER_VERTEX);
        // primitive set
        geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::LINES, 0, 4));
        geom->getOrCreateStateSet()->setAttributeAndModes(
            new osg::LineStipple(2, 0xAAAA), osg::StateAttribute::ON);
    }
    _csys = csys;
    _root->addChild(csys.get());

    this->update(_initSketchPlane);
}

SketchCsysTransient::~SketchCsysTransient()
{
}

void SketchCsysTransient::update(const wy::Vector2& origin, double angle)
{
    wy::Vector3 newOrigin = _initSketchPlane.value(origin);
    wy::Vector3 newXDir = _initSketchPlane.value(std::cos(angle), std::sin(angle)) - _initSketchPlane.getOrigin();
    newXDir.normalize();
    wy3d::Plane plane(newOrigin, _initSketchPlane.getNormal(), newXDir);
    this->update(plane);
}

void SketchCsysTransient::update(const wy3d::Plane& plane)
{
    // 平移
    _csys->setPosition(MathUtils::toVec3d(plane.getOrigin()));

    // 旋转
    wy::Vector3 xDir = plane.getXDir();
    wy::Vector3 yDir = plane.getYDir();
    wy::Vector3 zDir = plane.getNormal();
    osg::Matrix matrix;
    matrix.set(
        xDir.x(), yDir.x(), zDir.x(), 0,
        xDir.y(), yDir.y(), zDir.y(), 0,
        xDir.z(), yDir.z(), zDir.z(), 0,
        0.0, 0.0, 0.0, 1.0);
    matrix.transpose(matrix); // 转置矩阵以适配OSG的列优先存储
    osg::Quat quat = matrix.getRotate();
    _csys->setRotation(quat);
}

} // namespace wy3dApp
