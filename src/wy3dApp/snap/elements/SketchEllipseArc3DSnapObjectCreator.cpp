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

#include "SketchEllipseArc3DSnapObjectCreator.h"
#include <cassert>
#include <wy3dSketchEllipseArc3D.h>
#include "snap/SnapObject.h"

std::list<wyap::SnapObjectSPtr> SketchEllipseArc3DSnapObjectCreator::createSnapObjects(const wydb::Element* pElem)
{
    assert(pElem);
    const wy3d::SketchEllipseArc3D* pEllipseArc = wy3d::SketchEllipseArc3D::cast(pElem);
    if (!pEllipseArc)
    {
        assert(false);
        return std::list<wyap::SnapObjectSPtr>();
    }

    // 中心 + 起点 + 终点(与2D椭圆弧一致)
    std::list<wyap::SnapObjectSPtr> snapPoints;
    snapPoints.emplace_back(this->newSnapPoint<SnapCenterPoint>(pEllipseArc->getId(), pEllipseArc->getCenter()));
    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(pEllipseArc->getId(), pEllipseArc->getStartPoint()));
    snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(pEllipseArc->getId(), pEllipseArc->getEndPoint()));
    return snapPoints;
}
