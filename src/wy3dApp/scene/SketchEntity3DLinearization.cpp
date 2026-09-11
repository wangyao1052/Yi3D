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

#include "scene/SketchEntity3DLinearization.h"
#include <cassert>
#include <wy3dMath.h>
#include <wy3dSketchLine3D.h>
#include <wy3dSketchCircle3D.h>
#include <wy3dSketchArc3D.h>
#include <wy3dSketchEllipse3D.h>
#include <wy3dSketchEllipseArc3D.h>
#include <wy3dSketchSpline3D.h>
#include <Geom_BSplineCurve.hxx>
#include <gp_Pnt.hxx>

static const unsigned int kCirclePointsNum = 100;
static const unsigned int kEllipsePointsNum = 200;
static const unsigned int kSplinePointsNumPerSegment = 40;

// 把 xDir 正交化到曲线平面,并取 yDir = normal × xDir
static inline void buildCurveFrame(const wy::Vector3& normal, const wy::Vector3& xDirIn,
    wy::Vector3& u, wy::Vector3& v)
{
    u = xDirIn - normal * xDirIn.dot(normal);
    if (u.length() < 0.5)
    {
        wy::Vector3 refAxis = (std::fabs(normal.z()) < 0.9) ? wy::Vector3::kZAxis : wy::Vector3::kXAxis;
        u = normal.cross(refAxis);
    }
    u.normalize();
    v = normal.cross(u);
}

static inline void lineLinearization(const wy::Vector3& startPnt, const wy::Vector3& endPnt,
    std::vector<wy::Vector3>& vertices, std::vector<unsigned int>& indices)
{
    vertices.reserve(2);
    vertices.emplace_back(startPnt);
    vertices.emplace_back(endPnt);
    indices.reserve(2);
    indices.emplace_back(0);
    indices.emplace_back(1);
}

static inline void circleLinearization(
    const wy::Vector3& center, const wy::Vector3& normal, const wy::Vector3& xDir, double radius,
    std::vector<wy::Vector3>& vertices, std::vector<unsigned int>& indices)
{
    // Orthogonalize xDir to the circle plane (fallback to an arbitrary axis)
    wy::Vector3 u;
    wy::Vector3 v;
    buildCurveFrame(normal, xDir, u, v);

    vertices.reserve(kCirclePointsNum);
    double delta = (wy3d::TWO_PI) / kCirclePointsNum;
    for (unsigned int i = 0; i < kCirclePointsNum; ++i)
    {
        double c = std::cos(i * delta);
        double s = std::sin(i * delta);
        vertices.emplace_back(center + u * (c * radius) + v * (s * radius));
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
    const wy::Vector3& center, const wy::Vector3& normal, const wy::Vector3& xDir,
    double radius, double startAngle, double endAngle,
    std::vector<wy::Vector3>& vertices, std::vector<unsigned int>& indices)
{
    // Orthogonalize xDir to the arc plane (fallback to an arbitrary axis)
    wy::Vector3 u;
    wy::Vector3 v;
    buildCurveFrame(normal, xDir, u, v);

    double totalAngle = wy3d::normalizeRadian(endAngle - startAngle);
    unsigned int pointsNum = static_cast<unsigned int>(std::llround(kCirclePointsNum * totalAngle / wy3d::TWO_PI));
    if (pointsNum < 4) pointsNum = 4;

    vertices.reserve(pointsNum + 1);
    for (unsigned int i = 0; i <= pointsNum; ++i)
    {
        double angle = startAngle + totalAngle * (static_cast<double>(i) / pointsNum);
        vertices.emplace_back(center + u * (std::cos(angle) * radius) + v * (std::sin(angle) * radius));
    }

    // The polyline is open: no closing edge back to the first point
    indices.reserve(2 * pointsNum);
    for (unsigned int i = 0; i < pointsNum; ++i)
    {
        indices.push_back(i);
        indices.push_back(i + 1);
    }
}

// 整椭圆:整圈按参数角采样并闭合
static inline void ellipseLinearization(
    const wy::Vector3& center, const wy::Vector3& normal, const wy::Vector3& xDir,
    double majorRadius, double minorRadius,
    std::vector<wy::Vector3>& vertices, std::vector<unsigned int>& indices)
{
    wy::Vector3 u;
    wy::Vector3 v;
    buildCurveFrame(normal, xDir, u, v);

    vertices.reserve(kEllipsePointsNum);
    double delta = wy3d::TWO_PI / kEllipsePointsNum;
    for (unsigned int i = 0; i < kEllipsePointsNum; ++i)
    {
        double c = std::cos(i * delta);
        double s = std::sin(i * delta);
        vertices.emplace_back(center + u * (c * majorRadius) + v * (s * minorRadius));
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

// 椭圆弧:起止角是极角,先换到参数角再按区间采样(开放折线)
static inline void ellipseArcLinearization(
    const wy::Vector3& center, const wy::Vector3& normal, const wy::Vector3& xDir,
    double majorRadius, double minorRadius, double startAngle, double endAngle,
    std::vector<wy::Vector3>& vertices, std::vector<unsigned int>& indices)
{
    wy::Vector3 u;
    wy::Vector3 v;
    buildCurveFrame(normal, xDir, u, v);

    const double startPolar = wy3d::normalizeRadian(startAngle);
    const double totalPolar = wy3d::normalizeRadian(endAngle - startAngle);
    const double startParam = wy3d::ellipsePolarAngleToParametricAngle(startPolar, majorRadius, minorRadius);
    double totalParam = wy3d::ellipsePolarAngleToParametricAngle(startPolar + totalPolar, majorRadius, minorRadius) - startParam;
    while (totalParam < 0.0)
    {
        totalParam += wy3d::TWO_PI;
    }

    unsigned int pointsNum = static_cast<unsigned int>(std::llround(kEllipsePointsNum * totalParam / wy3d::TWO_PI));
    if (pointsNum < 4) pointsNum = 4;

    vertices.reserve(pointsNum + 1);
    for (unsigned int i = 0; i <= pointsNum; ++i)
    {
        double angle = startParam + totalParam * (static_cast<double>(i) / pointsNum);
        vertices.emplace_back(center + u * (std::cos(angle) * majorRadius) + v * (std::sin(angle) * minorRadius));
    }

    // The polyline is open: no closing edge back to the first point
    indices.reserve(2 * pointsNum);
    for (unsigned int i = 0; i < pointsNum; ++i)
    {
        indices.push_back(i);
        indices.push_back(i + 1);
    }
}

// 样条:按节点数 ×40 采样;非有理 2 极点退化为直线段
static inline void bspline3DLinearization(const Handle(Geom_BSplineCurve)& pBSpline,
    std::vector<wy::Vector3>& vertices, std::vector<unsigned int>& indices)
{
    if (pBSpline.IsNull())
    {
        return;
    }

    if (!pBSpline->IsRational() && 2 == pBSpline->NbPoles())
    {
        const gp_Pnt pnt1 = pBSpline->Pole(1);
        const gp_Pnt pnt2 = pBSpline->Pole(2);
        vertices.emplace_back(pnt1.X(), pnt1.Y(), pnt1.Z());
        vertices.emplace_back(pnt2.X(), pnt2.Y(), pnt2.Z());
        indices.push_back(0);
        indices.push_back(1);
        return;
    }

    const unsigned int numVertices = static_cast<unsigned int>(pBSpline->NbKnots() * kSplinePointsNumPerSegment);
    if (numVertices < 2)
    {
        return;
    }

    const double firstParam = pBSpline->FirstParameter();
    const double lastParam = pBSpline->LastParameter();
    vertices.reserve(numVertices);
    for (unsigned int i = 0; i < numVertices; ++i)
    {
        const double param = firstParam + (lastParam - firstParam) * (static_cast<double>(i) / (numVertices - 1));
        const gp_Pnt pnt = pBSpline->Value(param);
        vertices.emplace_back(pnt.X(), pnt.Y(), pnt.Z());
    }

    indices.reserve(2 * (numVertices - 1));
    for (unsigned int i = 0; i + 1 < numVertices; ++i)
    {
        indices.push_back(i);
        indices.push_back(i + 1);
    }
}

SketchEntity3DLinearization::SketchEntity3DLinearization(const wy3d::SketchEntity3D* pEntity)
{
    assert(pEntity);
    if (const wy3d::SketchLine3D* pLine = wy3d::SketchLine3D::cast(pEntity))
    {
        lineLinearization(pLine->getStartPoint(), pLine->getEndPoint(), _vertices, _indices);
    }
    else if (const wy3d::SketchCircle3D* pCircle = wy3d::SketchCircle3D::cast(pEntity))
    {
        circleLinearization(pCircle->getCenter(), pCircle->getNormal(), pCircle->getXDir(), pCircle->getRadius(), _vertices, _indices);
    }
    else if (const wy3d::SketchArc3D* pArc = wy3d::SketchArc3D::cast(pEntity))
    {
        arcLinearization(pArc->getCenter(), pArc->getNormal(), pArc->getXDir(), pArc->getRadius(),
            pArc->getStartAngle(), pArc->getEndAngle(), _vertices, _indices);
    }
    else if (const wy3d::SketchEllipse3D* pEllipse = wy3d::SketchEllipse3D::cast(pEntity))
    {
        ellipseLinearization(pEllipse->getCenter(), pEllipse->getNormal(), pEllipse->getXDir(),
            pEllipse->getMajorRadius(), pEllipse->getMinorRadius(), _vertices, _indices);
    }
    else if (const wy3d::SketchEllipseArc3D* pEllipseArc = wy3d::SketchEllipseArc3D::cast(pEntity))
    {
        ellipseArcLinearization(pEllipseArc->getCenter(), pEllipseArc->getNormal(), pEllipseArc->getXDir(),
            pEllipseArc->getMajorRadius(), pEllipseArc->getMinorRadius(),
            pEllipseArc->getStartAngle(), pEllipseArc->getEndAngle(), _vertices, _indices);
    }
    else if (const wy3d::SketchSpline3D* pSpline = wy3d::SketchSpline3D::cast(pEntity))
    {
        bspline3DLinearization(pSpline->getOccSpline(), _vertices, _indices);
    }
    else
    {
        assert(false);
    }
}

SketchEntity3DLinearization::SketchEntity3DLinearization(const wy::Vector3& startPnt, const wy::Vector3& endPnt)
{
    lineLinearization(startPnt, endPnt, _vertices, _indices);
}

SketchEntity3DLinearization::SketchEntity3DLinearization(const wy::Vector3& center, const wy::Vector3& normal, const wy::Vector3& xDir, double radius)
{
    circleLinearization(center, normal, xDir, radius, _vertices, _indices);
}
