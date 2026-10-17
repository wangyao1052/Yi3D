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
    // EXTRA字段可能的值:元素类型
    static const inline short UNDEFINED  = 0;
    static const inline short LINE       = 0x01;
    static const inline short CIRCLE     = 0x02;
    static const inline short ARC        = 0x04;
    static const inline short ELLIPSE    = 0x08;
    static const inline short ELLIPSEARC = 0x10;
    // EXTRA字段可能的值:起点还是终点
    static const inline short START = 0;
    static const inline short END = 1;
    // 坐标轴X
    static const inline short X_AXIS_0 = 0;
    static const inline short X_AXIS_1 = 1;
    static const inline short X_AXIS_2 = 2;
    static const inline short X_AXIS_3 = 3;
    static const inline short X_AXIS_4 = 4;
    // 坐标轴Y
    static const inline short Y_AXIS_0 = 10;
    static const inline short Y_AXIS_1 = 11;
    static const inline short Y_AXIS_2 = 12;
    static const inline short Y_AXIS_3 = 13;
    static const inline short Y_AXIS_4 = 14;

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
    explicit SketchSnapObject(const wydb::ElementId& id, short extra = 0) : _id(id), _extra(extra) {}
    virtual ~SketchSnapObject() {}

    // 类型
    virtual Snap2dType getType() const = 0;

    // 元素ID
    wydb::ElementId getId() const { return _id; }
    // 额外信息
    short getExtra() const { return _extra; }

private:
    wydb::ElementId _id; // id
    short _extra;        // 额外信息
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
    // extra --- 当id表示的元素类型为直线段时,表达是起点还是终点:START or END
    explicit AlignXSnapObject(const wydb::ElementId& id, const wy::Vector2& pnt, short extra = 0)
        : SketchSnapObject(id, extra), _pnt(pnt) {}

    // 类型
    virtual Snap2dType getType() const override { return Snap2dType::AlignX; }

    // 值
    double getValue() const { return _pnt.x(); }
    // 点坐标
    wy::Vector2 getPosition() const { return _pnt; }

private:
    wy::Vector2 _pnt;
};
typedef std::shared_ptr<AlignXSnapObject> AlignXSnapObjectSPtr;

// 对齐Y捕捉对象
class AlignYSnapObject : public SketchSnapObject
{
public:
    // extra --- 当id表示的元素类型为直线段时,表达是起点还是终点:START or END
    explicit AlignYSnapObject(const wydb::ElementId& id, const wy::Vector2& pnt, short extra = 0)
        : SketchSnapObject(id, extra), _pnt(pnt) {}

    // 类型
    virtual Snap2dType getType() const override { return Snap2dType::AlignY; }

    // 值
    double getValue() const { return _pnt.y(); }
    // 点坐标
    wy::Vector2 getPosition() const { return _pnt; }

private:
    wy::Vector2 _pnt;
};
typedef std::shared_ptr<AlignYSnapObject> AlignYSnapObjectSPtr;

// 相等捕捉
class EqualSnapObject : public SketchSnapObject
{
public:
    // extra  --- 对象类型:LINE or CIRCLE or ...
    // value2 --- 对于对象类型为LINE,该值为直线段的角度,因为只有平行或垂直的直线段才捕捉相等,该值用于辅助判断
    EqualSnapObject(const wydb::ElementId& id, double value, short extra, double value2 = 0.0)
        : SketchSnapObject(id, extra), _value(value), _value2(value2) {}

    // 类型 
    virtual Snap2dType getType() const override { return Snap2dType::Equal; }

    // 获取值
    double getValue() const { return _value; }
    double getValue2() const { return _value2; }

private:
    double _value;
    double _value2;
};
typedef std::shared_ptr<EqualSnapObject> EqualSnapObjectSPtr;

// 点在线上
class PointOnCurveSnapObject : public SketchSnapObject
{
public:
    // extra  --- 对象类型:LINE or CIRCLE or ...
    PointOnCurveSnapObject(const wydb::ElementId& id, short extra)
        : SketchSnapObject(id, extra) {}

    // 类型
    virtual Snap2dType getType() const override { return Snap2dType::PointOnCurve; }
};
typedef std::shared_ptr<PointOnCurveSnapObject> PointOnCurveSnapObjectSPtr;

// 点捕捉对象
class PointSnapObject : public SketchSnapObject
{
public:
    // extra  --- 对象类型:LINE or CIRCLE or ...
    PointSnapObject(const wydb::ElementId& id, const wy::Vector2& point, short extra = UNDEFINED)
        : SketchSnapObject(id, extra), _point(point) {}

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
    // extra --- 额外信息
    PerpendicularPointSnapObject(const wydb::ElementId& id, const wy::Vector2& point, short extra)
        : PointSnapObject(id, point, extra) {}

    // 类型
    virtual Snap2dType getType() const override { return Snap2dType::PerpendicularPoint; }
};
typedef std::shared_ptr<PerpendicularPointSnapObject> PerpendicularPointSnapObjectSPtr;

// 切点捕捉对象
class TangentPointSnapObject : public PointSnapObject
{
public:
    // extra --- 额外信息
    TangentPointSnapObject(const wydb::ElementId& id, const wy::Vector2& point, short extra)
        : PointSnapObject(id, point, extra) {}

    // 类型
    virtual Snap2dType getType() const override { return Snap2dType::TangentPoint; }
};
typedef std::shared_ptr<TangentPointSnapObject> TangentPointSnapObjectSPtr;

// 交点捕捉对象
class IntersectionPointSnapObject : public PointSnapObject
{
public:
    // extra  --- 对象类型:LINE or CIRCLE or ...
    IntersectionPointSnapObject(const wydb::ElementId& id, const wy::Vector2& point, short extra)
        : PointSnapObject(id, point, extra) {}

    // 类型
    virtual Snap2dType getType() const override { return Snap2dType::IntersectionPoint; }
};
typedef std::shared_ptr<IntersectionPointSnapObject> IntersectionPointSnapObjectSPtr;

// 关键点捕捉对象
class KeyPointSnapObject : public PointSnapObject
{
public:
    // extra --- 额外信息
    KeyPointSnapObject(
        const wydb::ElementId& id,
        const wy::Vector2& point,
        short extra,
        const wy::Vector2& refPoint)
        : PointSnapObject(id, point, extra), _refPoint(refPoint) {}

    // 类型
    virtual Snap2dType getType() const override { return Snap2dType::KeyPoint; }
    // 参照点
    wy::Vector2 getRefPoint() const { return _refPoint; }

private:
    wy::Vector2 _refPoint;
};
typedef std::shared_ptr<KeyPointSnapObject> KeyPointSnapObjectSPtr;

// 默认捕捉系统中的:端点
class EndPointSnapObject : public PointSnapObject
{
public:
    // extra --- which end point of the curve: START or END
    EndPointSnapObject(const wydb::ElementId& id, const wy::Vector2& point, short extra = 0)
        : PointSnapObject(id, point, extra) {}

    // 类型
    virtual Snap2dType getType() const override { return Snap2dType::EndPoint; }
};
typedef std::shared_ptr<EndPointSnapObject> EndPointSnapObjectSPtr;

// 默认捕捉系统中的:中点
class MiddlePointSnapObject : public PointSnapObject
{
public:
    MiddlePointSnapObject(const wydb::ElementId& id, const wy::Vector2& point)
        : PointSnapObject(id, point, 0) {}

    // 类型
    virtual Snap2dType getType() const override { return Snap2dType::MiddlePoint; }
};
typedef std::shared_ptr<MiddlePointSnapObject> MiddlePointSnapObjectSPtr;

// 默认捕捉系统中的:中心点
class CenterPointSnapObject : public PointSnapObject
{
public:
    CenterPointSnapObject(const wydb::ElementId& id, const wy::Vector2& point)
        : PointSnapObject(id, point, 0) {}

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

// 外部规则命中(id ===> 可选的高亮元素)
class RuleHitSnapObject : public SketchSnapObject
{
public:
    RuleHitSnapObject(const wydb::ElementId& id, Snap2dType snapType)
        : SketchSnapObject(id), _snapType(snapType) {}

    // 类型
    virtual Snap2dType getType() const override { return _snapType; }

private:
    Snap2dType _snapType;
};
typedef std::shared_ptr<RuleHitSnapObject> RuleHitSnapObjectSPtr;

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_SNAP_OBJECT_H
