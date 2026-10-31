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

#ifndef WY3DAPP_MESH_RENDER_OBJECT_H
#define WY3DAPP_MESH_RENDER_OBJECT_H

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

// 网格渲染样式:由节点根据元素状态与颜色解析后传入
struct MeshRenderStyle
{
    // Sheet 为 true(双面渲染+双面光照), Solid 为 false
    bool doubleSided = false;

    // 元素状态
    bool active = true;
    bool highlighted = false;
    bool cut = false;

    // 颜色
    osg::Vec4 faceColor;
    osg::Vec4 edgeColor;
    osg::Vec4 inactiveColor;
    osg::Vec4 faceHighlightColor;
    osg::Vec4 facePreviewColor;
    osg::Vec4 edgeHighlightColor;
    osg::Vec4 edgePreviewColor;
    osg::Vec4 edgePreviewWireframeColor;
    osg::Vec4 edgeHighlightOverlayColor;
    osg::Vec4 edgePreviewOverlayColor;
};

// 实体/片体共用的网格渲染对象
class MeshRenderObject final : public RenderObject
{
public:
    MeshRenderObject(const wydb::ElementId& id,
        ElementNodeType nodeType,
        MeshRenderData& data,
        const osg::Matrix& matrix);
    ~MeshRenderObject() override;

    // 构建面/边几何; initialNoBatchFace 用于跨重建保持按面渲染状态
    void build(const MeshRenderStyle& style, bool initialNoBatchFace = false);

    // 更新显示效果(颜色/透明/深度/渲染排序)
    void applyAppearance(const MeshRenderStyle& style);

    bool isNoBatchFace() const { return _noBatchFace; }

    // 线框模式
    void setWireframe(bool flag);
    bool isWireframe() const { return _wireframe; }

    // 边是否可见
    void setEdgesVisible(bool flag);
    bool isEdgesVisible() const { return _showEdges; }

    // 预览(元素级)
    void setPreview(bool flag);

    // 面高亮/预览
    void highlightFace(unsigned int faceIndex, bool flag);
    void highlightFace(unsigned int faceIndex, const osg::Vec4& color);
    void previewFace(unsigned int faceIndex, bool flag);

    // 边高亮/预览
    void highlightEdge(unsigned int edgeIndex, bool flag);
    void previewEdge(unsigned int edgeIndex, bool flag);

    // 清空动态渲染对象(NoBatch渲染结束时调用)
    void clearDynamicRenderGeometry() override;

private:
    // 生成面几何
    osg::ref_ptr<osg::Geometry> generateShapeGeom(bool batch) const;
    // 生成边几何
    osg::ref_ptr<osg::Geometry> generateEdgeGeom() const;
    // 生成高亮边几何
    osg::ref_ptr<osg::Geometry> generateEdgeGeom_Highlight() const;
    // 生成预览边几何
    osg::ref_ptr<osg::Geometry> generateEdgeGeom_Preview(unsigned int edgeIndex) const;

    // 启用NoBatch渲染
    void startNoBatchRender_Face();
    // 停止NoBatch渲染
    void endNoBatchRender_Face();
    // 设置面的颜色
    bool setFaceColor(unsigned int faceIndex, const osg::Vec4& color);

private:
    wydb::ElementId _id;
    ElementNodeType _nodeType;
    MeshRenderData* _data;
    osg::Matrix _matrix;
    MeshRenderStyle _style;

    // 线框模式
    bool _wireframe = false;
    // 边是否可见
    bool _showEdges = true;
    // 按面渲染(NoBatch)
    bool _noBatchFace = false;

    //---------------------------------
    // 渲染对象
    //---------------------------------
    // 面
    osg::ref_ptr<osg::MatrixTransform> _shapeNode;
    osg::ref_ptr<osg::Geometry> _shapeGeom;        // batch
    osg::ref_ptr<osg::Geometry> _nobatchShapeGeom; // no batch

    // 边
    osg::ref_ptr<osg::MatrixTransform> _edgeNode;
    osg::ref_ptr<osg::Group> _edgeWrap;
    osg::ref_ptr<osg::Geometry> _edgeGeom;
    osg::ref_ptr<osg::Geometry> _edgeGeomHighlight; // 高亮
    osg::ref_ptr<osg::Geometry> _edgeGeomPreview;   // 预览
};

} // namespace wy3dApp

#endif // WY3DAPP_MESH_RENDER_OBJECT_H
