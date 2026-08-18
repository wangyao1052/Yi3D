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

#include "scene/nodes/ElementNode.h"

class SheetElementNode : public ElementNode
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
        std::vector<int> edgeIndices;
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
    SheetElementNode(const wydb::ElementId& id, osg::Vec4 color)
        : ElementNode(id),
        _sheetFaceColor(color),
        _flags(0)
    {}

    virtual ElementNodeType getNodeType() const override { return ElementNodeType::Sheet; }

    const std::vector<FaceInfo>& getFaceInfos() const { return _faceInfos; }
    const std::vector<EdgeInfo>& getEdgeInfos() const { return _edgeInfos; }
    osg::ref_ptr<osg::Vec3Array> getNormals() const { return _normals; }
    osg::ref_ptr<osg::UIntArray> getTriangleIndices() const { return _triangleIndices; }
    osg::ref_ptr<osg::UIntArray> getLineIndices() const { return _lineIndices; }
    const osg::Matrix& getMatrix() const { return _matrix; }
    void setSheetFaceColor(const osg::Vec4& color) { _sheetFaceColor = color; }
    const osg::Vec4& getSheetFaceColor() const { return _sheetFaceColor; }

    unsigned int getFaceIndex(unsigned int primitiveIndex) const;
    unsigned int getEdgeIndex(unsigned int primitiveIndex) const;

    void highlightFace(unsigned int faceIndex, bool flag);
    void highlightFace(unsigned int faceIndex, const osg::Vec4& color);
    void previewFace(unsigned int faceIndex, bool flag);

    void highlightEdge(unsigned int edgeIndex, bool flag);
    void previewEdge(unsigned int edgeIndex, bool flag);

    void clearDynamicRenderGeometry();

    void setWireframe(bool flag);
    bool isWireframe() const { return _wireframe; }

protected:
    virtual bool pickByNormalBoxImpl(osg::Polytope& polytope) const override;
    virtual bool transform(wydb::Database* pDb) override;

    virtual void generateRenderObjectImpl(Scene* pScene, const wydb::Element* pElem) override;
    virtual void generateRenderObjectFinished(const wydb::Element* pElem) override;
    virtual GenRenderDataRet generateRenderDataImpl(Scene* pScene, const wydb::Element* pElement) override;

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

protected:
    virtual void clearRenderObjects() override
    {
        ElementNode::clearRenderObjects();
        _shapeNode = nullptr;
        _shapeGeom = nullptr;
        _nobatchShapeGeom = nullptr;
        _edgeNode = nullptr;
        _edgeGeom = nullptr;
        _edgeGeomHighlight = nullptr;
        _edgeGeomPreview = nullptr;
    }

    virtual void clearRenderData() override
    {
        ElementNode::clearRenderData();
        _normals = nullptr;
        _triangleIndices = nullptr;
        _lineIndices = nullptr;
        _faceInfos.clear();
        _edgeInfos.clear();
    }

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
    GenRenderDataRet generateRenderData(TopoDS_Shape& shape);
    osg::ref_ptr<osg::Geometry> generateShapeGeom(const wydb::ElementId& id, bool batch) const;
    osg::ref_ptr<osg::Geometry> generateEdgeGeom(const wydb::ElementId& id) const;
    void startNoBatchRender_Face();
    void endNoBatchRender_Face();
    bool setFaceColor(unsigned int faceIndex, const osg::Vec4& color);
    osg::Vec4 getFaceDefaultColor() const;
    osg::Vec4 getEdgeDefaultColor() const;

    osg::ref_ptr<osg::Geometry> generateEdgeGeom_Highlight();
    osg::ref_ptr<osg::Geometry> generateEdgeGeom_Preview(unsigned int edgeIndex);

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
    osg::BoundingBox _boundBoxInit;
    osg::Vec4 _sheetFaceColor;
    unsigned int _flags;
    bool _wireframe = false;

    osg::ref_ptr<osg::Group> _shapeNode;
    osg::ref_ptr<osg::Geometry> _shapeGeom;
    osg::ref_ptr<osg::Geometry> _nobatchShapeGeom;

    osg::ref_ptr<osg::Group> _edgeNode;
    osg::ref_ptr<osg::Geometry> _edgeGeom;
    osg::ref_ptr<osg::Geometry> _edgeGeomHighlight;
    osg::ref_ptr<osg::Geometry> _edgeGeomPreview;

    osg::ref_ptr<osg::Vec3Array> _normals;
    osg::ref_ptr<osg::UIntArray> _triangleIndices;
    osg::ref_ptr<osg::UIntArray> _lineIndices;

    std::vector<FaceInfo> _faceInfos;
    std::vector<EdgeInfo> _edgeInfos;

    osg::Matrix _matrix;
};

#endif // WY3DAPP_SHEET_ELEMENT_NODE_H
