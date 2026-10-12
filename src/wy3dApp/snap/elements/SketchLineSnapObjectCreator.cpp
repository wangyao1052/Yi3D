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

#include "SketchLineSnapObjectCreator.h"
#include <cassert>
#include <wyVector2.h>
#include <wy3dSketchLine.h>
#include "utils/MathUtils.h"
#include "snap/SnapObject.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

std::list<wyap::SnapObjectSPtr> SketchLineSnapObjectCreator::createSnapObjects(const wydb::Element* pElem)
{
    assert(pElem);
    const wy3d::SketchLine* pSketchLine = wy3d::SketchLine::cast(pElem);
    if (!pSketchLine)
    {
        assert(false);
        return std::list<wyap::SnapObjectSPtr>();
    }

    std::list<wyap::SnapObjectSPtr> snapPoints;
    wy3d::Plane sketchPlane;
    if (!this->getSketchPlane(pSketchLine, sketchPlane))
    {
        assert(false);
        return snapPoints;
    }

    wydb::ElementId id = pSketchLine->getId();
    wy::Vector2 startPnt = pSketchLine->getStartPoint();
    wy::Vector2 endPnt = pSketchLine->getEndPoint();
    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(id, sketchPlane.value(startPnt)));
    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(id, sketchPlane.value(endPnt)));
    snapPoints.emplace_back(this->newSnapPoint<SnapMiddlePoint>(id, sketchPlane.value((startPnt + endPnt) / 2)));
    return snapPoints;
}

} // namespace wy3dApp
