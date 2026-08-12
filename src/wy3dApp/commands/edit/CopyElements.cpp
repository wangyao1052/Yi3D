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

#include "CopyElements.h"
#include <cassert>
#include <osg/Group>
#include <osg/ref_ptr>
#include <wy3dSketch.h>
#include <wy3dSketchEntity.h>
#include "application/Application.h"
#include "scene/Scene.h"
#include "scene/nodes/ElementNode.h"
#include "scene/RenderConst.h"
#include "utils/MathUtils.h"
#include "utils/CopyPasteUtil.h"


CopyElemens::CopyElemens(GuiCommand* pGuiCmd) : GuiCmdMakeElement(pGuiCmd)
{
    _pat = new osg::PositionAttitudeTransform();
    Scene* pActiveScene = Application::instance().getActiveScene();
    if (pActiveScene)
    {
        pActiveScene->addTransient(_pat);
    }
}

CopyElemens::~CopyElemens()
{
    Scene* pActiveScene = Application::instance().getActiveScene();
    if (pActiveScene)
    {
        pActiveScene->removeTransient(_pat);
    }
}

bool CopyElemens::init(const wyap::SelectionSet& ss)
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
        osg::ref_ptr<osg::Group> copy = new osg::Group(*pElemOsgRoot); // 浅拷贝
        copy->setNodeMask(~PICK_MASK); // added by wangyao 2025.08.30 不可PICK
        _pat->addChild(copy);
    }

    return true;
}

bool CopyElemens::update(const wy3d::SketchPlane& plane, const wy::Vector2& moveVec2d)
{
    wy::Vector3 moveVec = plane.value(moveVec2d) - plane.getOrigin();
    _pat->setPosition(MathUtils::toVec3d(moveVec));
    return false;
}

bool CopyElemens::update(const wy::Vector3& moveVec)
{
    _pat->setPosition(MathUtils::toVec3d(moveVec));
    return false;
}

static void getElementAllLevelsChildren(wydb::Database* pDb, const wydb::Element* pElem, std::list<wydb::ElementId>& allLevelChildren)
{
    std::vector<wydb::ElementId> children = pElem->getChildren();
    for (const wydb::ElementId& childId : children)
    {
        allLevelChildren.emplace_back(childId);
        const wydb::Element* pChildElem = pDb->getElement(childId);
        if (!pChildElem) continue;
        getElementAllLevelsChildren(pDb, pChildElem, allLevelChildren);
    }
}

bool CopyElemens::perform(
    const wyap::SelectionSet& ss,
    const wy::Vector3& moveVec,
    wydb::ElementId sketchId)
{
    if (!_pDb)
    {
        assert(false);
        return false;
    }

    std::set<wydb::ElementId> ids;
    for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
    {
        wydb::ElementId id = iter.current().getElementId();
        ids.insert(id);
    }
    if (ids.empty()) return false;

    std::vector<wydb::ElementId> elemIds;
    for (const wydb::ElementId& id : ids)
    {
        const wydb::Element* pElem = _pDb->getElement(id);
        if (!pElem) continue;
        elemIds.push_back(id);
    }
    if (elemIds.empty()) return false;

    std::shared_ptr<wyap::ElementsClipData> pClipData = wyap::Clipboard::newElementsClipData(_pDb, elemIds);
    if (!pClipData) return false;

    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans)
    {
        assert(false);
        return false;
    }
    std::vector<wydb::Element*> copyedElems;
    if (wy::ErrorStatus::Ok != wyap::Clipboard::createElements(pTrans, *pClipData, copyedElems))
    {
        assert(copyedElems.empty());
        return false;
    }
    if (copyedElems.empty())
    {
        assert(false);
        return false;
    }
    wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
    if (!pSketch)
    {
        _pDb->getTransactionManager()->abortTransaction();
        return false;
    }
    for (wydb::Element* pCopyElem : copyedElems)
    {
        pTrans->addNewlyCreatedElement(pCopyElem);
        wy3d::SketchEntity* pSketchEntity = wy3d::SketchEntity::cast(pCopyElem);
        if (!pSketchEntity) continue;
        pSketch->addEntity(pSketchEntity);
        pSketchEntity->translate(wy::Vector2(moveVec.x(), moveVec.y()));
    }
    _pDb->getTransactionManager()->endTransaction();

    return true;
}
