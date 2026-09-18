#ifndef WY3D_MATRIX3_H
#define WY3D_MATRIX3_H

#include <iostream>
#include <iomanip>
#include <cmath>
#include <utility>
#include <cassert>

#include <wy3dDefs.h>
#include <wy3dMath.h>
#include <wy3dVector2.h>

NS_WY3D_GEOM_BEG

class WY3D_EXPORT Matrix3
{
public:
    Matrix3()
    {
        setIdentity();
    }

    Matrix3(
        double m00, double m01, double m02,
        double m10, double m11, double m12,
        double m20, double m21, double m22)
    {
        m[0][0] = m00; m[0][1] = m01; m[0][2] = m02;
        m[1][0] = m10; m[1][1] = m11; m[1][2] = m12;
        m[2][0] = m20; m[2][1] = m21; m[2][2] = m22;
    }

    Matrix3(const Matrix3& other) = default;
    Matrix3& operator=(const Matrix3& other) = default;

    Matrix3(Matrix3&& other) noexcept
    {
        std::swap(m, other.m);
    }

    Matrix3& operator=(Matrix3&& other) noexcept
    {
        std::swap(m, other.m);
        return *this;
    }

    static const Matrix3 kZero;
    static const Matrix3 kIdentity;

    inline double get(int row, int col) const
    {
        assert(row >= 0 && row < 3 && col >= 0 && col < 3);
        return m[row][col];
    }

    inline void set(int row, int col, double value)
    {
        assert(row >= 0 && row < 3 && col >= 0 && col < 3);
        m[row][col] = value;
    }

    inline Matrix3 transpose() const;
    inline double determinant() const;
    inline Matrix3 inverse() const;
    inline void setIdentity();

    static inline Matrix3 createTranslation(const Vector2& translation);
    static inline Matrix3 createScale(const Vector2& scaling);
    static inline Matrix3 createRotation(double radians);
    static inline Matrix3 createReflection2D(const Vector2& p1, const Vector2& p2);
    static inline Matrix3 createReflection2D_optimized(const Vector2& p1, const Vector2& p2);

    inline Vector2 transformPoint(const Vector2& point) const;
    inline void print() const;

    friend inline bool operator==(const Matrix3& lhs, const Matrix3& rhs);
    friend inline bool operator!=(const Matrix3& lhs, const Matrix3& rhs);
    friend inline const Matrix3 operator*(const Matrix3& lhs, const Matrix3& rhs);
    friend inline const Vector2 operator*(const Vector2& point, const Matrix3& matrix);

private:
    // 数据存储方式:行主序
    // m[row][col]对应矩阵数学表示中的{row+1,col+1}
    double m[3][3];
};

inline Matrix3 Matrix3::transpose() const
{
    return Matrix3(
        m[0][0], m[1][0], m[2][0],
        m[0][1], m[1][1], m[2][1],
        m[0][2], m[1][2], m[2][2]
    );
}

inline double Matrix3::determinant() const
{
    return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
        - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
        + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
}

inline Matrix3 Matrix3::inverse() const
{
    double det = determinant();
    if (std::fabs(det) < 1e-10)
    {
        return kIdentity;
    }
    double invDet = 1.0 / det;

    return Matrix3(
        (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * invDet,
        (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * invDet,
        (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * invDet,

        (m[1][2] * m[2][0] - m[1][0] * m[2][2]) * invDet,
        (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * invDet,
        (m[0][2] * m[1][0] - m[0][0] * m[1][2]) * invDet,

        (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * invDet,
        (m[0][1] * m[2][0] - m[0][0] * m[2][1]) * invDet,
        (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * invDet
    );
}

inline void Matrix3::setIdentity()
{
    m[0][0] = 1; m[0][1] = 0; m[0][2] = 0;
    m[1][0] = 0; m[1][1] = 1; m[1][2] = 0;
    m[2][0] = 0; m[2][1] = 0; m[2][2] = 1;
}

inline Matrix3 Matrix3::createTranslation(const Vector2& translation)
{
    return Matrix3(
        1, 0, 0,
        0, 1, 0,
        translation.x(), translation.y(), 1
    );
}

inline Matrix3 Matrix3::createScale(const Vector2& scaling)
{
    return Matrix3(
        scaling.x(), 0, 0,
        0, scaling.y(), 0,
        0, 0, 1
    );
}

inline Matrix3 Matrix3::createRotation(double radians)
{
    double c = cos(radians);
    double s = sin(radians);
    return Matrix3(
        c, s, 0,
        -s, c, 0,
        0, 0, 1
    );
}

inline Matrix3 Matrix3::createReflection2D(const Vector2& p1, const Vector2& p2)
{
    Vector2 dir = p2 - p1;
    dir.normalize();
    if (dir.length() < EPS)
    {
        return kIdentity;
    }
    double cosTheta = dir.x();
    double sinTheta = dir.y();

    Matrix3 T = createTranslation(-p1);
    Matrix3 R = Matrix3(cosTheta, -sinTheta, 0.0, sinTheta, cosTheta, 0.0, 0.0, 0.0, 1.0);
    Matrix3 M = Matrix3(1, 0, 0, 0, -1, 0, 0, 0, 1);
    Matrix3 inverseR = Matrix3(cosTheta, sinTheta, 0.0, -sinTheta, cosTheta, 0.0, 0.0, 0.0, 1.0);
    Matrix3 inverseT = createTranslation(p1);

    return T * R * M * inverseR * inverseT;
}

inline Matrix3 Matrix3::createReflection2D_optimized(const Vector2& p1, const Vector2& p2)
{
    Vector2 d = p2 - p1;
    double dx = d.x();
    double dy = d.y();
    double lenSq = dx * dx + dy * dy;
    //assert(lenSq > MATH_EPSILON);

    double a = dx * dx - dy * dy;
    double b = 2 * dx * dy;
    double tx = 2 * (p1.y() * dx * dy - p1.x() * dy * dy);
    double ty = 2 * (p1.x() * dx * dy - p1.y() * dx * dx);

    return Matrix3(
        a / lenSq, b / lenSq, tx / lenSq,
        b / lenSq, -a / lenSq, ty / lenSq,
        0, 0, 1
    );
}

inline Vector2 Matrix3::transformPoint(const Vector2& point) const
{
    return Vector2(
        point.x() * m[0][0] + point.y() * m[1][0] + m[2][0],
        point.x() * m[0][1] + point.y() * m[1][1] + m[2][1]
    );
}

inline void Matrix3::print() const
{
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "[" << m[0][0] << ", " << m[0][1] << ", " << m[0][2] << "]\n"
        << "[" << m[1][0] << ", " << m[1][1] << ", " << m[1][2] << "]\n"
        << "[" << m[2][0] << ", " << m[2][1] << ", " << m[2][2] << "]\n";
}

inline bool operator==(const Matrix3& lhs, const Matrix3& rhs)
{
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            if (lhs.m[i][j] != rhs.m[i][j])
                return false;
    return true;
}

inline bool operator!=(const Matrix3& lhs, const Matrix3& rhs)
{
    return !(lhs == rhs);
}

inline const Matrix3 operator*(const Matrix3& lhs, const Matrix3& rhs)
{
    Matrix3 result;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            result.m[i][j] = lhs.m[i][0] * rhs.m[0][j]
                           + lhs.m[i][1] * rhs.m[1][j]
                           + lhs.m[i][2] * rhs.m[2][j];
        }
    }
    return result;
}

inline const Vector2 operator*(const Vector2& point, const Matrix3& matrix)
{
    return matrix.transformPoint(point);
}

NS_WY3D_GEOM_END

#endif // WY3D_MATRIX3_H