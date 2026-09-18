///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_ARC2_H
#define WY3D_ARC2_H

#include <wy3dDefs.h>
#include <wy3dMath.h>
#include <wy3dVector2.h>
#include <geom/wy3dCurve2.h>
#include <geom/wy3dBoundingBox2.h>
#include <cmath>

NS_WY3D_GEOM_BEG

class WY3D_EXPORT Arc2 : public Curve2
{
public:
    // Constructs an arc.
    //
    // @param center Center of the arc.
    // @param radius Radius of the arc. Negative values are converted to their
    //        absolute value.
    // @param startAngle Start angle in radians.
    // @param endAngle End angle in radians.
    Arc2(const Vector2& center, double radius, double startAngle, double endAngle)
        : _center(center), _radius(std::fabs(radius)), _startAngle(startAngle), _endAngle(endAngle)
    {
    }

    // Returns the arc length.
    virtual double length() const override;

    // Returns the start point of the arc.
    virtual Vector2 startPoint() const override;

    // Returns the end point of the arc.
    virtual Vector2 endPoint() const override;

    // Returns the point on the arc at parameter t.
    //
    // @param t Normalized curve parameter in the range [0, 1], where 0
    //        corresponds to startAngle and 1 corresponds to endAngle.
    // @param clamp If true, t is clamped internally to the range [0, 1].
    //
    // @return The corresponding point on the arc.
    virtual Vector2 pointAt(double t, bool clamp = true) const override;

    // Returns the tangent direction at parameter t on the arc.
    //
    // @param t Normalized curve parameter in the range [0, 1].
    // @param clamp If true, t is clamped internally to the range [0, 1].
    //
    // @return The unit tangent direction on the arc.
    virtual Vector2 tangentAt(double t, bool clamp = true) const override;

    // Indicates whether the arc is closed.
    virtual bool isClosed() const override { return false; }

    // Returns the axis-aligned bounding box (AABB) of the arc.
    virtual BoundingBox2 boundingBox() const override;

    // Returns the midpoint of the arc.
    virtual Vector2 midPoint() const
    {
        return pointAt(0.5, true);
    }

    // Returns the center of the arc.
    inline Vector2 center() const { return _center; }

    // Returns the radius of the arc.
    inline double radius() const { return _radius; }

    // Returns the start angle of the arc.
    inline double startAngle() const { return _startAngle; }

    // Returns the end angle of the arc.
    inline double endAngle() const { return _endAngle; }

    // Returns the counterclockwise sweep angle of the arc.
    inline double totalAngle() const;

private:
    Vector2 _center;
    double _radius;
    double _startAngle;
    double _endAngle;
};

inline double Arc2::totalAngle() const
{
    return computeTotalAngle(_startAngle, _endAngle);
}

inline double Arc2::length() const
{
    return _radius * this->totalAngle();
}

inline Vector2 Arc2::startPoint() const
{
    return _center + Vector2(_radius * std::cos(_startAngle),
        _radius * std::sin(_startAngle));
}

inline Vector2 Arc2::endPoint() const
{
    return _center + Vector2(_radius * std::cos(_endAngle),
        _radius * std::sin(_endAngle));
}

inline Vector2 Arc2::pointAt(double t, bool clamp) const
{
    if (clamp)
    {
        if (t < 0.0)
            t = 0.0;
        else if (t > 1.0)
            t = 1.0;
    }
    double angle = _startAngle + t * this->totalAngle();
    return _center + Vector2(_radius * std::cos(angle),
        _radius * std::sin(angle));
}

inline Vector2 Arc2::tangentAt(double t, bool clamp) const
{
    if (clamp)
    {
        if (t < 0.0)
            t = 0.0;
        else if (t > 1.0)
            t = 1.0;
    }
    double angle = _startAngle + t * this->totalAngle();
    return Vector2(-std::sin(angle), std::cos(angle));
}

inline BoundingBox2 Arc2::boundingBox() const
{
    BoundingBox2 bbox;
    bbox.merge(this->startPoint());
    bbox.merge(this->endPoint());

    double total = this->totalAngle();
    for (int i = 0; i < 4; ++i)
    {
        double angle = PI_2 * i;
        double delta = computeTotalAngle(_startAngle, angle);
        if (delta <= total)
        {
            bbox.merge(_center +
                Vector2(_radius * std::cos(angle), _radius * std::sin(angle)));
        }
    }
    return bbox;
}

NS_WY3D_GEOM_END

#endif // WY3D_ARC2_H
