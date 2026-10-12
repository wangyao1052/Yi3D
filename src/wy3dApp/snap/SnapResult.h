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

#ifndef WY3DAPP_SNAP_RESULT_H
#define WY3DAPP_SNAP_RESULT_H

#include <wyVector3.h>
#include "SnapSystemBase.h"
#include "SnapObject.h"
#include "wy3dAppDefs.h"

#define REAL_SNAP_RESULT(pSnapObject) static_cast<SnapResult*>(pSnapObject)

namespace wy3dApp {

class SnapResult : public wyap::SnapResult
{
public:
    explicit SnapResult(const wy::Vector3& position): wyap::SnapResult(position) {}

    // 获取或生成渲染对象
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

// 捕捉结果:点
class SnapResultPoint : public SnapResult
{
public:
    SnapResultPoint(const wy::Vector3& position, wyap::SnapObjectSPtr pSnapObject)
        : SnapResult(position), _pSnapObject(pSnapObject)
    {}

    wyap::SnapObjectSPtr getSnapObject() const { return _pSnapObject; }

protected:
    // 生成渲染对象
    virtual osg::ref_ptr<osg::Node> generateOsgNode() const override;

private:
    wyap::SnapObjectSPtr _pSnapObject;
};

}

#endif // WY3DAPP_SNAP_RESULT_H
