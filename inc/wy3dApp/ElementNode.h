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

#ifndef WY3DAPP_ELEMENT_NODE_H
#define WY3DAPP_ELEMENT_NODE_H

#include <cassert>
#include <memory>

#include <osg/Array>
#include <osg/BoundingBox>
#include <osg/Group>
#include <osg/Matrix>

#include <wydbElementId.h>
#include <wy3dSelectionType.h>
#include <wyapSelection.h>
#include <wy3dApp/ElementNodeType.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace osg
{
    class Drawable;
    class Polytope;
}

namespace wydb
{
    class Database;
    class Element;
}

namespace wy3dApp {

class ElementNodeImpl;
class Scene;
class SceneImpl;

class WY3DAPP_EXPORT RenderData
{
public:
    RenderData();
    virtual ~RenderData();

    RenderData(const RenderData&) = delete;
    RenderData& operator=(const RenderData&) = delete;

    // 顶点数据
    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
};

// 元素节点
// 生命周期由Scene类管理
class WY3DAPP_EXPORT ElementNode
{
protected:
    // 构造析构
    explicit ElementNode(const wydb::ElementId& id);
    virtual ~ElementNode();

public:
    virtual ElementNodeType getNodeType() const = 0;

    // 由拾取到的渲染对象创建选择对象
    // 默认实现:请求整元素时返回整个元素,其余情况视为调用错误
    virtual wyap::Selection createSelection(
        osg::Drawable* pDrawable,
        unsigned int primitiveIndex,
        wy3d::SelectionType selType) const;

    // 获取顶点数组;元素没有形体或生成失败时返回 nullptr
    osg::ref_ptr<osg::Vec3Array> getVertices() const
    {
        return _renderData ? _renderData->vertices : nullptr;
    }

public:
    // 高亮
    void highlight(bool flag, bool forced = false);
    // 预览
    void preview(bool flag);

    // 是否激活
    inline bool isActive() const
    {
        return !(_status & static_cast<unsigned int>(Status::Inactive));
    }
    // 激活
    virtual void setActive(bool flag);
    // 计算是否Active
    virtual bool computeWhetherActive(const wydb::Element* pCurElem) const { return true; }

public:
    // 获取元素ID
    const wydb::ElementId& getElementId() const { return _id; }
    // 获取外包围盒
    const osg::BoundingBox& getBoundingBox() const
    {
        return _boundBox;
    }
    // 获取Osg节点
    osg::Group* getOsgNode() const { return _osgNode.get(); }
    // 重新计算NodeMask
    void recomputeNodeMask();

private:
    friend class BodyModificationElementNode;
    friend class SceneImpl;

    enum NodeMask
    {
        Visible   = 0x3FFFFFFF, // 可见:最高位为0,由高亮状态控制
        Highlight = 0x80000000, // 最高位为1
        Preview   = 0x40000000, // 次高位为1
    };

    // 状态标志
    enum class Status : unsigned int
    {
        Highlighted = 0x00000001, // 高亮
        Inactive    = 0x00000002, // 非活动状态,布尔体的成员为此状态
        Hidden      = 0x00000004, // 隐藏状态
    };
    // 是否隐藏
    inline bool isHidden() const
    {
        return _status & static_cast<unsigned int>(Status::Hidden);
    }
    // 隐藏
    virtual void hide(bool flag);

    void addStatus(Status status)
    {
        _status |= static_cast<unsigned int>(status);
    }

    void removeStatus(Status status)
    {
        _status &= ~static_cast<unsigned int>(status);
    }

    // 默认框选(完全框住才选中)
    bool pickByNormalBox(osg::Polytope& polytope) const;
    // 交叉框选(任何部分有交集就选中)
    bool pickByCrossBox(osg::Polytope& polytope) const;

protected:
    // 是否高亮
    inline bool isHighlighted() const
    {
        return _status & static_cast<unsigned int>(Status::Highlighted);
    }

    // 清空动态渲染对象(NoBatch渲染结束时调用);默认不做处理
    virtual void clearDynamicRenderGeometry() {}

    // 本地坐标到世界坐标的变换矩阵;默认单位矩阵
    virtual osg::Matrix getModelMatrix() const
    {
        return osg::Matrix::identity();
    }

    // 更新显示效果
    virtual bool updateApperance(wydb::Database* pDb);
     
    // 生成渲染对象
    bool generateRenderObject(Scene* pScene, wydb::Database* pDb);
    virtual void generateRenderObjectImpl(Scene* pScene, const wydb::Element* pElem) {}
    virtual void generateRenderObjectFinished(const wydb::Element* pElem) {}

    // 生成渲染数据；返回 nullptr 表示元素没有形体或生成失败
    // 实现内部必须自己捕获所有异常，不能抛出
    virtual std::unique_ptr<RenderData> generateRenderData(
        Scene* pScene, const wydb::Element* pElement) noexcept;

    // 按节点实际类型获取渲染数据；类型不匹配时断言
    template <typename T>
    T* getRenderDataAs() const
    {
        assert(!_renderData || dynamic_cast<T*>(_renderData.get()));
        return static_cast<T*>(_renderData.get());
    }

protected:
    // 清空渲染对象
    virtual void clearRenderObjects()
    {
        unsigned int numChild = _osgNode->getNumChildren();
        _osgNode->removeChildren(0, numChild);
    }

protected:
    virtual void highlightImpl(bool flag) {}
    virtual void previewImpl(bool flag) {}
    virtual void setActiveImpl(bool flag) {}

private:
    inline void updateVisibleNodeMask()
    {
        // 激活并显示
        if (this->isActive() && !this->isHidden())
        {
            // 显示:控制第1-31位为1                           0111 1111 1111 1111
            _osgNode->setNodeMask(_osgNode->getNodeMask() | NodeMask::Visible);
        }
        else
        {
            // 隐藏:控制第1-31位为0                            1000 0000 0000 0000
            _osgNode->setNodeMask(_osgNode->getNodeMask() & (~NodeMask::Visible));
        }
    }

protected:
    wydb::ElementId _id;

private:
    unsigned int _status;

protected:
    // OSG节点
    // 默认的NodeMask是:0111 1111 1111 1111 (0x7FFFFFFF) NodeMask::Default
    // 最高位(32位)代表高亮状态,高亮时为1,普通状态时为0;
    // 其余位:(1-31为)由Inactive和Hidden状态联合控制
    osg::ref_ptr<osg::Group> _osgNode;
    // 包围盒
    osg::BoundingBox _boundBox;

private:
    //---------------------------------
    // 渲染数据
    //---------------------------------
    std::unique_ptr<RenderData> _renderData;

    // 内部实现；当前为空壳，后续逐步迁入状态机等细节
    std::unique_ptr<ElementNodeImpl> _pImpl;
};

} // namespace wy3dApp

#endif // WY3DAPP_ELEMENT_NODE_H
