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

#include "SketchEllipse3DSnapObjectCreator.h"
#include <cassert>
#include <wy3dSketchEllipse3D.h>
#include "snap/SnapObject.h"

std::list<wyap::SnapObjectSPtr> SketchEllipse3DSnapObjectCreator::createSnapObjects(const wydb::Element* pElem)
{
    assert(pElem);
    const wy3d::SketchEllipse3D* pEllipse = wy3d::SketchEllipse3D::cast(pElem);
    if (!pEllipse)
    {
        assert(false);
        return std::list<wyap::SnapObjectSPtr>();
    }

    // 中心 + 四个轴端点
    const wy::Vector3& center = pEllipse->getCenter();
    const wy::Vector3 xDir = pEllipse->getXDir();
    const wy::Vector3 yDir = pEllipse->getNormal().cross(xDir);
    const double majorRadius = pEllipse->getMajorRadius();
    const double minorRadius = pEllipse->getMinorRadius();

    std::list<wyap::SnapObjectSPtr> snapPoints;
    snapPoints.emplace_back(this->newSnapPoint<SnapCenterPoint>(pEllipse->getId(), center));
    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(pEllipse->getId(), center + xDir * majorRadius));
    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(pEllipse->getId(), center + yDir * minorRadius));
    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(pEllipse->getId(), center - xDir * majorRadius));
    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(pEllipse->getId(), center - yDir * minorRadius));
    return snapPoints;
}
