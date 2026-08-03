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

#ifndef WY3DAPP_SKETCH_ELEMENT_NODE_H
#define WY3DAPP_SKETCH_ELEMENT_NODE_H

#include "scene/nodes/ElementNode.h"

class SketchElementNode : public ElementNode
{
public:
    enum class CurveInfoFlag
    {
        Highlight = 0x00000001,
    };
    struct CurveInfo
    {
        unsigned int id;
        unsigned int numLines;
        unsigned int flags;

        inline void addFlag(CurveInfoFlag flag)
        {
            flags |= static_cast<unsigned int>(flag);
        }
        inline void removeFlag(CurveInfoFlag flag)
        {
            flags &= ~static_cast<unsigned int>(flag);
        }
        inline bool hasFlag(CurveInfoFlag flag) const
        {
            return flags & static_cast<unsigned int>(flag);
        }

        CurveInfo() : id(0), numLines(0), flags(0) {}
    };

public:
    explicit SketchElementNode(const wydb::ElementId& id) : ElementNode(id) {}

    // 结点类型
    virtual ElementNodeType getNodeType() const override { return ElementNodeType::Sketch; }

    // 获取中心线渲染对象
    osg::Geometry* getCenterLinesGeom() const { return _centerLinesGeom.get(); }

    // 由PrimitiveIndex获取线的ID
    // 没有找到的话返回unsigned int(0)
    unsigned int getCurveId(unsigned int primitiveIndex) const;
    // 由PrimitiveIndex获取中心线的ID
    // 没有找到的话返回unsigned int(0)
    unsigned int getCenterLineCurveId(unsigned int primitiveIndex) const;

    void highlightCurveByIndex(unsigned int curveIndex, bool flag);
    void highlightCurveById(unsigned int id, bool flag);
    void previewCurveByIndex(unsigned int curveIndex, bool flag);
    void previewCurveById(unsigned int id, bool flag);

    // 清空动态渲染对象
    void clearDynamicRenderGeometry();

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

    void highlightGeom(const osg::ref_ptr<osg::Geometry>& geom, bool flag, const osg::Vec4& color);

    // 计算是否Active
    virtual bool computeWhetherActive(const wydb::Element* pCurElem) const override;

    osg::ref_ptr<osg::Geometry> generateCurveGeom_Highlight();
    osg::ref_ptr<osg::Geometry> generateCurveGeom_Preview(unsigned int curveIndex);
    unsigned int getCurveIndexById(unsigned int id) const;
    unsigned int getCenterLineCurveIndexById(unsigned int id) const;
    void highlightCenterLineByIndex(unsigned int curveIndex, bool flag);
    void previewCenterLineByIndex(unsigned int curveIndex, bool flag);
    osg::ref_ptr<osg::Geometry> generateCenterLineGeom_Preview(unsigned int curveIndex);

protected:
    // 清空渲染对象
    virtual void clearRenderObjects() override
    {
        ElementNode::clearRenderObjects();

        _curveNode = nullptr;
        _curveGeom = nullptr;
        _curveGeomHighlight = nullptr;
        _curveGeomPreview = nullptr;

        _centerLinesGeom = nullptr;
        _pointsGeom = nullptr;
    }

    // 清空渲染数据
    virtual void clearRenderData() override
    {
        ElementNode::clearRenderData();

        _lineIndices = nullptr;
        _centerLineIndices = nullptr;
        _pointIndices = nullptr;
        _curveInfos.clear();
        _centerLineInfos.clear();
    }

    // 初始化渲染数据
    virtual void initRenderData() override
    {
        ElementNode::initRenderData();

        _lineIndices = new osg::UIntArray();
        _centerLineIndices = new osg::UIntArray();
        _pointIndices = new osg::UIntArray();
        _curveInfos.clear();
        _centerLineInfos.clear();
    }

private:
    //---------------------------------
    // 渲染对象
    //---------------------------------
    // 普通线
    osg::ref_ptr<osg::Group> _curveNode;
    osg::ref_ptr<osg::Geometry> _curveGeom;
    osg::ref_ptr<osg::Geometry> _curveGeomHighlight; // 高亮
    osg::ref_ptr<osg::Geometry> _curveGeomPreview;   // 预览

    // 中心线
    osg::ref_ptr<osg::Geometry> _centerLinesGeom;
    // 点
    osg::ref_ptr<osg::Geometry> _pointsGeom;

    //---------------------------------
    // 渲染数据
    //---------------------------------
    // 普通线索引
    osg::ref_ptr<osg::UIntArray> _lineIndices;
    // 中心线索引
    osg::ref_ptr<osg::UIntArray> _centerLineIndices;
    // 点索引
    osg::ref_ptr<osg::UIntArray> _pointIndices;

    // 线数据
    std::vector<CurveInfo> _curveInfos;
    // 中心线数据
    std::vector<CurveInfo> _centerLineInfos;
};

#endif // WY3DAPP_SKETCH_ELEMENT_NODE_H