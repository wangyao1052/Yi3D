///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_PLANE_H
#define WY3D_PLANE_H

#include <wy3dDefs.h>
#include <wy3dVector3.h>

NS_WY3D_GEOM_BEG

// 平面方程: Ax + By + Cz + D = 0;
class WY3D_EXPORT Plane
{
public:
    Plane() : _A(0.0), _B(0.0), _C(1.0), _D(0.0) {}
    Plane(double A, double B, double C, double D) : _A(A), _B(B), _C(C), _D(D) {}

    // 获取系数
    inline double getA() const { return _A; }
    inline double getB() const { return _B; }
    inline double getC() const { return _C; }
    inline double getD() const { return _D; }

    // 获取平面的法向量
    inline Vector3 getNormal() const;

    inline void set(double A, double B, double C, double D);
    // 已经平面法向和平面上的点构造平面
    inline void set(const Vector3& normal, const Vector3& point);
    // 过三点构造平面
    inline void set(const Vector3& pnt1, const Vector3& pnt2, const Vector3& pnt3);

    // 是否有效
    inline bool isValid() const;

    // 操作符
    friend inline bool operator==(const Plane& lhs, const Plane& rhs);
    friend inline bool operator!=(const Plane& lhs, const Plane& rhs);

private:
    double _A;
    double _B;
    double _C;
    double _D;
};

inline Vector3 Plane::getNormal() const
{
    Vector3 normal(_A, _B, _C);
    normal.normalize();
    return normal;
}

void Plane::set(double A, double B, double C, double D)
{
    _A = A;
    _B = B;
    _C = C;
    _D = D;
}

inline void Plane::set(const wy3d::Vector3& normal, const wy3d::Vector3& point)
{
    Vector3 N = normal;
    N.normalize();
    this->set(N.x(), N.y(), N.z(), -point.dot(N));
}

inline void Plane::set(const Vector3& pnt1, const Vector3& pnt2, const Vector3& pnt3)
{
    Vector3 N = (pnt2 - pnt1).cross(pnt3 - pnt1);
    N.normalize();
    this->set(N.x(), N.y(), N.z(), -pnt1.dot(N));
}

inline bool Plane::isValid() const
{
    return std::fabs(this->getNormal().length() - 1.0) < 1e-3;
}

inline bool operator==(const Plane& lhs, const Plane& rhs)
{
    return lhs._A == rhs._A && lhs._B == rhs._B && lhs._C == rhs._C && lhs._D == rhs._D;
}

inline bool operator!=(const Plane& lhs, const Plane& rhs)
{
    return lhs._A != rhs._A || lhs._B != rhs._B || lhs._C != rhs._C || lhs._D != rhs._D;
}

NS_WY3D_GEOM_END

#endif // WY3D_PLANE_H