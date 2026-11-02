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

#include "scene/nodes/ElementBounds.h"
#include "scene/nodes/ElementNodeImpl.h"

namespace wy3dApp {

ElementNode::ElementNode(const wydb::ElementId& id)
    : _id(id), _pImpl(std::make_unique<ElementNodeImpl>())
{
    _osgNode = new osg::Group();
    _pImpl->updateNodeMask(_osgNode.get());
}

ElementNode::~ElementNode() = default;

bool ElementNode::isHighlighted() const
{
    return _pImpl->isHighlighted();
}

bool ElementNode::isHidden() const
{
    return _pImpl->isHidden();
}

bool ElementNode::isActive() const
{
    return _pImpl->isActive();
}

bool ElementNode::isPreviewed() const
{
    return _pImpl->isPreviewed();
}

bool ElementNode::regenerate(Scene* pScene, wydb::Database* pDb)
{
    assert(pScene);
    assert(pDb);

    _osgNode->removeChildren(0, _osgNode->getNumChildren());
    _renderObject.reset();
    _renderData.reset();
    _boundBox.init();

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

    _renderObject = this->generateRenderObject(pScene, pElem);
    if (_renderObject)
    {
        _osgNode->addChild(_renderObject->getContentRoot());
    }

    _boundBox = this->computeBoundingBox(_renderData.get(), _renderObject.get());

    this->updateAppearance(pDb);

    return true;
}

osg::BoundingBox ElementNode::computeBoundingBox(
    const ElementRenderData* pRenderData,
    const ElementRenderObject* pRenderObject) const
{
    if (!pRenderData || !pRenderData->vertices || !pRenderObject)
    {
        return osg::BoundingBox();
    }

    osg::BoundingBox bbox = ElementBounds::compute(*pRenderData->vertices);
    ElementBounds::transform(bbox, pRenderData->matrix);
    return bbox;
}

bool ElementNode::updateAppearance(wydb::Database* pDb)
{
    if (!pDb)
    {
        assert(false);
        return false;
    }

    const wydb::Element* pElem = pDb->getElement(_id);
    if (!pElem)
    {
        assert(false);
        return false;
    }

    const bool active = this->shouldBeActive(pElem);
    if (active) _pImpl->removeStatus(ElementNodeImpl::Status::Inactive);
    else _pImpl->addStatus(ElementNodeImpl::Status::Inactive);

    const bool hidden = pElem->isHidden();
    if (hidden) _pImpl->addStatus(ElementNodeImpl::Status::Hidden);
    else _pImpl->removeStatus(ElementNodeImpl::Status::Hidden);

    _pImpl->updateNodeMask(_osgNode.get());

    if (_renderObject)
    {
        _renderObject->updateAppearance(_pImpl->getDisplayState(), pElem);
    }
    return true;
}

void ElementNode::highlight(bool flag)
{
    if (this->isHighlighted() == flag) return;

    if (flag)
    {
        _pImpl->addStatus(ElementNodeImpl::Status::Highlighted);
        _pImpl->removeStatus(ElementNodeImpl::Status::Previewed);
    }
    else
    {
        _pImpl->removeStatus(ElementNodeImpl::Status::Highlighted);
    }
    _pImpl->updateNodeMask(_osgNode.get());

    if (_renderObject)
    {
        _renderObject->updateAppearance(_pImpl->getDisplayState(), nullptr);
    }
}

void ElementNode::preview(bool flag)
{
    if (this->isHighlighted())
    {
        assert(!this->isPreviewed());
        return;
    }

    if (this->isPreviewed() == flag) return;
    if (flag)
    {
        _pImpl->addStatus(ElementNodeImpl::Status::Previewed);
    }
    else
    {
        _pImpl->removeStatus(ElementNodeImpl::Status::Previewed);
    }
    _pImpl->updateNodeMask(_osgNode.get());

    if (_renderObject)
    {
        _renderObject->updateAppearance(_pImpl->getDisplayState(), nullptr);
    }
}

void ElementNode::activate(bool flag)
{
    if (this->isActive() == flag) return;
    if (flag)
    {
        _pImpl->removeStatus(ElementNodeImpl::Status::Inactive);
    }
    else
    {
        _pImpl->addStatus(ElementNodeImpl::Status::Inactive);
    }
    _pImpl->updateNodeMask(_osgNode.get());

    if (_renderObject)
    {
        _renderObject->updateAppearance(_pImpl->getDisplayState(), nullptr);
    }
}

void ElementNode::hide(bool flag)
{
    if (this->isHidden() == flag) return;
    if (flag)
    {
        _pImpl->addStatus(ElementNodeImpl::Status::Hidden);
    }
    else
    {
        _pImpl->removeStatus(ElementNodeImpl::Status::Hidden);
    }
    _pImpl->updateNodeMask(_osgNode.get());
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
