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

#include "SketchSpline3DSnapObjectCreator.h"
#include <cassert>
#include <wy3dSketchSpline3D.h>
#include "snap/SnapObject.h"

std::list<wyap::SnapObjectSPtr> SketchSpline3DSnapObjectCreator::createSnapObjects(const wydb::Element* pElem)
{
    assert(pElem);
    const wy3d::SketchSpline3D* pSpline = wy3d::SketchSpline3D::cast(pElem);
    if (!pSpline)
    {
        assert(false);
        return std::list<wyap::SnapObjectSPtr>();
    }

    // 两端点(与2D样条一致:取自点表,不取曲线端点)
    std::list<wyap::SnapObjectSPtr> snapPoints;
    const std::vector<wy::Vector3>& points = pSpline->getPoints();
    if (points.size() >= 2)
    {
        snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(pSpline->getId(), points.front()));
        snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(pSpline->getId(), points.back()));
    }
    return snapPoints;
}
