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

#ifndef WY3DAPP_BODY_MODIFICATION_ELEMENT_NODE_H
#define WY3DAPP_BODY_MODIFICATION_ELEMENT_NODE_H

#include <wy3dApp/ElementNode.h>
#include <wy3dApp/wy3dAppDefs.h>

#include "scene/nodes/ElementRenderData.h"

namespace wy3dApp {

class SceneImpl;
class BodyModificationRenderObject;

class BodyModificationElementNode : public ElementNode
{
    friend class SceneImpl;
public:
    explicit BodyModificationElementNode(const wydb::ElementId& id);

    // 结点类型
    virtual ElementNodeType getNodeType() const override { return ElementNodeType::BodyModification; }

    // 由拾取结果创建选择对象:该节点没有可拾取几何,不应走到这里
    virtual wyap::Selection createSelection(
        osg::Drawable* pDrawable,
        unsigned int primitiveIndex,
        wy3d::SelectionType selType) const override;

protected:
    // 创建渲染对象
    virtual std::unique_ptr<RenderObject> createRenderObject(
        Scene* pScene, const wydb::Element* pElem) override;
    // 生成渲染数据
    virtual std::unique_ptr<RenderData> generateRenderData(
        Scene* pScene, const wydb::Element* pElement) noexcept override;

    virtual void highlightImpl(bool flag) override;
    virtual void previewImpl(bool flag) override;
    virtual void setActiveImpl(bool flag) override;
    void updateColorAndTransparent();

    virtual bool computeWhetherActive(const wydb::Element* pCurElem) const override;

private:
    LineRenderData* renderData() const { return this->getRenderDataAs<LineRenderData>(); }
    BodyModificationRenderObject* renderObject() const;
};

} // namespace wy3dApp

#endif // WY3DAPP_BODY_MODIFICATION_ELEMENT_NODE_H
