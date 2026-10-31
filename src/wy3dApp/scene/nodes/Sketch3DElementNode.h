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

#ifndef WY3DAPP_SKETCH3D_ELEMENT_NODE_H
#define WY3DAPP_SKETCH3D_ELEMENT_NODE_H

#include <wy3dApp/ElementNode.h>
#include <wy3dApp/wy3dAppDefs.h>

#include "scene/nodes/ElementRenderData.h"

namespace wy3dApp {

class CurveRenderObject;

class Sketch3DElementNode : public ElementNode
{
public:
    explicit Sketch3DElementNode(const wydb::ElementId& id) : ElementNode(id) {}

    // 结点类型
    virtual ElementNodeType getNodeType() const override { return ElementNodeType::Sketch3D; }

    // 由PrimitiveIndex获取图元的ID
    // 没有找到的话返回unsigned int(0)
    unsigned int getCurveId(unsigned int primitiveIndex) const;

    // 由拾取结果创建选择对象
    virtual wyap::Selection createSelection(
        osg::Drawable* pDrawable,
        unsigned int primitiveIndex,
        wy3d::SelectionType selType) const override;

    void highlightCurveByIndex(unsigned int curveIndex, bool flag);
    void highlightCurveById(unsigned int id, bool flag);
    void previewCurveByIndex(unsigned int curveIndex, bool flag);
    void previewCurveById(unsigned int id, bool flag);

protected:
    virtual std::unique_ptr<RenderObject> createRenderObject(
        Scene* pScene, const wydb::Element* pElem) override;
    virtual std::unique_ptr<RenderData> generateRenderData(
        Scene* pScene, const wydb::Element* pElement) noexcept override;

    virtual void highlightImpl(bool flag) override;
    virtual void previewImpl(bool flag) override;
    virtual void setActiveImpl(bool flag) override {}

    // 计算是否Active
    virtual bool computeWhetherActive(const wydb::Element* pCurElem) const override;

private:
    CurveRenderData* renderData() const { return this->getRenderDataAs<CurveRenderData>(); }
    CurveRenderObject* renderObject() const;
    unsigned int getCurveIndexById(unsigned int id) const;
};

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH3D_ELEMENT_NODE_H
