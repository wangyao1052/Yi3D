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

#ifndef WY3DAPP_BASIC_TRANSIENT_H
#define WY3DAPP_BASIC_TRANSIENT_H

#include <osg/PositionAttitudeTransform>
#include <osg/LineStipple>
#include <osg/LineWidth>
#include <wyVector2.h>
#include <wyVector3.h>
#include <wy3dVector2.h>
#include <wy3dVector3.h>
#include <wy3dSketchPlane.h>
#include "GuiCmdTransient.h"

class LineTransient : public GuiCmdTransient
{
public:
    LineTransient(
        osg::ref_ptr<osg::LineStipple> lineStipple = nullptr,
        osg::ref_ptr<osg::LineWidth> lineWidth = nullptr,
        const osg::Vec4& color = osg::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
    ~LineTransient();

    void update(const wy3d::SketchPlane& plane,
        const wy::Vector2& startPnt2d, const wy::Vector2& endPnt2d);
    void update(const wy::Vector3& pnt1, const wy::Vector3& pnt2);
    void setColor(const osg::Vec4& color);

private:
    osg::ref_ptr<osg::Geometry> _geom;
    osg::ref_ptr<osg::Vec3Array> _vertices;
    osg::ref_ptr<osg::Vec4Array> _colors;
    osg::ref_ptr<osg::LineStipple> _lineStipple;
    osg::ref_ptr<osg::LineWidth> _lineWidth;
};
typedef std::shared_ptr<LineTransient> LineTransientSPtr;

class PointTransient : public GuiCmdTransient
{
public:
    PointTransient(const wy::Vector3& pnt, const osg::Vec4& color, float pointSize);
    ~PointTransient();

private:
    osg::ref_ptr<osg::Geometry> _geom;
    osg::ref_ptr<osg::Vec3Array> _vertices;
};
typedef std::shared_ptr<PointTransient> PointTransientSPtr;

class CenterPointTransient : public GuiCmdTransient
{
public:
    CenterPointTransient();
    ~CenterPointTransient();

    void update(const wy3d::SketchPlane& plane, const wy::Vector2& position);

private:
    float _len;
    osg::ref_ptr<osg::PositionAttitudeTransform> _pat;
};
typedef std::shared_ptr<CenterPointTransient> CenterPointTransientSPtr;

#endif // WY3DAPP_BASIC_TRANSIENT_H