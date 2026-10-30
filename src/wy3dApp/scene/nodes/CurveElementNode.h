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

#ifndef WY3DAPP_CURVE_ELEMENT_NODE_H
#define WY3DAPP_CURVE_ELEMENT_NODE_H

#include <wy3dApp/ElementNode.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class CurveElementNode : public ElementNode
{
public:
    explicit CurveElementNode(const wydb::ElementId& id) : ElementNode(id) {}

    // 结点类型
    virtual ElementNodeType getNodeType() const override { return ElementNodeType::Curve; }

protected:
    virtual void generateRenderObjectImpl(Scene* pScene, const wydb::Element* pElem) override;
    virtual GenRenderDataRet generateRenderDataImpl(Scene* pScene, const wydb::Element* pElement) override;

    virtual void highlightImpl(bool flag) override;
    virtual void previewImpl(bool flag) override;
    virtual void setActiveImpl(bool flag) override;

    void highlightGeom(const osg::ref_ptr<osg::Geometry>& geom, bool flag, const osg::Vec4& color);

    // 计算是否Active
    virtual bool computeWhetherActive(const wydb::Element* pCurElem) const override;

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
    // 线
    osg::ref_ptr<osg::Geometry> _curvesGeom;

    //---------------------------------
    // 渲染数据
    //---------------------------------
    // 线索引
    osg::ref_ptr<osg::UIntArray> _lineIndices;
};

} // namespace wy3dApp

#endif // WY3DAPP_CURVE_ELEMENT_NODE_H
