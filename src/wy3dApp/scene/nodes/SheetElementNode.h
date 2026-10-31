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

#include "scene/nodes/ElementRenderData.h"

#include <TopoDS_Shape.hxx>
#include <wydbElement.h>

namespace wy3dApp {

class SceneImpl;
struct MeshRenderStyle;
class MeshRenderObject;

class SheetElementNode : public ElementNode
{
    friend class SceneImpl;

public:
    SheetElementNode(const wydb::ElementId& id, osg::Vec4 color)
        : ElementNode(id),
        _sheetFaceColor(color),
        _flags(0)
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
    const osg::Matrix& getMatrix() const { return _matrix; }
    void setSheetFaceColor(const osg::Vec4& color) { _sheetFaceColor = color; }
    const osg::Vec4& getSheetFaceColor() const { return _sheetFaceColor; }

    unsigned int getFaceIndex(unsigned int primitiveIndex) const;
    unsigned int getEdgeIndex(unsigned int primitiveIndex) const;

    // 由拾取结果创建选择对象
    virtual wyap::Selection createSelection(
        osg::Drawable* pDrawable,
        unsigned int primitiveIndex,
        wy3d::SelectionType selType) const override;

    void highlightFace(unsigned int faceIndex, bool flag);
    void highlightFace(unsigned int faceIndex, const osg::Vec4& color);
    void previewFace(unsigned int faceIndex, bool flag);

    void highlightEdge(unsigned int edgeIndex, bool flag);
    void previewEdge(unsigned int edgeIndex, bool flag);

    virtual void clearDynamicRenderGeometry() override;

    void setWireframe(bool flag);
    bool isWireframe() const;

    void setEdgesVisible(bool flag);
    bool isEdgesVisible() const;

protected:
    // 本地坐标到世界坐标的变换矩阵
    osg::Matrix getModelMatrix() const override { return _matrix; }

    virtual std::unique_ptr<RenderObject> createRenderObject(
        Scene* pScene, const wydb::Element* pElem) override;
    virtual void generateRenderObjectFinished(const wydb::Element* pElem) override;
    virtual std::unique_ptr<RenderData> generateRenderData(
        Scene* pScene, const wydb::Element* pElement) noexcept override;

    virtual void highlightImpl(bool flag) override;
    virtual void previewImpl(bool flag) override;
    virtual void setActiveImpl(bool flag) override;
    virtual bool updateApperance(wydb::Database* pDb) override;
    void updateColorAndTransparent();

    virtual bool computeWhetherActive(const wydb::Element* pCurElem) const override
    {
        assert(pCurElem);
        return pCurElem->getParent().isNull();
    }

private:
    MeshRenderData* renderData() const { return this->getRenderDataAs<MeshRenderData>(); }
    MeshRenderObject* renderObject() const;
    MeshRenderStyle makeStyle() const;

    bool generateMeshData(TopoDS_Shape& shape, MeshRenderData& data);
    osg::Vec4 getFaceDefaultColor() const;
    osg::Vec4 getEdgeDefaultColor() const;

private:
    enum class Flag : unsigned int
    {
        NoBatchFace = 0x00000001,
    };

    inline void addFlag(Flag flag)
    {
        _flags |= static_cast<unsigned int>(flag);
    }
    inline void removeFlag(Flag flag)
    {
        _flags &= ~static_cast<unsigned int>(flag);
    }
    inline bool hasFlag(Flag flag) const
    {
        return _flags & static_cast<unsigned int>(flag);
    }

private:
    osg::Vec4 _sheetFaceColor;
    unsigned int _flags;

    osg::Matrix _matrix;
};

} // namespace wy3dApp

#endif // WY3DAPP_SHEET_ELEMENT_NODE_H
