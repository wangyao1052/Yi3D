///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 Wang Yao <wangyao1052@163.com>
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

#ifndef WY3DAPP_SKETCH3D_SNAP_CONTEXT_H
#define WY3DAPP_SKETCH3D_SNAP_CONTEXT_H

#include <wyVector3.h>

enum class Sketch3DSnapContextType
{
    Locate     = 0,
    DrawLine   = 1,
    DrawCircle = 2,
};

class Sketch3DSnapContext
{
public:
    Sketch3DSnapContext(Sketch3DSnapContextType type) : _type(type) {}
    virtual ~Sketch3DSnapContext() {}

    Sketch3DSnapContextType getType() const { return _type; }

private:
    Sketch3DSnapContextType _type;
};

class Sketch3DLocateContext : public Sketch3DSnapContext
{
public:
    Sketch3DLocateContext() : Sketch3DSnapContext(Sketch3DSnapContextType::Locate) {}
};

class Sketch3DDrawLineContext : public Sketch3DSnapContext
{
public:
    explicit Sketch3DDrawLineContext(const wy::Vector3& startPnt)
        : Sketch3DSnapContext(Sketch3DSnapContextType::DrawLine), _startPnt(startPnt) {}

    const wy::Vector3& getStartPoint() const { return _startPnt; }

private:
    wy::Vector3 _startPnt;
};

class Sketch3DDrawCircleContext : public Sketch3DSnapContext
{
public:
    explicit Sketch3DDrawCircleContext(const wy::Vector3& centerPnt)
        : Sketch3DSnapContext(Sketch3DSnapContextType::DrawCircle), _centerPnt(centerPnt) {}

    const wy::Vector3& getCenterPoint() const { return _centerPnt; }

private:
    wy::Vector3 _centerPnt;
};

#endif // WY3DAPP_SKETCH3D_SNAP_CONTEXT_H