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

#ifndef WY3DAPP_SHEET_ELEMENT_NODE_H
#define WY3DAPP_SHEET_ELEMENT_NODE_H

#include <wy3dApp/ElementNode.h>
#include <wy3dApp/wy3dAppDefs.h>

#include "scene/nodes/ElementRenderDataTypes.h"

#include <TopoDS_Shape.hxx>
#include <wydbElement.h>

namespace wy3d
{
    class Sheet;
}

namespace wy3dApp {

class SceneImpl;
class MeshRenderObject;

class SheetElementNode : public ElementNode
{
    friend class SceneImpl;

public:
    explicit SheetElementNode(const wydb::ElementId& id)
        : ElementNode(id)
    {}

    virtual ElementNodeType getNodeType() const override { return ElementNodeType::Sheet; }

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

    unsigned int getFaceIndex(unsigned int primitiveIndex) const;
    unsigned int getEdgeIndex(unsigned int primitiveIndex) const;

    // 由拾取结果创建选择对象
    virtual wyap::Selection createSelectionFromPick(
        osg::Drawable* pDrawable,
        unsigned int primitiveIndex,
        wy3d::SelectionType selType) const override;

    void highlightFace(unsigned int faceIndex, bool flag);
    void highlightFace(unsigned int faceIndex, const osg::Vec4& color);
    void previewFace(unsigned int faceIndex, bool flag);

    void highlightEdge(unsigned int edgeIndex, bool flag);
    void previewEdge(unsigned int edgeIndex, bool flag);

    void setWireframe(bool flag);
    bool isWireframe() const;

    void setEdgesVisible(bool flag);
    bool isEdgesVisible() const;

protected:
    virtual std::unique_ptr<ElementRenderObject> generateRenderObject(
        Scene* pScene, const wydb::Element* pElem) override;
    virtual std::unique_ptr<ElementRenderData> generateRenderData(
        Scene* pScene, const wydb::Element* pElement) noexcept override;

    virtual bool shouldBeActive(const wydb::Element* pCurElem) const override
    {
        assert(pCurElem);
        return pCurElem->getParent().isNull();
    }

private:
    MeshRenderData* renderData() const { return this->getRenderDataAs<MeshRenderData>(); }
    MeshRenderObject* renderObject() const;

    bool generateMeshData(TopoDS_Shape& shape, MeshRenderData& data);
};

} // namespace wy3dApp

#endif // WY3DAPP_SHEET_ELEMENT_NODE_H
