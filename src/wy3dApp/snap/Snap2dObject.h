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

#ifndef WY3DAPP_SNAP2D_OBJECT_H
#define WY3DAPP_SNAP2D_OBJECT_H

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
class Snap2dObject
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

protected:
    explicit Snap2dObject(const wydb::ElementId& id) : _id(id) {}
    virtual ~Snap2dObject() {}

public:
    wydb::ElementId getId() const { return _id; }

    virtual Snap2dType getType() const = 0;

    virtual wy::Vector2 getRefPoint() const { return wy::Vector2(); }

private:
    wydb::ElementId _id; // id
};
typedef std::shared_ptr<Snap2dObject> Snap2dObjectSPtr;

class Snap2dAngle : public Snap2dObject
{
protected:
    explicit Snap2dAngle(const wydb::ElementId& id, double angle)
        : Snap2dObject(id), _angle(angle) {}

public:
    double getValue() const { return _angle; }
    
protected:
    double _angle;
};
typedef std::shared_ptr<Snap2dAngle> Snap2dAngleSPtr;

class Snap2dHorizontal : public Snap2dAngle
{
public:
    Snap2dHorizontal() : Snap2dAngle(wydb::ElementId::kNull, 0.0) {}

    virtual Snap2dType getType() const override { return Snap2dType::Horizontal; }
};
typedef std::shared_ptr<Snap2dHorizontal> Snap2dHorizontalSPtr;

class Snap2dVertical : public Snap2dAngle
{
public:
    Snap2dVertical() : Snap2dAngle(wydb::ElementId::kNull, wy3d::PI_2) {}

    virtual Snap2dType getType() const override { return Snap2dType::Vertical; }
};
typedef std::shared_ptr<Snap2dVertical> Snap2dVerticalSPtr;

class Snap2dParallel : public Snap2dAngle
{
public:
    Snap2dParallel(const wydb::ElementId& id, const wy::Vector2& startPnt, const wy::Vector2& endPnt);

    virtual Snap2dType getType() const override { return Snap2dType::Parallel; }
};
typedef std::shared_ptr<Snap2dParallel> Snap2dParallelSPtr;

class Snap2dPerpendicular : public Snap2dAngle
{
public:
    Snap2dPerpendicular(const wydb::ElementId& id, const wy::Vector2& startPnt, const wy::Vector2& endPnt);

    virtual Snap2dType getType() const override { return Snap2dType::Perpendicular; }
};
typedef std::shared_ptr<Snap2dPerpendicular> Snap2dPerpendicularSPtr;

class Snap2dEqualLength : public Snap2dObject
{
public:
    Snap2dEqualLength(const wydb::ElementId& id, double length, double lineAngle)
        : Snap2dObject(id), _length(length), _lineAngle(lineAngle) {}

    virtual Snap2dType getType() const override { return Snap2dType::EqualLength; }

    double getLength() const { return _length; }

    double getLineAngle() const { return _lineAngle; }

private:
    double _length;
    double _lineAngle; // [0, PI)
};
typedef std::shared_ptr<Snap2dEqualLength> Snap2dEqualLengthSPtr;

class Snap2dEqualRadius : public Snap2dObject
{
public:
    explicit Snap2dEqualRadius(const wydb::ElementId& id, double radius)
        : Snap2dObject(id), _radius(radius) {}

    virtual Snap2dType getType() const override { return Snap2dType::EqualRadius; }

    double getRadius() const { return _radius; }

private:
    double _radius;
};
typedef std::shared_ptr<Snap2dEqualRadius> Snap2dEqualRadiusSPtr;

class Snap2dAlignX : public Snap2dObject
{
public:
    explicit Snap2dAlignX(const wydb::ElementId& id, const wy::Vector2& pnt)
        : Snap2dObject(id), _pnt(pnt) {}

    virtual Snap2dType getType() const override { return Snap2dType::AlignX; }

    double getValue() const { return _pnt.x(); }
    
    virtual wy::Vector2 getRefPoint() const override { return _pnt; }

private:
    wy::Vector2 _pnt;
};
typedef std::shared_ptr<Snap2dAlignX> Snap2dAlignXSPtr;

class Snap2dAlignY : public Snap2dObject
{
public:
    explicit Snap2dAlignY(const wydb::ElementId& id, const wy::Vector2& pnt)
        : Snap2dObject(id), _pnt(pnt) {}

    virtual Snap2dType getType() const override { return Snap2dType::AlignY; }

    double getValue() const { return _pnt.y(); }
    
    virtual wy::Vector2 getRefPoint() const override { return _pnt; }

private:
    wy::Vector2 _pnt;
};
typedef std::shared_ptr<Snap2dAlignY> Snap2dAlignYSPtr;

class Snap2dNearest : public Snap2dObject
{
public:
    explicit Snap2dNearest(const wydb::ElementId& id)
        : Snap2dObject(id) {}

    virtual Snap2dType getType() const override { return Snap2dType::Nearest; }
};
typedef std::shared_ptr<Snap2dNearest> Snap2dNearestSPtr;

class Snap2dPoint : public Snap2dObject
{
public:
    Snap2dPoint(const wydb::ElementId& id, const wy::Vector2& point)
        : Snap2dObject(id), _point(point) {}

    wy::Vector2 getValue() const { return _point; }

private:
    wy::Vector2 _point;
};
typedef std::shared_ptr<Snap2dPoint> Snap2dPointSPtr;

class Snap2dGenericPoint : public Snap2dPoint
{
public:
    explicit Snap2dGenericPoint(const wy::Vector2& point)
        : Snap2dPoint(wydb::ElementId::kNull, point) {}

    virtual Snap2dType getType() const override { return Snap2dType::GenericPoint; }
};
typedef std::shared_ptr<Snap2dGenericPoint> Snap2dGenericPointSPtr;

class Snap2dOrigin : public Snap2dPoint
{
public:
    Snap2dOrigin() : Snap2dPoint(wydb::ElementId::kNull, wy::Vector2(0.0, 0.0)) {}

    virtual Snap2dType getType() const override { return Snap2dType::Origin; }
};
typedef std::shared_ptr<Snap2dOrigin> Snap2dOriginSPtr;

class Snap2dEndPoint : public Snap2dPoint
{
public:
    Snap2dEndPoint(const wydb::ElementId& id, const wy::Vector2& point)
        : Snap2dPoint(id, point) {}

    virtual Snap2dType getType() const override { return Snap2dType::EndPoint; }
};
typedef std::shared_ptr<Snap2dEndPoint> Snap2dEndPointSPtr;

class Snap2dMiddlePoint : public Snap2dPoint
{
public:
    Snap2dMiddlePoint(const wydb::ElementId& id, const wy::Vector2& point)
        : Snap2dPoint(id, point) {}

    virtual Snap2dType getType() const override { return Snap2dType::MiddlePoint; }
};
typedef std::shared_ptr<Snap2dMiddlePoint> Snap2dMiddlePointSPtr;

class Snap2dCenterPoint : public Snap2dPoint
{
public:
    Snap2dCenterPoint(const wydb::ElementId& id, const wy::Vector2& point)
        : Snap2dPoint(id, point) {}

    virtual Snap2dType getType() const override { return Snap2dType::CenterPoint; }
};
typedef std::shared_ptr<Snap2dCenterPoint> Snap2dCenterPointSPtr;

class Snap2dQuadrantPoint : public Snap2dPoint
{
public:
    Snap2dQuadrantPoint(const wydb::ElementId& id, const wy::Vector2& point)
        : Snap2dPoint(id, point) {}

    virtual Snap2dType getType() const override { return Snap2dType::QuadrantPoint; }
};
typedef std::shared_ptr<Snap2dQuadrantPoint> Snap2dQuadrantPointSPtr;

class Snap2dTangentPoint : public Snap2dPoint
{
public:
    Snap2dTangentPoint(const wydb::ElementId& id, const wy::Vector2& point)
        : Snap2dPoint(id, point) {}

    virtual Snap2dType getType() const override { return Snap2dType::TangentPoint; }
};
typedef std::shared_ptr<Snap2dTangentPoint> Snap2dTangentPointSPtr;

class Snap2dPerpendicularPoint : public Snap2dPoint
{
public:
    Snap2dPerpendicularPoint(const wydb::ElementId& id, const wy::Vector2& point)
        : Snap2dPoint(id, point) {}

    virtual Snap2dType getType() const override { return Snap2dType::PerpendicularPoint; }
};
typedef std::shared_ptr<Snap2dPerpendicularPoint> Snap2dPerpendicularPointSPtr;

class Snap2dIntersectionPoint : public Snap2dPoint
{
public:
    Snap2dIntersectionPoint(const wydb::ElementId& id, const wy::Vector2& point)
        : Snap2dPoint(id, point) {}

    virtual Snap2dType getType() const override { return Snap2dType::IntersectionPoint; }
};
typedef std::shared_ptr<Snap2dIntersectionPoint> Snap2dIntersectionPointSPtr;

class Snap2dAlignPoint : public Snap2dPoint
{
public:
    Snap2dAlignPoint(
        const wydb::ElementId& id,
        const wy::Vector2& point,
        const wy::Vector2& refPoint,
        Snap2dType type)
        : Snap2dPoint(id, point), _refPoint(refPoint), _type(type) {}

    virtual Snap2dType getType() const override { return _type; }
    
    virtual wy::Vector2 getRefPoint() const override { return _refPoint; }

private:
    wy::Vector2 _refPoint;
    Snap2dType _type;
};
typedef std::shared_ptr<Snap2dAlignPoint> Snap2dAlignPointSPtr;

} // namespace wy3dApp

#endif // WY3DAPP_SNAP2D_OBJECT_H
