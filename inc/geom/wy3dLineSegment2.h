///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_LINE_SEGMENT2_H
#define WY3D_LINE_SEGMENT2_H

#include <wy3dDefs.h>
#include <wy3dVector2.h>
#include <geom/wy3dCurve2.h>
#include <geom/wy3dBoundingBox2.h>

NS_WY3D_GEOM_BEG

class LineSegment2 : public Curve2
{
public:
    LineSegment2(const Vector2& startPt, const Vector2& endPt)
        : _startPoint(startPt), _endPoint(endPt)
    {}

    // Returns the length of the line segment.
    inline virtual double length() const override;

    // Returns the unit direction vector of the line segment.
    inline Vector2 direction() const;

    // Returns the start point of the line segment.
    inline virtual Vector2 startPoint() const override { return _startPoint; }

    // Returns the end point of the line segment.
    inline virtual Vector2 endPoint() const override { return _endPoint; }

    // Returns the midpoint of the line segment.
    inline Vector2 midPoint() const;

    // Returns the point on the line segment at parameter t.
    //
    // @param t Normalized curve parameter in the range [0, 1], where 0
    //        corresponds to the start point and 1 corresponds to the end point.
    // @param clamp If true, t is clamped internally to the range [0, 1].
    //
    // @return The corresponding point on the line segment.
    inline virtual Vector2 pointAt(double t, bool clamp = true) const override;

    // Returns the tangent direction at parameter t on the line segment.
    //
    // @param t Normalized curve parameter in the range [0, 1]. This parameter
    //        does not affect the result for a line segment.
    // @param clamp Reserved for interface consistency. This parameter does not
    //        affect the result for a line segment.
    //
    // @return The unit direction vector of the line segment. Returns (0, 0) if
    //         the line segment is degenerate.
    inline virtual Vector2 tangentAt(double t, bool clamp = true) const override
    {
        return this->direction();
    }

    // Indicates whether the line segment is closed.
    virtual bool isClosed() const override { return false; }

    // Returns the axis-aligned bounding box (AABB) of the line segment.
    inline virtual BoundingBox2 boundingBox() const override;

private:
    Vector2 _startPoint;
    Vector2 _endPoint;
};

inline double LineSegment2::length() const
{
    return (_endPoint - _startPoint).length();
}

inline Vector2 LineSegment2::pointAt(double t, bool clamp) const
{
    if (clamp)
    {
        if (t < 0.0)
            t = 0.0;
        else if (t > 1.0)
            t = 1.0;
    }
    return _startPoint + t * (_endPoint - _startPoint);
}

inline BoundingBox2 LineSegment2::boundingBox() const
{
    return BoundingBox2(_startPoint, _endPoint);
}

inline Vector2 LineSegment2::midPoint() const
{
    return (_startPoint + _endPoint) / 2;
}

inline Vector2 LineSegment2::direction() const
{
    Vector2 dir = _endPoint - _startPoint;
    dir.normalize();
    return dir;
}

NS_WY3D_GEOM_END

#endif // WY3D_LINE_SEGMENT2_H
