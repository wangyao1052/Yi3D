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

#ifndef WY3DAPP_MATH_UTILS_H
#define WY3DAPP_MATH_UTILS_H

#include <osg/Vec2d>
#include <osg/Vec3>
#include <osg/Vec3d>
#include <osg/Matrix>
#include <osg/Quat>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <gp_Dir.hxx>
#include <gp_Trsf.hxx>
#include <gp_Quaternion.hxx>
#include <wyVector2.h>
#include <wyVector3.h>
#include <wy3dMath.h>
#include <wy3dVector2.h>
#include <wy3dVector3.h>
#include <geom/wy3dArc2.h>
#include <wy3dSketchPlane.h>

class MathUtils
{
public:
    // 创建矩阵
    // pos --- 位移
    // rot --- 旋转欧拉角(Z-->X-->Y)
    static osg::Matrix createMatrix(const wy::Vector3& pos, const wy::Vector3& rot);

    // 创建Transformation
    // pos --- 位移
    // rot --- 旋转欧拉角(Z-->X-->Y)
    static gp_Trsf createTrsf(const wy::Vector3& pos, const wy::Vector3& rot);

    // 四元数转欧拉角(Z-->X-->Y)
    // quat  --- 四元数
    // yaw   --- Z轴旋转角度(弧度)
    // roll  --- X轴旋转角度(弧度)
    // pitch --- Y轴旋转角度(弧度)
    static void quaternionToEulerZXY(const osg::Quat& quat, double& yaw, double& roll, double& pitch);
    static void quaternionToEulerZXY(const gp_Quaternion& quat, double& yaw, double& roll, double& pitch);
    static wy::Vector3 quaternionToEulerZXY(const osg::Quat& quat);
    static wy::Vector3 quaternionToEulerZXY(const gp_Quaternion& quat);

    // 计算工作平面的旋转欧拉角(Z-->X-->Y)
    static wy::Vector3 computeEulerZXY(const wy3d::SketchPlane& workPln);

    // 计算工作平面的四元数
    static osg::Quat computeQuat(const wy3d::SketchPlane& workPln);

    // 计算向量的旋转角度(v1-->v2)
    // refDir --- 参照方向向量
    static double getRotateAngle(const osg::Vec3d& v1, const osg::Vec3d& v2, const osg::Vec3d& refDir);
    static double getRotateAngle(const wy::Vector3& v1, const wy::Vector3& v2, const wy::Vector3& refDir);

    // 计算二维向量点绕中心点旋转之后的坐标值(by ChatGPT)
    static wy::Vector2 rotateAround(const wy::Vector2& pnt, const wy::Vector2& centerPnt, double angle);

    // 数据转换
    static inline osg::Vec3d toVec3d(const wy::Vector3& vec)
    {
        return osg::Vec3d(vec.x(), vec.y(), vec.z());
    }
    static inline osg::Vec3d toVec3(const wy::Vector3& vec)
    {
        return osg::Vec3(vec.x(), vec.y(), vec.z());
    }
    static inline osg::Vec2d toVec2d(const wy::Vector2& vec)
    {
        return osg::Vec2d(vec.x(), vec.y());
    }

    // 数据转换
    static inline wy::Vector3 toVector3(const osg::Vec3d& vec)
    {
        return wy::Vector3(vec.x(), vec.y(), vec.z());
    }
    static inline wy::Vector3 toVector3(const gp_Pnt& pnt)
    {
        return wy::Vector3(pnt.X(), pnt.Y(), pnt.Z());
    }
    static inline wy::Vector3 toVector3(const gp_Dir& dir)
    {
        return wy::Vector3(dir.X(), dir.Y(), dir.Z());
    }
    static inline wy::Vector3 toVector3(const wy::Vector2& vec)
    {
        return wy::Vector3(vec.x(), vec.y(), 0.0);
    }

    // 数据转换
    static inline wy::Vector2 toVector2(const osg::Vec2d& vec)
    {
        return wy::Vector2(vec.x(), vec.y());
    }
    static inline wy::Vector2 toVector2(const wy::Vector3& vec)
    {
        return wy::Vector2(vec.x(), vec.y());
    }

    // 数据转换
    static inline gp_Pnt toPnt(const wy::Vector3& pnt)
    {
        return gp_Pnt(pnt.x(), pnt.y(), pnt.z());
    }
    static inline gp_Dir toDir(const wy::Vector3& dir)
    {
        return gp_Dir(dir.x(), dir.y(), dir.z());
    }

    // 计算点到直线的距离
    static inline double distancePoint2Line(const wy::Vector2& point, const wy::Vector2& lineStart, const wy::Vector2& lineEnd)
    {
        wy::Vector2 lineVec = lineEnd - lineStart;
        double length = lineVec.length();
        if (length <= wy3d::EPS) // 直线退化为点
        {
            return (point - lineStart).length();
        }
        return std::fabs((point - lineStart).cross(lineVec)) / length;
    }

    // 计算点到直线的垂足
    static inline bool perpendicularFootOnLine(
        const wy::Vector2& point,
        const wy::Vector2& lineOrigin, const wy::Vector2& lineDir,
        wy::Vector2& foot)
    {
        assert(lineDir.length() == 1.0);

        wy::Vector2 pointVec = point - lineOrigin;
        double projLen = pointVec.dot(lineDir);
        if (std::fabs(pointVec.cross(lineDir)) <= wy3d::EPS) // 两向量平行
        {
            return false;
        }
        foot = lineOrigin + projLen * lineDir;
        return true;
    }

    // 计算点到直线段的垂足
    static inline bool perpendicularFootOnLineseg(
        const wy::Vector2& point,
        const wy::Vector2& lineStart, const wy::Vector2& lineEnd,
        wy::Vector2& foot)
    {
        wy::Vector2 lineVec = lineEnd - lineStart;
        double length = lineVec.length();
        if (length <= wy3d::EPS) // 直线段退化为点
        {
            return false;
        }
        lineVec.normalize();

        wy::Vector2 pointVec = point - lineStart;
        double projLen = pointVec.dot(lineVec);
        if (projLen >= 0.0 && projLen <= length)
        {
            if (std::fabs(pointVec.cross(lineVec)) <= wy3d::EPS) // 两向量平行
            {
                return false;
            }
            foot = lineStart + projLen * lineVec;
            return true;
        }
        else
        {
            return false;
        }
    }

    // 计算点到圆的垂足
    static inline bool perpendicularFootsOnCircle(
        const wy::Vector2& point,
        const wy::Vector2& center, double radius,
        wy::Vector2& foot1, wy::Vector2& foot2)
    {
        if (radius <= wy3d::EPS) // 圆退化为点
        {
            return false;
        }

        wy::Vector2 vec = point - center;
        if (vec.length2() <= wy3d::EPS * wy3d::EPS)
        {
            return false; // 点与圆心几乎重合无法确定唯一垂足方向
        }
        vec.normalize();
        vec *= radius;

        foot1 = center + vec;
        foot2 = center - vec;
        return true;
    }

    // 计算点到圆弧的垂足
    static inline bool perpendicularFootsOnArc(
        const wy::Vector2& point,
        const wy::Vector2& center, double radius, double startAngle, double endAngle,
        std::vector<wy::Vector2>& foots)
    {
        assert(radius > 0.0);
        if (radius <= wy3d::EPS) // 圆弧退化为点
        {
            return false;
        }

        wy::Vector2 vec = point - center;
        if (vec.length2() <= wy3d::EPS * wy3d::EPS)
        {
            return false; // 点与圆心几乎重合无法确定唯一垂足方向
        }
        vec.normalize();
        vec *= radius;

        startAngle = wy3d::normalizeRadian(startAngle);
        endAngle = wy3d::normalizeRadian(endAngle);
        if (endAngle < startAngle) endAngle += wy3d::TWO_PI;
        double totalAngle = endAngle - startAngle;

        double pointAngle = wy::Vector2::rotationAngle(wy::Vector2::kXAxis, vec);
        double angles[2] = { pointAngle, pointAngle >= wy3d::PI ?
            pointAngle - wy3d::PI : pointAngle + wy3d::PI };
        for (int i = 0; i <= 1; ++i)
        {
            double angle = angles[i];
            if (angle < startAngle) angle += wy3d::TWO_PI;
            double delta = angle - startAngle;
            if (delta > 0.0 && delta < totalAngle)
            {
                foots.emplace_back(wy::Vector2(
                    center.x() + radius * std::cos(angle),
                    center.y() + radius * std::sin(angle)));
            }
        }

        return !foots.empty();
    }

    // 计算点到圆的切点
    static inline bool tangentPointsToCircle(
        const wy::Vector2& point,
        const wy::Vector2& center, double radius,
        wy::Vector2& tangentPnt1, wy::Vector2& tangentPnt2)
    {
        assert(radius > 0.0);
        wy::Vector2 vec = point - center;
        double dist = vec.length();
        if (dist <= radius + wy3d::EPS)
        {
            return false; // 点在圆内或圆上
        }

        double angle = wy::Vector2::rotationAngle(wy::Vector2::kXAxis, vec);
        double theta = std::acos(radius / dist); // >0 && <1 
        assert(theta > 0.0 && theta < wy3d::PI_2);
        tangentPnt1 = center + wy::Vector2(radius * std::cos(angle + theta), radius * std::sin(angle + theta));
        tangentPnt2 = center + wy::Vector2(radius * std::cos(angle - theta), radius * std::sin(angle - theta));
        return true;
    }

    // 计算点到圆弧的切点
    static inline bool tangentPointsToArc(
        const wy::Vector2& point,
        const wy::Vector2& center, double radius, double startAngle, double endAngle,
        std::vector<wy::Vector2>& tangentPnts)
    {
        assert(radius > 0.0);
        wy::Vector2 vec = point - center;
        double dist = vec.length();
        if (dist <= radius + wy3d::EPS)
        {
            return false; // 点在圆内或圆上
        }

        startAngle = wy3d::normalizeRadian(startAngle);
        endAngle = wy3d::normalizeRadian(endAngle);
        if (endAngle < startAngle) endAngle += wy3d::TWO_PI;
        double totalAngle = endAngle - startAngle;

        double pointAngle = wy::Vector2::rotationAngle(wy::Vector2::kXAxis, vec);
        double theta = std::acos(radius / dist); // >0 && <1
        assert(theta > 0.0 && theta < wy3d::PI_2);
        double angles[2] = {
            wy3d::normalizeRadian(pointAngle + theta),
            wy3d::normalizeRadian(pointAngle - theta) };
        for (int i = 0; i <= 1; ++i)
        {
            double angle = angles[i];
            if (angle < startAngle) angle += wy3d::TWO_PI;
            double delta = angle - startAngle;
            if (delta > 0.0 && delta < totalAngle)
            {
                tangentPnts.push_back(center + wy::Vector2(radius * std::cos(angle), radius * std::sin(angle)));
            }
        }

        return !tangentPnts.empty();
    }

    // 计算两个圆之间的所有切线
    static bool tangentLinesOfCircleCircle(
        const wy::Vector2& center1st, double radius1st,
        const wy::Vector2& center2nd, double radius2nd,
        std::vector<std::pair<wy::Vector2, wy::Vector2>>& tangentLines,
        double tol);

    // 计算圆和圆弧之间的所有切线
    static bool tangentLinesOfCircleArc(
        const wy::Vector2& center1st, double radius1st,
        const wy::Vector2& center2nd, double radius2nd, double startAngle, double endAngle,
        std::vector<std::pair<wy::Vector2, wy::Vector2>>& tangentLines,
        double tol);

    // 计算圆弧与圆弧之间的所有切线
    static bool tangentLinesOfArcArc(
        const wy::Vector2& center1st, double radius1st, double startAngle1st, double endAngle1st,
        const wy::Vector2& center2nd, double radius2nd, double startAngle2nd, double endAngle2nd,
        std::vector<std::pair<wy::Vector2, wy::Vector2>>& tangentLines,
        double tol);

    // 求过三点的圆
    static bool computeCircleBy3Points(const wy::Vector2& p1, const wy::Vector2& p2, const wy::Vector2& p3,
        wy::Vector2& center, double& radius);

    // 求过三点的圆弧
    static bool computeArcBy3Points(const wy::Vector2& p1, const wy::Vector2& p2, const wy::Vector2& p3,
        wy::Vector2& center, double& radius, double& startAngle, double& endAngle);

    // 求过三点的圆弧
    static wy3d::geom::Arc2 computeArcFromThreePoints(const wy::Vector2& p1, const wy::Vector2& p2, const wy::Vector2& p3);

    // 椭圆几何角度转参数角度
    // a --- 椭圆长半轴半径
    // b --- 椭圆短半轴半径
    // 返回值范围[0,2PI)
    static inline double ellipseGeometricToParametricAngle(double phi, double a, double b)
    {
        if (a <= wy3d::EPS || b <= wy3d::EPS)
        {
            assert(false);
            return 0.0;
        }

        double value = std::atan2(a * std::sin(phi), b * std::cos(phi)); // [-PI, +PI]
        if (value < 0.0)
        {
            value += wy3d::TWO_PI;
            // added by wangyao 2025.04.10 {
            // 当value很小时如-1e-16时,由于浮点数精度,value + wy3d::TWO_PI == wy3d::TWO_PI
            if (value == wy3d::TWO_PI)
            {
                value = 0.0;
            }
            // }
        }

        return value;
    }

    // 椭圆参数角度转几何角度
    // a --- 椭圆长半轴半径
    // b --- 椭圆短半轴半径
    // 返回值范围[0,2PI)
    static inline double ellipseParametricToGeometricAngle(double theta, double a, double b)
    {
        if (a <= wy3d::EPS || b <= wy3d::EPS)
        {
            assert(false);
            return 0.0;
        }

        double value = std::atan2(b * std::sin(theta), a * std::cos(theta));
        if (value < 0.0)
        {
            value += wy3d::TWO_PI;
            // added by wangyao 2025.04.10 {
            // 当value很小时如-1e-16时,由于浮点数精度,value + wy3d::TWO_PI == wy3d::TWO_PI
            if (value == wy3d::TWO_PI)
            {
                value = 0.0;
            }
            // }
        }

        return value;
    }

    // 规整化弧度值到范围[0,2PI)
    static inline double normalizeRadian(double rad)
    {
        static const double twoPI = 2 * wy3d::PI;
        double value = std::fmod(rad, twoPI);
        if (value < 0.0) value += twoPI;
        return value;
    }

    // 计算椭圆的参数点
    static wy::Vector2 getPointAtEllipse(const wy::Vector2& center, const wy::Vector2& majorAxis, double radiusRatio, double t);

    // 计算长度角度
    static std::pair<double, double> computeLengthAngle(const wy::Vector2& startPnt, const wy::Vector2& endPnt)
    {
        wy::Vector2 vec = endPnt - startPnt;
        return std::pair<double, double>(
            vec.length(),
            wy::Vector2::rotationAngle(wy::Vector2::kXAxis, vec));
    }

private:
    static gp_Vec kAxisX;
    static gp_Vec kAxisY;
    static gp_Vec kAxisZ;
};

#endif // WY3DAPP_MATH_UTILS_H