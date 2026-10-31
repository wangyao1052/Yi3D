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

#ifndef WY3DAPP_SKETCH_RENDER_OBJECT_H
#define WY3DAPP_SKETCH_RENDER_OBJECT_H

#include <memory>

#include "scene/nodes/LineRenderObject.h"

namespace wy3dApp {

// 草图渲染样式:普通曲线/中心线/点
struct SketchRenderStyle
{
    LineRenderStyle curveStyle;
    LineRenderStyle centerLineStyle;
    LineRenderStyle pointStyle;
    osg::Vec4 highlightOverlayColor;
    osg::Vec4 previewOverlayColor;
};

// 草图渲染对象:普通曲线 + 中心线 + 点.
// 主曲线与中心线共享同一对高亮/预览叠加几何(高亮累积,预览 last-writer-wins).
class SketchRenderObject : public RenderObject
{
public:
    SketchRenderObject(const wydb::ElementId& id,
        SketchRenderData& data,
        const SketchRenderStyle& style);
    ~SketchRenderObject() override;

    void build();

    // 元素级高亮/预览
    void setHighlighted(bool flag) override;
    void setPreviewed(bool flag) override;

    // 主曲线高亮/预览
    void highlightCurveByIndex(unsigned int curveIndex, bool flag);
    void previewCurveByIndex(unsigned int curveIndex, bool flag);

    // 中心线高亮/预览
    void highlightCenterLineByIndex(unsigned int curveIndex, bool flag);
    void previewCenterLineByIndex(unsigned int curveIndex, bool flag);

    // 中心线几何(BoxPick 指针身份)
    osg::Geometry* getCenterLinesGeom() const;

    void clearDynamicRenderGeometry() override;

private:
    osg::ref_ptr<osg::Geometry> generateHighlightGeom() const;
    osg::ref_ptr<osg::Geometry> generatePreviewGeom(const osg::UIntArray& indices,
        const std::vector<CurveRenderInfo>& infos,
        unsigned int curveIndex) const;
    osg::ref_ptr<osg::Geometry> createOverlayGeom(const osg::UIntArray& indices,
        const osg::Vec4& color) const;

private:
    wydb::ElementId _id;
    SketchRenderData* _data;
    SketchRenderStyle _style;

    std::unique_ptr<LineRenderObject> _curves;
    std::unique_ptr<LineRenderObject> _centerLines;
    std::unique_ptr<LineRenderObject> _points;

    osg::ref_ptr<osg::Geometry> _highlightGeom;
    osg::ref_ptr<osg::Geometry> _previewGeom;
};

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_RENDER_OBJECT_H
