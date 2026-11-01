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

#include <wy3dApp/ElementNode.h>

#include <cassert>

#include <wy3dApp/ElementRenderObject.h>

#include <osg/Matrix>
#include <osg/Polytope>

#include <wydbDatabase.h>
#include <wy3dFeature.h>

#include "scene/nodes/ElementBounds.h"
#include "scene/nodes/ElementNodeImpl.h"

namespace wy3dApp {

namespace {

// 元素节点状态位,仅本实现内使用
enum class Status : unsigned int
{
    Highlighted = 0x00000001, // 高亮
    Inactive    = 0x00000002, // 非活动状态,布尔体的成员为此状态
    Hidden      = 0x00000004, // 隐藏状态
    Previewed   = 0x00000008, // 预览(临时交互态,高亮优先)
};

void addStatus(unsigned int& status, Status s)
{
    status |= static_cast<unsigned int>(s);
}

void removeStatus(unsigned int& status, Status s)
{
    status &= ~static_cast<unsigned int>(s);
}

// 元素节点掩码:低 24 位为类型/可拾取区,高 8 位为仅渲染区
constexpr unsigned int kVisibleNodeMask = static_cast<unsigned int>(ElementNodeType::All);
constexpr unsigned int kRenderOnlyNodeMask = 0x80000000u;

// 按状态推导元素节点掩码
void updateVisibleNodeMask(ElementNode& node)
{
    if (node.isActive() && !node.isHidden())
    {
        // 正常可见:可渲染,可拾取
        node.getOsgNode()->setNodeMask(kVisibleNodeMask);
    }
    else if (node.isHighlighted() || node.isPreviewed())
    {
        // 隐藏/未激活,但处于高亮或预览:只渲染,不可拾取
        node.getOsgNode()->setNodeMask(kRenderOnlyNodeMask);
    }
    else
    {
        node.getOsgNode()->setNodeMask(0);
    }
}

} // namespace

bool ElementNode::isHighlighted() const
{
    return _status & static_cast<unsigned int>(Status::Highlighted);
}

bool ElementNode::isHidden() const
{
    return _status & static_cast<unsigned int>(Status::Hidden);
}

bool ElementNode::isActive() const
{
    return !(_status & static_cast<unsigned int>(Status::Inactive));
}

bool ElementNode::isPreviewed() const
{
    return _status & static_cast<unsigned int>(Status::Previewed);
}

RenderData::RenderData() = default;
RenderData::~RenderData() = default;

ElementNode::ElementNode(const wydb::ElementId& id)
    : _id(id), _status(0), _pImpl(std::make_unique<ElementNodeImpl>())
{
    _osgNode = new osg::Group();
    updateVisibleNodeMask(*this);
}

ElementNode::~ElementNode() = default;

std::unique_ptr<RenderData> ElementNode::generateRenderData(Scene* pScene, const wydb::Element* pElement)
{
    (void)pScene;
    (void)pElement;
    return nullptr;
}

std::unique_ptr<RenderObject> ElementNode::createRenderObject(Scene* pScene, const wydb::Element* pElement)
{
    (void)pScene;
    (void)pElement;
    return nullptr;
}

void ElementNode::clearDynamicRenderGeometry()
{
    if (_renderObject)
    {
        _renderObject->clearDynamicRenderGeometry();
    }
}

bool ElementNode::updateApperance(wydb::Database* pDb)
{
    // 获取对应元素
    assert(pDb);
    const wydb::Element* pElem = pDb->getElement(_id);
    if (!pElem)
    {
        assert(false);
        return false;
    }

    // 特征
    const wy3d::Feature* pFeature = wy3d::Feature::cast(pElem);
    if (pFeature)
    {
        this->activate(this->computeWhetherActive(pElem));

        if (pFeature->isHidden())
        {
            this->hide(true);
        }
        else
        {
            this->hide(false);
        }
    }

    return true;
}

bool ElementNode::updateRenderObject(Scene* pScene, wydb::Database* pDb)
{
    assert(pScene);

    _osgNode->removeChildren(0, _osgNode->getNumChildren());
    _renderObject.reset();
    _renderData.reset();
    _boundBox.init();

    // 获取对应元素
    assert(pDb);
    const wydb::Element* pElem = pDb->getElement(_id);
    if (!pElem)
    {
        assert(false);
        return false;
    }

    _renderData = this->generateRenderData(pScene, pElem);
    if (!_renderData)
    {
        return true;
    }

    _renderObject = this->createRenderObject(pScene, pElem);
    if (_renderObject)
    {
        _osgNode->addChild(_renderObject->getContentRoot());
    }

    // 自动计算包围盒(局部坐标),再按模型矩阵变换到世界坐标
    if (_pImpl->autoComputeBoundingBox)
    {
        if (_renderData && _renderData->vertices)
        {
            _boundBox = ElementBounds::compute(*_renderData->vertices);
            ElementBounds::transform(_boundBox, _renderData->matrix);
        }
    }

    // 布尔体的成员默认不显示
    this->activate(this->computeWhetherActive(pElem));

    // added by wangyao 2025.06.04 {
    // FixBug:新建模型;隐藏某些特征;保存;再打开;隐藏的特征在模型树上是灰显的,但在场景中依然可见;
    // 在生成渲染对象时需要设置显示和隐藏状态
    // 感觉可以和上一语句的activate合并起来就可以只调用updateApperance;稳妥起见还是先按照最小化改动的方式来修改后续再仔细斟酌
    if (pElem->isHidden())
    {
        this->hide(true);
    }
    else
    {
        this->hide(false);
    }
    // }

    // 几何重新生成后清除临时预览态,并把当前显示状态同步到新渲染对象
    // (FixBug:Gizmo拖拽布尔运算结果体松开鼠标后没有高亮显示)
    removeStatus(_status, Status::Previewed);
    updateVisibleNodeMask(*this);
    if (_renderObject)
    {
        _renderObject->setActive(this->isActive());
        _renderObject->setHighlighted(this->isHighlighted());
    }

    return true;
}

void ElementNode::highlight(bool flag)
{
    if (this->isHighlighted() == flag)
    {
        return;
    }

    if (flag)
    {
        addStatus(_status, Status::Highlighted);
        // 高亮优先于预览
        removeStatus(_status, Status::Previewed);
    }
    else
    {
        removeStatus(_status, Status::Highlighted);
    }

    updateVisibleNodeMask(*this);

    if (_renderObject)
    {
        _renderObject->setHighlighted(flag);
    }
}

void ElementNode::preview(bool flag)
{
    // 预览态是临时交互态,高亮优先
    if (this->isHighlighted())
    {
        return;
    }

    if (flag)
    {
        addStatus(_status, Status::Previewed);
    }
    else
    {
        removeStatus(_status, Status::Previewed);
    }

    updateVisibleNodeMask(*this);

    if (_renderObject)
    {
        _renderObject->setPreviewed(flag);
    }
}

void ElementNode::activate(bool flag)
{
    if (this->isActive() == flag)
    {
        return;
    }

    if (flag)
        removeStatus(_status, Status::Inactive);
    else
        addStatus(_status, Status::Inactive);

    updateVisibleNodeMask(*this);
    if (_renderObject)
    {
        _renderObject->setActive(flag);
    }
}

void ElementNode::hide(bool flag)
{
    if (this->isHidden() == flag)
    {
        return;
    }

    if (flag)
        addStatus(_status, Status::Hidden);
    else
        removeStatus(_status, Status::Hidden);

    updateVisibleNodeMask(*this);
}

wyap::Selection ElementNode::createSelectionFromPick(
    osg::Drawable* pDrawable,
    unsigned int primitiveIndex,
    wy3d::SelectionType selType) const
{
    (void)pDrawable;
    (void)primitiveIndex;

    // 没有子元素的节点:整元素即选择结果
    if (wy3d::SelectionTypeUtil::HasValue(selType, wy3d::SelectionType::Element))
    {
        return wyap::Selection(_id);
    }

    assert(false);
    return wyap::Selection(wydb::ElementId::kNull);
}

bool ElementNode::selectByWindow(osg::Polytope& polytope) const
{
    if (!_boundBox.valid()) return false;
    if (!_renderData || !_renderData->vertices || _renderData->vertices->empty()) return false;
    const osg::ref_ptr<osg::Vec3Array> vertices = _renderData->vertices;
    
    // 过滤掉非活动的节点
    // 布尔体的成员节点为Inactive状态
    if (!this->isActive()) return false;

    // 是否与外包围盒(世界坐标系下)有交集
    if (!polytope.contains(_boundBox))
    {
        return false;
    }

    // 是否包含所有点
    const osg::Matrix matrix = _renderData->matrix;
    if (matrix.isIdentity())
    {
        for (const osg::Vec3& vertex : *vertices)
        {
            if (!polytope.contains(vertex))
            {
                return false;
            }
        }
    }
    else
    {
        // 顶点是局部坐标:把多面体逆变换到模型坐标系再判断
        osg::Polytope transformedPolytope;
        transformedPolytope.setAndTransformProvidingInverse(polytope, matrix);

        for (const osg::Vec3& vertex : *vertices)
        {
            if (!transformedPolytope.contains(vertex))
            {
                return false;
            }
        }
    }

    return true;
}

bool ElementNode::selectByCrossing(osg::Polytope& polytope) const
{
    if (!_boundBox.valid()) return false;
    if (!_renderData || !_renderData->vertices || _renderData->vertices->empty()) return false;
    const osg::ref_ptr<osg::Vec3Array> vertices = _renderData->vertices;

    // 过滤掉非活动的节点
    // 布尔体的成员节点为Inactive状态
    if (!this->isActive()) return false;

    // 是否与外包围盒(世界坐标系下)有交集
    if (!polytope.contains(_boundBox))
    {
        return false;
    }

    // added by wangyao 2025.01.17 {
    // TODO
    // 临时这么处理,后面的逻辑是错误的
    // 目前selectByCrossing只是在捕捉的时候会用到,交叉框选还是用的OSG那一套
    return true;
    // }

    //// 是否包含某一个点
    //assert(_shapeNode);
    //osg::MatrixTransform* pMatrixTransf = dynamic_cast<osg::MatrixTransform*>(_shapeNode.get());
    //if (pMatrixTransf)
    //{
    //    // 构建模型坐标系下的多面体
    //    osg::Polytope transformedPolytope;
    //    transformedPolytope.setAndTransformProvidingInverse(polytope, pMatrixTransf->getMatrix());

    //    for (const osg::Vec3& vertex : *_vertices) // 模型坐标系下的点坐标
    //    {
    //        if (transformedPolytope.contains(vertex))
    //        {
    //            return true;
    //        }
    //    }
    //}
    //else
    //{
    //    for (const osg::Vec3& vertex : *_vertices) // 模型坐标系下的点坐标
    //    {
    //        if (polytope.contains(vertex))
    //        {
    //            return true;
    //        }
    //    }
    //}
    
    return false;
}

} // namespace wy3dApp
