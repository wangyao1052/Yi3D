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

#ifndef WY3DAPP_SKETCH_EXTEND_NODE_H
#define WY3DAPP_SKETCH_EXTEND_NODE_H

#include <vector>
#include <map>
#include <memory>
#include <wyVector2.h>
#include <wy3dVector2.h>
#include <wydbDatabase.h>
#include <wy3dSketch.h>
#include <wy3dSketchCurve.h>
#include <wy3dSketchSpline.h>
#include <RTree/RTree.h>
#include "wy3dAppDefs.h"

namespace wy3dApp {

class SketchExtendGraph;

// 标记点
class SketchExtendKnot
{
public:
    // 获取参数
    inline double getParam() const
    {
        return _t;
    }
    // 设置参数
    inline void setParam(double t)
    {
        _t = t;
    }

    // 获取位置
    inline const wy::Vector2& getPosition() const
    {
        return _pos;
    }
    // 设置位置
    inline void setPosition(const wy::Vector2& pos)
    {
        _pos = pos;
    }

    // 获取另一个owner
    inline wydb::ElementId getOtherOwner() const
    {
        return _otherOwner;
    }

    // 构造函数
    SketchExtendKnot() : _pos(), _t(DBL_MAX), _otherOwner(wydb::ElementId::kNull) {}
    explicit SketchExtendKnot(const wy::Vector2& pos, double t, wydb::ElementId otherOwner = wydb::ElementId::kNull)
        : _pos(pos), _t(t), _otherOwner(otherOwner) {}

    // 是否有效
    inline bool isValid() const
    {
        return _t != DBL_MAX && _t != -DBL_MAX && !std::isnan(_t) && !std::isinf(_t);
    }

private:
    // 取值范围:实数范围
    // 默认值为DBL_MAX表示无效值
    double _t;
    // 实际位置
    wy::Vector2 _pos;
    // other owner
    wydb::ElementId _otherOwner;
};

// 曲线段
class SketchExtendSegment
{
public:
    SketchExtendKnot startKnot;
    SketchExtendKnot endKnot;

    inline bool isValid() const
    {
        return startKnot.isValid() && endKnot.isValid();
    }
};

// 结点
struct SketchExtendNode
{
    // 构造函数
    explicit SketchExtendNode(const wydb::ElementId& id);

    // 获取ID
    inline wydb::ElementId getId() const { return _id; }
    // 是否是闭合的
    inline bool isClosed() const { return _isClosed; }
    // 设置是否闭合
    inline void setIsClosed(bool isClosed) { _isClosed = isClosed; }

    // 添加标记点
    void appendKnot(const wy::Vector2& point, const wydb::ElementId& otherOwner)
    {
        _knots.emplace_back(SketchExtendKnot(point, DBL_MAX, otherOwner));
    }
    void appendKnot(const SketchExtendKnot& knot)
    {
        _knots.emplace_back(knot);
    }

    // 刷新标记点参数
    void refresh(const wydb::Database* pDb);

    // 拾取
    SketchExtendSegment pick(SketchExtendGraph* pGraph, const wy::Vector2& point, double tol = 1e-6);

private:
    // 是否在曲线上
    double isOnCurve(const wydb::Database& db, const wy::Vector2& pos) const;

    // 是否在另一条曲线上
    bool isOnOtherCurve(const wydb::Database& db, SketchExtendGraph* pGraph, const SketchExtendKnot& knot);

    void refreshLine(const wy::Vector2& startPnt, const wy::Vector2& endPnt);
    void refreshSpline(const wy3d::SketchSpline& sketchSpline);

private:
    // 草图曲线ID
    wydb::ElementId _id;
    // 标记点集合
    std::vector<SketchExtendKnot> _knots;
    // 是否是闭合的
    bool _isClosed;
};
typedef std::shared_ptr<SketchExtendNode> SketchExtendNodeSPtr;

}

#endif // WY3DAPP_SKETCH_EXTEND_NODE_H
