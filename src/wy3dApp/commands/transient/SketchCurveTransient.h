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

#ifndef WY3DAPP_SKETCH_CURVE_TRANSIENT_H
#define WY3DAPP_SKETCH_CURVE_TRANSIENT_H

#include <vector>
#include <memory>
#include <osg/Geometry>
#include <osg/Array>
#include <osg/LineStipple>
#include <osg/LineWidth>
#include <wyVector2.h>
#include <wy3dVector2.h>
#include <wydbElementId.h>
#include <wy3dSketchCurve.h>
#include <wy3dPlane.h>

#include "GuiCmdTransient.h"

class SketchEntityLinearization;

// 草图曲线临时渲染对象
class SketchCurveTransient : public GuiCmdTransient
{
public:
    SketchCurveTransient(const wydb::ElementId& id, double startParam, double endParam);
    explicit SketchCurveTransient(const wy3d::SketchCurve* pCurve,
        double startParam = 0.0, double endParam = 1.0);
    // 圆弧
    SketchCurveTransient(const wy3d::Plane& sketchPlane,
        const wy::Vector2& center, double radius, double startAngle, double endAngle);
    // 直线段
    SketchCurveTransient(const wy3d::Plane& sketchPlane,
        const wy::Vector2& startPnt, const wy::Vector2& endPnt,
        osg::ref_ptr<osg::LineStipple> lineStipple = nullptr,
        osg::ref_ptr<osg::LineWidth> lineWidth = nullptr);

    wydb::ElementId getId() const { return _id; }
    double getStartParam() const { return _startParam; }
    double getEndParam() const { return _endParam; }

private:
    bool init(const wy3d::SketchCurve* pCurve, double startParam, double endParam);
    bool init(const wy3d::Plane& sketchPlane, const wy::Vector2& startPnt, const wy::Vector2& endPnt, double startParam, double endParam);
    bool init(const wy3d::Plane& sketchPlane, const wy::Vector2& center, double radius, double startParam, double endParam);
    void initGeom(const wy3d::Plane& sketchPlane, SketchEntityLinearization* pLinearization);
    void initGeom(osg::ref_ptr<osg::Vec3Array> vertices, const std::vector<unsigned int>& indices);

    const wy3d::SketchCurve* getCurve(const wydb::ElementId& id);

private:
    wydb::ElementId _id;
    double _startParam;
    double _endParam;
    osg::ref_ptr<osg::LineStipple> _lineStipple;
    osg::ref_ptr<osg::LineWidth> _lineWidth;
};

typedef std::shared_ptr<SketchCurveTransient> SketchCurveTransientSPtr;

class SketchDotLineTransient : public GuiCmdTransient
{
public:
    SketchDotLineTransient(const wy3d::Plane& sketchPlane, const wy::Vector2& startPnt, const wy::Vector2& endPnt);

    void update(const wy::Vector2& startPnt, const wy::Vector2& endPnt);

private:
    wy3d::Plane _sketchPlane;
    osg::ref_ptr<osg::Geometry> _geom;
    osg::ref_ptr<osg::Vec3Array> _vertices;
};

#endif // WY3DAPP_SKETCH_CURVE_TRANSIENT_H