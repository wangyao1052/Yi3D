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

#include "scene/nodes/ElementNode.h"

class SolidElementNode : public ElementNode
{
    friend class Scene;
public:
    enum class FaceInfoFlag
    {
        Highlight = 0x00000001,
    };
    struct FaceInfo
    {
        unsigned int numTriangles;
        std::vector<int> edgeIndices; // 序号从0开始
        unsigned int flags;

        inline void addFlag(FaceInfoFlag flag)
        {
            flags |= static_cast<unsigned int>(flag);
        }
        inline void removeFlag(FaceInfoFlag flag)
        {
            flags &= ~static_cast<unsigned int>(flag);
        }
        inline bool hasFlag(FaceInfoFlag flag) const
        {
            return flags & static_cast<unsigned int>(flag);
        }

        FaceInfo() : numTriangles(0), flags(0) {}
    };

    enum class EdgeInfoFlag
    {
        Highlight = 0x00000001,
    };
    struct EdgeInfo
    {
        unsigned int numLines;
        unsigned int flags;

        inline void addFlag(EdgeInfoFlag flag)
        {
            flags |= static_cast<unsigned int>(flag);
        }
        inline void removeFlag(EdgeInfoFlag flag)
        {
            flags &= ~static_cast<unsigned int>(flag);
        }
        inline bool hasFlag(EdgeInfoFlag flag) const
        {
            return flags & static_cast<unsigned int>(flag);
        }

        EdgeInfo() : numLines(0), flags(0) {}
    };

public:
    SolidElementNode(const wydb::ElementId& id, osg::Vec4 color)
        : ElementNode(id),
        _solidFaceColor(color),
        _flags(0)
    {}

    // 结点类型
    virtual ElementNodeType getNodeType() const override { return ElementNodeType::Solid; }

    // 获取数据
    const std::vector<FaceInfo>& getFaceInfos() const { return _faceInfos; }
    const std::vector<EdgeInfo>& getEdgeInfos() const { return _edgeInfos; }
    osg::ref_ptr<osg::Vec3Array> getNormals() const { return _normals; }
    osg::ref_ptr<osg::UIntArray> getTriangleIndices() const { return _triangleIndices; }
    osg::ref_ptr<osg::UIntArray> getLineIndices() const { return _lineIndices; }
    const osg::Matrix& getMatrix() const { return _matrix; }
    void setSolidFaceColor(const osg::Vec4& color) { _solidFaceColor = color; }
    const osg::Vec4& getSolidFaceColor() const { return _solidFaceColor; }

    // 由PrimitiveIndex获取面的序号
    // 没有找到的话返回unsigned int(-1)
    unsigned int getFaceIndex(unsigned int primitiveIndex) const;

    // 由PrimitiveIndex获取边的序号
    // 没有找到的话返回unsigned int(-1)
    unsigned int getEdgeIndex(unsigned int primitiveIndex) const;

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
    void clearDynamicRenderGeometry();

    // 线框模式
    void setWireframe(bool flag);
    bool isWireframe() const { return _wireframe; }

    void setEdgesVisible(bool flag);
    bool isEdgesVisible() const { return _showEdges; }

protected:
    // 默认框选(完全框住才选中)
    virtual bool pickByNormalBoxImpl(osg::Polytope& polytope) const override;

    // 移动
    virtual bool transform(wydb::Database* pDb) override;

    // 生成渲染对象
    virtual void generateRenderObjectImpl(Scene* pScene, const wydb::Element* pElem) override;
    virtual void generateRenderObjectFinished(const wydb::Element* pElem) override;
    // 生成渲染数据
    virtual GenRenderDataRet generateRenderDataImpl(Scene* pScene, const wydb::Element* pElement) override;

    virtual void highlightImpl(bool flag) override;
    virtual void previewImpl(bool flag) override;
    virtual void setActiveImpl(bool flag) override;
    virtual bool updateApperance(wydb::Database* pDb) override;
    void updateColorAndTransparent();

    // 计算是否Active
    virtual bool computeWhetherActive(const wydb::Element* pCurElem) const override;

protected:
    // 清空渲染对象
    virtual void clearRenderObjects() override
    {
        ElementNode::clearRenderObjects();
        // 面
        _shapeNode = nullptr;
        _shapeGeom = nullptr;
        _nobatchShapeGeom = nullptr;
        // 边
        _edgeNode = nullptr;
        _edgeWrap = nullptr;
        _edgeGeom = nullptr;
        _edgeGeomHighlight = nullptr;
        _edgeGeomPreview = nullptr;
    }

    // 清空渲染数据
    virtual void clearRenderData() override
    {
        ElementNode::clearRenderData();
        _normals = nullptr;
        _triangleIndices = nullptr;
        _lineIndices = nullptr;
        _faceInfos.clear();
        _edgeInfos.clear();
    }

    // 初始化渲染数据
    virtual void initRenderData() override
    {
        ElementNode::initRenderData();
        _normals = new osg::Vec3Array();
        _triangleIndices = new osg::UIntArray();
        _lineIndices = new osg::UIntArray();
        _faceInfos.clear();
        _edgeInfos.clear();
    }

private:
    // 生成渲染数据
    GenRenderDataRet generateRenderData(TopoDS_Shape& shape);
    // 生成面几何
    osg::ref_ptr<osg::Geometry> generateShapeGeom(const wydb::ElementId& id, bool batch) const;
    // 生成边几何
    osg::ref_ptr<osg::Geometry> generateEdgeGeom(const wydb::ElementId& id) const;
    // 启用NoBatch渲染
    void startNoBatchRender_Face();
    // 停止NoBatch渲染
    void endNoBatchRender_Face();
    // 设置面的颜色
    bool setFaceColor(unsigned int faceIndex, const osg::Vec4& color);
    // 获取面的默认颜色
    osg::Vec4 getFaceDefaultColor() const;
    // 获取边的默认颜色
    osg::Vec4 getEdgeDefaultColor() const;

    // 生成高亮边几何
    osg::ref_ptr<osg::Geometry> generateEdgeGeom_Highlight();
    // 生成预览边几何
    // 按照目前的设计,任意时刻最多只有一条边处于预览状态
    osg::ref_ptr<osg::Geometry> generateEdgeGeom_Preview(unsigned int edgeIndex);

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
    // 初始包围盒
    osg::BoundingBox _boundBoxInit;
    // 实体默认面色
    osg::Vec4 _solidFaceColor;

    // 标志信息
    unsigned int _flags;
    // 线框模式
    bool _wireframe = false;
    // 边是否可见
    bool _showEdges = true;

    //---------------------------------
    // 渲染对象
    //---------------------------------
    // 面
    osg::ref_ptr<osg::Group> _shapeNode;
    osg::ref_ptr<osg::Geometry> _shapeGeom;        // batch
    osg::ref_ptr<osg::Geometry> _nobatchShapeGeom; // no batch

    // 边
    osg::ref_ptr<osg::Group> _edgeNode;
    osg::ref_ptr<osg::Group> _edgeWrap;
    osg::ref_ptr<osg::Geometry> _edgeGeom;
    osg::ref_ptr<osg::Geometry> _edgeGeomHighlight; // 高亮
    osg::ref_ptr<osg::Geometry> _edgeGeomPreview;   // 预览

    //---------------------------------
    // 渲染数据
    //---------------------------------
    // 法线数据
    osg::ref_ptr<osg::Vec3Array> _normals;
    // 三角面片索引
    osg::ref_ptr<osg::UIntArray> _triangleIndices;
    // 线索引
    osg::ref_ptr<osg::UIntArray> _lineIndices;

    // 面数据
    std::vector<FaceInfo> _faceInfos;

    // 边数据
    std::vector<EdgeInfo> _edgeInfos;

    //---------------------------------
    // 变换矩阵
    // 实体拓扑形体TopoDS_Shape中存储了变换矩阵的信息(TopoDS_Shape::Location())
    // 渲染数据是基于TopoDS_Shape中变换矩阵为单位矩阵生成的
    // TopoDS_Shape shape = pSolid->getShape();
    // shape.Location(loc, Standard_False);
    // return this->generateRenderData(shape);
    // 对应TopoDS_Shape::Location
    //---------------------------------
    osg::Matrix _matrix;
};

#endif // WY3DAPP_SOLID_ELEMENT_NODE_H
