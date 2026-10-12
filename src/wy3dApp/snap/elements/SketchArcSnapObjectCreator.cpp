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

#include "SketchArcSnapObjectCreator.h"
#include <cassert>
#include <wy3dSketchArc.h>
#include <wy3dSketchArc.h>
#include "utils/MathUtils.h"
#include "snap/SnapObject.h"
#include "wy3dAppDefs.h"

namespace wy3dApp {

std::list<wyap::SnapObjectSPtr> SketchArcSnapObjectCreator::createSnapObjects(const wydb::Element* pElem)
{
    assert(pElem);
    const wy3d::SketchArc* pSketchArc = wy3d::SketchArc::cast(pElem);
    if (!pSketchArc)
    {
        assert(false);
        return std::list<wyap::SnapObjectSPtr>();
    }    if (!pSketchArc)
    {
        assert(false);
        return std::list<wyap::SnapObjectSPtr>();
    }

    std::list<wyap::SnapObjectSPtr> snapPoints;
    wy3d::Plane sketchPlane;
    if (!this->getSketchPlane(pSketchArc, sketchPlane))
    {
        assert(false);
        return snapPoints;
    }

    wydb::ElementId id = pSketchArc->getId();
    snapPoints.emplace_back(this->newSnapPoint<SnapCenterPoint>(id, sketchPlane.value(pSketchArc->getCenter())));
    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(id, sketchPlane.value(pSketchArc->getStartPoint())));
    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(id, sketchPlane.value(pSketchArc->getEndPoint())));
    snapPoints.emplace_back(this->newSnapPoint<SnapMiddlePoint>(id, sketchPlane.value(pSketchArc->getMiddlePoint())));
    return snapPoints;
}

}
