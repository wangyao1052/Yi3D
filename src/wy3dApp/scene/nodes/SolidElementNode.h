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

#include "scene/nodes/ElementRenderData.h"

#include <TopoDS_Shape.hxx>

namespace wy3dApp {

class SceneImpl;
struct MeshRenderStyle;
class MeshRenderObject;

class SolidElementNode : public ElementNode
{
    friend class SceneImpl;

public:
    SolidElementNode(const wydb::ElementId& id, osg::Vec4 color)
        : ElementNode(id),
        _solidFaceColor(color),
        _flags(0)
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
    const osg::Matrix& getMatrix() const { return _matrix; }
    void setSolidFaceColor(const osg::Vec4& color) { _solidFaceColor = color; }
    const osg::Vec4& getSolidFaceColor() const { return _solidFaceColor; }

    // 由PrimitiveIndex获取面的序号
    // 没有找到的话返回unsigned int(-1)
    unsigned int getFaceIndex(unsigned int primitiveIndex) const;

    // 由PrimitiveIndex获取边的序号
    // 没有找到的话返回unsigned int(-1)
    unsigned int getEdgeIndex(unsigned int primitiveIndex) const;

    // 由拾取结果创建选择对象
    virtual wyap::Selection createSelection(
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

    // 清空动态渲染对象
    virtual void clearDynamicRenderGeometry() override;

    // 线框模式
    void setWireframe(bool flag);
    bool isWireframe() const;

    void setEdgesVisible(bool flag);
    bool isEdgesVisible() const;

protected:
    // 本地坐标到世界坐标的变换矩阵
    osg::Matrix getModelMatrix() const override { return _matrix; }

    // 创建渲染对象
    virtual std::unique_ptr<RenderObject> createRenderObject(
        Scene* pScene, const wydb::Element* pElem) override;
    virtual void generateRenderObjectFinished(const wydb::Element* pElem) override;
    // 生成渲染数据
    virtual std::unique_ptr<RenderData> generateRenderData(
        Scene* pScene, const wydb::Element* pElement) noexcept override;

    virtual void highlightImpl(bool flag) override;
    virtual void previewImpl(bool flag) override;
    virtual void setActiveImpl(bool flag) override;
    virtual bool updateApperance(wydb::Database* pDb) override;
    void updateColorAndTransparent();

    // 计算是否Active
    virtual bool computeWhetherActive(const wydb::Element* pCurElem) const override;

private:
    MeshRenderData* renderData() const { return this->getRenderDataAs<MeshRenderData>(); }
    MeshRenderObject* renderObject() const;
    MeshRenderStyle makeStyle() const;

    // 生成渲染数据
    bool generateMeshData(TopoDS_Shape& shape, MeshRenderData& data);
    // 获取面的默认颜色
    osg::Vec4 getFaceDefaultColor() const;
    // 获取边的默认颜色
    osg::Vec4 getEdgeDefaultColor() const;

private:
    // 标志位
    enum class Flag : unsigned int
    {
        NoBatchFace = 0x00000001, // 分离式渲染面
        Cut         = 0x00000010, // 切除材料
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
    // 实体默认面色
    osg::Vec4 _solidFaceColor;

    // 标志信息
    unsigned int _flags;

    //---------------------------------
    // 变换矩阵
    // 实体拓扑形体TopoDS_Shape中存储了变换矩阵的信息(TopoDS_Shape::Location())
    // 渲染数据是基于TopoDS_Shape中变换矩阵为单位矩阵生成的
    // TopoDS_Shape shape = pSolid->getShape();
    // shape.Location(loc, Standard_False);
    // return this->generateMeshData(shape);
    // 对应TopoDS_Shape::Location
    //---------------------------------
    osg::Matrix _matrix;
};

} // namespace wy3dApp

#endif // WY3DAPP_SOLID_ELEMENT_NODE_H
