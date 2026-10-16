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
#include "SketchCircleGripGizmo.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

SketchCircleGripGizmo::SketchCircleGripGizmo(const wy3d::SketchCircle* pSketchCircle, Type type)
    : SketchEntityGripGizmo(pSketchCircle), _type(type)
{
    assert(pSketchCircle);
}

wy::Vector2 SketchCircleGripGizmo::getGripPosition() const
{
    const wy3d::SketchCircle* pSketchCircle = wy3d::SketchCircle::cast(this->getSketchEntity());
    if (pSketchCircle)
    {
        switch (_type)
        {
        case Type::Center:
            return pSketchCircle->getCenter();
        case Type::Quarter0:
            return pSketchCircle->getCenter() + wy::Vector2(pSketchCircle->getRadius(), 0.0);
        case Type::Quarter1:
            return pSketchCircle->getCenter() + wy::Vector2(0.0, pSketchCircle->getRadius());
        case Type::Quarter2:
            return pSketchCircle->getCenter() + wy::Vector2(-pSketchCircle->getRadius(), 0.0);
        case Type::Quarter3:
            return pSketchCircle->getCenter() + wy::Vector2(0.0, -pSketchCircle->getRadius());
        default:
            break;
        }
    }

    assert(false);
    return wy::Vector2(0.0, 0.0);
}

bool SketchCircleGripGizmo::onBeginDrag(wydb::Database* pDb)
{
    assert(pDb);
    return true;
}

bool SketchCircleGripGizmo::onDragging(wydb::Transaction* pTrans, const wy::Vector2& curPos)
{
    assert(pTrans);
    wy3d::SketchCircle* pSketchCircle = wy3d::SketchCircle::cast(pTrans->getElementForWrite(_id));
    if (pSketchCircle)
    {
        switch (_type)
        {
        case Type::Center:
            return wy::ErrorStatus::Ok == pSketchCircle->setCenter(curPos);
        case Type::Quarter0:
        case Type::Quarter1:
        case Type::Quarter2:
        case Type::Quarter3:
        {
            double newRadius = (curPos - pSketchCircle->getCenter()).length();
            return wy::ErrorStatus::Ok == pSketchCircle->setRadius(newRadius);
        }
        default:
            break;
        }
    }

    assert(false);
    return false;
}

Snap2dContextSPtr SketchCircleGripGizmo::newDragContext() const
{
    switch (_type)
    {
    case Type::Center:
        return std::make_shared<Locate2dContext>();
    case Type::Quarter0:
    case Type::Quarter1:
    case Type::Quarter2:
    case Type::Quarter3:
    {
        const wy3d::SketchCircle* pCircle = wy3d::SketchCircle::cast(this->getSketchEntity());
        if (pCircle) return std::make_shared<DrawCircle2dContext>(pCircle->getCenter());
        else return std::make_shared<Locate2dContext>();
    }
    default:
        return SketchEntityGripGizmo::newDragContext();
    }
}

} // namespace wy3dApp
