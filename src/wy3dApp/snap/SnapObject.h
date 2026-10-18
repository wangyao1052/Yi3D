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

#ifndef WY3DAPP_SNAP_OBJECT_H
#define WY3DAPP_SNAP_OBJECT_H

#include <osg/Node>
#include <osg/Polytope>
#include <wyVector3.h>
#include "SnapSystemBase.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class SnapObject : public wyap::SnapObject
{
public:
    explicit SnapObject(const wydb::ElementId& id) : wyap::SnapObject(id) {}

    // 捕捉
    // polytope --- 多面体
    // position --- 如果捕捉到则返回捕捉到的点坐标
    virtual bool snap(osg::Polytope& polytope, wy::Vector3& position) const
    {
        return false;
    }

    // 获取渲染对象
    virtual osg::ref_ptr<osg::Node> getOrCreateOsgNode()
    {
        if (!_pOsgNode)
        {
            _pOsgNode = this->generateOsgNode();
        }
        return _pOsgNode;
    }

protected:
    // 生成渲染对象
    virtual osg::ref_ptr<osg::Node> generateOsgNode() const
    {
        return nullptr;
    }

private:
    osg::ref_ptr<osg::Node> _pOsgNode;
};

// 点
class SnapPoint : public SnapObject
{
public:
    enum class Type
    {
        Undefined = 0,
        End = 1,
        Middle = 2,
        Center = 3,
        Origin = 4,
        Point = 5,
        Quadrant = 6,
    };

public:
    SnapPoint(const wydb::ElementId& id, const wy::Vector3& position, Type type)
        : SnapObject(id), _position(position), _type(type) {}

    // 类型
    Type getType() const { return _type; }

    // 获取位置
    inline const wy::Vector3& getPosition() const
    {
        return _position;
    }
    // 设置位置
    inline void setPosition(const wy::Vector3& position)
    {
        _position = position;
    }

    // 捕捉
    virtual bool snap(osg::Polytope& polytope, wy::Vector3& position) const override;

    // 生成渲染对象
    virtual osg::ref_ptr<osg::Node> generateOsgNode() const override;

private:
    wy::Vector3 _position;
    Type _type;
};

} // namespace wy3dApp

#endif // WY3DAPP_SNAP_OBJECT_H
