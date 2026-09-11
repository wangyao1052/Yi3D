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

#include "Sketch3DSnapObjectCreator.h"
#include <cassert>
#include <wydbDatabase.h>
#include <wy3dSketch3D.h>
#include <wy3dSketchEntity3D.h>
#include <wy3dSketchLine3D.h>
#include <wy3dSketchCircle3D.h>
#include <wy3dSketchArc3D.h>
#include <wy3dSketchEllipse3D.h>
#include <wy3dSketchEllipseArc3D.h>
#include <wy3dSketchSpline3D.h>
#include "snap/SnapObject.h"

std::list<wyap::SnapObjectSPtr> Sketch3DSnapObjectCreator::createSnapObjects(const wydb::Element* pElem)
{
    assert(pElem);
    const wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pElem);
    if (!pSketch3D)
    {
        assert(false);
        return std::list<wyap::SnapObjectSPtr>();
    }

    std::list<wyap::SnapObjectSPtr> snapPoints;
    const wydb::Database* pDb = pSketch3D->getDatabase();
    if (!pDb)
    {
        assert(false);
        return snapPoints;
    }

    for (auto iter = pSketch3D->createIterator(); !iter.isDone(); iter.moveNext())
    {
        const wydb::Element* pEntityElem = pDb->getElement(iter.current());
        if (!pEntityElem)
        {
            assert(false);
            continue;
        }
        const wy3d::SketchEntity3D* pEntity = wy3d::SketchEntity3D::cast(pEntityElem);
        if (!pEntity)
        {
            continue;
        }

        if (const wy3d::SketchLine3D* pLine = wy3d::SketchLine3D::cast(pEntity))
        {
            snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(pLine->getId(), pLine->getStartPoint()));
            snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(pLine->getId(), pLine->getEndPoint()));
        }
        else if (const wy3d::SketchCircle3D* pCircle = wy3d::SketchCircle3D::cast(pEntity))
        {
            const wy::Vector3 center = pCircle->getCenter();
            const wy::Vector3 xDir = pCircle->getXDir();
            const wy::Vector3 yDir = pCircle->getNormal().cross(xDir);
            const double radius = pCircle->getRadius();
            snapPoints.emplace_back(this->newSnapPoint<SnapCenterPoint>(pCircle->getId(), center));
            snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(pCircle->getId(), center + xDir * radius));
            snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(pCircle->getId(), center + yDir * radius));
            snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(pCircle->getId(), center - xDir * radius));
            snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(pCircle->getId(), center - yDir * radius));
        }
        else if (const wy3d::SketchArc3D* pArc = wy3d::SketchArc3D::cast(pEntity))
        {
            snapPoints.emplace_back(this->newSnapPoint<SnapCenterPoint>(pArc->getId(), pArc->getCenter()));
            snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(pArc->getId(), pArc->getStartPoint()));
            snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(pArc->getId(), pArc->getEndPoint()));
            snapPoints.emplace_back(this->newSnapPoint<SnapMiddlePoint>(pArc->getId(), pArc->getMiddlePoint()));
        }
        else if (const wy3d::SketchEllipse3D* pEllipse = wy3d::SketchEllipse3D::cast(pEntity))
        {
            const wy::Vector3 center = pEllipse->getCenter();
            const wy::Vector3 xDir = pEllipse->getXDir();
            const wy::Vector3 yDir = pEllipse->getNormal().cross(xDir);
            const double majorRadius = pEllipse->getMajorRadius();
            const double minorRadius = pEllipse->getMinorRadius();
            snapPoints.emplace_back(this->newSnapPoint<SnapCenterPoint>(pEllipse->getId(), center));
            snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(pEllipse->getId(), center + xDir * majorRadius));
            snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(pEllipse->getId(), center + yDir * minorRadius));
            snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(pEllipse->getId(), center - xDir * majorRadius));
            snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(pEllipse->getId(), center - yDir * minorRadius));
        }
        else if (const wy3d::SketchEllipseArc3D* pEllipseArc = wy3d::SketchEllipseArc3D::cast(pEntity))
        {
            snapPoints.emplace_back(this->newSnapPoint<SnapCenterPoint>(pEllipseArc->getId(), pEllipseArc->getCenter()));
            snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(pEllipseArc->getId(), pEllipseArc->getStartPoint()));
            snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(pEllipseArc->getId(), pEllipseArc->getEndPoint()));
        }
        else if (const wy3d::SketchSpline3D* pSpline = wy3d::SketchSpline3D::cast(pEntity))
        {
            // 草图级:每个过点/控制点都给一个端点捕捉
            for (const wy::Vector3& pnt : pSpline->getPoints())
            {
                snapPoints.emplace_back(this->newSnapPoint<SnapEndPoint>(pSpline->getId(), pnt));
            }
        }
        else
        {
            // TODO 如添加了3D草图元素需要在此添加对应的代码
            assert(false);
        }
    }

    return snapPoints;
}
