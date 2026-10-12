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

#ifndef WY3DAPP_SKETCH_BASIC_TRANSIENT_H
#define WY3DAPP_SKETCH_BASIC_TRANSIENT_H

#include <memory>
#include <osg/AutoTransform>
#include <wyVector2.h>
#include <wyVector3.h>
#include <wy3dVector2.h>
#include <wy3dPlane.h>

#include <wy3dApp/GuiCommand.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class CircleTransient : public GuiCmdTransient
{
public:
    CircleTransient();
    ~CircleTransient();

    void update(const wy::Vector2& pnt1, const wy::Vector2& pnt2, double z);

private:
    void fillVertices(const osg::Vec3& center, float radius);

private:
    osg::ref_ptr<osg::Geometry> _geom;
    osg::ref_ptr<osg::Vec3Array> _vertices;
    size_t _vertexCnt;
};

class SketchCircleTransient : public GuiCmdTransient
{
public:
    SketchCircleTransient(const wy3d::Plane& sketchPlane);
    ~SketchCircleTransient();

    void update(const wy::Vector2& center, float radius);

private:
    void fillVertices(const wy::Vector2& center, float radius);

private:
    wy3d::Plane _sketchPlane;
    wy::Vector3 _origin;
    wy::Vector3 _xDir;
    wy::Vector3 _yDir;
    osg::ref_ptr<osg::Geometry> _geom;
    osg::ref_ptr<osg::Vec3Array> _vertices;
    size_t _vertexCnt;
};
typedef std::shared_ptr<SketchCircleTransient> SketchCircleTransientSPtr;

class SketchArcTransient : public GuiCmdTransient
{
public:
    SketchArcTransient(const wy3d::Plane& sketchPlane, const osg::Vec4& color, float lineWidth);
    ~SketchArcTransient();

    void update(const wy::Vector2& center, double radius, double startAngle, double endAngle);

private:
    void fillVertices(const wy::Vector2& center, float radius, float startAngle, float endAngle);

private:
    wy3d::Plane _sketchPlane;
    wy::Vector3 _origin;
    wy::Vector3 _xDir;
    wy::Vector3 _yDir;
    osg::ref_ptr<osg::Geometry> _geom;
    osg::ref_ptr<osg::Vec3Array> _vertices;
    size_t _vertexCnt;
};
typedef std::shared_ptr<SketchArcTransient> SketchArcTransientSPtr;

class SketchEllipseTransient : public GuiCmdTransient
{
public:
    SketchEllipseTransient(const wy3d::Plane& sketchPlane);
    ~SketchEllipseTransient();

    void update(const wy::Vector2& center, const wy::Vector2& majorAxis, double radiusRatio);

private:
    void fillVertices(const wy::Vector2& center, const wy::Vector2& majorAxis, double radiusRatio);

private:
    wy3d::Plane _sketchPlane;
    wy::Vector3 _origin;
    wy::Vector3 _xDir;
    wy::Vector3 _yDir;
    osg::ref_ptr<osg::Geometry> _geom;
    osg::ref_ptr<osg::Vec3Array> _vertices;
    size_t _vertexCnt;
};

class SketchCsysTransient : public GuiCmdTransient
{
public:
    SketchCsysTransient(const wy3d::Plane& sketchPlane);
    ~SketchCsysTransient();

    // ����ڳ�ʼ��ͼƽ���λ�ƺ���ת
    void update(const wy::Vector2& origin, double angle);

private:
    void update(const wy3d::Plane& plane);

private:
    wy3d::Plane _initSketchPlane;
    osg::ref_ptr<osg::AutoTransform> _csys;
};
typedef std::shared_ptr<SketchCsysTransient> SketchCsysTransientSPtr;

}

#endif // WY3DAPP_SKETCH_BASIC_TRANSIENT_H
