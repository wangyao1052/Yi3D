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

#ifndef WY3DAPP_SKETCH_SNAP_OBJECT_H
#define WY3DAPP_SKETCH_SNAP_OBJECT_H

#include <memory>
#include <wyVector2.h>
#include <wy3dVector2.h>
#include <wydbElementId.h>
#include <wydbElement.h>
#include <wy3dSketchCurve.h>
#include <wy3dApp/Snap2dType.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

// 草图捕捉对象
class SketchSnapObject
{
public:
    // 计算平行角度[0, PI)
    inline static double computeParallelAngle(const wy::Vector2& startPnt, const wy::Vector2& endPnt)
    {
        double angle = wy::Vector2::rotationAngle(wy::Vector2::kXAxis, endPnt - startPnt); // [0, 2PI)
        if (angle >= wy3d::PI) angle -= wy3d::PI;
        assert(angle >= 0.0 && angle < wy3d::PI);
        return angle;
    }

    // 计算垂直角度[0, PI)
    inline static double computePerpendicularAngle(const wy::Vector2& startPnt, const wy::Vector2& endPnt)
    {
        double angle = wy::Vector2::rotationAngle(wy::Vector2::kXAxis,
            wy::Vector2(-endPnt.y() + startPnt.y(), endPnt.x() - startPnt.x())); // [0, 2PI)
        if (angle >= wy3d::PI) angle -= wy3d::PI;
        assert(angle >= 0.0 && angle < wy3d::PI);
        return angle;
    }

public:
    explicit SketchSnapObject(const wydb::ElementId& id) : _id(id) {}
    virtual ~SketchSnapObject() {}

    // 类型
    virtual Snap2dType getType() const = 0;

    // 引导线参照点(默认原点,非对齐类不消费)
    virtual wy::Vector2 getRefPoint() const { return wy::Vector2(); }

    // 元素ID
    wydb::ElementId getId() const { return _id; }

private:
    wydb::ElementId _id; // id
};
typedef std::shared_ptr<SketchSnapObject> SketchSnapObjectSPtr;

// 角度捕捉对象
class AngleSnapObject : public SketchSnapObject
{
public:
    explicit AngleSnapObject(const wydb::ElementId& id, double angle = 0.0)
        : SketchSnapObject(id), _angle(angle) {}

    // 获取角度
    double getValue() const { return _angle; }

protected:
    double _angle;
};
typedef std::shared_ptr<AngleSnapObject> AngleSnapObjectSPtr;

// 水平捕捉对象
class HorizontalSnapObject : public AngleSnapObject
{
public:
    HorizontalSnapObject() : AngleSnapObject(wydb::ElementId::kNull, 0.0) {}

    // 类型
    virtual Snap2dType getType() const override { return Snap2dType::Horizontal; }
};
typedef std::shared_ptr<HorizontalSnapObject> HorizontalSnapObjectSPtr;

// 竖直捕捉对象
class VerticalSnapObject : public AngleSnapObject
{
public:
    VerticalSnapObject() : AngleSnapObject(wydb::ElementId::kNull, wy3d::PI_2) {}

    // 类型
    virtual Snap2dType getType() const override { return Snap2dType::Vertical; }
};
typedef std::shared_ptr<VerticalSnapObject> VerticalSnapObjectSPtr;

// 平行捕捉对象
class ParallelSnapObject : public AngleSnapObject
{
public:
    ParallelSnapObject(const wydb::ElementId& id, const wy::Vector2& startPnt, const wy::Vector2& endPnt);

    // 类型
    virtual Snap2dType getType() const override { return Snap2dType::Parallel; }
};
typedef std::shared_ptr<ParallelSnapObject> ParallelSnapObjectSPtr;

// 垂直捕捉对象
class PerpendicularSnapObject : public AngleSnapObject
{
public:
    PerpendicularSnapObject(const wydb::ElementId& id, const wy::Vector2& startPnt, const wy::Vector2& endPnt);

    // 类型
    virtual Snap2dType getType() const override { return Snap2dType::Perpendicular; }
};
typedef std::shared_ptr<PerpendicularSnapObject> PerpendicularSnapObjectSPtr;

// 对齐X捕捉对象
class AlignXSnapObject : public SketchSnapObject
{
public:
    explicit AlignXSnapObject(const wydb::ElementId& id, const wy::Vector2& pnt)
        : SketchSnapObject(id), _pnt(pnt) {}

    // 类型
    virtual Snap2dType getType() const override { return Snap2dType::AlignX; }

    // 值
    double getValue() const { return _pnt.x(); }
    // 引导线参照点
    virtual wy::Vector2 getRefPoint() const override { return _pnt; }

private:
    wy::Vector2 _pnt;
};
typedef std::shared_ptr<AlignXSnapObject> AlignXSnapObjectSPtr;

// 对齐Y捕捉对象
class AlignYSnapObject : public SketchSnapObject
{
public:
    explicit AlignYSnapObject(const wydb::ElementId& id, const wy::Vector2& pnt)
        : SketchSnapObject(id), _pnt(pnt) {}

    // 类型
    virtual Snap2dType getType() const override { return Snap2dType::AlignY; }

    // 值
    double getValue() const { return _pnt.y(); }
    // 引导线参照点
    virtual wy::Vector2 getRefPoint() const override { return _pnt; }

private:
    wy::Vector2 _pnt;
};
typedef std::shared_ptr<AlignYSnapObject> AlignYSnapObjectSPtr;

// 相等捕捉:长度
class EqualLengthSnapObject : public SketchSnapObject
{
public:
    EqualLengthSnapObject(const wydb::ElementId& id, double length, double lineAngle)
        : SketchSnapObject(id), _length(length), _lineAngle(lineAngle) {}

    // 类型
    virtual Snap2dType getType() const override { return Snap2dType::EqualLength; }

    // 长度
    double getLength() const { return _length; }
    // 直线角度[0, PI),仅平行或垂直的直线段才捕捉相等
    double getLineAngle() const { return _lineAngle; }

private:
    double _length;
    double _lineAngle;
};
typedef std::shared_ptr<EqualLengthSnapObject> EqualLengthSnapObjectSPtr;

// 相等捕捉:半径
class EqualRadiusSnapObject : public SketchSnapObject
{
public:
    explicit EqualRadiusSnapObject(const wydb::ElementId& id, double radius)
        : SketchSnapObject(id), _radius(radius) {}

    // 类型
    virtual Snap2dType getType() const override { return Snap2dType::EqualRadius; }

    // 半径
    double getRadius() const { return _radius; }

private:
    double _radius;
};
typedef std::shared_ptr<EqualRadiusSnapObject> EqualRadiusSnapObjectSPtr;

// 最近点
class NearestSnapObject : public SketchSnapObject
{
public:
    explicit NearestSnapObject(const wydb::ElementId& id)
        : SketchSnapObject(id) {}

    // 类型
    virtual Snap2dType getType() const override { return Snap2dType::Nearest; }
};
typedef std::shared_ptr<NearestSnapObject> NearestSnapObjectSPtr;

// 点捕捉对象
class PointSnapObject : public SketchSnapObject
{
public:
    PointSnapObject(const wydb::ElementId& id, const wy::Vector2& point)
        : SketchSnapObject(id), _point(point) {}

    // 获取值
    wy::Vector2 getValue() const { return _point; }

private:
    wy::Vector2 _point;
};
typedef std::shared_ptr<PointSnapObject> PointSnapObjectSPtr;

// 垂足捕捉对象
class PerpendicularPointSnapObject : public PointSnapObject
{
public:
    PerpendicularPointSnapObject(const wydb::ElementId& id, const wy::Vector2& point)
        : PointSnapObject(id, point) {}

    // 类型
    virtual Snap2dType getType() const override { return Snap2dType::PerpendicularPoint; }
};
typedef std::shared_ptr<PerpendicularPointSnapObject> PerpendicularPointSnapObjectSPtr;

// 切点捕捉对象
class TangentPointSnapObject : public PointSnapObject
{
public:
    TangentPointSnapObject(const wydb::ElementId& id, const wy::Vector2& point)
        : PointSnapObject(id, point) {}

    // 类型
    virtual Snap2dType getType() const override { return Snap2dType::TangentPoint; }
};
typedef std::shared_ptr<TangentPointSnapObject> TangentPointSnapObjectSPtr;

// 交点捕捉对象
class IntersectionPointSnapObject : public PointSnapObject
{
public:
    IntersectionPointSnapObject(const wydb::ElementId& id, const wy::Vector2& point)
        : PointSnapObject(id, point) {}

    // 类型
    virtual Snap2dType getType() const override { return Snap2dType::IntersectionPoint; }
};
typedef std::shared_ptr<IntersectionPointSnapObject> IntersectionPointSnapObjectSPtr;

// 对齐点捕捉对象(两坐标精确吸附,类型为 AlignX/AlignY)
class AlignPointSnapObject : public PointSnapObject
{
public:
    AlignPointSnapObject(
        const wydb::ElementId& id,
        const wy::Vector2& point,
        const wy::Vector2& refPoint,
        Snap2dType type)
        : PointSnapObject(id, point), _refPoint(refPoint), _type(type) {}

    // 类型
    virtual Snap2dType getType() const override { return _type; }
    // 引导线参照点
    virtual wy::Vector2 getRefPoint() const override { return _refPoint; }

private:
    wy::Vector2 _refPoint;
    Snap2dType _type;
};
typedef std::shared_ptr<AlignPointSnapObject> AlignPointSnapObjectSPtr;

// 默认捕捉系统中的:端点
class EndPointSnapObject : public PointSnapObject
{
public:
    EndPointSnapObject(const wydb::ElementId& id, const wy::Vector2& point)
        : PointSnapObject(id, point) {}

    // 类型
    virtual Snap2dType getType() const override { return Snap2dType::EndPoint; }
};
typedef std::shared_ptr<EndPointSnapObject> EndPointSnapObjectSPtr;

// 默认捕捉系统中的:象限点
class QuadrantPointSnapObject : public PointSnapObject
{
public:
    QuadrantPointSnapObject(const wydb::ElementId& id, const wy::Vector2& point)
        : PointSnapObject(id, point) {}

    // 类型
    virtual Snap2dType getType() const override { return Snap2dType::QuadrantPoint; }
};
typedef std::shared_ptr<QuadrantPointSnapObject> QuadrantPointSnapObjectSPtr;

// 默认捕捉系统中的:中点
class MiddlePointSnapObject : public PointSnapObject
{
public:
    MiddlePointSnapObject(const wydb::ElementId& id, const wy::Vector2& point)
        : PointSnapObject(id, point) {}

    // 类型
    virtual Snap2dType getType() const override { return Snap2dType::MiddlePoint; }
};
typedef std::shared_ptr<MiddlePointSnapObject> MiddlePointSnapObjectSPtr;

// 默认捕捉系统中的:中心点
class CenterPointSnapObject : public PointSnapObject
{
public:
    CenterPointSnapObject(const wydb::ElementId& id, const wy::Vector2& point)
        : PointSnapObject(id, point) {}

    // 类型
    virtual Snap2dType getType() const override { return Snap2dType::CenterPoint; }
};
typedef std::shared_ptr<CenterPointSnapObject> CenterPointSnapObjectSPtr;

// 默认捕捉系统中的:原点
class OriginSnapObject : public SketchSnapObject
{
public:
    OriginSnapObject() : SketchSnapObject(wydb::ElementId::kNull) {}

    // 类型
    virtual Snap2dType getType() const override { return Snap2dType::Origin; }
};
typedef std::shared_ptr<OriginSnapObject> OriginSnapObjectSPtr;

// 一般点(样条首点)
class GenericPointSnapObject : public PointSnapObject
{
public:
    explicit GenericPointSnapObject(const wy::Vector2& point)
        : PointSnapObject(wydb::ElementId::kNull, point) {}

    // 类型
    virtual Snap2dType getType() const override { return Snap2dType::GenericPoint; }
};
typedef std::shared_ptr<GenericPointSnapObject> GenericPointSnapObjectSPtr;

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_SNAP_OBJECT_H
