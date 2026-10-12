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

#include "SolidDragOperation.h"
#include <wyVector3.h>
#include <wy3dBoolean.h>
#include <wy3dPrimitive.h>
#include "gizmo/element/SolidMoveGizmo.h"
#include "wy3dAppDefs.h"

namespace wy3dApp {

SolidDragOperation::SolidDragOperation(wydb::Database* pDb, BaseGizmo* pGizmo)
    : GizmoOperation(pDb, pGizmo)
{
}

SolidDragOperation::~SolidDragOperation()
{
}

bool SolidDragOperation::onBeginDrag(const wy::Vector3& startPos)
{
    _dragStartPnt = startPos;
    return true;
}

bool SolidDragOperation::onDragging(const wy::Vector3& curPos)
{
    // SolidMove 的数据修改在 end 时一次性完成，progressing 只做渲染
    return true;
}

void SolidDragOperation::onEndDrag(const wy::Vector3& endPos)
{
    _dragEndPnt = endPos;
    if (this->perform())
        this->commitTopTransaction();
    else
        this->onCancelDrag();
}

void SolidDragOperation::onCancelDrag()
{
    this->abortTopTransaction();
}

void getBooleanChildren(wydb::Transaction* pTrans, wy3d::Boolean* pBoolean, std::vector<wy3d::Solid*>& children)
{
    // target
    {
        wydb::ElementId targetId = pBoolean->getTarget();
        wy3d::Solid* pTargetSolid = wy3d::Solid::cast(pTrans->getElementForWrite(targetId));
        if (pTargetSolid)
        {
            wy3d::Boolean* pTargetBoolean = wy3d::Boolean::cast(pTargetSolid);
            if (pTargetBoolean)
            {
                getBooleanChildren(pTrans, pTargetBoolean, children);
            }
            else
            {
                children.emplace_back(pTargetSolid);
            }
        }
        else
        {
            assert(false);
        }
    }

    // tools
    {
        const std::vector<wydb::ElementId>& toolIds = pBoolean->getTools();
        for (const wydb::ElementId& toolId : toolIds)
        {
            wy3d::Solid* pToolSolid = wy3d::Solid::cast(pTrans->getElementForWrite(toolId));
            if (pToolSolid)
            {
                wy3d::Boolean* pToolBoolean = wy3d::Boolean::cast(pToolSolid);
                if (pToolBoolean)
                {
                    getBooleanChildren(pTrans, pToolBoolean, children);
                }
                else
                {
                    children.emplace_back(pToolSolid);
                }
            }
            else
            {
                assert(false);
            }
        }
    }
}

void getNonBooleanSolidChildren(wydb::Transaction* pTrans, wy3d::Solid* pSolid, std::vector<wy3d::Solid*>& children)
{
    const std::vector<wydb::ElementId>& modifications = pSolid->getModifications();
    for (const wydb::ElementId& id : modifications)
    {
        const wy3d::Solid* pConstChildSolid = wy3d::Solid::cast(pSolid->getDatabase()->getElement(id));
        if (!pConstChildSolid) continue;
        wy3d::Solid* pChildSolid = wy3d::Solid::cast(pTrans->getElementForWrite(id));
        if (!pChildSolid)
        {
            assert(false);
            continue;
        }
        wy3d::Boolean* pChildBoolean = wy3d::Boolean::cast(pChildSolid);
        if (pChildBoolean)
        {
            getBooleanChildren(pTrans, pChildBoolean, children);
        }
        else
        {
            children.emplace_back(pChildSolid);
            getNonBooleanSolidChildren(pTrans, pChildSolid, children);
        }
    }
}

bool SolidDragOperation::perform()
{
    SolidMoveGizmo* pElemPosGizmo = dynamic_cast<SolidMoveGizmo*>(_pGizmo);
    if (!pElemPosGizmo)
    {
        assert(false);
        return false;
    }
    wydb::ElementId id = pElemPosGizmo->getModifiedElement();
    assert(_pDb);
    const wydb::Element* pConstElem = _pDb->getElement(id);
    if (!pConstElem) return false;
    wydb::TransactionManager* pTransMgr = _pDb->getTransactionManager();
    if (!pTransMgr) return false;

    wydb::Transaction* pTrans = pTransMgr->startTransaction();
    if (!pTrans) return false;
    wydb::Element* pElem = pTrans->getElementForWrite(id);
    wy3d::Primitive* pPrimitive = wy3d::Primitive::cast(pElem);
    if (pPrimitive)
    {
        wy::Vector3 delta = _dragEndPnt - _dragStartPnt;
        pPrimitive->setPosition(pPrimitive->getPosition() + delta);

        pTransMgr->endTransaction();
        return true;
    }
    else
    {
        pTransMgr->abortTransaction();
        return false;
    }
}

}
