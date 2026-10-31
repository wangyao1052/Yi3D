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

#ifndef WY3DAPP_BODY_MODIFICATION_RENDER_OBJECT_H
#define WY3DAPP_BODY_MODIFICATION_RENDER_OBJECT_H

#include <osg/Matrix>
#include <osg/Vec4>

#include <wydbElementId.h>
#include <wy3dApp/ElementNodeType.h>
#include <wy3dApp/ElementRenderObject.h>

#include "scene/nodes/ElementRenderData.h"

namespace osg
{
    class Geometry;
    class MatrixTransform;
}

namespace wy3dApp {

// 实体修改渲染样式
struct BodyModificationRenderStyle
{
    osg::Vec4 edgeColor;
    osg::Vec4 highlightColor;
    osg::Vec4 previewColor;
    osg::Vec4 inactiveColor;
};

// 实体修改渲染对象:只显示修改边,默认隐藏,高亮/预览时显示
class BodyModificationRenderObject : public RenderObject
{
public:
    BodyModificationRenderObject(const wydb::ElementId& id,
        ElementNodeType nodeType,
        LineRenderData& data,
        const osg::Matrix& hostMatrix,
        const BodyModificationRenderStyle& style);
    ~BodyModificationRenderObject() override;

    void build();

    // 元素级显示状态
    void setActive(bool flag) override;
    void setHighlighted(bool flag) override;
    void setPreviewed(bool flag) override;

private:
    // 按 _active/_highlighted 更新显示效果
    void applyAppearance();

    wydb::ElementId _id;
    ElementNodeType _nodeType;
    LineRenderData* _data;
    osg::Matrix _hostMatrix;
    BodyModificationRenderStyle _style;
    bool _active = true;
    bool _highlighted = false;

    osg::ref_ptr<osg::MatrixTransform> _edgeNode;
    osg::ref_ptr<osg::Geometry> _edgeGeom;
};

} // namespace wy3dApp

#endif // WY3DAPP_BODY_MODIFICATION_RENDER_OBJECT_H
