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

#ifndef WY3DAPP_SNAP2D_CONTEXT_H
#define WY3DAPP_SNAP2D_CONTEXT_H

#include <memory>
#include <wyVector2.h>
#include <wydbElementId.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class WY3DAPP_EXPORT Snap2dContext
{
protected:
    Snap2dContext() = default;
    virtual ~Snap2dContext() = default;
};
typedef std::shared_ptr<Snap2dContext> Snap2dContextSPtr;

class WY3DAPP_EXPORT Locate2dContext : public Snap2dContext
{
public:
    Locate2dContext() {}
};
typedef std::shared_ptr<Locate2dContext> Locate2dContextSPtr;

// 绘制线
class WY3DAPP_EXPORT SketchDrawLineContext : public Snap2dContext
{
public:
    explicit SketchDrawLineContext(const wy::Vector2& startPoint)
        : _startPoint(startPoint) {}

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

class WY3DAPP_EXPORT DrawCircle2dContext : public Snap2dContext
{
public:
    explicit DrawCircle2dContext(const wy::Vector2& center)
        : _center(center) {}

    wy::Vector2 getCenter() const
    {
        return _center;
    }

private:
    wy::Vector2 _center;
};
typedef std::shared_ptr<DrawCircle2dContext> DrawCircle2dContextSPtr;

class WY3DAPP_EXPORT DrawArc2dContext : public Snap2dContext
{
public:
    DrawArc2dContext(
        const wy::Vector2& center, double radius, const wy::Vector2& startPnt)
        : _center(center), _radius(radius), _startPnt(startPnt) {}

    wy::Vector2 getCenter() const
    {
        return _center;
    }
    double getRadius() const
    {
        return _radius;
    }
    wy::Vector2 getStartPoint() const
    {
        return _startPnt;
    }

private:
    wy::Vector2 _center;
    double _radius;
    wy::Vector2 _startPnt;
};
typedef std::shared_ptr<DrawArc2dContext> DrawArc2dContextSPtr;

} // namespace wy3dApp

#endif // WY3DAPP_SNAP2D_CONTEXT_H
