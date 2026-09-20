///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 Wang Yao <wangyao1052@163.com>
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

#ifndef WY3DAPP_SKETCH3D_CURVE_TRANSIENT_H
#define WY3DAPP_SKETCH3D_CURVE_TRANSIENT_H

#include <memory>
#include <vector>

#include <osg/Geometry>
#include <wydbElementId.h>
#include <wy3dSketchCurve3D.h>

#include "GuiCmdTransient.h"

// 3D草图曲线临时渲染对象(高亮被捕捉参考的3D曲线,样式与2D草图一致:橙色加粗)
// 通过SketchCurve3D::getPointAt采样折线化,空间曲线(圆/弧/样条)通用
class Sketch3DCurveTransient : public GuiCmdTransient
{
public:
    explicit Sketch3DCurveTransient(const wy3d::SketchCurve3D* pCurve);

    // An arc that is not an entity yet, sampled by SketchArc3D's own evaluation rather than through
    // getPointAt: there is no entity to ask.

    Sketch3DCurveTransient(const wy::Vector3& center, const wy::Vector3& normal,
        const wy::Vector3& xDir, double radius, double startAngle, double endAngle);

    ~Sketch3DCurveTransient();

private:
    // 采样折线化并构建几何(闭合曲线自动补尾首段)
    void initGeom(const std::vector<wy::Vector3>& points, bool closed);

private:
    wydb::ElementId _id;
};

typedef std::shared_ptr<Sketch3DCurveTransient> Sketch3DCurveTransientSPtr;

#endif // WY3DAPP_SKETCH3D_CURVE_TRANSIENT_H