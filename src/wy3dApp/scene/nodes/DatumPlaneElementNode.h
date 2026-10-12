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

#ifndef WY3DAPP_DATUM_PLANE_ELEMENT_NODE_H
#define WY3DAPP_DATUM_PLANE_ELEMENT_NODE_H

#include <wyVector2.h>
#include <wy3dApp/ElementNode.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class DatumPlaneElementNode : public ElementNode
{
public:
    explicit DatumPlaneElementNode(const wydb::ElementId& id);

    // 结点类型
    virtual ElementNodeType getNodeType() const override { return ElementNodeType::DatumPlane; }

    // 刷新
    void update(const wydb::Database* pDb, const osg::BoundingBox& bbox);

    // 获取数据
    const wy::Vector2& getMinPoint() const { return _min; }
    const wy::Vector2& getMaxPoint() const { return _max; }

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
        _planeGeom = nullptr;
        _curvesGeom = nullptr;
    }

    // 清空渲染数据
    virtual void clearRenderData() override
    {
        ElementNode::clearRenderData();
        if (_normals) _normals = nullptr;
        if (_triangleIndices) _triangleIndices = nullptr;
        if (_lineIndices) _lineIndices = nullptr;
    }

    // 初始化渲染数据
    virtual void initRenderData() override
    {
        ElementNode::initRenderData();
        _normals = new osg::Vec3Array();
        _triangleIndices = new osg::UIntArray();
        _lineIndices = new osg::UIntArray();
    }

private:
    //---------------------------------
    // 渲染对象
    //---------------------------------
    osg::ref_ptr<osg::Geometry> _planeGeom;  // 面
    osg::ref_ptr<osg::Geometry> _curvesGeom; // 边

    //---------------------------------
    // 渲染数据
    //---------------------------------
    // 法线数据
    osg::ref_ptr<osg::Vec3Array> _normals;
    // 三角面片索引
    osg::ref_ptr<osg::UIntArray> _triangleIndices;
    // 线索引
    osg::ref_ptr<osg::UIntArray> _lineIndices;

    //---------------------------------
    // 尺寸数据
    //---------------------------------
    wy::Vector2 _min;
    wy::Vector2 _max;
};

}

#endif // WY3DAPP_DATUM_PLANE_ELEMENT_NODE_H
