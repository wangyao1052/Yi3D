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

#include "SketchArcGizmoCreator.h"
#include <cassert>
#include <wy3dSketchArc.h>
#include "gizmo/element/SketchArcGripGizmo.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

std::list<wyap::GizmoSPtr> SketchArcGizmoCreator::createGizmos(const wydb::Element* pElem) const
{
    assert(pElem);

    const wy3d::SketchArc* pSketchArc = wy3d::SketchArc::cast(pElem);
    if (!pSketchArc)
    {
        assert(false);
        return std::list<wyap::GizmoSPtr>();
    }

    std::list<wyap::GizmoSPtr> gizmos;
    gizmos.emplace_back(std::make_shared<SketchArcGripGizmo>(pSketchArc, SketchArcGripGizmo::Type::Center));
    gizmos.emplace_back(std::make_shared<SketchArcGripGizmo>(pSketchArc, SketchArcGripGizmo::Type::Start));
    gizmos.emplace_back(std::make_shared<SketchArcGripGizmo>(pSketchArc, SketchArcGripGizmo::Type::End));
    gizmos.emplace_back(std::make_shared<SketchArcGripGizmo>(pSketchArc, SketchArcGripGizmo::Type::Middle));
    return gizmos;
}

} // namespace wy3dApp
