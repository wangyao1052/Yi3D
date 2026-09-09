///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 Wang Yao <wangyao1052@163.com>
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

#include "SketchLine3DSnapObjectCreator.h"
#include <cassert>
#include <wy3dSketchLine3D.h>
#include "snap/SnapObject.h"

std::list<wyap::SnapObjectSPtr> SketchLine3DSnapObjectCreator::createSnapObjects(const wydb::Element* pElem)
{
    assert(pElem);
    const wy3d::SketchLine3D* pSketchLine3D = wy3d::SketchLine3D::cast(pElem);
    if (!pSketchLine3D)
    {
        assert(false);
        return std::list<wyap::SnapObjectSPtr>();
    }

    std::list<wyap::SnapObjectSPtr> snapPoints;
    wydb::ElementId id = pSketchLine3D->getId();
    const wy::Vector3 startPnt = pSketchLine3D->getStartPoint();
    const wy::Vector3 endPnt = pSketchLine3D->getEndPoint();
    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(id, startPnt));
    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(id, endPnt));
    snapPoints.emplace_back(this->newSnapPoint<SnapMiddlePoint>(id, (startPnt + endPnt) / 2));
    return snapPoints;
}
