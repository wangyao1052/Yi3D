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

#include "SketchEllipseGizmoCreator.h"
#include <cassert>
#include <wy3dSketchEllipse.h>
#include "gizmo/element/SketchEllipseGripGizmo.h"
#include "wy3dAppDefs.h"

namespace wy3dApp {

std::list<wyap::GizmoSPtr> SketchEllipseGizmoCreator::createGizmos(const wydb::Element* pElem) const
{
    assert(pElem);

    const wy3d::SketchEllipse* pSketchEllipse = wy3d::SketchEllipse::cast(pElem);
    if (!pSketchEllipse)
    {
        assert(false);
        return std::list<wyap::GizmoSPtr>();
    }

    std::list<wyap::GizmoSPtr> gizmos;
    gizmos.emplace_back(std::make_shared<SketchEllipseGripGizmo>(pSketchEllipse, SketchEllipseGripGizmo::Type::Center));
    gizmos.emplace_back(std::make_shared<SketchEllipseGripGizmo>(pSketchEllipse, SketchEllipseGripGizmo::Type::Quarter0));
    gizmos.emplace_back(std::make_shared<SketchEllipseGripGizmo>(pSketchEllipse, SketchEllipseGripGizmo::Type::Quarter1));
    gizmos.emplace_back(std::make_shared<SketchEllipseGripGizmo>(pSketchEllipse, SketchEllipseGripGizmo::Type::Quarter2));
    gizmos.emplace_back(std::make_shared<SketchEllipseGripGizmo>(pSketchEllipse, SketchEllipseGripGizmo::Type::Quarter3));
    return gizmos;
}

}
