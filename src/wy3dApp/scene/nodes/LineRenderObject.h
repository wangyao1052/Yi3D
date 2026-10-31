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

#ifndef WY3DAPP_LINE_RENDER_OBJECT_H
#define WY3DAPP_LINE_RENDER_OBJECT_H

#include <string>

#include <osg/Array>
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

// 线/点渲染样式
struct LineRenderStyle
{
    ElementNodeType nodeType = ElementNodeType::Undefined;

    // 颜色
    osg::Vec4 color;
    osg::Vec4 highlightColor;
    osg::Vec4 previewColor;

    // 线宽
    float lineWidth = 1.0f;
    float highlightLineWidth = 1.0f;
    bool changeLineWidthOnHighlight = true;

    // 深度测试
    bool depthTest = true;
    bool highlightDepthTest = true;
    bool changeDepthTestOnHighlight = true;

    // 虚线
    bool stipple = false;
    int stippleFactor = 1;
    unsigned short stipplePattern = 0xFFFF;

    // 点
    bool pointsWhenNoLines = false; // 没有线索引时按点渲染(只画第一个顶点)
    float pointSize = 1.0f;
    float highlightPointSize = 1.0f;
    bool changePointSizeOnHighlight = false;

    // 图元模式
    bool stripFromSegments = false; // 把线段对转换为 GL_LINE_STRIP
    bool indicesAsPoints = false;   // 索引数组按 GL_POINTS 渲染

    // 状态
    bool lighting = false;
    bool applyStateToRoot = false; // lighting/depth/renderBin 挂到内容根,否则挂到几何
    int renderBin = 0;
    std::string renderBinName = "RenderBin";
};

// 线/点渲染对象
class LineRenderObject : public RenderObject
{
public:
    LineRenderObject(const wydb::ElementId& id,
        ElementNodeType nodeType,
        LineRenderData& data,
        osg::UIntArray& indices,
        const LineRenderStyle& style);
    ~LineRenderObject() override;

    void build();

    // 元素级高亮/预览
    void setHighlighted(bool flag) override;
    void setPreviewed(bool flag) override;

    osg::Geometry* getGeometry() const { return _geom.get(); }

private:
    void applyElementState(bool flag, const osg::Vec4& color);

private:
    wydb::ElementId _id;
    ElementNodeType _nodeType;
    LineRenderData* _data;
    osg::ref_ptr<osg::UIntArray> _indices;
    LineRenderStyle _style;

    osg::ref_ptr<osg::Geometry> _geom;
    bool _isPoints = false;
};

} // namespace wy3dApp

#endif // WY3DAPP_LINE_RENDER_OBJECT_H
