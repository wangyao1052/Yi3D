///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_TOPO_SHAPE_MAP_H
#define WY3D_TOPO_SHAPE_MAP_H

#include <cassert>
#include <unordered_map>
#include <unordered_set>
#include <TopoDS_Shape.hxx>
#include <wy3dDefs.h>

NS_WY3D_BEG

struct ShapeHasher
{
    size_t operator()(const TopoDS_Shape& shape) const
    {
        return static_cast<size_t>(shape.HashCode(std::numeric_limits<int>::max()));
    }
};

struct ShapeEqual
{
    bool operator()(const TopoDS_Shape& a, const TopoDS_Shape& b) const
    {
        return a.IsSame(b);
    }
};

typedef std::unordered_map<TopoDS_Shape, unsigned int, ShapeHasher, ShapeEqual> TopoShape2IdMap;
typedef std::unordered_map<TopoDS_Shape, TopoDS_Shape, ShapeHasher, ShapeEqual> TopoShape2TopoShapeMap;
typedef std::unordered_map<TopoDS_Shape, std::pair<TopoDS_Shape, TopoDS_Shape>, ShapeHasher, ShapeEqual> TopoShape2TopoShapePairMap;
typedef std::unordered_set<TopoDS_Shape, ShapeHasher, ShapeEqual> TopoShapeSet;

enum class ShapeEvolution
{
    Generated,          // 一个源生成一个结果
    GeneratedMultiple,  // 一个源生成多个结果
    Split               // 一个源被拆分成多个结果
};

inline std::pair<TopoDS_Shape, TopoDS_Shape> makeTopoShapePair(
    const TopoDS_Shape& shape1, const TopoDS_Shape& shape2)
{
    return std::pair<TopoDS_Shape, TopoDS_Shape>(shape1, shape2);
}

// 变化
struct ShapeDelta
{
    // 保留的(old = new)
    TopoShapeSet kept;
    // 删除的(old)
    TopoShapeSet deleted;
    // 修改的
    // old <> new
    TopoShape2TopoShapeMap modified;
    // 单来源新增:new <> source + evolution + resultIndex
    struct SingleSourceInfo
    {
        TopoDS_Shape source;
        ShapeEvolution evolution;
        unsigned int resultIndex;

        SingleSourceInfo()
            : evolution(ShapeEvolution::Generated), resultIndex(0)
        {
        }

        static SingleSourceInfo generated(const TopoDS_Shape& source)
        {
            return SingleSourceInfo(source, ShapeEvolution::Generated, 0);
        }

        static SingleSourceInfo generatedMultiple(const TopoDS_Shape& source, unsigned int resultIndex)
        {
            assert(resultIndex > 0);
            return SingleSourceInfo(source, ShapeEvolution::GeneratedMultiple, resultIndex);
        }

        static SingleSourceInfo split(const TopoDS_Shape& source, unsigned int resultIndex)
        {
            assert(resultIndex > 0);
            return SingleSourceInfo(source, ShapeEvolution::Split, resultIndex);
        }

    private:
        SingleSourceInfo(const TopoDS_Shape& source, ShapeEvolution evolution, unsigned int resultIndex)
            : source(source), evolution(evolution), resultIndex(resultIndex)
        {
        }
    };
    std::unordered_map<TopoDS_Shape, SingleSourceInfo, ShapeHasher, ShapeEqual> addedSingle;
    // 1 <> 2
    struct DoubleSourceInfo
    {
        TopoDS_Shape source1;
        TopoDS_Shape source2;
        unsigned int index;
        DoubleSourceInfo() : source1(), source2(), index(0) {}
    };
    std::unordered_map<TopoDS_Shape, DoubleSourceInfo, ShapeHasher, ShapeEqual> addedDouble;
    // 1 <> 多
    struct MultiSourceInfo
    {
        TopoShapeSet sources;
        unsigned int index;
        MultiSourceInfo() : index(0) {}
    };
    std::unordered_map<TopoDS_Shape, MultiSourceInfo, ShapeHasher, ShapeEqual> addedMulti;
};

NS_WY3D_END

#endif // WY3D_TOPO_SHAPE_MAP_H
