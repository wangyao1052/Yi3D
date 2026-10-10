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

#include <cassert>
#include <wyVector2.h>
#include <wyVector3.h>
#include <wy3dPlane.h>

NS_WY3D_BEG

Plane Plane::kInvalid = Plane(wy::Vector3::kZero, wy::Vector3::kZero, wy::Vector3::kZero);

static wy::Vector3 autoComputeXDir(const wy::Vector3& normal)
{
    wy::Vector3 yDir = normal.cross(wy::Vector3(1.0, 0.0, 0.0));
    if (yDir.length() < 0.5) // 无效
    {
        yDir = normal.cross(wy::Vector3(0.0, 1.0, 0.0));
        assert(yDir.length() >  0.5);
    }

    return yDir.cross(normal);
}

Plane::Plane() : _origin(0.0, 0.0, 0.0), _normal(0.0, 0.0, 1.0), _xDir(1.0, 0.0, 0.0)
{
}

Plane::Plane(const wy::Vector3& origin, const wy::Vector3& normal, const wy::Vector3& xDir)
    : _origin(origin), _normal(normal), _xDir(xDir)
{
    _normal.normalize();
    _xDir.normalize();
    if (_normal.length() < 0.5) // 法向量无效
    {
        _normal.set(0.0, 0.0, 0.0);
        _xDir.set(0.0, 0.0, 0.0);
    }
    else // 法向量有效
    {
        if (_xDir.length() < 0.5) // X轴向向量无效
        {
            _xDir = autoComputeXDir(_normal);
        }
        else // X轴向向量有效
        {
            wy::Vector3 yDir = _normal.cross(_xDir);
            if (yDir.length() < 0.5) // 无效
            {
                _xDir = autoComputeXDir(_normal);
            }
            else
            {
                _xDir = yDir.cross(_normal);
            }
        }
    }
}

bool Plane::isValid() const
{
    static wy::Vector3 zeroVec(0.0, 0.0, 0.0);
    return _normal != zeroVec && _xDir != zeroVec;
}

wy::Vector3 Plane::value(double u, double v) const
{
    if (!this->isValid())
    {
        return wy::Vector3(0.0, 0.0, 0.0);
    }

    wy::Vector3 yDir = this->getYDir();
    return wy::Vector3(
        u * _xDir.x() + v * yDir.x() + _origin.x(),
        u * _xDir.y() + v * yDir.y() + _origin.y(),
        u * _xDir.z() + v * yDir.z() + _origin.z());
}

wy::Vector2 Plane::uv(const wy::Vector3& point) const
{
    if (!this->isValid())
    {
        return wy::Vector2(0.0, 0.0);
    }

    wy::Vector3 vec = point - _origin;
    double u = vec.dot(_xDir);
    double v = vec.dot(getYDir());
    return wy::Vector2(u, v);
}

wy::Vector3 Plane::project(const wy::Vector3& pnt) const
{
    return pnt - (pnt - _origin).dot(_normal) * _normal;
}

double Plane::distanceTo(const wy::Vector3& pnt) const
{
    return std::fabs((pnt - _origin).dot(_normal));
}

Plane Plane::offset(const Plane& plane, double distance)
{
    wy::Vector3 origin = plane._origin + plane._normal * distance;
    return wy3d::Plane(origin, plane._normal, plane._xDir);
}

NS_WY3D_END