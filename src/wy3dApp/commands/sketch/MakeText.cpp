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

#include "MakeText.h"
#include <cassert>
#include <osg/Group>
#include <osg/ref_ptr>
#include <wy3dSketchSpline.h>
#include <wy3dMakeSketchText.h>
#include <wy3dApp/Application.h>
#include <wy3dApp/Scene.h>
#include <wy3dApp/ElementNode.h>
#include "scene/RenderConst.h"
#include "utils/MathUtils.h"

MakeText::MakeText(GuiCommand* pGuiCmd) : GuiCmdMakeElement(pGuiCmd)
{
    // 添加临时显示节点
    _pat = new osg::PositionAttitudeTransform();
    Scene* pActiveScene = Application::instance().getActiveScene();
    if (pActiveScene)
    {
        pActiveScene->addTransient(_pat);
    }
}

MakeText::~MakeText()
{
    Scene* pActiveScene = Application::instance().getActiveScene();
    if (!pActiveScene)
    {
        assert(false);
        return;
    }

    // 删除临时显示节点
    if (_pat)
    {
        pActiveScene->removeTransient(_pat);
    }

    // 恢复隐藏节点的显示
    for (const wydb::ElementId& id : _ids)
    {
        ElementNode* pElemNode = pActiveScene->getElementNode(id);
        if (!pElemNode)
        {
            assert(false);
            continue;
        }
        pElemNode->recomputeNodeMask();
    }
}

void MakeText::collectElements(std::set<wydb::ElementId>& idSet) const
{
    for (const wydb::ElementId& id : _ids)
    {
        idSet.insert(id);
    }
}

bool MakeText::create(wydb::ElementId sketchId, const wy3d::MakeSketchText::Data& data)
{
    if (!_pDb || !_pTopTrans || !_ids.empty() || _isFinished)
    {
        return false;
    }

    std::shared_ptr<wy3d::MakeSketchText> pMakeSketchText;
    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wy::ErrorStatus error;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
    if (!pSketch) goto ABORT_TRANS;
    _sketchPlane = pSketch->getPlane();

    // 创建草图文字
    pMakeSketchText = std::make_shared<wy3d::MakeSketchText>(data);
    error = pMakeSketchText->perform(_pDb, pTrans, pSketch);
    if (wy::ErrorStatus::Ok != error)
    {
        goto ABORT_TRANS;
    }
    _pDb->getTransactionManager()->endTransaction();
    _ids = pMakeSketchText->getCreatedCurves();
    this->initCopyNodes();
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    pMakeSketchText = nullptr;
    return false;
}

void MakeText::initCopyNodes()
{
    Scene* pActiveScene = Application::instance().getActiveScene();
    if (!pActiveScene) return;
    if (!_pat) return;

    for (const wydb::ElementId& id : _ids)
    {
        ElementNode* pElemNode = pActiveScene->getElementNode(id);
        if (!pElemNode)
        {
            assert(false);
            continue;
        }
        osg::Group* pElemOsgRoot = pElemNode->getOsgNode();
        if (!pElemOsgRoot)
        {
            assert(false);
            continue;
        }
        osg::ref_ptr<osg::Group> copy = new osg::Group(*pElemOsgRoot); // 浅拷贝
        copy->setNodeMask(~PICK_MASK); // 不可PICK
        _pat->addChild(copy);

        // 暂时隐藏元素节点
        pElemOsgRoot->setNodeMask(0);
    }
}

bool MakeText::update(const wy::Vector2& pos)
{
    if (!_pat)
    {
        return false;
    }
    wy::Vector3 moveVec = _sketchPlane.value(pos) - _sketchPlane.getOrigin();
    _pat->setPosition(MathUtils::toVec3d(moveVec));
    return true;
}

bool MakeText::performTranslate(const wy::Vector2& pos)
{
    if (!_pDb || !_pTopTrans || _ids.empty() || _isFinished)
    {
        return false;
    }

    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    for (const wydb::ElementId& id : _ids)
    {
        wy3d::SketchCurve* pSketchCurve = wy3d::SketchCurve::cast(pTrans->getElementForWrite(id));
        if (!pSketchCurve)
        {
            assert(false);
            continue;
        }
        pSketchCurve->translate(pos);
    }
    _pDb->getTransactionManager()->endTransaction();

    return true;
}
