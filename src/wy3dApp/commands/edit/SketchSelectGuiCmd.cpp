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

#include "SketchSelectGuiCmd.h"

#include <wy3dSketch.h>
#include <wy3dSketchEntity.h>

#include <wy3dApp/Application.h>
#include "application/ApplicationImpl.h"
#include <wy3dApp/GizmoFactory.h>
#include "gizmo/creator/SketchDimensionGizmoCreator.h"
#include "gizmo/creator/SketchPolygonGizmoCreator.h"
#include "gizmo/creator/SketchRectangleGizmoCreator.h"
#include "scene/nodes/ElementNodeType.h"
#include <wy3dApp/CommonSelFilters.h>
#include "snap/SketchSnapSystem.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

void SketchSelectGuiCmd::createSelectionGizmos(
    const wyap::SelectionSet& sels, std::list<wyap::GizmoSPtr>& gizmos)
{
    wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    if (!pDb) return;
    GizmoFactory* pGizmoFactory = ApplicationImpl::instance().getGizmoFactory();
    if (!pGizmoFactory) return;

    if (SketchRectangleGizmoCreator::tryCreateGizmos(sels, gizmos))
    {
        return;
    }
    if (SketchPolygonGizmoCreator::tryCreateGizmos(sels, gizmos))
    {
        return;
    }

    for (auto iter = sels.createIterator(); !iter.isDone(); iter.moveNext())
    {
        const wydb::Element* pElem = pDb->getElement(iter.current().getElementId());
        if (!pElem)
        {
            assert(false);
            continue;
        }
        const wy3d::SketchEntity* pSketchEntity = wy3d::SketchEntity::cast(pElem);
        if (!pSketchEntity)
        {
            assert(false);
            continue;
        }
        std::list<wyap::GizmoSPtr> elemGizmos = pGizmoFactory->createGizmos(pElem);
        gizmos.insert(gizmos.cend(), elemGizmos.cbegin(), elemGizmos.cend());
    }

    SketchDimensionGizmoCreator::tryCreateGizmos(sels, gizmos);
}

SketchSelectGuiCmd::SketchSelectGuiCmd() : SelectGuiCmd()
{
}

SketchSelectGuiCmd::~SketchSelectGuiCmd()
{
}

wyap::CmdExecution::StartResult SketchSelectGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = __baseClass::onStart();
    _sketchInfo = GuiCommandUtil::initSketchInfo();
    if (_sketchInfo.pSketchSnapSys) _sketchInfo.pSketchSnapSys->clearSnapResult();
    return ret;
}

void SketchSelectGuiCmd::initializeSelectOptions(SelectOptions& selectOptions)
{
    SelectGuiCmd::initializeSelectOptions(selectOptions);
    selectOptions.allowedElemTypes = ElementNodeType::SketchEntity;
    selectOptions.postFilter = SingleClassSelPostFilter::create<wy3d::SketchEntity>();
}

void SketchSelectGuiCmd::selectAll_Impl(wyap::SelectionSet& ss)
{
    wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    if (!pDb) return;
    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(_sketchInfo.sketchId));
    if (pSketch)
    {
        for (auto iter = pSketch->createIterator(); !iter.isDone(); iter.moveNext())
        {
            ss.add(wyap::Selection(iter.current()));
        }
    }
}

wy::Vector3 SketchSelectGuiCmd::computePastePosition(double x, double y,
    const std::set<wydb::ElementId>& excludeIds)
{
    wy::Vector2 pos2d = this->computePoint2d(x, y, _sketchInfo.pSketchSnapSys,
        _pPasteOp ? _pPasteOp->pSnapContext : nullptr, excludeIds).first;
    return wy::Vector3(pos2d.x(), pos2d.y(), 0.0);
}

} // namespace wy3dApp
