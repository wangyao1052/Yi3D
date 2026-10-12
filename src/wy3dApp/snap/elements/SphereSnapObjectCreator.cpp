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

#include "SphereSnapObjectCreator.h"
#include <cassert>
#include <wy3dSphere.h>
#include "utils/MathUtils.h"
#include "snap/SnapObject.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

std::list<wyap::SnapObjectSPtr> SphereSnapObjectCreator::createSnapObjects(const wydb::Element* pElem)
{
    assert(pElem);
    const wy3d::Sphere* pSphere = wy3d::Sphere::cast(pElem);
    if (!pSphere)
    {
        assert(false);
        return std::list<wyap::SnapObjectSPtr>();
    }

    // 根据Shape生成捕捉点
    wydb::ElementId id = pSphere->getId();
    TopoDS_Shape resultShape = pSphere->getShape();
    std::list<wyap::SnapObjectSPtr> snapObjs = this->newSnapObjects(id, resultShape);

    // 加上球心
    osg::Matrix matrix = MathUtils::createMatrix(pSphere->getPosition(), pSphere->getRotation());
    double radius = pSphere->getRadius();
    snapObjs.emplace_back(this->newSnapPoint<SnapCenterPoint>(id, 0.0, 0.0, 0.0, matrix));

    return snapObjs;
}

} // namespace wy3dApp
