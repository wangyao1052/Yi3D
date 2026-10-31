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

#ifndef WY3DAPP_DATUM_PLANE_ELEMENT_NODE_H
#define WY3DAPP_DATUM_PLANE_ELEMENT_NODE_H

#include <wyVector2.h>
#include "DatumElementNode.h"
#include "scene/nodes/ElementRenderData.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class DatumPlaneRenderObject;

class DatumPlaneElementNode : public DatumElementNode
{
public:
    explicit DatumPlaneElementNode(const wydb::ElementId& id);

    // 刷新:按实体范围重算基准面的显示尺寸
    virtual void updateVisualSize(
        const wydb::Database* pDb, const osg::BoundingBox& bbox) override;

    // 获取数据
    const wy::Vector2& getMinPoint() const { return _min; }
    const wy::Vector2& getMaxPoint() const { return _max; }

protected:
    virtual std::unique_ptr<RenderObject> createRenderObject(
        Scene* pScene, const wydb::Element* pElem) override;
    virtual std::unique_ptr<RenderData> generateRenderData(
        Scene* pScene, const wydb::Element* pElement) noexcept override;

    virtual void highlightImpl(bool flag) override;
    virtual void previewImpl(bool flag) override;
    virtual void setActiveImpl(bool flag) override {}

private:
    MeshRenderData* renderData() const { return this->getRenderDataAs<MeshRenderData>(); }
    DatumPlaneRenderObject* renderObject() const;

    //---------------------------------
    // 尺寸数据
    //---------------------------------
    wy::Vector2 _min;
    wy::Vector2 _max;
};

} // namespace wy3dApp

#endif // WY3DAPP_DATUM_PLANE_ELEMENT_NODE_H
