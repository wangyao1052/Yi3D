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
#include <wyrxObject.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class WY3DAPP_EXPORT Snap2dContext : public wyrx::Object
{
    WYRX_DECLARE_NO_CREATE_MEMBERS(Snap2dContext, wy3dApp::Snap2dContext, wyrx::Object)
protected:
    Snap2dContext() = default;
    virtual ~Snap2dContext() override = default;
    Snap2dContext(const Snap2dContext&) = delete;
    Snap2dContext& operator=(const Snap2dContext&) = delete;
    Snap2dContext(Snap2dContext&&) = delete;
    Snap2dContext& operator=(Snap2dContext&&) = delete;
};
typedef std::shared_ptr<Snap2dContext> Snap2dContextSPtr;

class WY3DAPP_EXPORT Locate2dContext : public Snap2dContext
{
    WYRX_DECLARE_MEMBERS(Locate2dContext, wy3dApp::Locate2dContext, Snap2dContext)
public:
    Locate2dContext() = default;
};
typedef std::shared_ptr<Locate2dContext> Locate2dContextSPtr;

class WY3DAPP_EXPORT DrawLine2dContext : public Snap2dContext
{
    WYRX_DECLARE_NO_CREATE_MEMBERS(DrawLine2dContext, wy3dApp::DrawLine2dContext, Snap2dContext)
public:
    explicit DrawLine2dContext(const wy::Vector2& startPoint)
        : _startPoint(startPoint) {}

    wy::Vector2 getStartPoint() const { return _startPoint; }

protected:
    wy::Vector2 _startPoint;
};
typedef std::shared_ptr<DrawLine2dContext> DrawLine2dContextSPtr;

class WY3DAPP_EXPORT DrawCircle2dContext : public Snap2dContext
{
    WYRX_DECLARE_NO_CREATE_MEMBERS(DrawCircle2dContext, wy3dApp::DrawCircle2dContext, Snap2dContext)
public:
    explicit DrawCircle2dContext(const wy::Vector2& center)
        : _center(center) {}

    wy::Vector2 getCenter() const { return _center; }

protected:
    wy::Vector2 _center;
};
typedef std::shared_ptr<DrawCircle2dContext> DrawCircle2dContextSPtr;

class WY3DAPP_EXPORT DrawArc2dContext : public Snap2dContext
{
    WYRX_DECLARE_NO_CREATE_MEMBERS(DrawArc2dContext, wy3dApp::DrawArc2dContext, Snap2dContext)
public:
    DrawArc2dContext(const wy::Vector2& center, double radius, const wy::Vector2& startPoint)
        : _center(center), _radius(radius), _startPoint(startPoint) {}

    wy::Vector2 getCenter() const { return _center; }
    double getRadius() const { return _radius; }
    wy::Vector2 getStartPoint() const { return _startPoint; }

protected:
    wy::Vector2 _center;
    double _radius;
    wy::Vector2 _startPoint;
};
typedef std::shared_ptr<DrawArc2dContext> DrawArc2dContextSPtr;

} // namespace wy3dApp

#endif // WY3DAPP_SNAP2D_CONTEXT_H
