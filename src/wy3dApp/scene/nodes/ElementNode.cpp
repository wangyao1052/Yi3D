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

#include <osg/Matrix>
#include <osg/Polytope>

#include <wydbDatabase.h>
#include <wy3dFeature.h>

#include "scene/nodes/ElementBounds.h"
#include "scene/nodes/ElementNodeImpl.h"

namespace wy3dApp {

RenderData::RenderData() = default;
RenderData::~RenderData() = default;

ElementNode::ElementNode(const wydb::ElementId& id)
    : _id(id), _status(0), _pImpl(std::make_unique<ElementNodeImpl>())
{
    _osgNode = new osg::Group();
    _osgNode->setNodeMask(NodeMask::Visible);
}

ElementNode::~ElementNode() = default;

std::unique_ptr<RenderData> ElementNode::generateRenderData(Scene* pScene, const wydb::Element* pElement) noexcept
{
    (void)pScene;
    (void)pElement;
    return nullptr;
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
        this->setActive(this->computeWhetherActive(pElem));

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

    // 第一次生成(新建元素)不刷新高亮;之后的调用(重生)需要刷新
    const bool isRegeneration = _pImpl->renderObjectGenerated;
    _pImpl->renderObjectGenerated = true;

    // 清空渲染对象
    this->clearRenderObjects();
    // 清空渲染数据
    _renderData.reset();
    // 重置包围盒
    _boundBox.init();

    // 获取对应元素
    assert(pDb);
    const wydb::Element* pElem = pDb->getElement(_id);
    if (!pElem)
    {
        assert(false);
        return false;
    }

    // 生成渲染数据；实现内部保证不抛异常
    _renderData = this->generateRenderData(pScene, pElem);
    if (!_renderData) // 元素没有形体或生成失败
    {
        return true;
    }

    // 根据渲染数据生成OSG对象
    this->generateRenderObject(pScene, pElem);

    // 自动计算包围盒(局部坐标),再按模型矩阵变换到世界坐标
    if (_pImpl->autoComputeBoundingBox)
    {
        if (auto vertices = this->getVertices())
        {
            _boundBox = ElementBounds::compute(*vertices);
            ElementBounds::transform(_boundBox, this->getModelMatrix());
        }
    }

    // 布尔体的成员默认不显示
    this->setActive(this->computeWhetherActive(pElem));

    // added by wangyao 2025.06.04 {
    // FixBug:新建模型;隐藏某些特征;保存;再打开;隐藏的特征在模型树上是灰显的,但在场景中依然可见;
    // 在生成渲染对象时需要设置显示和隐藏状态
    // 感觉可以和上一语句的setActive合并起来就可以只调用updateApperance;稳妥起见还是先按照最小化改动的方式来修改后续再仔细斟酌
    if (pElem->isHidden())
    {
        this->hide(true);
    }
    else
    {
        this->hide(false);
    }
    // }

    // 重生时几何重新生成,需要重新应用高亮并清理可能残留的预览态
    if (isRegeneration)
    {
        this->highlight(this->isHighlighted(), true); // 强制刷新显示效果(FixBug:Gizmo拖拽布尔运算结果体松开鼠标后没有高亮显示)
    }

    // 后处理
    this->generateRenderObjectFinished(pElem);

    return true;
}

void ElementNode::recomputeNodeMask()
{
    unsigned int nodeMask(0);
    if (this->isHighlighted())
    {
        nodeMask |= NodeMask::Highlight;
    }
    _osgNode->setNodeMask(nodeMask);
    this->updateVisibleNodeMask();
}

void ElementNode::highlight(bool flag, bool forced)
{
    if (!forced)
    {
        if (this->isHighlighted() == flag)
        {
            return;
        }
    }

    // 取消预览:控制次高位为0
    _osgNode->setNodeMask(_osgNode->getNodeMask() & (~NodeMask::Preview));

    if (flag)
    {
        // 高亮:控制最高位为1
        _osgNode->setNodeMask(_osgNode->getNodeMask() | NodeMask::Highlight);
        this->addStatus(Status::Highlighted);
    }
    else
    {
        // 取消高亮:控制最高位为0
        _osgNode->setNodeMask(_osgNode->getNodeMask() & (~NodeMask::Highlight));
        this->removeStatus(Status::Highlighted);
    }

    this->highlightImpl(flag);
}

void ElementNode::preview(bool flag)
{
    // 预览态是临时态(没有变量记录是否在该状态下),不能影响当前的高亮态.
    if (this->isHighlighted())
    {
        return;
    }

    if (flag)
    {
        // 预览:控制次高位为1
        _osgNode->setNodeMask(_osgNode->getNodeMask() | NodeMask::Preview);
    }
    else
    {
        // 取消预览:控制次高位为0
        _osgNode->setNodeMask(_osgNode->getNodeMask() & (~NodeMask::Preview));
    }

    this->previewImpl(flag);
}

void ElementNode::setActive(bool flag)
{
    if (this->isActive() == flag)
    {
        return;
    }

    if (flag)
        this->removeStatus(Status::Inactive);
    else
        this->addStatus(Status::Inactive);

    this->updateVisibleNodeMask();
    this->setActiveImpl(flag);
}

void ElementNode::hide(bool flag)
{
    if (this->isHidden() == flag)
    {
        return;
    }

    if (flag)
        this->addStatus(Status::Hidden);
    else
        this->removeStatus(Status::Hidden);

    this->updateVisibleNodeMask();
}

wyap::Selection ElementNode::createSelection(
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

bool ElementNode::pickByNormalBox(osg::Polytope& polytope) const
{
    if (!_boundBox.valid()) return false;
    const osg::ref_ptr<osg::Vec3Array> vertices = this->getVertices();
    if (!vertices || vertices->empty()) return false;
    
    // 过滤掉非活动的节点
    // 布尔体的成员节点为Inactive状态
    if (!this->isActive()) return false;

    // 是否与外包围盒(世界坐标系下)有交集
    if (!polytope.contains(_boundBox))
    {
        return false;
    }

    // 是否包含所有点
    const osg::Matrix matrix = this->getModelMatrix();
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

bool ElementNode::pickByCrossBox(osg::Polytope& polytope) const
{
    if (!_boundBox.valid()) return false;
    const osg::ref_ptr<osg::Vec3Array> vertices = this->getVertices();
    if (!vertices || vertices->empty()) return false;

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
    // 目前pickByCrossBox只是在捕捉的时候会用到,cross框选还是用的OSG那一套
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
