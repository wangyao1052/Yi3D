///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_BOUNDINGBOX2_H
#define WY3D_BOUNDINGBOX2_H

#include <algorithm>
#include <cassert>
#include <cfloat>

#include <wy3dDefs.h>
#include <wy3dVector2.h>

NS_WY3D_GEOM_BEG

class WY3D_EXPORT BoundingBox2
{
public:
    // Constructs an empty bounding box.
    BoundingBox2()
        : _min(DBL_MAX, DBL_MAX), _max(-DBL_MAX, -DBL_MAX)
    {
    }

    // Constructs a bounding box from two corner points.
    //
    // @param min First corner point.
    // @param max Second corner point.
    BoundingBox2(const Vector2& min, const Vector2& max)
    {
        set(min, max);
    }

    // Returns the minimum point of the bounding box.
    inline const Vector2& min() const { return _min; }

    // Returns the maximum point of the bounding box.
    inline const Vector2& max() const { return _max; }

    // Sets the bounding box from two corner points.
    //
    // @param min First corner point.
    // @param max Second corner point.
    void set(const Vector2& min, const Vector2& max)
    {
        double minX = std::min(min.x(), max.x());
        double maxX = std::max(min.x(), max.x());
        double minY = std::min(min.y(), max.y());
        double maxY = std::max(min.y(), max.y());
        _min.set(minX, minY);
        _max.set(maxX, maxY);
    }

    // Expands the bounding box to include the given point.
    //
    // @param point Point to merge into the bounding box.
    void merge(const Vector2& point)
    {
        if (isEmpty())
        {
            _min = point;
            _max = point;
        }
        else
        {
            _min.setX(std::min(_min.x(), point.x()));
            _min.setY(std::min(_min.y(), point.y()));
            _max.setX(std::max(_max.x(), point.x()));
            _max.setY(std::max(_max.y(), point.y()));
        }
    }

    // Expands the bounding box to include another bounding box.
    //
    // @param other Bounding box to merge.
    void merge(const BoundingBox2& other)
    {
        if (other.isEmpty())
            return;

        if (isEmpty())
        {
            _min = other.min();
            _max = other.max();
        }
        else
        {
            merge(other.min());
            merge(other.max());
        }
    }

    // Expands the bounding box by the given non-negative tolerance in all directions.
    //
    // @param tol Expansion distance. Must be greater than or equal to 0.
    void expand(double tol)
    {
        assert(tol >= 0.0);
        if (tol < 0.0) return;
        if (this->isEmpty()) return;

        _min.set(_min.x() - tol, _min.y() - tol);
        _max.set(_max.x() + tol, _max.y() + tol);
    }

    // Tests whether the bounding box contains the given point.
    //
    // @param point Point to test.
    //
    // @return true if the point lies inside or on the boundary; otherwise false.
    bool contains(const Vector2& point) const
    {
        return (point.x() >= _min.x() && point.x() <= _max.x() &&
            point.y() >= _min.y() && point.y() <= _max.y());
    }

    // Tests whether the bounding box intersects another bounding box.
    //
    // @param other Bounding box to test.
    //
    // @return true if the two bounding boxes intersect; otherwise false.
    bool intersects(const BoundingBox2& other) const
    {
        if (isEmpty() || other.isEmpty())
            return false;

        if (_max.x() < other.min().x() || _min.x() > other.max().x())
            return false;
        if (_max.y() < other.min().y() || _min.y() > other.max().y())
            return false;
        return true;
    }

    // Tests whether the bounding box completely contains another bounding box.
    //
    // @param other Bounding box to test.
    //
    // @return true if this bounding box fully contains the other bounding box;
    //         otherwise false.
    bool contains(const BoundingBox2& other) const
    {
        if (other.isEmpty())
            return true;

        if (this->isEmpty())
            return false;

        return (other._min.x() >= _min.x() &&
                other._max.x() <= _max.x() &&
                other._min.y() >= _min.y() &&
                other._max.y() <= _max.y());
    }

    // Returns the width of the bounding box.
    double width() const { return isEmpty() ? 0.0 : (_max.x() - _min.x()); }

    // Returns the height of the bounding box.
    double height() const { return isEmpty() ? 0.0 : (_max.y() - _min.y()); }

    // Returns the area of the bounding box.
    double area() const { return width() * height(); }

    // Returns the center point of the bounding box.
    // The result has no geometric meaning if the bounding box is empty.
    Vector2 center() const
    {
        return Vector2((_min.x() + _max.x()) * 0.5, (_min.y() + _max.y()) * 0.5);
    }

    // Indicates whether the bounding box is empty.
    //
    // @return true if the bounding box is empty; otherwise false.
    bool isEmpty() const
    {
        return (_min.x() > _max.x() || _min.y() > _max.y());
    }

    // Tests whether two bounding boxes are equal.
    //
    // @param other Bounding box to compare.
    //
    // @return true if the two bounding boxes have the same minimum and maximum
    //         points; otherwise false.
    bool operator==(const BoundingBox2& other) const
    {
        return (_min == other._min && _max == other._max);
    }

    // Tests whether two bounding boxes are not equal.
    //
    // @param other Bounding box to compare.
    //
    // @return true if the two bounding boxes are not equal; otherwise false.
    bool operator!=(const BoundingBox2& other) const
    {
        return !(*this == other);
    }

private:
    Vector2 _min;
    Vector2 _max;
};

NS_WY3D_GEOM_END

#endif // WY3D_BOUNDINGBOX2_H
