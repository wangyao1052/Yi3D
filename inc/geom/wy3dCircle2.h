///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_CIRCLE2_H
#define WY3D_CIRCLE2_H

#include <cmath>
#include <wy3dDefs.h>
#include <wy3dMath.h>
#include <wy3dVector2.h>
#include <geom/wy3dCurve2.h>
#include <geom/wy3dBoundingBox2.h>

NS_WY3D_GEOM_BEG

class WY3D_EXPORT Circle2 : public Curve2
{
public:
    Circle2(const Vector2& center, double radius)
        : _center(center), _radius(std::fabs(radius))
    {}

    // Returns the circumference of the circle.
    virtual double length() const override;

    // Returns the start point of the circle at t = 0.
    virtual Vector2 startPoint() const override;

    // Returns the end point of the circle at t = 1.
    // The same point as startPoint(), because the circle is closed.
    virtual Vector2 endPoint() const override;

    // Returns the point on the circle at parameter t.
    //
    // @param t Normalized curve parameter in the range [0, 1], where 0
    //        corresponds to angle 0 and 1 corresponds to angle 2PI.
    // @param clamp If true, t is clamped internally to the range [0, 1].
    //
    // @return The corresponding point on the circle.
    virtual Vector2 pointAt(double t, bool clamp = true) const override;

    // Returns the tangent direction at parameter t on the circle.
    //
    // @param t Normalized curve parameter in the range [0, 1].
    // @param clamp If true, t is clamped internally to the range [0, 1].
    //
    // @return The unit tangent direction on the circle.
    virtual Vector2 tangentAt(double t, bool clamp = true) const override;

    // Indicates whether the circle is closed.
    virtual bool isClosed() const override { return true; }

    // Returns the axis-aligned bounding box (AABB) of the circle.
    virtual BoundingBox2 boundingBox() const override;

    // Returns the center of the circle.
    inline Vector2 center() const { return _center; }

    // Returns the radius of the circle.
    inline double radius() const { return _radius; }

private:
    Vector2 _center;
    double _radius;
};

inline double Circle2::length() const
{
    return TWO_PI * _radius;
}

inline Vector2 Circle2::startPoint() const
{
    return _center + Vector2(_radius, 0.0);
}

inline Vector2 Circle2::endPoint() const
{
    return startPoint();
}

inline Vector2 Circle2::pointAt(double t, bool clamp) const
{
    if (clamp)
    {
        if (t < 0.0)
            t = 0.0;
        else if (t > 1.0)
            t = 1.0;
    }
    double angle = TWO_PI * t;
    return _center + Vector2(_radius * std::cos(angle), _radius * std::sin(angle));
}

inline Vector2 Circle2::tangentAt(double t, bool clamp) const
{
    if (clamp)
    {
        if (t < 0.0)
            t = 0.0;
        else if (t > 1.0)
            t = 1.0;
    }
    double angle = TWO_PI * t;
    return Vector2(-std::sin(angle), std::cos(angle));
}

inline BoundingBox2 Circle2::boundingBox() const
{
    return BoundingBox2(_center - Vector2(_radius, _radius),
        _center + Vector2(_radius, _radius));
}

NS_WY3D_GEOM_END

#endif // WY3D_CIRCLE2_H
