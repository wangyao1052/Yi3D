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

#include "SketchCircle3DSnapObjectCreator.h"
#include <cassert>
#include <wy3dSketchCircle3D.h>
#include "snap/SnapObject.h"

std::list<wyap::SnapObjectSPtr> SketchCircle3DSnapObjectCreator::createSnapObjects(const wydb::Element* pElem)
{
    assert(pElem);
    const wy3d::SketchCircle3D* pSketchCircle3D = wy3d::SketchCircle3D::cast(pElem);
    if (!pSketchCircle3D)
    {
        assert(false);
        return std::list<wyap::SnapObjectSPtr>();
    }

    std::list<wyap::SnapObjectSPtr> snapPoints;
    const wy3d::SketchCircle3D* pCircle = pSketchCircle3D;
    const wy::Vector3 center = pCircle->getCenter();
    const wy::Vector3 xDir = pCircle->getXDir();
    const wy::Vector3 yDir = pCircle->getNormal().cross(xDir);
    const double radius = pCircle->getRadius();

    snapPoints.emplace_back(this->newSnapPoint<SnapCenterPoint>(pCircle->getId(), center));
    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(pCircle->getId(), center + xDir * radius));
    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(pCircle->getId(), center + yDir * radius));
    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(pCircle->getId(), center - xDir * radius));
    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(pCircle->getId(), center - yDir * radius));
    return snapPoints;
}
