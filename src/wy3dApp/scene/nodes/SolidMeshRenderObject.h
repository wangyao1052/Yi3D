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

#ifndef WY3DAPP_SOLID_MESH_RENDER_OBJECT_H
#define WY3DAPP_SOLID_MESH_RENDER_OBJECT_H

#include "scene/nodes/MeshRenderObject.h"

namespace wy3d { class Solid; }

namespace wy3dApp {

// 实体的网格渲染对象：外观状态机独立实现
class SolidMeshRenderObject final : public MeshRenderObject
{
public:
    SolidMeshRenderObject(const wydb::ElementId& id,
        ElementNodeType nodeType,
        MeshRenderData& data,
        const wy3d::Solid* pSolid);

private:
    void updateAppearance(
        const DisplayState& dispState,
        const wydb::Element* pElement) override;

    osg::Vec4 getFaceDefaultColor() const override;
    osg::Vec4 getEdgeDefaultColor() const override;

    // 从元素刷新颜色与 cut 状态
    void updateStyle(const wy3d::Solid& solid);

    osg::Vec4 _faceColor;
    bool _cut = false;
};

} // namespace wy3dApp

#endif // WY3DAPP_SOLID_MESH_RENDER_OBJECT_H
