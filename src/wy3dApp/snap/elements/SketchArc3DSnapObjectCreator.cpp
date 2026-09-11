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

#include "SketchArc3DSnapObjectCreator.h"
#include <cassert>
#include <wy3dSketchArc3D.h>
#include "snap/SnapObject.h"

std::list<wyap::SnapObjectSPtr> SketchArc3DSnapObjectCreator::createSnapObjects(const wydb::Element* pElem)
{
    assert(pElem);
    const wy3d::SketchArc3D* pSketchArc3D = wy3d::SketchArc3D::cast(pElem);
    if (!pSketchArc3D)
    {
        assert(false);
        return std::list<wyap::SnapObjectSPtr>();
    }

    std::list<wyap::SnapObjectSPtr> snapPoints;
    const wy3d::SketchArc3D* pArc = pSketchArc3D;
    snapPoints.emplace_back(this->newSnapPoint<SnapCenterPoint>(pArc->getId(), pArc->getCenter()));
    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(pArc->getId(), pArc->getStartPoint()));
    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(pArc->getId(), pArc->getEndPoint()));
    snapPoints.emplace_back(this->newSnapPoint<SnapMiddlePoint>(pArc->getId(), pArc->getMiddlePoint()));
    return snapPoints;
}
