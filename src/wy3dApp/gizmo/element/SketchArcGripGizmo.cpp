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

#include <wyVector2.h>
#include <geom/wy3dArc2.h>
#include "SketchArcGripGizmo.h"
#include "utils/MathUtils.h"

SketchArcGripGizmo::SketchArcGripGizmo(const wy3d::SketchArc* pSketchArc, Type type)
    : SketchEntityGripGizmo(pSketchArc), _type(type)
{
    assert(pSketchArc);
}

wy::Vector2 SketchArcGripGizmo::getGripPosition() const
{
    const wy3d::SketchArc* pSketchArc = wy3d::SketchArc::cast(this->getSketchEntity());
    if (pSketchArc)
    {
        switch (_type)
        {
        case Type::Center:
            return pSketchArc->getCenter();
        case Type::Start:
            return pSketchArc->getStartPoint();
        case Type::End:
            return pSketchArc->getEndPoint();
        case Type::Middle:
            return pSketchArc->getMiddlePoint();
        default:
            break;
        }
    }

    assert(false);
    return wy::Vector2(0.0, 0.0);
}

bool SketchArcGripGizmo::onBeginDrag(wydb::Database* pDb)
{
    assert(pDb);
    const wy3d::SketchArc* pSketchArc = wy3d::SketchArc::cast(pDb->getElement(_id));
    if (!pSketchArc)
    {
        assert(false);
        return false;
    }
    _startSPnt = pSketchArc->getStartPoint();
    _startEPnt = pSketchArc->getEndPoint();
    _startMPnt = pSketchArc->getMiddlePoint();
    return true;
}

bool SketchArcGripGizmo::onDragging(wydb::Transaction* pTrans, const wy::Vector2& curPos)
{
    assert(pTrans);
    wy3d::SketchArc* pSketchArc = wy3d::SketchArc::cast(pTrans->getElementForWrite(_id));
    if (pSketchArc)
    {
        switch (_type)
        {
        case Type::Center:
            return wy::ErrorStatus::Ok == pSketchArc->setCenter(curPos);
        case Type::Start:
            return this->updateArcByThreePoints(pSketchArc, curPos, _startMPnt, _startEPnt);
        case Type::End:
            return this->updateArcByThreePoints(pSketchArc, _startSPnt, _startMPnt, curPos);
        case Type::Middle:
            return this->updateArcByThreePoints(pSketchArc, _startSPnt, curPos, _startEPnt);
        default:
            break;
        }
    }

    assert(false);
    return false;
}

bool SketchArcGripGizmo::updateArcByThreePoints(
    wy3d::SketchArc* pArc,
    const wy::Vector2& p1, const wy::Vector2& p2, const wy::Vector2& p3)
{
    assert(pArc);
    wy3d::geom::Arc2 arc = MathUtils::computeArcFromThreePoints(p1, p2, p3);
    if (arc.radius() == 0.0)
    {
        return false;
    }
    if (wy::ErrorStatus::Ok != pArc->setCenter(arc.center())) return false;
    if (wy::ErrorStatus::Ok != pArc->setRadius(arc.radius())) return false;
    if (wy::ErrorStatus::Ok != pArc->setStartAngle(arc.startAngle())) return false;
    if (wy::ErrorStatus::Ok != pArc->setEndAngle(arc.endAngle())) return false;

    return true;
}