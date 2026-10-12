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

#include "BoxSnapObjectCreator.h"
#include <cassert>
#include <wy3dBox.h>
#include "utils/MathUtils.h"
#include "snap/SnapObject.h"
#include "wy3dAppDefs.h"

namespace wy3dApp {

std::list<wyap::SnapObjectSPtr> BoxSnapObjectCreator::createSnapObjects(const wydb::Element* pElem)
{
    assert(pElem);
    const wy3d::Box* pBox = wy3d::Box::cast(pElem);
    if (!pBox)
    {
        assert(false);
        return std::list<wyap::SnapObjectSPtr>();
    }

    std::list<wyap::SnapObjectSPtr> snapPoints;
    wydb::ElementId id = pBox->getId();
    osg::Matrix matrix = MathUtils::createMatrix(pBox->getPosition(), pBox->getRotation());
    double length = pBox->getLength();
    double width = pBox->getWidth();
    double height = pBox->getHeight();
    // 端点
    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(id, 0.0, 0.0, 0.0, matrix));
    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(id, length, 0.0, 0.0, matrix));
    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(id, length, width, 0.0, matrix));
    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(id, 0.0, width, 0.0, matrix));
    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(id, 0.0, 0.0, height, matrix));
    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(id, length, 0.0, height, matrix));
    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(id, length, width, height, matrix));
    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(id, 0.0, width, height, matrix));

    return snapPoints;
}

}
