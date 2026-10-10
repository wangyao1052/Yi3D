///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_PLANE_H
#define WY3D_PLANE_H

#include <wyVector2.h>
#include <wyVector3.h>
#include <wy3dVector2.h>
#include <wy3dVector3.h>
#include <wy3dDefs.h>

NS_WY3D_BEG

class WY3D_EXPORT Plane
{
public:
    Plane();
    Plane(const wy::Vector3& origin, const wy::Vector3& normal, const wy::Vector3& xDir);

    // 无效的平面
    static Plane kInvalid;

    // 获取原点
    inline wy::Vector3 getOrigin() const { return _origin; }
    // 设置原点
    inline void setOrigin(const wy::Vector3& origin)
    {
        _origin = origin;
    }
    // 获取平面法向
    inline wy::Vector3 getNormal() const { return _normal; }
    // 获取X轴向
    inline wy::Vector3 getXDir() const { return _xDir; }
    // 获取Y轴向
    inline wy::Vector3 getYDir() const { return _normal.cross(_xDir); }
    // 是否有效
    bool isValid() const;

    // 将UV坐标转三维坐标
    wy::Vector3 value(double u, double v) const;
    inline wy::Vector3 value(const wy::Vector2& uv) const
    {
        return this->value(uv.x(), uv.y());
    }

    // 将三维坐标转UV坐标
    wy::Vector2 uv(const wy::Vector3& pnt) const;
    inline wy::Vector2 uv(double x, double y, double z) const
    {
        return this->uv(wy::Vector3(x, y, z));
    }

    // 求投影点
    wy::Vector3 project(const wy::Vector3& pnt) const;
    // 计算点到平面的垂直距离
    double distanceTo(const wy::Vector3& pnt) const;

    // 偏移
    static Plane offset(const Plane& plane, double distance);

    // 操作符
    friend inline bool operator==(const Plane& lhs, const Plane& rhs);
    friend inline bool operator!=(const Plane& lhs, const Plane& rhs);

private:
    wy::Vector3 _origin;
    wy::Vector3 _normal;
    wy::Vector3 _xDir;
};

// 操作符
inline bool operator==(const Plane& lhs, const Plane& rhs)
{
    return lhs._origin == rhs._origin && lhs._normal == rhs._normal && lhs._xDir == rhs._xDir;
}

inline bool operator!=(const Plane& lhs, const Plane& rhs)
{
    return lhs._origin != rhs._origin || lhs._normal != rhs._normal || lhs._xDir != rhs._xDir;
}

NS_WY3D_END

#endif // WY3D_PLANE_H