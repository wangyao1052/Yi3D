///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_CURVE2_H
#define WY3D_CURVE2_H

#include <wy3dDefs.h>
#include <wy3dVector2.h>
#include <geom/wy3dBoundingBox2.h>

NS_WY3D_GEOM_BEG

class Curve2
{
public:
    virtual ~Curve2() = default;

    // Returns the length of the curve.
    virtual double length() const = 0;

    // Returns the start point of the curve.
    virtual Vector2 startPoint() const = 0;

    // Returns the end point of the curve.
    virtual Vector2 endPoint() const = 0;

    // Returns the point on the curve at parameter t.
    //
    // @param t Normalized curve parameter in the range [0, 1], where 0
    //        corresponds to the start point and 1 corresponds to the end point.
    // @param clamp If true, t is clamped internally to the range [0, 1].
    //
    // @return The corresponding point on the curve.
    virtual Vector2 pointAt(double t, bool clamp = true) const = 0;

    // Returns the unit tangent direction at parameter t on the curve.
    //
    // @param t Normalized curve parameter in the range [0, 1].
    // @param clamp If true, t is clamped internally to the range [0, 1].
    //
    // @return The tangent direction on the curve as a unit vector.
    virtual Vector2 tangentAt(double t, bool clamp = true) const = 0;

    // Indicates whether the curve is closed.
    virtual bool isClosed() const = 0;

    // Returns the axis-aligned bounding box (AABB) of the curve.
    virtual BoundingBox2 boundingBox() const
    {
        return BoundingBox2();
    }
};

NS_WY3D_GEOM_END

#endif // WY3D_CURVE2_H
