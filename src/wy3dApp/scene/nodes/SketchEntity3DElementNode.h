///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 Wang Yao <wangyao1052@163.com>
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

#ifndef WY3DAPP_SKETCH_ENTITY3D_ELEMENT_NODE_H
#define WY3DAPP_SKETCH_ENTITY3D_ELEMENT_NODE_H

#include "scene/nodes/ElementNode.h"

class SketchEntity3DElementNode : public ElementNode
{
public:
    explicit SketchEntity3DElementNode(const wydb::ElementId& id);

    // 结点类型
    virtual ElementNodeType getNodeType() const override { return ElementNodeType::Sketch3DEntity; }

protected:
    // 默认框选(完全框住才选中)
    virtual bool pickByNormalBoxImpl(osg::Polytope& polytope) const override;

    // 移动
    virtual bool transform(wydb::Database* pDb) override { return true; }

    virtual void generateRenderObjectImpl(Scene* pScene, const wydb::Element* pElem) override;
    virtual GenRenderDataRet generateRenderDataImpl(Scene* pScene, const wydb::Element* pElement) override;

    virtual void highlightImpl(bool flag) override;
    virtual void previewImpl(bool flag) override;
    virtual void setActiveImpl(bool flag) override;

protected:
    // 清空渲染对象
    virtual void clearRenderObjects() override
    {
        ElementNode::clearRenderObjects();
        _curvesGeom = nullptr;
    }

    // 清空渲染数据
    virtual void clearRenderData() override
    {
        ElementNode::clearRenderData();
        if (_lineIndices) _lineIndices = nullptr;
    }

    // 初始化渲染数据
    virtual void initRenderData() override
    {
        ElementNode::initRenderData();
        _lineIndices = new osg::UIntArray();
    }

private:
    //---------------------------------
    // 渲染对象
    //---------------------------------
    // OSG具体节点
    osg::ref_ptr<osg::Geometry> _curvesGeom;

    //---------------------------------
    // 渲染数据
    //---------------------------------
    // 线索引
    osg::ref_ptr<osg::UIntArray> _lineIndices;
};

#endif // WY3DAPP_SKETCH_ENTITY3D_ELEMENT_NODE_H
