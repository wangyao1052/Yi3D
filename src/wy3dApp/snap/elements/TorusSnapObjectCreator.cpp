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

#include "TorusSnapObjectCreator.h"
#include <cassert>
#include <wy3dTorus.h>
#include "utils/MathUtils.h"
#include "snap/SnapObject.h"
#include "wy3dAppDefs.h"

namespace wy3dApp {

std::list<wyap::SnapObjectSPtr> TorusSnapObjectCreator::createSnapObjects(const wydb::Element* pElem)
{
    assert(pElem);
    const wy3d::Torus* pTorus = wy3d::Torus::cast(pElem);
    if (!pTorus)
    {
        assert(false);
        return std::list<wyap::SnapObjectSPtr>();
    }

    std::list<wyap::SnapObjectSPtr> snapPoints;
    wydb::ElementId id = pTorus->getId();
    osg::Matrix matrix = MathUtils::createMatrix(pTorus->getPosition(), pTorus->getRotation());
    double majorRadius = pTorus->getMajorRadius();
    // 中心点
    snapPoints.emplace_back(this->newSnapPoint<SnapCenterPoint>(id, 0.0, 0.0, 0.0, matrix));
    // 端点
    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(id, majorRadius, 0.0, 0.0, matrix));
    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(id, 0.0, majorRadius, 0.0, matrix));
    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(id, -majorRadius, 0.0, 0.0, matrix));
    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(id, 0.0, -majorRadius, 0.0, matrix));

    return snapPoints;
}

}
