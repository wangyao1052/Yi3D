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

#include "MoveElements.h"
#include <cassert>

#include <osg/Group>
#include <osg/ref_ptr>

#include <wy3dPrimitive.h>
#include <wy3dSketch.h>
#include <wy3dSketchEntity.h>
#include <wy3dSheet.h>
#include <wy3dMove.h>

#include "application/Application.h"
#include "scene/Scene.h"
#include "scene/nodes/ElementNode.h"
#include "scene/RenderConst.h"


MoveElemens::MoveElemens(GuiCommand* pGuiCmd) : GuiCmdMakeElement(pGuiCmd)
{
    _pat = new osg::PositionAttitudeTransform();
    Scene* pActiveScene = Application::instance().getActiveScene();
    if (pActiveScene)
    {
        pActiveScene->addTransient(_pat);
    }
}

MoveElemens::~MoveElemens()
{
    Scene* pActiveScene = Application::instance().getActiveScene();
    if (pActiveScene)
    {
        pActiveScene->removeTransient(_pat);
    }
}

bool MoveElemens::init(const wyap::SelectionSet& ss)
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

bool MoveElemens::update(const wy3d::SketchPlane& plane, const wy::Vector2& moveVec2d)
{
    wy::Vector3 moveVec = plane.value(moveVec2d) - plane.getOrigin();
    _pat->setPosition(osg::Vec3d(moveVec.x(), moveVec.y(), moveVec.z()));
    return true;
}

bool MoveElemens::update(const wy::Vector3& moveVec)
{
    _pat->setPosition(osg::Vec3d(moveVec.x(), moveVec.y(), moveVec.z()));
    return true;
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

bool MoveElemens::perform(const wyap::SelectionSet& ss, const wy::Vector3& moveVec, GuiCmdEnvType mode)
{
    if (!_pDb)
    {
        assert(false);
        return true;
    }

    // 选择的元素
    std::set<wydb::ElementId> ids;
    for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
    {
        ids.insert(iter.current().getElementId());
    }
    if (ids.empty()) return true;

    // 启动事务
    wydb::TransactionOption option;
    option.chainUpdateScope = GuiCmdEnvType::Sketching == mode ? wydb::ChainUpdateScope::Local : wydb::ChainUpdateScope::Cascade;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans)
    {
        assert(false);
        return false;
    }

    // 草图环境
    if (GuiCmdEnvType::Sketching == mode)
    {
        for (const wydb::ElementId& id : ids)
        {
            wy3d::SketchEntity* pSketchEntity = wy3d::SketchEntity::cast(pTrans->getElementForWrite(id));
            if (!pSketchEntity)
            {
                assert(false);
                continue;
            }
            pSketchEntity->translate(wy::Vector2(moveVec.x(), moveVec.y()));
        }
    }
    else // 建模环境
    {
        for (const wydb::ElementId& id : ids)
        {
            wydb::Element* pElem = pTrans->getElementForWrite(id);
            wy3d::Solid* pSolid = wy3d::Solid::cast(pElem);
            if (pSolid)
            {
                if (!pSolid->getParent().isNull())
                {
                    assert(false);
                    continue;
                }
                wy3d::Primitive* pPrimitive = wy3d::Primitive::cast(pSolid);
                const std::vector<wydb::ElementId>& modifications = pSolid->getModifications();
                if (pPrimitive && modifications.empty())
                {
                    pPrimitive->setPosition(pPrimitive->getPosition() + moveVec);
                }
                else
                {
                    wy3d::Move* pMove(nullptr);
                    if (wy::ErrorStatus::Ok == wy3d::Move::create(pTrans, pSolid, moveVec, pMove))
                    {
                    }
                    else
                    {
                        assert(false);
                    }
                }
                continue;
            }

            // 片体没有位置参数, 一律走 Move 特征
            wy3d::Sheet* pSheet = wy3d::Sheet::cast(pElem);
            if (pSheet)
            {
                if (!pSheet->getParent().isNull())
                {
                    assert(false);
                    continue;
                }
                wy3d::Move* pMove(nullptr);
                if (wy::ErrorStatus::Ok == wy3d::Move::create(pTrans, pSheet, moveVec, pMove))
                {
                }
                else
                {
                    assert(false);
                }
                continue;
            }

            assert(false);
        }
    }
    _pDb->getTransactionManager()->endTransaction();

    return true;
}
