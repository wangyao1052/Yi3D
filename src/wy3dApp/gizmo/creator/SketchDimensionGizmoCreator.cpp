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

#include "SketchDimensionGizmoCreator.h"

#include <cassert>
#include <memory>

#include <wy3dSketch.h>
#include <wy3dSketchArc.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchLine.h>

#include <wy3dApp/Application.h>
#include "application/ApplicationImpl.h"
#include "gizmo/element/SketchArcDimensionGizmo.h"
#include "gizmo/element/SketchCircleDimensionGizmo.h"
#include "gizmo/element/SketchLineDimensionGizmo.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

bool SketchDimensionGizmoCreator::tryCreateGizmos(const wyap::SelectionSet& sels,
                                                  std::list<wyap::GizmoSPtr>& gizmos)
{
    if (1 != sels.getCount())
    {
        return false;
    }

    auto iter = sels.createIterator();
    if (iter.isDone())
    {
        return false;
    }
    const wyap::Selection& sel = iter.current();
    if (wyap::Selection::kWholeElement != sel.getSelectionType())
    {
        return false;
    }

    const wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    if (!pDb)
    {
        return false;
    }
    const wydb::Element* pElem = pDb->getElement(sel.getElementId());
    if (!pElem)
    {
        return false;
    }

    const wy3d::SketchEntity* pEntity = wy3d::SketchEntity::cast(pElem);
    if (!pEntity)
    {
        return false;
    }
    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(pEntity->getParent()));
    if (!pSketch || !pSketch->getPlane().isValid())
    {
        return false;
    }

    std::list<wyap::GizmoSPtr> created;
    if (const wy3d::SketchLine* pLine = wy3d::SketchLine::cast(pElem))
    {
        created.emplace_back(std::make_shared<SketchLineDimensionGizmo>(
            pLine, SketchLineDimensionGizmo::Type::Length));
        created.emplace_back(std::make_shared<SketchLineDimensionGizmo>(
            pLine, SketchLineDimensionGizmo::Type::Angle));
    }
    else if (const wy3d::SketchCircle* pCircle = wy3d::SketchCircle::cast(pElem))
    {
        created.emplace_back(std::make_shared<SketchCircleDimensionGizmo>(pCircle));
    }
    else if (const wy3d::SketchArc* pArc = wy3d::SketchArc::cast(pElem))
    {
        created.emplace_back(std::make_shared<SketchArcDimensionGizmo>(pArc));
    }
    else
    {
        return false;
    }

    gizmos.insert(gizmos.cend(), created.cbegin(), created.cend());
    return true;
}

} // namespace wy3dApp
