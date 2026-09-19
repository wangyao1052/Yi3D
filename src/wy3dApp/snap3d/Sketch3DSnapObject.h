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

#ifndef WY3DAPP_SKETCH3D_SNAP_OBJECT_H
#define WY3DAPP_SKETCH3D_SNAP_OBJECT_H

#include <wydbElementId.h>

enum class Sketch3DSnapType
{
    Horizontal    = 0,
    Vertical      = 1,
    Parallel      = 2,
    Perpendicular = 3,
    Equal         = 4,
    Tangent       = 5,
    EndPoint      = 6,
    MiddlePoint   = 7,
    CenterPoint   = 8,
};

class Sketch3DSnapObject
{
public:
    explicit Sketch3DSnapObject(const wydb::ElementId& refId) : _refId(refId) {}
    virtual ~Sketch3DSnapObject() {}

    virtual Sketch3DSnapType getType() const = 0;

    wydb::ElementId getRefId() const { return _refId; }

private:
    wydb::ElementId _refId;
};

class Sketch3DAngleSnapObject : public Sketch3DSnapObject
{
public:
    Sketch3DAngleSnapObject(Sketch3DSnapType type, wydb::ElementId id)
        : Sketch3DSnapObject(id), _type(type) {}

    virtual Sketch3DSnapType getType() const override { return _type; }

private:
    Sketch3DSnapType _type;
};

class Sketch3DEqualSnapObject : public Sketch3DSnapObject
{
public:
    Sketch3DEqualSnapObject(wydb::ElementId id, double value)
        : Sketch3DSnapObject(id), _value(value) {}

    virtual Sketch3DSnapType getType() const override { return Sketch3DSnapType::Equal; }

    double getValue() const { return _value; }

private:
    double _value;
};

class Sketch3DPointSnapObject : public Sketch3DSnapObject
{
public:
    Sketch3DPointSnapObject(Sketch3DSnapType type, wydb::ElementId id)
        : Sketch3DSnapObject(id), _type(type) {}

    virtual Sketch3DSnapType getType() const override { return _type; }

private:
    Sketch3DSnapType _type;
};

#endif // WY3DAPP_SKETCH3D_SNAP_OBJECT_H