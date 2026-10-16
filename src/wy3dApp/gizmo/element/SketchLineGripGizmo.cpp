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
#include "SketchLineGripGizmo.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

SketchLineGripGizmo::SketchLineGripGizmo(const wy3d::SketchLine* pSketchLine, Type type)
    : SketchEntityGripGizmo(pSketchLine), _type(type)
{
    assert(pSketchLine);
}

wy::Vector2 SketchLineGripGizmo::getGripPosition() const
{
    const wy3d::SketchLine* pSketchLine = wy3d::SketchLine::cast(this->getSketchEntity());
    if (pSketchLine)
    {
        switch (_type)
        {
        case Type::StartPoint:
            return pSketchLine->getStartPoint();
        case Type::EndPoint:
            return pSketchLine->getEndPoint();
        case Type::MiddlePoint:
            return (pSketchLine->getStartPoint() + pSketchLine->getEndPoint()) / 2;
        default:
            break;
        }
    }
    assert(false);
    return wy::Vector2(0.0, 0.0);
}

bool SketchLineGripGizmo::onBeginDrag(wydb::Database* pDb)
{
    assert(pDb);
    const wy3d::SketchLine* pSketchLine = wy3d::SketchLine::cast(pDb->getElement(_id));
    if (!pSketchLine)
    {
        assert(false);
        return false;
    }
    _startSPnt = pSketchLine->getStartPoint();
    _startEPnt = pSketchLine->getEndPoint();
    _startMPnt = (_startSPnt + _startEPnt) / 2;
    return true;
}

bool SketchLineGripGizmo::onDragging(wydb::Transaction* pTrans, const wy::Vector2& curPos)
{
    assert(pTrans);
    wy3d::SketchLine* pSketchLine = wy3d::SketchLine::cast(pTrans->getElementForWrite(_id));
    if (pSketchLine)
    {
        switch (_type)
        {
        case Type::StartPoint:
            return wy::ErrorStatus::Ok == pSketchLine->setStartPoint(curPos);
        case Type::EndPoint:
            return wy::ErrorStatus::Ok == pSketchLine->setEndPoint(curPos);
        case Type::MiddlePoint:
        {
            wy::Vector2 delta = curPos - _startMPnt;
            wy::ErrorStatus error1 = pSketchLine->setStartPoint(_startSPnt + delta);
            wy::ErrorStatus error2 = pSketchLine->setEndPoint(_startEPnt + delta);
            return (wy::ErrorStatus::Ok == error1) && (wy::ErrorStatus::Ok == error2);
        }
        default:
            break;
        }
    }

    assert(false);
    return false;
}

Snap2dContextSPtr SketchLineGripGizmo::newDragContext() const
{
    switch (_type)
    {
    case Type::StartPoint:
        return std::make_shared<SketchDrawLineContext>(_startEPnt);
    case Type::EndPoint:
        return std::make_shared<SketchDrawLineContext>(_startSPnt);
    case Type::MiddlePoint:
        return std::make_shared<Locate2dContext>();
    default:
        return SketchEntityGripGizmo::newDragContext();
    }
}

} // namespace wy3dApp
