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

#include "PrimitiveGizmoCreator.h"
#include <cassert>
#include <wy3dPrimitive.h>
#include "gizmo/element/SolidMoveGizmo.h"
#include "application/Application.h"
#include "scene/Scene.h"
#include "scene/nodes/ElementNode.h"

std::list<wyap::GizmoSPtr> PrimitiveGizmoCreator::createGizmos(const wydb::Element* pElem) const
{
    assert(pElem);

    const wy3d::Primitive* pPrimitive = wy3d::Primitive::cast(pElem);
    if (!pPrimitive)
    {
        assert(false);
        return std::list<wyap::GizmoSPtr>();
    }

    // added by wangyao 2025.06.03 {
    // 切除材料实体不支持Gizmo
    if (pPrimitive->isCut())
    {
        return std::list<wyap::GizmoSPtr>();
    }
    // }

    // 节点类型为实体修改,则不支持Gizmo
    if (Scene* pScene = Application::instance().getActiveScene())
    {
        ElementNode* pElemNode = pScene->getElementNode(pPrimitive->getId());
        if (pElemNode && pElemNode->getNodeType() == ElementNodeType::BodyModification)
        {
            return std::list<wyap::GizmoSPtr>();
        }
    }

    // 有实体修改元素不支持Gizmo
    if (!pPrimitive->getModifications().empty())
    {
        return std::list<wyap::GizmoSPtr>();
    }
    // }

    std::list<wyap::GizmoSPtr> gizmos;
    auto pGizmoX = std::make_shared<SolidMoveXGizmo>(pPrimitive);
    auto pGizmoY = std::make_shared<SolidMoveYGizmo>(pPrimitive);
    auto pGizmoZ = std::make_shared<SolidMoveZGizmo>(pPrimitive);
    pGizmoX->setSiblings(pGizmoY.get(), pGizmoZ.get());
    pGizmoY->setSiblings(pGizmoX.get(), pGizmoZ.get());
    pGizmoZ->setSiblings(pGizmoX.get(), pGizmoY.get());
    gizmos.emplace_back(std::move(pGizmoX));
    gizmos.emplace_back(std::move(pGizmoY));
    gizmos.emplace_back(std::move(pGizmoZ));
    return gizmos;
}