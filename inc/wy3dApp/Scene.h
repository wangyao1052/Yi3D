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

#ifndef WY3DAPP_SCENE_H
#define WY3DAPP_SCENE_H

#include <osg/BoundingSphere>

#include <wydbElementId.h>
#include <wyapScene.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace osg
{
    class Group;
    class Node;
}

namespace wy3dApp {

class ElementNode;

// 场景的公共接口。具体实现见 src/wy3dApp/scene/SceneImpl.h。
// 外部模块只通过该接口访问场景，不依赖场景内部节点和实现细节。
class WY3DAPP_EXPORT Scene : public wyap::Scene
{
protected:
    explicit Scene(wyap::Document* pDoc);
    ~Scene() override;
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;
    Scene(Scene&&) = delete;
    Scene& operator=(Scene&&) = delete;

public:
    // 显示模式
    enum class DisplayMode { ShadedWithEdges = 0, Shaded = 1, Wireframe = 2 };

    // 场景根节点
    virtual osg::Group* getRoot() const = 0;
    // Gizmo 组节点
    virtual osg::Group* getGizmosGroup() const = 0;

    // 临时渲染对象
    virtual osg::Group* getTransientRoot() const = 0;
    virtual void addTransient(osg::Node* pNode) = 0;
    virtual void removeTransient(osg::Node* pNode) = 0;
    virtual void clearTransients() = 0;

    // 捕捉对象
    virtual osg::Group* getSnapObjectsRoot() const = 0;
    virtual void addSnapObject(osg::Node* pNode) = 0;
    virtual void removeSnapObject(osg::Node* pNode) = 0;
    virtual void clearSnapObjects() = 0;

    // 获取元素节点
    virtual ElementNode* getElementNode(const wydb::ElementId& id) const = 0;

    // 显示模式
    virtual DisplayMode getDisplayMode() const = 0;
    virtual void setDisplayMode(DisplayMode mode) = 0;

    // fit-view 使用的包围球（排除基准面和不可见元素）
    virtual osg::BoundingSphere getFitViewBoundingSphere() const = 0;
};

} // namespace wy3dApp

#endif // WY3DAPP_SCENE_H
