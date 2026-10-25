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

#include "SketchPolygonGizmoCreator.h"

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include <wy3dSketchLine.h>

#include <wy3dApp/Application.h>
#include "application/ApplicationImpl.h"
#include "gizmo/element/sketchpolygon/SketchPolygonCircleGhostGizmo.h"
#include "gizmo/element/sketchpolygon/SketchPolygonDimensionGizmo.h"
#include "gizmo/element/sketchpolygon/SketchPolygonGizmo.h"
#include "utils/SketchPolygonGeometry.h"
#include "utils/SketchPolygonRecognizer.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

bool SketchPolygonGizmoCreator::tryCreateGizmos(const wyap::SelectionSet& sels,
                                                std::list<wyap::GizmoSPtr>& gizmos)
{
    const int count = static_cast<int>(sels.getCount());
    if (count < kMinPolygonSides || count > kMaxPolygonSides) return false;
    // A square stays with the rectangle Gizmo (width/height labels)
    if (4 == count) return false;

    wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    if (!pDb) return false;

    std::vector<const wy3d::SketchLine*> lines;
    lines.reserve(count);
    std::set<wydb::ElementId> lineIds;
    for (auto iter = sels.createIterator(); !iter.isDone(); iter.moveNext())
    {
        const wyap::Selection& sel = iter.current();
        if (sel.getSelectionType() != wyap::Selection::kWholeElement) return false;

        const wy3d::SketchLine* pLine = wy3d::SketchLine::cast(pDb->getElement(sel.getElementId()));
        if (!pLine) return false;
        if (!lineIds.insert(pLine->getId()).second) return false;
        lines.push_back(pLine);
    }
    if (static_cast<int>(lines.size()) != count) return false;

    auto pPolygon = tryDetectSketchPolygon(lines);
    if (!pPolygon) return false;

    const int sides = pPolygon->sides;
    std::list<wyap::GizmoSPtr> created;
    for (int i = 0; i < sides; ++i)
    {
        created.emplace_back(std::make_shared<SketchPolygonGizmo>(
            pPolygon, lines, SketchPolygonGizmo::Type::Corner, i));
    }
    for (int i = 0; i < sides; ++i)
    {
        created.emplace_back(std::make_shared<SketchPolygonGizmo>(
            pPolygon, lines, SketchPolygonGizmo::Type::EdgeMid, i));
    }

    created.emplace_back(std::make_shared<SketchPolygonGizmo>(
        pPolygon, lines, SketchPolygonGizmo::Type::Center, 0));

    // The label drives the ghost's reading, so keep the ghost alive in a named
    // local and hand the label a weak reference to it
    const std::shared_ptr<SketchPolygonCircleGhostGizmo> pCircleGhost =
        std::make_shared<SketchPolygonCircleGhostGizmo>(pPolygon);
    created.emplace_back(pCircleGhost);
    created.emplace_back(std::make_shared<SketchPolygonDimensionGizmo>(pPolygon, pCircleGhost));

    gizmos.insert(gizmos.cend(), created.cbegin(), created.cend());
    return true;
}

} // namespace wy3dApp
