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

#include "SketchRectangleGizmoCreator.h"

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include <wy3dSketchLine.h>

#include <wy3dApp/Application.h>
#include "application/ApplicationImpl.h"
#include <wy3dApp/wy3dAppDefs.h>

#include "gizmo/element/sketchrectangle/SketchRectangleDiagonalsGhostGizmo.h"
#include "gizmo/element/sketchrectangle/SketchRectangleDimensionGizmo.h"
#include "gizmo/element/sketchrectangle/SketchRectangleGizmo.h"
#include "utils/SketchRectangleDimensionMath.h"
#include "utils/SketchRectangleRecognizer.h"

namespace wy3dApp {

bool SketchRectangleGizmoCreator::tryCreateGizmos(const wyap::SelectionSet& sels,
                                                  std::list<wyap::GizmoSPtr>& gizmos)
{
    if (sels.getCount() != 4) return false;

    wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    if (!pDb) return false;

    std::vector<const wy3d::SketchLine*> lines;
    lines.reserve(4);
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

    auto pRect = tryDetectSketchRectangle(lines);
    if (!pRect) return false;

    std::list<wyap::GizmoSPtr> created;
    for (int i = 0; i < 4; ++i)
    {
        created.emplace_back(std::make_shared<SketchRectangleGizmo>(
            pRect, lines, SketchRectangleGizmo::Type::Corner, i));
    }
    for (int i = 0; i < 4; ++i)
    {
        created.emplace_back(std::make_shared<SketchRectangleGizmo>(
            pRect, lines, SketchRectangleGizmo::Type::EdgeMid, i));
    }
    created.emplace_back(std::make_shared<SketchRectangleGizmo>(
        pRect, lines, SketchRectangleGizmo::Type::Center, 0));

    created.emplace_back(std::make_shared<SketchRectangleDiagonalsGhostGizmo>(pRect));

    created.emplace_back(std::make_shared<SketchRectangleDimensionGizmo>(
        pRect, SketchRectangleDimension::Width));
    created.emplace_back(std::make_shared<SketchRectangleDimensionGizmo>(
        pRect, SketchRectangleDimension::Height));

    gizmos.insert(gizmos.cend(), created.cbegin(), created.cend());
    return true;
}

} // namespace wy3dApp
