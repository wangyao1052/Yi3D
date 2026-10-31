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
class RenderObject;
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
    void highlight(bool flag);
    // 预览
    void preview(bool flag);


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

    inline bool isHighlighted() const
    {
        return _status & static_cast<unsigned int>(Status::Highlighted);
    }

    inline bool isHidden() const
    {
        return _status & static_cast<unsigned int>(Status::Hidden);
    }

    inline bool isActive() const
    {
        return !(_status & static_cast<unsigned int>(Status::Inactive));
    }

    inline bool isPreviewed() const
    {
        return _status & static_cast<unsigned int>(Status::Previewed);
    }

    // 是否正常可见:激活且未隐藏(高亮/预览不改变该判断)
    inline bool isVisible() const
    {
        return this->isActive() && !this->isHidden();
    }

private:
    friend class BodyModificationElementNode;
    friend class SceneImpl;

    // 状态标志
    enum class Status : unsigned int
    {
        Highlighted = 0x00000001, // 高亮
        Inactive    = 0x00000002, // 非活动状态,布尔体的成员为此状态
        Hidden      = 0x00000004, // 隐藏状态
        Previewed   = 0x00000008, // 预览(临时交互态,高亮优先)
    };
    // 隐藏
    void hide(bool flag);

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
    // 清空动态渲染对象(NoBatch渲染结束时调用);默认委托给 RenderObject
    virtual void clearDynamicRenderGeometry();

    // 本地坐标到世界坐标的变换矩阵;默认单位矩阵
    virtual osg::Matrix getModelMatrix() const
    {
        return osg::Matrix::identity();
    }

    // 更新显示效果
    virtual bool updateApperance(wydb::Database* pDb);
     
    // 更新渲染对象(生成渲染数据、OSG对象及显示状态)
    bool updateRenderObject(Scene* pScene, wydb::Database* pDb);
    // 根据渲染数据创建渲染对象;返回 nullptr 表示该节点没有渲染对象.
    // 插件重写该函数时需包含 <wy3dApp/ElementRenderObject.h>.
    virtual std::unique_ptr<RenderObject> createRenderObject(
        Scene* pScene, const wydb::Element* pElem);
    virtual void generateRenderObjectFinished(const wydb::Element* pElem) {}

    virtual std::unique_ptr<RenderData> generateRenderData(
        Scene* pScene, const wydb::Element* pElement) noexcept;

    template <typename T>
    T* getRenderDataAs() const
    {
        assert(!_renderData || dynamic_cast<T*>(_renderData.get()));
        return static_cast<T*>(_renderData.get());
    }

    template <typename T>
    T* getRenderObjectAs() const
    {
        assert(!_renderObject || dynamic_cast<T*>(_renderObject.get()));
        return static_cast<T*>(_renderObject.get());
    }

protected:
    wydb::ElementId _id;

private:
    unsigned int _status;

protected:
    // OSG节点
    // NodeMask 由状态在实现内推导:正常可见时为 ElementNodeType::All(可渲染可拾取),
    // 隐藏/未激活但在高亮或预览中时为仅渲染掩码(高 8 位,不可拾取),其余为 0。
    osg::ref_ptr<osg::Group> _osgNode;
    // 包围盒
    osg::BoundingBox _boundBox;

private:
    //---------------------------------
    // 渲染数据
    //---------------------------------
    std::unique_ptr<RenderData> _renderData;

    //---------------------------------
    // 渲染对象(OSG表示层)
    //---------------------------------
    // 必须声明在 _renderData 之后: 析构逆序保证渲染对象先于数据销毁.
    std::unique_ptr<RenderObject> _renderObject;

    // 内部实现；当前为空壳，后续逐步迁入状态机等细节
    std::unique_ptr<ElementNodeImpl> _pImpl;
};

} // namespace wy3dApp

#endif // WY3DAPP_ELEMENT_NODE_H
