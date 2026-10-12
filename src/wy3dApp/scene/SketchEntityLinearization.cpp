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

#include "scene/SketchEntityLinearization.h"
#include <cassert>
#include <wyVector2.h>
#include <wy3dMath.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>
#include <wy3dSketchEllipse.h>
#include <wy3dSketchEllipseArc.h>
#include <wy3dSketchSpline.h>
#include <wy3dSketchSpline.h>
#include "utils/MathUtils.h"
#include "commands/sketch/SketchTrimExtendUtil.h"
#include "wy3dAppDefs.h"

namespace wy3dApp {

static const unsigned int kCirclePointsNum  = 100;
static const unsigned int kEllipsePointsNum = 200;
static const unsigned int kSplinePointsNumPerSegment = 40;

static inline void lineLinearization(const wy::Vector2& startPnt, const wy::Vector2& endPnt,
    std::vector<wy::Vector2>& vertices, std::vector<unsigned int>& indices)
{
    vertices.reserve(2);
    vertices.emplace_back(startPnt);
    vertices.emplace_back(endPnt);
    indices.reserve(2);
    indices.emplace_back(0);
    indices.emplace_back(1);
}

static inline void circleLinearization(
    const wy::Vector2& center, double radius,
    std::vector<wy::Vector2>& vertices, std::vector<unsigned int>& indices)
{
    vertices.reserve(kCirclePointsNum);
    double delta = (wy3d::TWO_PI) / kCirclePointsNum;
    for (unsigned int i = 0; i < kCirclePointsNum; ++i)
    {
        vertices.emplace_back(wy::Vector2(
            std::cos(i * delta) * radius + center.x(),
            std::sin(i * delta) * radius + center.y()));
    }

    indices.reserve(2 * kCirclePointsNum);
    for (unsigned int i = 0; i < kCirclePointsNum - 1; ++i)
    {
        indices.push_back(i);
        indices.push_back(i + 1);
    }
    indices.push_back(kCirclePointsNum - 1);
    indices.push_back(0);
}

static inline void arcLinearization(
    const wy::Vector2& center, double radius, double startAngle, double endAngle,
    std::vector<wy::Vector2>& vertices, std::vector<unsigned int>& indices)
{
    startAngle = wy3d::normalizeRadian(startAngle);
    endAngle = wy3d::normalizeRadian(endAngle);
    if (endAngle < startAngle) endAngle += wy3d::TWO_PI;
    double totalAngle = endAngle - startAngle;
    assert(totalAngle >= 0 && totalAngle < wy3d::TWO_PI);
    size_t numVertices = (totalAngle / wy3d::TWO_PI) * kCirclePointsNum;
    if (numVertices < 20)
    {
        numVertices = 20;
    }

    vertices.reserve(numVertices);
    double delta = totalAngle / (numVertices - 1);
    double angle = startAngle;
    for (size_t i = 0; i < numVertices; ++i)
    {
        angle = startAngle + i * delta;
        vertices.emplace_back(wy::Vector2(
            std::cos(angle) * radius + center.x(),
            std::sin(angle) * radius + center.y()));
    }

    indices.reserve(2 * (numVertices - 1));
    for (size_t i = 0; i < numVertices - 1; ++i)
    {
        indices.push_back(i);
        indices.push_back(i + 1);
    }
}

static inline void ellipseLinearization(
    const wy::Vector2& center, const wy::Vector2& majorAxis, double radiusRatio,
    std::vector<wy::Vector2>& vertices, std::vector<unsigned int>& indices)
{
    double majorRadius = majorAxis.length();
    double minorRadius = radiusRatio * majorRadius;

    // 计算长轴与X轴的夹角
    double angle = std::atan2(majorAxis.y(), majorAxis.x());
    double cosAngle = std::cos(angle);
    double sinAngle = std::sin(angle);

    vertices.reserve(kEllipsePointsNum);
    double delta = (wy3d::TWO_PI) / kEllipsePointsNum;
    for (unsigned int i = 0; i < kEllipsePointsNum; ++i)
    {
        // 计算椭圆的每个点（长轴和短轴）
        double x = std::cos(i * delta) * majorRadius;
        double y = std::sin(i * delta) * minorRadius;

        // 使用旋转矩阵旋转点
        double xRot = x * cosAngle - y * sinAngle;
        double yRot = x * sinAngle + y * cosAngle;

        // 计算旋转后的点的位置并添加到顶点数组
        vertices.emplace_back(wy::Vector2(xRot + center.x(), yRot + center.y()));
    }

    indices.reserve(2 * kEllipsePointsNum);
    for (unsigned int i = 0; i < kEllipsePointsNum - 1; ++i)
    {
        indices.push_back(i);
        indices.push_back(i + 1);
    }
    indices.push_back(kEllipsePointsNum - 1);
    indices.push_back(0);
}

static inline void ellipseArcLinearization(
    const wy::Vector2& center, const wy::Vector2& majorAxis, double radiusRatio,
    double startAngle, double endAngle,
    std::vector<wy::Vector2>& vertices, std::vector<unsigned int>& indices)
{
    double majorRadius = majorAxis.length();
    double minorRadius = radiusRatio * majorRadius;

    // 计算长轴与X轴的夹角
    double angle = std::atan2(majorAxis.y(), majorAxis.x());
    double cosAngle = std::cos(angle);
    double sinAngle = std::sin(angle);

    // 椭圆弧的几何角度
    startAngle = wy3d::normalizeRadian(startAngle);
    endAngle = wy3d::normalizeRadian(endAngle);
    if (endAngle < startAngle) endAngle += wy3d::TWO_PI;
    double totalAngle = endAngle - startAngle;

    // 转换为参数角度
    double twoPI = wy3d::PI * 2;
    startAngle = wy3d::ellipsePolarAngleToParametricAngle(startAngle, majorRadius, minorRadius);
    endAngle = wy3d::ellipsePolarAngleToParametricAngle(endAngle, majorRadius, minorRadius);
    if (endAngle < startAngle) endAngle += twoPI;
    totalAngle = endAngle - startAngle;
    assert(totalAngle >= 0 && totalAngle < twoPI);

    // 离散化
    size_t numVertices = (totalAngle / twoPI) * kEllipsePointsNum;
    if (numVertices < 100)
    {
        numVertices = 100;
    }
    vertices.reserve(numVertices);
    double delta = totalAngle / (numVertices - 1);
    for (size_t i = 0; i < numVertices; ++i)
    {
        // 计算椭圆的每个点（长轴和短轴）
        double x = std::cos((i * delta + startAngle)) * majorRadius;
        double y = std::sin((i * delta + startAngle)) * minorRadius;

        // 使用旋转矩阵旋转点
        double xRot = x * cosAngle - y * sinAngle;
        double yRot = x * sinAngle + y * cosAngle;

        // 计算旋转后的点的位置并添加到顶点数组
        vertices.emplace_back(wy::Vector2(xRot + center.x(), yRot + center.y()));
    }
    indices.reserve(2 * (numVertices - 1));
    for (size_t i = 0; i < numVertices - 1; ++i)
    {
        indices.push_back(i);
        indices.push_back(i + 1);
    }
}

static inline void bsplineLinearization(
    Handle(Geom2d_BSplineCurve) pBSpline,
    std::vector<wy::Vector2>& vertices, std::vector<unsigned int>& indices,
    double startParam = 0.0, double endParam = 1.0)
{
    if (!pBSpline) return;

    size_t numVertices = pBSpline->NbKnots() * static_cast<size_t>(kSplinePointsNumPerSegment); // kSplinePointsNumPerSegment = 40
    
    // added by wangyao 2025.07.03 {
    // 判断B样条是否是直线段
    // 如何是直线段则离散点数设置为2
    if (!pBSpline->IsRational())
    {
        if (2 == pBSpline->NbPoles())
        {
            numVertices = 2;
        }
    }
    // }
    
    double startT(0.0), endT(1.0);
    if (startParam == 0.0 && endParam == 1.0)
    {
        startT = pBSpline->FirstParameter();
        endT = pBSpline->LastParameter();
    }
    else if (startParam >= 0.0 && endParam <= 1.0 && endParam > startParam)
    {
        // added by wangyao 2025.07.03 {
        // 对于样条曲线的部分,需要根据参数范围相应地减少离散点的数量
        if (numVertices > kSplinePointsNumPerSegment)
        {
            numVertices = (endParam - startParam) * numVertices;
            if (numVertices < kSplinePointsNumPerSegment)
            {
                numVertices = kSplinePointsNumPerSegment;
            }
        }
        // }
        
        double firstParam = pBSpline->FirstParameter();
        double lastParam = pBSpline->LastParameter();
        double paramRange = lastParam - firstParam;
        if (startParam == 0.0) startT = firstParam;
        else startT = firstParam + startParam * paramRange;
        if (endParam == 1.0) endT = lastParam;
        else endT = firstParam + endParam * paramRange;
    }
    else if (endParam > startParam) // 在延伸命令中,startParam或endParam不在[0,1]范围内
    {
        // added by wangyao 2025.07.03 {
        // 对于样条曲线的部分,需要根据参数范围相应地减少离散点的数量
        if (numVertices > kSplinePointsNumPerSegment)
        {
            numVertices = (std::clamp(endParam, 0.0, 1.0) - std::clamp(startParam, 0.0, 1.0)) * numVertices;
            if (numVertices < kSplinePointsNumPerSegment)
            {
                numVertices = kSplinePointsNumPerSegment;
            }
        }
        // }

        double firstParam = pBSpline->FirstParameter();
        double lastParam = pBSpline->LastParameter();
        double paramRange = lastParam - firstParam;
        if (startParam <= 0.0) startT = firstParam;
        else startT = firstParam + startParam * paramRange;
        if (endParam >= 1.0) endT = lastParam;
        else endT = firstParam + endParam * paramRange;
    }
    else
    {
        assert(false);
        return;
    }

    vertices.reserve(numVertices);
    indices.reserve(2 * (numVertices - 1));
    // added by wangyao 2025.07.03 {
    if (startParam < 0.0 || endParam > 1.0)
    {
        vertices.reserve(numVertices + 2);
        indices.reserve(indices.capacity() + 2);
    }
    // }

    const double step = (endT - startT) / (numVertices - 1);
    for (int i = 0; i < numVertices - 1; ++i)
    {
        const double param = startT + i * step;
        gp_Pnt2d pnt2d = pBSpline->Value(param);
        vertices.emplace_back(wy::Vector2(pnt2d.X(), pnt2d.Y()));
    }
    gp_Pnt2d lastPnt2d = pBSpline->Value(endT);
    vertices.emplace_back(wy::Vector2(lastPnt2d.X(), lastPnt2d.Y()));

    for (size_t i = 0; i < numVertices - 1; ++i)
    {
        indices.push_back(i);
        indices.push_back(i + 1);
    }

    if (startParam < 0.0 || endParam > 1.0)
    {
        wy::Vector2 startPnt, startDir;
        wy::Vector2 endPnt, endDir;
        if (!SketchTrimExtendUtil::getBSplineInfo(pBSpline, startPnt, startDir, endPnt, endDir))
        {
            assert(false);
            return;
        }

        if (startParam < 0.0)
        {
            wy::Vector2 pnt = startPnt + startDir * startParam;
            vertices.emplace_back(startPnt);
            vertices.emplace_back(pnt);
            indices.push_back(vertices.size() - 2);
            indices.push_back(vertices.size() - 1);
        }
        else if (endParam > 1.0)
        {
            wy::Vector2 pnt = endPnt + endDir * (endParam - 1.0);
            vertices.emplace_back(endPnt);
            vertices.emplace_back(pnt);
            indices.push_back(vertices.size() - 2);
            indices.push_back(vertices.size() - 1);
        }
    }
}

SketchEntityLinearization::SketchEntityLinearization(const wy3d::SketchEntity* pEntity)
{
    assert(pEntity);
    if (const wy3d::SketchLine* pLine = wy3d::SketchLine::cast(pEntity))
    {
        lineLinearization(pLine->getStartPoint(), pLine->getEndPoint(), _vertices, _indices);
    }
    else if (const wy3d::SketchCenterLine* pCenterLine = wy3d::SketchCenterLine::cast(pEntity))
    {
        lineLinearization(pCenterLine->getStartPoint(), pCenterLine->getEndPoint(), _vertices, _indices);
    }
    else if (const wy3d::SketchCircle* pCircle = wy3d::SketchCircle::cast(pEntity))
    {
        circleLinearization(pCircle->getCenter(), pCircle->getRadius(), _vertices, _indices);
    }
    else if (const wy3d::SketchArc* pArc = wy3d::SketchArc::cast(pEntity))
    {
        arcLinearization(pArc->getCenter(), pArc->getRadius(), pArc->getStartAngle(), pArc->getEndAngle(), _vertices, _indices);
    }
    else if (const wy3d::SketchEllipse* pEllipse = wy3d::SketchEllipse::cast(pEntity))
    {
        ellipseLinearization(pEllipse->getCenter(), pEllipse->getMajorAxis(), pEllipse->getRadiusRatio(), _vertices, _indices);
    }
    else if (const wy3d::SketchEllipseArc* pEllipseArc = wy3d::SketchEllipseArc::cast(pEntity))
    {
        ellipseArcLinearization(pEllipseArc->getCenter(), pEllipseArc->getMajorAxis(), pEllipseArc->getRadiusRatio(),
            pEllipseArc->getStartAngle(), pEllipseArc->getEndAngle(), _vertices, _indices);
    }
    else if (const wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pEntity))
    {        bsplineLinearization(pSpline->getOccSpline(), _vertices, _indices);
    }
    else
    {
        assert(false);
    }
}

SketchEntityLinearization::SketchEntityLinearization(const wy::Vector2& startPnt, const wy::Vector2& endPnt)
{
    lineLinearization(startPnt, endPnt, _vertices, _indices);
}

SketchEntityLinearization::SketchEntityLinearization(const wy::Vector2& center, double radius)
{
    circleLinearization(center, radius, _vertices, _indices);
}

SketchEntityLinearization::SketchEntityLinearization(const wy::Vector2& center, double radius, double startAngle, double endAngle)
{
    arcLinearization(center, radius, startAngle, endAngle, _vertices, _indices);
}

SketchEntityLinearization::SketchEntityLinearization(const wy::Vector2& center, const wy::Vector2& majorAxis, double radiusRatio)
{
    ellipseLinearization(center, majorAxis, radiusRatio, _vertices, _indices);
}

SketchEntityLinearization::SketchEntityLinearization(const wy::Vector2& center, const wy::Vector2& majorAxis, double radiusRatio, double startAngle, double endAngle)
{
    ellipseArcLinearization(center, majorAxis, radiusRatio, startAngle, endAngle, _vertices, _indices);
}

SketchEntityLinearization::SketchEntityLinearization(Handle(Geom2d_BSplineCurve) pBSpline, double startParam, double endParam)
{
    bsplineLinearization(pBSpline, _vertices, _indices, startParam, endParam);
}

}
