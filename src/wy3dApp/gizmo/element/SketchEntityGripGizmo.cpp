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

#include <wyVector2.h>
#include <wyVector3.h>
#include "SketchEntityGripGizmo.h"
#include <cassert>
#include <wy3dApp/GuiCommand.h> // MouseEvent
#include "commands/OsgCoordUtil.h"
#include <wy3dApp/Application.h>
#include "commands/OsgCoordUtil.h"
#include "utils/MathUtils.h"
#include <wy3dApp/SketchEnvironment.h>
#include "snap/SketchSnapSystem.h"
#include "gizmo/renderer/SketchGripOsgRenderer.h"
#include "gizmo/renderer/OsgGizmoRenderer.h"

SketchEntityGripGizmo::SketchEntityGripGizmo(const wy3d::SketchEntity* pSketchEntity)
    : BaseGizmo(pSketchEntity ? pSketchEntity->getId() : wydb::ElementId{}), _sketchId(), _sketchPlane()
{
    assert(pSketchEntity);
    const wydb::Database* pDb = pSketchEntity->getDatabase();
    assert(pDb);
    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(pSketchEntity->getParent()));
    if (pSketch)
    {
        _sketchId = pSketch->getId();
        _sketchPlane = pSketch->getPlane();
    }
    else
    {
        assert(false);
    }

    _pRenderer = std::make_unique<SketchGripOsgRenderer>(this);
}

SketchEntityGripGizmo::~SketchEntityGripGizmo()
{
}

static SketchSnapSystem* getCurSketchSnapSystem()
{
    wyap::Environment* pCurEnv = Application::instance().getEnvManager()->getActiveEnvironment();
    const SketchEnvironment* pSketchEnv = dynamic_cast<const SketchEnvironment*>(pCurEnv);
    if (pSketchEnv)
    {
        return pSketchEnv->getSnapSystem();
    }
    else
    {
        assert(false);
        return nullptr;
    }
}

void SketchEntityGripGizmo::onLeftMouseDown(const MouseEvent& event)
{
    if (_pGizmoOp)
    {
        assert(false);
        _pGizmoOp->onCancelDrag();
        if (_pRenderer) _pRenderer->onCancelDrag();
    }
    _pGizmoOp = nullptr;

    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return;
    }

    wy::Vector2 startPos2 = this->getGripPosition();
    wy::Vector3 startPos3(startPos2.x(), startPos2.y(), 0.0);

    _pGizmoOp = std::make_unique<SketchEntityDragOperation>(pDb, this);
    if (!_pGizmoOp->onBeginDrag(startPos3))
    {
        assert(false);
        _pGizmoOp = nullptr;
        return;
    }
    if (_pRenderer) _pRenderer->onBeginDrag(startPos3);

    // added by wangyao 2025.04.10 {
    _pDragContext = this->newDragContext();
    SketchSnapSystem* pSketchSnapSys = getCurSketchSnapSystem();
    assert(pSketchSnapSys);
    if (pSketchSnapSys)
        pSketchSnapSys->partiallyUpdate(Application::instance().getActiveDatabase());
    // }
}

void SketchEntityGripGizmo::onLeftMouseDrag(const MouseEvent& event)
{
    if (!_pGizmoOp)
    {
        return;
    }

    SketchSnapSystem* pSketchSnapSys = getCurSketchSnapSystem();
    if (!pSketchSnapSys)
    {
        assert(false);
        _pGizmoOp->onCancelDrag();
        if (_pRenderer) _pRenderer->onCancelDrag();
        _pGizmoOp = nullptr;
        return;
    }
    wy::Vector2 curPos = OsgCoordUtil::computePosition2d(
        static_cast<OsgGizmoRenderer*>(_pRenderer.get())->getOsgView(),
        event.x, event.y, _sketchPlane, { _id, _sketchId }, _pDragContext, pSketchSnapSys);
    if (_pGizmoOp->onDragging(MathUtils::toVector3(curPos)))
    {
        if (_pRenderer) _pRenderer->onDragging(MathUtils::toVector3(curPos));
    }
}

void SketchEntityGripGizmo::onLeftMouseUp(const MouseEvent& event)
{
    if (!_pGizmoOp)
    {
        return;
    }

    SketchSnapSystem* pSketchSnapSys = getCurSketchSnapSystem();
    if (!pSketchSnapSys)
    {
        assert(false);
        _pGizmoOp->onCancelDrag();
        if (_pRenderer) _pRenderer->onCancelDrag();
        _pGizmoOp = nullptr;
        return;
    }
    wy::Vector2 curPos = OsgCoordUtil::computePosition2d(static_cast<OsgGizmoRenderer*>(_pRenderer.get())->getOsgView(), event.x, event.y, _sketchPlane,
        { _id, _sketchId }, _pDragContext, pSketchSnapSys);

    _pGizmoOp->onEndDrag(MathUtils::toVector3(curPos));
    if (_pRenderer) _pRenderer->onEndDrag(MathUtils::toVector3(curPos));
    _pGizmoOp = nullptr;
    _pDragContext = nullptr;

    // added by wangyao 2025.04.10 {
    // 拖拽完成之后清空草图捕捉结果
    if (pSketchSnapSys) pSketchSnapSys->clearSnapResult();
    // }
}

void SketchEntityGripGizmo::onEscapeKey()
{
    if (_pGizmoOp)
    {
        _pGizmoOp->onCancelDrag();
        _pGizmoOp = nullptr;
        _pDragContext = nullptr;
        if (_pRenderer) _pRenderer->onCancelDrag();
    }

    // added by wangyao 2025.04.10 {
    // 取消拖拽之后清空草图捕捉结果
    SketchSnapSystem* pSketchSnapSys = getCurSketchSnapSystem();
    assert(pSketchSnapSys);
    if (pSketchSnapSys) pSketchSnapSys->clearSnapResult();
    // }
}

const wy3d::SketchEntity* SketchEntityGripGizmo::getSketchEntity() const
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return nullptr;
    const wydb::Element* pElem = pDb->getElement(_id);
    return wy3d::SketchEntity::cast(pElem);
}
