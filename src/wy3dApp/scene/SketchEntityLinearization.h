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

#ifndef WY3DAPP_SKETCH_ENTITY_LINEARIZATION_H
#define WY3DAPP_SKETCH_ENTITY_LINEARIZATION_H

#include <vector>
#include <Geom2d_BSplineCurve.hxx>
#include <wyVector2.h>
#include <wy3dVector2.h>
#include <wy3dSketchEntity.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class SketchEntityLinearization
{
public:
    explicit SketchEntityLinearization(const wy3d::SketchEntity* pEntity);

    // 直线段
    SketchEntityLinearization(const wy::Vector2& startPnt, const wy::Vector2& endPnt);
    // 圆
    SketchEntityLinearization(const wy::Vector2& center, double radius);
    // 圆弧
    SketchEntityLinearization(const wy::Vector2& center, double radius, double startAngle, double endAngle);
    // 椭圆
    SketchEntityLinearization(const wy::Vector2& center, const wy::Vector2& majorAxis, double radiusRatio);
    // 椭圆弧
    SketchEntityLinearization(const wy::Vector2& center, const wy::Vector2& majorAxis, double radiusRatio, double startAngle, double endAngle);
    // 样条曲线
    SketchEntityLinearization(Handle(Geom2d_BSplineCurve) pBSpline, double startParam = 0.0, double endParam = 1.0);

    const std::vector<wy::Vector2>& getVertices() const { return _vertices; }
    const std::vector<unsigned int>& getIndices() const { return _indices; }

private:
    std::vector<wy::Vector2> _vertices;
    std::vector<unsigned int> _indices;
};

}

#endif // WY3DAPP_SKETCH_ENTITY_LINEARIZATION_H
