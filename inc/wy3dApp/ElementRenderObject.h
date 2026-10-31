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

#ifndef WY3DAPP_ELEMENT_RENDER_OBJECT_H
#define WY3DAPP_ELEMENT_RENDER_OBJECT_H

#include <osg/Group>
#include <osg/ref_ptr>

#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

// 元素渲染对象基类.
// 插件节点可派生该类, 把自己的 OSG 内容挂到 getContentRoot() 下,
// 并通过重写 ElementNode::createRenderObject() 返回.
// 生命周期由 ElementNode 管理; 内容根由 ElementNode 挂到稳定的 _osgNode 下.
class WY3DAPP_EXPORT RenderObject
{
public:
    RenderObject();
    virtual ~RenderObject();

    RenderObject(const RenderObject&) = delete;
    RenderObject& operator=(const RenderObject&) = delete;

    // 内容根
    osg::Group* getContentRoot() const { return _node.get(); }

    // 清空全部内容
    virtual void clear();

    // 清理临时高亮/预览/NoBatch 几何
    virtual void clearDynamicRenderGeometry() {}

protected:
    osg::ref_ptr<osg::Group> _node;
};

} // namespace wy3dApp

#endif // WY3DAPP_ELEMENT_RENDER_OBJECT_H
