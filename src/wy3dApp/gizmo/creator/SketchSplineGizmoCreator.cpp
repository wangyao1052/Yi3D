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

#include "SketchSplineGizmoCreator.h"
#include <cassert>
#include <wyVector2.h>
#include <wy3dSketchSpline.h>
#include "gizmo/element/SketchSplineGripGizmo.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

std::list<wyap::GizmoSPtr> SketchSplineGizmoCreator::createGizmos(const wydb::Element* pElem) const
{
    assert(pElem);

    const wy3d::SketchSpline* pSketchSpline = wy3d::SketchSpline::cast(pElem);
    if (!pSketchSpline)
    {
        assert(false);
        return std::list<wyap::GizmoSPtr>();
    }
    const std::vector<wy::Vector2>& points = pSketchSpline->getPoints();
    std::list<wyap::GizmoSPtr> gizmos;
    if (wy3d::SplineMode::ControlPoints == pSketchSpline->getMode())
    {
        gizmos.emplace_back(std::make_shared<SplineCtrolPointsPathGhostGizmo>(pSketchSpline));
    }
    for (unsigned int i = 0; i < static_cast<unsigned int>(points.size()); ++i)
    {
        gizmos.emplace_back(std::make_shared<SketchSplineGripGizmo>(pSketchSpline, i));
    }
    return gizmos;
}

} // namespace wy3dApp
