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

#ifndef WY3DAPP_DATUM_PLANE_RENDER_OBJECT_H
#define WY3DAPP_DATUM_PLANE_RENDER_OBJECT_H

#include <osg/Vec4>

#include <wydbElementId.h>
#include <wy3dApp/ElementNodeType.h>
#include <wy3dApp/ElementRenderObject.h>

#include "scene/nodes/ElementRenderData.h"

namespace osg
{
    class Geometry;
}

namespace wy3dApp {

// 基准面渲染样式
struct DatumPlaneRenderStyle
{
    osg::Vec4 faceColor;
    osg::Vec4 edgeColor;
    osg::Vec4 faceHighlightColor;
    osg::Vec4 edgeHighlightColor;
    osg::Vec4 facePreviewColor;
    osg::Vec4 edgePreviewColor;
};

// 基准面渲染对象:半透明面 + 边线
class DatumPlaneRenderObject : public RenderObject
{
public:
    DatumPlaneRenderObject(const wydb::ElementId& id,
        ElementNodeType nodeType,
        MeshRenderData& data,
        const DatumPlaneRenderStyle& style);
    ~DatumPlaneRenderObject() override;

    void build();

    void setHighlighted(bool flag) override;
    void setPreviewed(bool flag) override;

    // 顶点被原地修改后刷新包围球
    void dirtyGeometryBound();

private:
    wydb::ElementId _id;
    ElementNodeType _nodeType;
    MeshRenderData* _data;
    DatumPlaneRenderStyle _style;

    osg::ref_ptr<osg::Geometry> _planeGeom;
    osg::ref_ptr<osg::Geometry> _curvesGeom;
};

} // namespace wy3dApp

#endif // WY3DAPP_DATUM_PLANE_RENDER_OBJECT_H
