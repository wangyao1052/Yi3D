///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_SKETCH_CURVE_GRAPH_H
#define WY3D_SKETCH_CURVE_GRAPH_H

#include <vector>
#include <list>
#include <map>
#include <memory>

#include <wyVector2.h>
#include <geom/wy3dBoundingBox2.h>
#include <wy3dDefs.h>
#include <wy3dErrorCode.h>
#include <wy3dSketch.h>
#include <wy3dSketchCurve.h>

NS_WY3D_BEG

struct SketchError
{
    // 错误类型
    ErrorCode type;
    // 额外信息
    std::vector<wydb::ElementId> ids;

    SketchError() : type(ErrorCode::NoError) {}
};

struct IntVector2
{
    long long X;
    long long Y;

    IntVector2(long long x, long long y) : X(x), Y(y) {}

    bool operator<(const IntVector2& other) const
    {
        if (X < other.X) return true;
        else if (X > other.X) return false;
        else return Y < other.Y;
    }
};

struct BiCurve
{
    BiCurve(const wy3d::SketchCurve* pCurve, bool bOrient) :
        curve(pCurve), orient(bOrient)
    {}

    const wy3d::SketchCurve* curve;
    bool orient;
};

class SketchCurveGraph
{
public:
    enum class Orientation
    {
        Normal,
        Reversed
    };

    struct CurveEntry
    {
        size_t index;
        Orientation orient;
    };

public:
    // 构造函数
    explicit SketchCurveGraph(const std::vector<const SketchCurve*>& curves, double tol = 1e-5);

    // 是否有效
    bool isValid() const { return _isValid; }

    // 获取错误
    std::shared_ptr<SketchError> getError() const { return _pError; }

protected:
    bool buildGraph();

    // 判断两个Vector2是否在容差内相等
    inline bool nearlyEqual(const wy::Vector2& a, const wy::Vector2& b) const
    {
        return (a - b).length() < _tol;
    }

    // 获取CurveEntry的有效起点
    inline wy::Vector2 effectiveStartPoint(const CurveEntry& entry) const
    {
        return (entry.orient == Orientation::Normal) ? _curves[entry.index]->getStartPoint() : _curves[entry.index]->getEndPoint();
    }

    // 获取CurveEntry的有效终点
    inline wy::Vector2 effectiveEndPoint(const CurveEntry& entry) const
    {
        return (entry.orient == Orientation::Normal) ? _curves[entry.index]->getEndPoint() : _curves[entry.index]->getStartPoint();
    }

    static IntVector2 convertToIntVec2(const wy::Vector2& pnt, double tol);

protected:
    // 曲线集
    std::vector<const SketchCurve*> _curves;
    // 容差
    double _tol;
    // 是否有效
    bool _isValid;
    // 错误信息
    std::shared_ptr<SketchError> _pError;

    // 曲线的邻接信息
    std::vector<std::vector<CurveEntry>> _endPointAdjacency;
    std::vector<std::vector<CurveEntry>> _startPointAdjacency;

protected:
    std::shared_ptr<SketchError> newError(ErrorCode errorType, wydb::ElementId id,
        const std::vector<CurveEntry>& curveEntries) const;
    std::shared_ptr<SketchError> newErrorWithIndices(ErrorCode errorType, wydb::ElementId id,
        const std::list<size_t>& indices) const;
};

NS_WY3D_END

#endif // WY3D_SKETCH_CURVE_GRAPH_H