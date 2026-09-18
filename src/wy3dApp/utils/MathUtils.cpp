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

#include "MathUtils.h"
#include <algorithm>
#include <gp_Quaternion.hxx>
#include <gp_Ax3.hxx>
#include <gp_Trsf.hxx>
#include <wyVector2.h>
#include <wyVector3.h>
#include <wy3dMath.h>
#include <wy3dCurveIntersectionUtil.h>
#include <wy3dImpl.h>

gp_Vec MathUtils::kAxisX(1.0, 0.0, 0.0);
gp_Vec MathUtils::kAxisY(0.0, 1.0, 0.0);
gp_Vec MathUtils::kAxisZ(0.0, 0.0, 1.0);

osg::Matrix MathUtils::createMatrix(const wy::Vector3& pos, const wy::Vector3& rot)
{
    osg::Matrix matrix;
    // 移动
    matrix.makeTranslate(pos.x(), pos.y(), pos.z());
    // 旋转(四元数) TODO:此处有优化的空间,可以一次性构造出四元数,而不是三个四元数做乘法
    gp_Quaternion quat;
    gp_Quaternion rotZ(kAxisZ, rot.z());
    gp_Quaternion rotX(kAxisX, rot.x());
    gp_Quaternion rotY(kAxisY, rot.y());
    quat = rotY * rotX * rotZ;
    matrix.setRotate(osg::Quat(quat.X(), quat.Y(), quat.Z(), quat.W()));

    return matrix;
}

gp_Trsf MathUtils::createTrsf(const wy::Vector3& pos, const wy::Vector3& rot)
{
    gp_Trsf trsf;
    // 平移
    trsf.SetTranslationPart(gp_Vec(pos.x(), pos.y(), pos.z()));
    // 旋转:Z-->X-->Y
    gp_Quaternion quat;
    gp_Quaternion rotZ(kAxisZ, rot.z());
    gp_Quaternion rotX(kAxisX, rot.x());
    gp_Quaternion rotY(kAxisY, rot.y());
    quat = rotY * rotX * rotZ;
    trsf.SetRotationPart(quat);

    return trsf;
}

void MathUtils::quaternionToEulerZXY(const osg::Quat& quaternion, double& yaw, double& roll, double& pitch)
{
    double w = quaternion.w();
    double x = quaternion.x();
    double y = quaternion.y();
    double z = quaternion.z();
    
    double sinRoll = std::clamp(2 * (w * x - y * z), -1.0, 1.0);
    
    // 检查是否接近万向节锁
    if (std::fabs(std::fabs(sinRoll) - 1.0) <= 1e-5)
    {
        // 接近万向节锁,此时pitch角度为0
        pitch = 0;
        roll = sinRoll > 0 ? wy3d::PI_2 : -wy3d::PI_2;
        yaw = std::atan2(2 * (w * z - x * y), 1 - 2 * (y * y + z * z));
    }
    else // 正常情况
    {
        roll = std::asin(sinRoll);
        yaw = std::atan2(2 * (w * z + x * y), 1 - 2 * (z * z + x * x));
        pitch = std::atan2(2 * (w * y + z * x), 1 - 2 * (x * x + y * y));
    }
}

void MathUtils::quaternionToEulerZXY(const gp_Quaternion& quat, double& yaw, double& roll, double& pitch)
{
    return quaternionToEulerZXY(osg::Quat(quat.X(), quat.Y(), quat.Z(), quat.W()), yaw, roll, pitch);
}

wy::Vector3 MathUtils::quaternionToEulerZXY(const osg::Quat& quat)
{
    double z(0.0), x(0.0), y(0.0);
    quaternionToEulerZXY(quat, z, x, y);
    return wy::Vector3(x, y, z);
}

wy::Vector3 MathUtils::quaternionToEulerZXY(const gp_Quaternion& quat)
{
    double z(0.0), x(0.0), y(0.0);
    quaternionToEulerZXY(quat, z, x, y);
    return wy::Vector3(x, y, z);
}

wy::Vector3 MathUtils::computeEulerZXY(const wy3d::SketchPlane& workPln)
{
    wy::Vector3 xAxis = workPln.getXDir();
    wy::Vector3 yAxis = workPln.getYDir();
    wy::Vector3 zAxis = workPln.getNormal();
    if (1) // 使用OSG
    {
        osg::Matrixd transform;
        transform.set(
            xAxis.x(), xAxis.y(), xAxis.z(), 0.0,
            yAxis.x(), yAxis.y(), yAxis.z(), 0.0,
            zAxis.x(), zAxis.y(), zAxis.z(), 0.0,
            0.0, 0.0, 0.0, 1.0);
        osg::Quat quat = transform.getRotate();
        return MathUtils::quaternionToEulerZXY(quat);
    }
    else // 使用OCC
    {
        // 世界坐标系
        gp_Ax3 worldCoordinateSystem(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1), gp_Dir(1, 0, 0));
        // 创建本地坐标系的轴系
        gp_Ax3 localCoordinateSystem(gp_Pnt(0, 0, 0), gp_Dir(zAxis.x(), zAxis.y(), zAxis.z()), gp_Dir(xAxis.x(), xAxis.y(), xAxis.z()));
        // 创建变换对象
        gp_Trsf transformation;
        // 设置从世界坐标系到本地坐标系的变换
        transformation.SetTransformation(localCoordinateSystem, worldCoordinateSystem);
        // 欧拉角
        return MathUtils::quaternionToEulerZXY(transformation.GetRotation());
    }
}

osg::Quat MathUtils::computeQuat(const wy3d::SketchPlane& workPln)
{
    wy::Vector3 xAxis = workPln.getXDir();
    wy::Vector3 yAxis = workPln.getYDir();
    wy::Vector3 zAxis = workPln.getNormal();
    osg::Matrixd transform;
    transform.set(
        xAxis.x(), xAxis.y(), xAxis.z(), 0.0,
        yAxis.x(), yAxis.y(), yAxis.z(), 0.0,
        zAxis.x(), zAxis.y(), zAxis.z(), 0.0,
        0.0, 0.0, 0.0, 1.0);
    return transform.getRotate();
}

double MathUtils::getRotateAngle(const osg::Vec3d& v1, const osg::Vec3d& v2, const osg::Vec3d& refDir)
{
    // 获取夹角
    double temp = v1.length() * v2.length();
    if (temp < 1e-5)
    {
        return 0.0;
    }
    double angle = std::acos((v1 * v2) / temp);

    // 依据参照方向确定顺逆时针
    if ((v1 ^ v2) * refDir >= 0)
    {
        return angle;
    }
    else
    {
        return wy3d::PI * 2 - angle;
    }
}

double MathUtils::getRotateAngle(const wy::Vector3& v1, const wy::Vector3& v2, const wy::Vector3& refDir)
{
    // 获取夹角
    double temp = v1.length() * v2.length();
    if (temp < 1e-5)
    {
        return 0.0;
    }
    double angle = std::acos(v1.dot(v2) / temp);

    // 依据参照方向确定顺逆时针
    if (v1.cross(v2).dot(refDir) >= 0)
    {
        return angle;
    }
    else
    {
        return wy3d::PI * 2 - angle;
    }
}

wy::Vector2 MathUtils::rotateAround(const wy::Vector2& pnt, const wy::Vector2& centerPnt, double angle)
{
    // 平移：将 pnt 平移到以 centerPnt 为原点的坐标系
    double dx = pnt.x() - centerPnt.x();
    double dy = pnt.y() - centerPnt.y();

    // 旋转：使用旋转矩阵
    double cosTheta = std::cos(angle);
    double sinTheta = std::sin(angle);
    double newX = cosTheta * dx - sinTheta * dy;
    double newY = sinTheta * dx + cosTheta * dy;

    // 平移回去：将旋转后的点平移回原来的位置
    return wy::Vector2(newX + centerPnt.x(), newY + centerPnt.y());
}

bool MathUtils::computeCircleBy3Points(const wy::Vector2& p1, const wy::Vector2& p2, const wy::Vector2& p3,
    wy::Vector2& center, double& radius)
{
    if (p1 == p2 || p2 == p3 || p1 == p3)
    {
        return false;
    }

    // 计算向量
    wy::Vector2 v1 = p2 - p1;
    wy::Vector2 v2 = p3 - p2;

    // 计算中点
    wy::Vector2 mid1 = (p1 + p2) / 2;
    wy::Vector2 mid2 = (p2 + p3) / 2;

    // 计算垂直向量（旋转90度）
    wy::Vector2 normal1(-v1.y(), v1.x());
    wy::Vector2 normal2(-v2.y(), v2.x());

    // 检查向量是否平行（三点共线）
    double crossNormals = normal1.cross(normal2);
    if (std::fabs(crossNormals) <= wy3d::EPS)
    {
        return false;
    }

    // 构建参数方程：
    // mid1 + t * normal1 = mid2 + s * normal2
    // 转换为线性方程组：
    // t * normal1 - s * normal2 = mid2 - mid1
    wy::Vector2 d = mid2 - mid1;
    double det = normal1.x() * normal2.y() - normal1.y() * normal2.x();

    // 使用克莱姆法则求解参数
    double t = (d.x() * normal2.y() - d.y() * normal2.x()) / det;

    // 计算圆心坐标
    center = mid1 + t * normal1;

    // 计算半径（圆心到任意一点的距离）
    radius = (center - p1).length();

    return true;
}

// p1 --- 端点
// p2 --- 端点
// p3 --- 圆弧上的第三点
bool MathUtils::computeArcBy3Points(const wy::Vector2& p1, const wy::Vector2& p2, const wy::Vector2& p3,
    wy::Vector2& center, double& radius, double& startAngle, double& endAngle)
{
    // 检查点是否重合
    if (p1 == p2 || p2 == p3 || p1 == p3)
    {
        return false;
    }

    // 计算向量
    wy::Vector2 v1 = p2 - p1;
    wy::Vector2 v2 = p3 - p2;

    // 计算中点
    wy::Vector2 mid1 = (p1 + p2) / 2;
    wy::Vector2 mid2 = (p2 + p3) / 2;

    // 计算垂直向量（旋转90度）
    wy::Vector2 normal1(-v1.y(), v1.x());
    wy::Vector2 normal2(-v2.y(), v2.x());

    // 检查向量是否平行（三点共线）
    double crossNormals = normal1.cross(normal2);
    if (std::fabs(crossNormals) <= wy3d::EPS)
    {
        return false;
    }

    // 构建参数方程：
    // mid1 + t * normal1 = mid2 + s * normal2
    // 转换为线性方程组：
    // t * normal1 - s * normal2 = mid2 - mid1
    wy::Vector2 d = mid2 - mid1;
    double det = normal1.x() * normal2.y() - normal1.y() * normal2.x();

    // 使用克莱姆法则求解参数
    double t = (d.x() * normal2.y() - d.y() * normal2.x()) / det;

    // 计算圆心坐标
    center = mid1 + t * normal1;

    // 计算半径（圆心到任意一点的距离）
    radius = (center - p1).length();

    // 计算角度（使用atan2，范围[-π, π]）
    double angle1 = std::atan2(p1.y() - center.y(), p1.x() - center.x());
    double angle2 = std::atan2(p2.y() - center.y(), p2.x() - center.x());
    double angle3 = std::atan2(p3.y() - center.y(), p3.x() - center.x());

    // 将角度转换为[0, 2π)范围
    angle1 = wy3d::normalizeRadian(angle1);
    angle2 = wy3d::normalizeRadian(angle2);
    angle3 = wy3d::normalizeRadian(angle3);

    // 设置起始角和终止角
    startAngle = angle1;
    endAngle = angle2;
    double middleAngle = angle3;

    // 找出圆弧方向
    if (endAngle < startAngle) endAngle += wy3d::TWO_PI;
    if (middleAngle < startAngle) middleAngle += wy3d::TWO_PI;
    if (middleAngle < endAngle)
    {
        return true;
    }
    else
    {
        std::swap(startAngle, endAngle);
        return true;
    }
}

wy3d::geom::Arc2 MathUtils::computeArcFromThreePoints(const wy::Vector2& p1, const wy::Vector2& p2, const wy::Vector2& p3)
{
    double x1 = p1.x(), y1 = p1.y();
    double x2 = p2.x(), y2 = p2.y();
    double x3 = p3.x(), y3 = p3.y();

    // 计算叉积来判断是否共线
    double crossProduct = (x2 - x1) * (y3 - y1) - (y2 - y1) * (x3 - x1); // p1,p2,p3任意两点相同时,叉积结果为0
    const double EPSILON = 1e-10; // 容差
    if (std::fabs(crossProduct) < EPSILON)
    {
        // 返回一个无效的圆弧
        return wy3d::geom::Arc2(wy::Vector2(0.0, 0.0), 0.0, 0.0, 0.0);
    }

    // 计算中点
    double mx1 = (x1 + x2) / 2.0, my1 = (y1 + y2) / 2.0;
    double mx2 = (x2 + x3) / 2.0, my2 = (y2 + y3) / 2.0;

    // 计算斜率，处理垂直线的情况
    double m1, m2;
    bool isVertical1 = false, isVertical2 = false;

    if (x2 != x1) {
        m1 = (y2 - y1) / (x2 - x1);  // 普通斜率计算
    }
    else {
        m1 = 0;  // 垂直线，斜率无穷大，设为 0 表示特殊情况
        isVertical1 = true;
    }

    if (x3 != x2) {
        m2 = (y3 - y2) / (x3 - x2);  // 普通斜率计算
    }
    else {
        m2 = 0;  // 垂直线，斜率无穷大，设为 0 表示特殊情况
        isVertical2 = true;
    }

    // 计算中垂线的斜率
    double m_perp1, m_perp2;

    if (isVertical1) {
        m_perp1 = 0;  // 垂直线的中垂线是水平线
    }
    else {
        m_perp1 = -1 / m1;  // 正常的中垂线斜率计算
    }

    if (isVertical2) {
        m_perp2 = 0;  // 垂直线的中垂线是水平线
    }
    else {
        m_perp2 = -1 / m2;  // 正常的中垂线斜率计算
    }

    // 计算中垂线的方程
    double b1 = my1 - m_perp1 * mx1;
    double b2 = my2 - m_perp2 * mx2;

    // 求解中垂线交点，得到圆心
    double cx, cy;

    if (isVertical1) {
        // 如果第一条线是垂直线，则直接使用它的中点，x 坐标为常数
        cx = mx1;
        cy = m_perp2 * cx + b2;
    }
    else if (isVertical2) {
        // 如果第二条线是垂直线，则直接使用它的中点，x 坐标为常数
        cx = mx2;
        cy = m_perp1 * cx + b1;
    }
    else {
        // 普通情况，解这两条直线的交点
        cx = (b2 - b1) / (m_perp1 - m_perp2);
        cy = m_perp1 * cx + b1;
    }

    wy::Vector2 center(cx, cy);

    // 计算半径
    double radius = (center - p1).length();
    if (radius < wy3d::kMinValue)
    {
        return wy3d::geom::Arc2(wy::Vector2(0, 0), 0, 0, 0);
    }

    // 计算起始角度和终止角度
    double startAngle = std::atan2(p1.y() - cy, p1.x() - cx);  // (-PI,PI]
    double endAngle = std::atan2(p3.y() - cy, p3.x() - cx);    // (-PI,PI]
    double middleAngle = std::atan2(p2.y() - cy, p2.x() - cx); // (-PI,PI]
    if (startAngle == endAngle)
    {
        return wy3d::geom::Arc2(wy::Vector2(0, 0), 0, 0, 0);
    }

    // 调整角度到[0,2PI)
    double TwoPI = wy3d::PI * 2;
    if (startAngle < 0.0) startAngle += TwoPI;
    if (endAngle < 0.0) endAngle += TwoPI;
    if (middleAngle < 0.0) middleAngle += TwoPI;

    // 找出圆弧方向
    if (endAngle < startAngle) endAngle += TwoPI;
    if (middleAngle < startAngle) middleAngle += TwoPI;
    // 此时startAngle的角度范围是[0,2PI),endAngle有可能大于2PI
    if (middleAngle < endAngle)
    {
        if (endAngle > TwoPI) endAngle -= TwoPI;
        return wy3d::geom::Arc2(center, radius, startAngle, endAngle);
    }
    else
    {
        if (endAngle > TwoPI) endAngle -= TwoPI;
        return wy3d::geom::Arc2(center, radius, endAngle, startAngle);
    }
}

wy::Vector2 MathUtils::getPointAtEllipse(
    const wy::Vector2& center,
    const wy::Vector2& majorAxis,
    double radiusRatio,
    double t)
{
    if (t < 0.0) t = 0.0;
    else if (t > 1.0) t = 1.0;

    double majorRadius = majorAxis.length();
    double minorRadius = majorRadius * radiusRatio;

    // 局部点坐标
    double angle = ellipseGeometricToParametricAngle(wy3d::PI * 2 * t, majorRadius, minorRadius);
    double x = majorRadius * std::cos(angle);
    double y = minorRadius * std::sin(angle);

    // 旋转
    double majorAxisAngle = std::atan2(majorAxis.y(), majorAxis.x());
    double cosTheta = std::cos(majorAxisAngle);
    double sinTheta = std::sin(majorAxisAngle);
    double worldX = cosTheta * x - sinTheta * y;
    double worldY = sinTheta * x + cosTheta * y;

    return wy::Vector2(center.x() + worldX, center.y() + worldY);
}

bool MathUtils::tangentLinesOfCircleCircle(
    const wy::Vector2& center1st, double radius1st,
    const wy::Vector2& center2nd, double radius2nd,
    std::vector<std::pair<wy::Vector2, wy::Vector2>>& tangentLines,
    double tol)
{
    tangentLines.clear();
    tangentLines.reserve(4);

    // 圆心重合没有切线
    wy::Vector2 vec = center2nd - center1st;
    double dis = vec.length();
    if (dis <= tol)
    {
        return false;
    }

    // 圆退化为点也不处理
    if (radius1st <= tol || radius2nd <= tol)
    {
        return false;
    }

    // 归一化圆心向量
    wy::Vector2 u = vec.normalized();
    wy::Vector2 v(-u.y(), u.x());  // 垂直向量(逆时针旋转90度)

    // 1. 计算外切线 (外部公切线)
    double r_diff = std::fabs(radius1st - radius2nd);
    if (dis > r_diff + tol) // 严格大于
    {
        double cos_theta = r_diff / dis;
        cos_theta = std::clamp(cos_theta, -1.0, 1.0);

        // 安全计算正弦值，避免 NaN
        double sin_theta_sq = 1.0 - cos_theta * cos_theta;
        if (sin_theta_sq < 0) sin_theta_sq = 0;  // 防止浮点误差导致负数
        double sin_theta = std::sqrt(sin_theta_sq);

        // 计算方向向量
        double sign = (radius1st > radius2nd) ? 1.0 : -1.0;
        wy::Vector2 dir1 = u * (sign * cos_theta) + v * sin_theta;
        wy::Vector2 dir2 = u * (sign * cos_theta) - v * sin_theta;

        // 添加外切线
        tangentLines.emplace_back(
            center1st + dir1 * radius1st,
            center2nd + dir1 * radius2nd);

        tangentLines.emplace_back(
            center1st + dir2 * radius1st,
            center2nd + dir2 * radius2nd);
    }

    // 2. 计算内切线 (内部公切线)
    if (dis > (radius1st + radius2nd + tol)) // 严格大于
    {
        double cos_phi = (radius1st + radius2nd) / dis;
        cos_phi = std::clamp(cos_phi, -1.0, 1.0);

        // 安全计算正弦值
        double sin_phi_sq = 1.0 - cos_phi * cos_phi;
        if (sin_phi_sq < 0) sin_phi_sq = 0;
        double sin_phi = std::sqrt(sin_phi_sq);

        // 计算方向向量
        wy::Vector2 dir3 = u * cos_phi + v * sin_phi;
        wy::Vector2 dir4 = u * cos_phi - v * sin_phi;

        // 添加内切线
        tangentLines.emplace_back(
            center1st + dir3 * radius1st,
            center2nd - dir3 * radius2nd);

        tangentLines.emplace_back(
            center1st + dir4 * radius1st,
            center2nd - dir4 * radius2nd);
    }

    return !tangentLines.empty();
}

bool MathUtils::tangentLinesOfCircleArc(
    const wy::Vector2& center1st, double radius1st,
    const wy::Vector2& center2nd, double radius2nd, double startAngle, double endAngle,
    std::vector<std::pair<wy::Vector2, wy::Vector2>>& tangentLines,
    double tol)
{
    tangentLines.clear();
    tangentLines.reserve(4);

    // 计算两个完整圆之间的所有切线
    std::vector<std::pair<wy::Vector2, wy::Vector2>> fullTangentLines;
    if (!tangentLinesOfCircleCircle(
        center1st, radius1st, center2nd, radius2nd, fullTangentLines, tol))
    {
        return false;
    }

    // 过滤切线
    for (const auto& pair : fullTangentLines)
    {
        double angle = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), pair.second - center2nd);
        if (wy3d::isAngleInArc(angle, startAngle, endAngle))
        {
            tangentLines.emplace_back(pair);
        }
    }

    return !tangentLines.empty();
}

bool MathUtils::tangentLinesOfArcArc(
    const wy::Vector2& center1st, double radius1st, double startAngle1st, double endAngle1st,
    const wy::Vector2& center2nd, double radius2nd, double startAngle2nd, double endAngle2nd,
    std::vector<std::pair<wy::Vector2, wy::Vector2>>& tangentLines,
    double tol)
{
    tangentLines.clear();
    tangentLines.reserve(4);

    // 计算两个完整圆之间的所有切线
    std::vector<std::pair<wy::Vector2, wy::Vector2>> fullTangentLines;
    if (!tangentLinesOfCircleCircle(
        center1st, radius1st, center2nd, radius2nd, fullTangentLines, tol))
    {
        return false;
    }

    // 过滤切线
    for (const auto& pair : fullTangentLines)
    {
        double angle1st = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), pair.first - center1st);
        if (!wy3d::isAngleInArc(angle1st, startAngle1st, endAngle1st))
        {
            continue;
        }

        double angle2nd = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), pair.second - center2nd);
        if (!wy3d::isAngleInArc(angle2nd, startAngle2nd, endAngle2nd))
        {
            continue;
        }

        tangentLines.emplace_back(pair);
    }

    return !tangentLines.empty();
}