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

#include "SketchEllipseSnapObjectCreator.h"
#include <cassert>
#include <wyVector2.h>
#include <wy3dSketchEllipse.h>
#include "utils/MathUtils.h"
#include "snap/SnapObject.h"
#include "wy3dAppDefs.h"

namespace wy3dApp {

std::list<wyap::SnapObjectSPtr> SketchEllipseSnapObjectCreator::createSnapObjects(const wydb::Element* pElem)
{
    assert(pElem);
    const wy3d::SketchEllipse* pSketchEllipse = wy3d::SketchEllipse::cast(pElem);
    if (!pSketchEllipse)
    {
        assert(false);
        return std::list<wyap::SnapObjectSPtr>();
    }

    std::list<wyap::SnapObjectSPtr> snapPoints;
    wy3d::Plane sketchPlane;
    if (!this->getSketchPlane(pSketchEllipse, sketchPlane))
    {
        assert(false);
        return snapPoints;
    }

    wydb::ElementId id = pSketchEllipse->getId();
    // ��Բ��
    wy::Vector2 center = pSketchEllipse->getCenter();
    snapPoints.emplace_back(this->newSnapPoint<SnapCenterPoint>(id, sketchPlane.value(center)));
    // ���㳤����X��ļн�
    wy::Vector2 majorAxis = pSketchEllipse->getMajorAxis();
    double angle = std::atan2(majorAxis.y(), majorAxis.x());
    double cosAngle = std::cos(angle);
    double sinAngle = std::sin(angle);
    // �ĸ��˵�
    double majorRadius = pSketchEllipse->getMajorRadius();
    double minorRadius = pSketchEllipse->getMinorRadius();
    std::vector<wy::Vector2> endPnts{
        wy::Vector2(majorRadius, 0.0),
        wy::Vector2(0.0, minorRadius),
        wy::Vector2(-majorRadius, 0.0),
        wy::Vector2(0.0, -minorRadius)
    };
    for (const wy::Vector2& endPnt : endPnts)
    {
        double xRot = endPnt.x() * cosAngle - endPnt.y() * sinAngle;
        double yRot = endPnt.x() * sinAngle + endPnt.y() * cosAngle;
        snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(id,
            sketchPlane.value(wy::Vector2(center.x() + xRot, center.y() + yRot))));
    }
    return snapPoints;
}

}
