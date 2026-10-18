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

#include "SketchSnapObjectCreator.h"
#include <cassert>
#include <wyVector2.h>
#include <wydbDatabase.h>
#include <wy3dSketch.h>
#include <wy3dSketchCurve.h>
#include <wy3dSketchCurve.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>
#include <wy3dSketchEllipse.h>
#include <wy3dSketchEllipseArc.h>
#include <wy3dSketchSpline.h>

#include "utils/MathUtils.h"
#include "snap/SnapObject.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

std::list<wyap::SnapObjectSPtr> SketchSnapObjectCreator::createSnapObjects(const wydb::Element* pElem)
{
    assert(pElem);
    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pElem);
    if (!pSketch)
    {
        assert(false);
        return std::list<wyap::SnapObjectSPtr>();
    }

    std::list<wyap::SnapObjectSPtr> snapPoints;
    const wydb::Database* pDb = pSketch->getDatabase();
    if (!pDb)
    {
        assert(false);
        return snapPoints;
    }

    const wy3d::Plane& sketchPlane = pSketch->getPlane();
    if (!sketchPlane.isValid())
    {
        assert(false);
        return snapPoints;
    }

    wydb::ElementId id = pSketch->getId();
    for (auto iter = pSketch->createIterator(); !iter.isDone(); iter.moveNext())
    {
        wydb::ElementId id = iter.current();
        const wydb::Element* pElem = pDb->getElement(id);
        if (!pElem)
        {
            assert(false);
            continue;
        }

        const wy3d::SketchCurve* pSketchCurve = wy3d::SketchCurve::cast(pElem);
        if (!pSketchCurve)
        {
            continue;
        }

        if (const wy3d::SketchLine* pSketchLine = wy3d::SketchLine::cast(pSketchCurve))
        {
            snapPoints.emplace_back(this->newSnapPoint(SnapPoint::Type::End, id, sketchPlane.value(pSketchLine->getStartPoint())));
            snapPoints.emplace_back(this->newSnapPoint(SnapPoint::Type::End, id, sketchPlane.value(pSketchLine->getEndPoint())));
        }
        else if (const wy3d::SketchCenterLine* pCenterLine = wy3d::SketchCenterLine::cast(pSketchCurve))
        {
            snapPoints.emplace_back(this->newSnapPoint(SnapPoint::Type::End, id, sketchPlane.value(pCenterLine->getStartPoint())));
            snapPoints.emplace_back(this->newSnapPoint(SnapPoint::Type::End, id, sketchPlane.value(pCenterLine->getEndPoint())));
        }
        else if (const wy3d::SketchCircle* pSketchCircle = wy3d::SketchCircle::cast(pSketchCurve))
        {
            const wy::Vector2& center = pSketchCircle->getCenter();
            double radius = pSketchCircle->getRadius();
            snapPoints.emplace_back(this->newSnapPoint(SnapPoint::Type::Center, id, sketchPlane.value(center)));
            snapPoints.emplace_back(this->newSnapPoint(SnapPoint::Type::Quadrant, id, sketchPlane.value(center + wy::Vector2(radius, 0.0))));
            snapPoints.emplace_back(this->newSnapPoint(SnapPoint::Type::Quadrant, id, sketchPlane.value(center + wy::Vector2(0.0, radius))));
            snapPoints.emplace_back(this->newSnapPoint(SnapPoint::Type::Quadrant, id, sketchPlane.value(center + wy::Vector2(-radius, 0.0))));
            snapPoints.emplace_back(this->newSnapPoint(SnapPoint::Type::Quadrant, id, sketchPlane.value(center + wy::Vector2(0.0, -radius))));
        }
        else if (const wy3d::SketchArc* pSketchArc = wy3d::SketchArc::cast(pSketchCurve))
        {
            snapPoints.emplace_back(this->newSnapPoint(SnapPoint::Type::Center, id, sketchPlane.value(pSketchArc->getCenter())));
            snapPoints.emplace_back(this->newSnapPoint(SnapPoint::Type::End, id, sketchPlane.value(pSketchArc->getStartPoint())));
            snapPoints.emplace_back(this->newSnapPoint(SnapPoint::Type::End, id, sketchPlane.value(pSketchArc->getEndPoint())));
        }
        else if (const wy3d::SketchEllipse* pSketchEllipse = wy3d::SketchEllipse::cast(pSketchCurve))
        {            // 椭圆心
            snapPoints.emplace_back(this->newSnapPoint(SnapPoint::Type::Center, id, sketchPlane.value(pSketchEllipse->getCenter())));
            // 椭圆的四个端点
            snapPoints.emplace_back(this->newSnapPoint(SnapPoint::Type::Quadrant, id, sketchPlane.value(pSketchEllipse->getPointAt(0.0))));
            snapPoints.emplace_back(this->newSnapPoint(SnapPoint::Type::Quadrant, id, sketchPlane.value(pSketchEllipse->getPointAt(0.25))));
            snapPoints.emplace_back(this->newSnapPoint(SnapPoint::Type::Quadrant, id, sketchPlane.value(pSketchEllipse->getPointAt(0.5))));
            snapPoints.emplace_back(this->newSnapPoint(SnapPoint::Type::Quadrant, id, sketchPlane.value(pSketchEllipse->getPointAt(0.75))));
        }
        else if (const wy3d::SketchEllipseArc* pSketchEllipseArc = wy3d::SketchEllipseArc::cast(pSketchCurve))
        {
            // 椭圆心
            snapPoints.emplace_back(this->newSnapPoint(SnapPoint::Type::Center, id, sketchPlane.value(
                pSketchEllipseArc->getCenter())));
            // 椭圆弧起点
            snapPoints.emplace_back(this->newSnapPoint(SnapPoint::Type::End, id, sketchPlane.value(
                pSketchEllipseArc->getStartPoint())));
            // 椭圆弧终点
            snapPoints.emplace_back(this->newSnapPoint(SnapPoint::Type::End, id, sketchPlane.value(
                pSketchEllipseArc->getEndPoint())));
        }
        else if (const wy3d::SketchSpline* pSketchSpline = wy3d::SketchSpline::cast(pSketchCurve))
        {
            const std::vector<wy::Vector2>& points = pSketchSpline->getPoints();
            for (const wy::Vector2& fitPoint : points)
            {
                snapPoints.emplace_back(this->newSnapPoint(SnapPoint::Type::End, id, sketchPlane.value(
                    fitPoint)));
            }
        }
        else
        {
            // TODO 如添加了草图元素需要在此添加对应的代码
            assert(false);
        }
    }

    return snapPoints;
}

} // namespace wy3dApp
