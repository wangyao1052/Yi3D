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

#include "TubeSnapObjectCreator.h"
#include <cassert>
#include <wy3dTube.h>
#include "utils/MathUtils.h"
#include "snap/SnapObject.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

std::list<wyap::SnapObjectSPtr> TubeSnapObjectCreator::createSnapObjects(const wydb::Element* pElem)
{
    assert(pElem);
    const wy3d::Tube* pTube = wy3d::Tube::cast(pElem);
    if (!pTube)
    {
        assert(false);
        return std::list<wyap::SnapObjectSPtr>();
    }

    std::list<wyap::SnapObjectSPtr> snapPoints;
    wydb::ElementId id = pTube->getId();
    osg::Matrix matrix = MathUtils::createMatrix(pTube->getPosition(), pTube->getRotation());
    double outerRadius = pTube->getOuterRadius();
    double innerRadius = pTube->getInnerRadius();
    double height = pTube->getHeight();
    // 中心点
    snapPoints.emplace_back(this->newSnapPoint<SnapCenterPoint>(id, 0.0, 0.0, 0.0, matrix));
    snapPoints.emplace_back(this->newSnapPoint<SnapCenterPoint>(id, 0.0, 0.0, height, matrix));
    // 端点
    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(id, outerRadius, 0.0, 0.0, matrix));
    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(id, 0.0, outerRadius, 0.0, matrix));
    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(id, -outerRadius, 0.0, 0.0, matrix));
    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(id, 0.0, -outerRadius, 0.0, matrix));
    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(id, outerRadius, 0.0, height, matrix));
    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(id, 0.0, outerRadius, height, matrix));
    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(id, -outerRadius, 0.0, height, matrix));
    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(id, 0.0, -outerRadius, height, matrix));

    return snapPoints;
}

} // namespace wy3dApp
