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

#ifndef WY3DAPP_SOLID_MODIFICATION_ELEMENT_NODE_H
#define WY3DAPP_SOLID_MODIFICATION_ELEMENT_NODE_H

#include <osg/Geometry>

#include "scene/nodes/ElementNode.h"

class BodyModificationElementNode : public ElementNode
{
    friend class Scene;
public:
    explicit BodyModificationElementNode(const wydb::ElementId& id) : ElementNode(id) {}

    // 结点类型
    virtual ElementNodeType getNodeType() const override { return ElementNodeType::BodyModification; }

protected:
    // 移动(不支持)
    virtual bool transform(wydb::Database* pDb) override;

    // 生成渲染对象
    virtual void generateRenderObjectImpl(Scene* pScene, const wydb::Element* pElem) override;
    // 生成渲染数据
    virtual GenRenderDataRet generateRenderDataImpl(Scene* pScene, const wydb::Element* pElement) override;

    virtual void highlightImpl(bool flag) override;
    virtual void previewImpl(bool flag) override;
    virtual void setActiveImpl(bool flag) override;
    void updateColorAndTransparent();

    virtual bool computeWhetherActive(const wydb::Element* pCurElem) const override;

protected:
    // 清空渲染对象
    virtual void clearRenderObjects() override
    {
        ElementNode::clearRenderObjects();
        // 面
        _shapeNode = nullptr;
        _shapeGeom = nullptr;
        // 边
        _edgeNode = nullptr;
        _edgeGeom = nullptr;
    }

    // 清空渲染数据
    virtual void clearRenderData() override
    {
        ElementNode::clearRenderData();
        _triangleIndices = nullptr;
        _lineIndices = nullptr;
    }

    // 初始化渲染数据
    virtual void initRenderData() override
    {
        ElementNode::initRenderData();
        _triangleIndices = new osg::UIntArray();
        _lineIndices = new osg::UIntArray();
    }

private:
    osg::ref_ptr<osg::Geometry> generateShapeGeom(const wydb::ElementId& id) const;
    osg::ref_ptr<osg::Geometry> generateEdgeGeom(const wydb::ElementId& id) const;

private:
    // 初始包围盒
    osg::BoundingBox _boundBoxInit;

    //---------------------------------
    // 渲染对象
    //---------------------------------
    // 面
    osg::ref_ptr<osg::Group> _shapeNode;
    osg::ref_ptr<osg::Geometry> _shapeGeom;

    // 边
    osg::ref_ptr<osg::Group> _edgeNode;
    osg::ref_ptr<osg::Geometry> _edgeGeom;

    //---------------------------------
    // 渲染数据
    //---------------------------------
    // 法线数据
    osg::ref_ptr<osg::Vec3Array> _normals;
    // 三角面片索引
    osg::ref_ptr<osg::UIntArray> _triangleIndices;
    // 线索引
    osg::ref_ptr<osg::UIntArray> _lineIndices;
};

#endif // WY3DAPP_SOLID_MODIFICATION_ELEMENT_NODE_H