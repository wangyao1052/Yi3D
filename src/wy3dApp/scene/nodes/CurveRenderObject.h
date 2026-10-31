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

#ifndef WY3DAPP_CURVE_RENDER_OBJECT_H
#define WY3DAPP_CURVE_RENDER_OBJECT_H

#include <memory>

#include "scene/nodes/LineRenderObject.h"

namespace wy3dApp {

// 曲线渲染样式
struct CurveRenderStyle : LineRenderStyle
{
    osg::Vec4 highlightOverlayColor;
    osg::Vec4 previewOverlayColor;
};

// 曲线渲染对象:线几何 + 按曲线索引的高亮/预览叠加几何
class CurveRenderObject : public RenderObject
{
public:
    CurveRenderObject(const wydb::ElementId& id,
        ElementNodeType nodeType,
        CurveRenderData& data,
        osg::UIntArray& indices,
        const CurveRenderStyle& style);
    ~CurveRenderObject() override;

    void build();

    // 元素级高亮/预览
    void setElementHighlight(bool flag);
    void setElementPreview(bool flag);

    // 单条曲线高亮/预览
    void highlightCurveByIndex(unsigned int curveIndex, bool flag);
    void previewCurveByIndex(unsigned int curveIndex, bool flag);

    void clearDynamicRenderGeometry() override;

    osg::Geometry* getGeometry() const;

private:
    osg::ref_ptr<osg::Geometry> generateHighlightGeom() const;
    osg::ref_ptr<osg::Geometry> generatePreviewGeom(unsigned int curveIndex) const;

private:
    wydb::ElementId _id;
    ElementNodeType _nodeType;
    CurveRenderData* _data;
    osg::ref_ptr<osg::UIntArray> _indices;
    CurveRenderStyle _style;

    std::unique_ptr<LineRenderObject> _line;
    osg::ref_ptr<osg::Geometry> _highlightGeom;
    osg::ref_ptr<osg::Geometry> _previewGeom;
};

} // namespace wy3dApp

#endif // WY3DAPP_CURVE_RENDER_OBJECT_H
