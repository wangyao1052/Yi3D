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

#include "SketchSplineSnapObjectCreator.h"
#include <cassert>
#include <wyVector2.h>
#include <wy3dSketchSpline.h>
#include "utils/MathUtils.h"
#include "snap/SnapObject.h"
#include "wy3dAppDefs.h"

namespace wy3dApp {

std::list<wyap::SnapObjectSPtr> SketchSplineSnapObjectCreator::createSnapObjects(const wydb::Element* pElem)
{
    assert(pElem);
    const wy3d::SketchSpline* pSketchSpline = wy3d::SketchSpline::cast(pElem);
    if (!pSketchSpline)
    {
        assert(false);
        return std::list<wyap::SnapObjectSPtr>();
    }

    std::list<wyap::SnapObjectSPtr> snapPoints;
    wy3d::Plane sketchPlane;
    if (!this->getSketchPlane(pSketchSpline, sketchPlane))
    {
        assert(false);
        return snapPoints;
    }

    wydb::ElementId id = pSketchSpline->getId();
    const std::vector<wy::Vector2>& points = pSketchSpline->getPoints();
    if (points.size() >= 2)
    {
        snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(id, sketchPlane.value(points.front())));
        snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(id, sketchPlane.value(points.back())));
    }
    return snapPoints;
}

}
