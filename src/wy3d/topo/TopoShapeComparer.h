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

#ifndef WY3D_TOPO_SHAPE_COMPARER_H
#define WY3D_TOPO_SHAPE_COMPARER_H

#include <vector>
#include <TopoDS_Shape.hxx>
#include <BRepBuilderAPI_MakeShape.hxx>
#include <wy3dDefs.h>
#include <utils/wy3dTopoShapeMap.h>

NS_WY3D_BEG

// 拓扑元素比较器
class TopoShapeComparer
{
public:
    TopoShapeComparer(BRepBuilderAPI_MakeShape& mkShape, const TopoDS_Shape& oldShape);
    ~TopoShapeComparer();

    // 执行
    void perform();

    // 变化
    const ShapeDelta& getEdgeDelta() const { return _edgeDelta; }
    const ShapeDelta& getFaceDelta() const { return _faceDelta; }

    // 输出
    bool print(const std::string& fileFullPath) const;

protected:
    // 初始化
    virtual void init();

    // 保留的
    virtual void recordKept();
    // 删除的
    virtual void recordDeleted();
    // 修改的
    virtual void recordModified() {};
    // 新增的
    virtual void recordAdded() {}
    // 最终的校验
    virtual void finallyCheck();

private:
    void finallyCheckFace();
    void finallyCheckEdge();

protected:
    BRepBuilderAPI_MakeShape& _mkShape;

    // 拓扑形体
    TopoDS_Shape _oldShape;
    TopoDS_Shape _newShape;

    // 拓扑信息
    struct TopoShapeInfo
    {
        TopoDS_Shape shape;
        unsigned int index; // 在旧实体或新实体中的索引,以1为起始序号.

        TopoShapeInfo(const TopoDS_Shape& argShape, unsigned int argIndex)
            : shape(argShape), index(argIndex) {}
    };
    struct TopoShapeInfoHasher
    {
        size_t operator()(const TopoShapeInfo& info) const
        {
            return static_cast<size_t>(info.shape.HashCode(std::numeric_limits<int>::max()));
        }
    };
    struct TopoShapeInfoEqual
    {
        bool operator()(const TopoShapeInfo& a, const TopoShapeInfo& b) const
        {
            return a.shape.IsSame(b.shape);
        }
    };
    typedef std::unordered_set<TopoShapeInfo, TopoShapeInfoHasher, TopoShapeInfoEqual> TopoShapeInfoSet;

    // 边
    TopoShapeInfoSet _oldEdgeInfoSet;
    TopoShapeInfoSet _newEdgeInfoSet;
    // 面
    TopoShapeInfoSet _oldFaceInfoSet;
    TopoShapeInfoSet _newFaceInfoSet;

    // 变化
    ShapeDelta _edgeDelta;
    ShapeDelta _faceDelta;
};

struct TopoShapePairHasher
{
    size_t operator()(const std::pair<TopoDS_Shape, TopoDS_Shape>& pair) const
    {
        ShapeHasher shapeHasher;

        // 计算两个 Shape 的哈希值
        size_t h1 = shapeHasher(pair.first);
        size_t h2 = shapeHasher(pair.second);

        // 使用标准库的哈希组合方法
        // 注意：顺序敏感（(A,B) 和 (B,A) 会生成不同的哈希值）
        size_t seed = 0;
        seed ^= h1 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= h2 + 0x9e3779b9 + (seed << 6) + (seed >> 2);

        return seed;
    }
};

struct TopoShapePairEqual
{
    bool operator()(
        const std::pair<TopoDS_Shape, TopoDS_Shape>& a,
        const std::pair<TopoDS_Shape, TopoDS_Shape>& b) const
    {
        ShapeEqual shapeEqual;
        return shapeEqual(a.first, b.first) && shapeEqual(a.second, b.second);
    }
};

NS_WY3D_END

#endif // WY3D_TOPO_SHAPE_COMPARER_H