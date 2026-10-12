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

#ifndef WY3DAPP_SKETCH_SNAP_CONTEXT_H
#define WY3DAPP_SKETCH_SNAP_CONTEXT_H

#include <set>
#include <memory>
#include <wyVector2.h>
#include <wydbElementId.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

// 草图捕捉上下文
class SketchSnapContext
{
public:
    explicit SketchSnapContext(const wydb::ElementId& id) : _id(id)
    {
        if (!id.isNull())
        {
            _excludedIds.insert(id);
        }
    }
    virtual ~SketchSnapContext() {}

    // 获取ID
    wydb::ElementId getId() const { return _id; }

    // 获取不捕捉的元素IDS
    const std::set<wydb::ElementId>& getExcludedIds() const { return _excludedIds; }
    // 设置不捕捉的元素IDS
    void setExcludedIds(const std::set<wydb::ElementId>& excludedIds) { _excludedIds = excludedIds; }
    void setExcludedIds(std::set<wydb::ElementId>&& excludedIds) { _excludedIds = std::move(excludedIds); }

protected:
    // 当前正在绘制的元素ID
    wydb::ElementId _id;
    // 捕捉排除项
    std::set<wydb::ElementId> _excludedIds;
};
typedef std::shared_ptr<SketchSnapContext> SketchSnapContextSPtr;

// 空的捕捉上下文
class SketchEmptyContext : public SketchSnapContext
{
public:
    explicit SketchEmptyContext(const wydb::ElementId& id) : SketchSnapContext(id) {}
};
typedef std::shared_ptr<SketchEmptyContext> SketchEmptyContextSPtr;

// 定位点
class SketchLocateContext : public SketchSnapContext
{
public:
    explicit SketchLocateContext(const wydb::ElementId& id) : SketchSnapContext(id) {}
};
typedef std::shared_ptr<SketchLocateContext> SketchLocateContextSPtr;

// End point of an existing curve: only the start/end points of eligible curves snap
class SketchCurveEndPointContext : public SketchSnapContext
{
public:
    explicit SketchCurveEndPointContext(const wydb::ElementId& id) : SketchSnapContext(id) {}
};
typedef std::shared_ptr<SketchCurveEndPointContext> SketchCurveEndPointContextSPtr;

// 绘制线
class SketchDrawLineContext : public SketchSnapContext
{
public:
    explicit SketchDrawLineContext(const wydb::ElementId& id, const wy::Vector2& startPoint)
        : SketchSnapContext(id), _startPoint(startPoint) {}

    // 起始点
    wy::Vector2 getStartPoint() const
    {
        return _startPoint;
    }

    // 捕捉起点的切线
    struct SnapStartTangent
    {
        wydb::ElementId id;
        wy::Vector2 direction;
        SnapStartTangent() : id(wydb::ElementId::kNull), direction(1.0, 0.0) {}
        SnapStartTangent(const wydb::ElementId& argId, const wy::Vector2& argDir)
            : id(argId), direction(argDir) {}
    };
    std::shared_ptr<SnapStartTangent> getSnapStartTangent() const
    {
        return _pSnapStartTangent;
    }
    void setSnapStartTangent(std::shared_ptr<SnapStartTangent> pSnapStartTangent)
    {
        _pSnapStartTangent = pSnapStartTangent;
    }

private:
    // 起始点
    wy::Vector2 _startPoint;

    // 捕捉起点的切线
    std::shared_ptr<SnapStartTangent> _pSnapStartTangent;
};
typedef std::shared_ptr<SketchDrawLineContext> SketchDrawLineContextSPtr;

// 绘制圆
class SketchDrawCircleContext : public SketchSnapContext
{
public:
    SketchDrawCircleContext(const wydb::ElementId& id, const wy::Vector2& center)
        : SketchSnapContext(id), _center(center), _isForDrawArc(false) {}

    // 圆心
    wy::Vector2 getCenter() const
    {
        return _center;
    }

    // 是否是为了绘制圆弧
    bool isForDrawArc() const { return _isForDrawArc; }
    // 设置:是否是为了绘制圆弧
    void setIsForDrawArc(bool value)
    {
        _isForDrawArc = value;
    }

private:
    wy::Vector2 _center;
    bool _isForDrawArc;
};
typedef std::shared_ptr<SketchDrawCircleContext> SketchDrawCircleContextSPtr;

// 绘制圆弧
class SketchDrawArcContext : public SketchSnapContext
{
public:
    SketchDrawArcContext(const wydb::ElementId& id,
        const wy::Vector2& center, double radius, const wy::Vector2& startPnt)
        : SketchSnapContext(id), _center(center), _radius(radius), _startPnt(startPnt) {}

    // 圆心
    wy::Vector2 getCenter() const
    {
        return _center;
    }
    // 半径
    double getRadius() const
    {
        return _radius;
    }
    // 起始点
    wy::Vector2 getStartPoint() const
    {
        return _startPnt;
    }

private:
    wy::Vector2 _center;
    double _radius;
    wy::Vector2 _startPnt;
};
typedef std::shared_ptr<SketchDrawArcContext> SketchDrawArcContextSPtr;

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_SNAP_CONTEXT_H
