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

#include "ScaleElements.h"
#include <cassert>
#include <osg/Group>
#include <osg/ref_ptr>
#include <osg/CopyOp>
#include <wy3dImpl.h>
#include <wy3dSketch.h>
#include <wy3dSketchEntity.h>
#include "application/Application.h"
#include "scene/Scene.h"
#include "scene/nodes/ElementNode.h"
#include "scene/RenderConst.h"
#include "utils/MathUtils.h"
#include "scene/Colors.h"
#include "common/osg/OsgUtils.h"


ScaleElements::ScaleElements(GuiCommand* pGuiCmd) : GuiCmdMakeElement(pGuiCmd)
{
    _pMatrixTransform = new osg::MatrixTransform();
    Scene* pActiveScene = Application::instance().getActiveScene();
    if (pActiveScene)
    {
        pActiveScene->addTransient(_pMatrixTransform);
    }
}

ScaleElements::~ScaleElements()
{
    Scene* pActiveScene = Application::instance().getActiveScene();
    if (pActiveScene)
    {
        pActiveScene->removeTransient(_pMatrixTransform);
    }
}

bool ScaleElements::init(const wyap::SelectionSet& ss)
{
    assert(_pDb);

    std::set<wydb::ElementId> ids;
    for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
    {
        ids.insert(iter.current().getElementId());
    }
    if (ids.empty()) return false;

    Scene* pActiveScene = Application::instance().getActiveScene();
    if (!pActiveScene) return false;
    for (const wydb::ElementId& id : ids)
    {
        ElementNode* pElemNode = pActiveScene->getElementNode(id);
        if (!pElemNode)
        {
            assert(false);
            continue;
        }
        osg::Group* pElemOsgRoot = pElemNode->getOsgNode();
        if (!pElemOsgRoot) continue;
        osg::ref_ptr<osg::Group> copy = new osg::Group(*pElemOsgRoot, osg::CopyOp::DEEP_COPY_ALL); // 使用深拷贝
        OsgUtils::setNodeColor(copy.get(), Colors::kPink); // 缩放的形体使用不同的颜色
        copy->setNodeMask(~PICK_MASK);
        _pMatrixTransform->addChild(copy);
    }

    return true;
}

static osg::Matrix createScaleMatrix(const osg::Vec3d& pivotPoint, const double scale)
{
    osg::Matrix matrix = osg::Matrix::translate(-pivotPoint)
        * osg::Matrix::scale(scale, scale, scale)
        * osg::Matrix::translate(pivotPoint);
    return matrix;
}

bool ScaleElements::update(const wy3d::SketchPlane& sketchPlane, const wy::Vector2& basePnt, const double scale)
{
    if (!sketchPlane.isValid())
    {
        return false;
    }

    wy::Vector3 basePnt3d = sketchPlane.value(basePnt);
    osg::Matrix matrix = createScaleMatrix(MathUtils::toVec3d(basePnt3d), scale);
    if (_pMatrixTransform) _pMatrixTransform->setMatrix(matrix);
    return true;
}

bool ScaleElements::perform(
    const wyap::SelectionSet& ss,
    const wydb::ElementId& sketchId,
    const wy::Vector2& basePnt,
    const double scale)
{
    if (!_pDb)
    {
        assert(false);
        return false;
    }

    // 过滤元素
    std::set<wydb::ElementId> ids;
    std::list<const wydb::Element*> elements;
    for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
    {
        wydb::ElementId id = iter.current().getElementId();
        const wy3d::SketchEntity* pSketchEntity = wy3d::SketchEntity::cast(_pDb->getElement(id));
        if (!pSketchEntity) continue;
        ids.insert(id);
        elements.emplace_back(pSketchEntity);
    }
    if (ids.empty()) return false;
    if (elements.empty()) return false;

    // 缩放元素
    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans)
    {
        assert(false);
        return false;
    }
    wy3d::geom::Matrix3 mirrorMatrix;
    wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
    if (!pSketch) goto ABORT_TRANS;
    mirrorMatrix = wy3d::geom::Matrix3::createScale(wy::Vector2(scale, scale));
    for (const wydb::Element* pConstElem : elements)
    {
        if (!pConstElem)
        {
            assert(false);
            continue;
        }
        wy3d::SketchEntity* pSketchEntity = wy3d::SketchEntity::cast(pTrans->getElementForWrite(pConstElem->getId()));
        if (!pSketchEntity)
        {
            assert(false);
            continue;
        }        if (wy::ErrorStatus::Ok != pSketchEntity->translate(-basePnt))
        {
            assert(false);
            goto ABORT_TRANS;
        }
        if (wy::ErrorStatus::Ok != pSketchEntity->transform(mirrorMatrix))
        {
            assert(false);
            goto ABORT_TRANS;
        }
        if (wy::ErrorStatus::Ok != pSketchEntity->translate(basePnt))
        {
            assert(false);
            goto ABORT_TRANS;
        }
    }
    _pDb->getTransactionManager()->endTransaction();
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    return false;
}
