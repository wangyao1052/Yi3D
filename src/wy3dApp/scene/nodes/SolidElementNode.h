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

#ifndef WY3DAPP_SOLID_ELEMENT_NODE_H
#define WY3DAPP_SOLID_ELEMENT_NODE_H

#include <wy3dApp/ElementNode.h>
#include <wy3dApp/wy3dAppDefs.h>

#include "scene/nodes/ElementRenderDataTypes.h"

#include <TopoDS_Shape.hxx>

namespace wy3d
{
    class Solid;
}

namespace wy3dApp {

class SceneImpl;
class MeshRenderObject;

class SolidElementNode : public ElementNode
{
    friend class SceneImpl;

public:
    explicit SolidElementNode(const wydb::ElementId& id)
        : ElementNode(id)
    {}

    // 结点类型
    virtual ElementNodeType getNodeType() const override { return ElementNodeType::Solid; }

    // 获取数据;返回的引用在下一次重生后失效
    const std::vector<FaceInfo>& getFaceInfos() const
    {
        const MeshRenderData* data = this->renderData();
        static const std::vector<FaceInfo> kEmpty;
        return data ? data->faceInfos : kEmpty;
    }
    const std::vector<EdgeInfo>& getEdgeInfos() const
    {
        const MeshRenderData* data = this->renderData();
        static const std::vector<EdgeInfo> kEmpty;
        return data ? data->edgeInfos : kEmpty;
    }
    osg::ref_ptr<osg::Vec3Array> getNormals() const
    {
        const MeshRenderData* data = this->renderData();
        return data ? data->normals : nullptr;
    }
    osg::ref_ptr<osg::UIntArray> getTriangleIndices() const
    {
        const MeshRenderData* data = this->renderData();
        return data ? data->triangleIndices : nullptr;
    }
    osg::ref_ptr<osg::UIntArray> getLineIndices() const
    {
        const MeshRenderData* data = this->renderData();
        return data ? data->lineIndices : nullptr;
    }
    osg::ref_ptr<osg::Vec3Array> getVertices() const
    {
        const MeshRenderData* data = this->renderData();
        return data ? data->vertices : nullptr;
    }
    osg::Matrix getMatrix() const
    {
        const MeshRenderData* data = this->renderData();
        return data ? data->matrix : osg::Matrix::identity();
    }

    // 由PrimitiveIndex获取面的序号
    // 没有找到的话返回unsigned int(-1)
    unsigned int getFaceIndex(unsigned int primitiveIndex) const;

    // 由PrimitiveIndex获取边的序号
    // 没有找到的话返回unsigned int(-1)
    unsigned int getEdgeIndex(unsigned int primitiveIndex) const;

    // 由拾取结果创建选择对象
    virtual wyap::Selection createSelectionFromPick(
        osg::Drawable* pDrawable,
        unsigned int primitiveIndex,
        wy3d::SelectionType selType) const override;

    // 高亮面
    void highlightFace(unsigned int faceIndex, bool flag);
    void highlightFace(unsigned int faceIndex, const osg::Vec4& color);
    // 预览面
    void previewFace(unsigned int faceIndex, bool flag);

    // 高亮边
    void highlightEdge(unsigned int edgeIndex, bool flag);
    // 预览边
    void previewEdge(unsigned int edgeIndex, bool flag);

    // 线框模式
    void setWireframe(bool flag);
    bool isWireframe() const;

    void setEdgesVisible(bool flag);
    bool isEdgesVisible() const;

protected:
    // 生成渲染对象
    virtual std::unique_ptr<ElementRenderObject> generateRenderObject(
        Scene* pScene, const wydb::Element* pElem) override;
    // 生成渲染数据
    virtual std::unique_ptr<ElementRenderData> generateRenderData(
        Scene* pScene, const wydb::Element* pElement) noexcept override;

    // 计算是否Active
    virtual bool computeWhetherActive(const wydb::Element* pCurElem) const override;

private:
    MeshRenderData* renderData() const { return this->getRenderDataAs<MeshRenderData>(); }
    MeshRenderObject* renderObject() const;

    // 生成渲染数据
    bool generateMeshData(TopoDS_Shape& shape, MeshRenderData& data);
};

} // namespace wy3dApp

#endif // WY3DAPP_SOLID_ELEMENT_NODE_H
