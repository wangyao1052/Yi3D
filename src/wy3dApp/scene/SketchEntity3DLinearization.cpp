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

static const unsigned int kCirclePointsNum = 100;

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
    wy::Vector3 u = xDir - normal * xDir.dot(normal);
    if (u.length() < 0.5)
    {
        wy::Vector3 refAxis = (std::fabs(normal.z()) < 0.9) ? wy::Vector3::kZAxis : wy::Vector3::kXAxis;
        u = normal.cross(refAxis);
    }
    u.normalize();
    wy::Vector3 v = normal.cross(u);

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
