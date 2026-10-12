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
#include "SketchCenterLineGripGizmo.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

SketchCenterLineGripGizmo::SketchCenterLineGripGizmo(const wy3d::SketchCenterLine* pCenterLine, Type type)
    : SketchEntityGripGizmo(pCenterLine), _type(type)
{
    assert(pCenterLine);
}

wy::Vector2 SketchCenterLineGripGizmo::getGripPosition() const
{
    const wy3d::SketchCenterLine* pCenterLine = wy3d::SketchCenterLine::cast(this->getSketchEntity());
    if (pCenterLine)
    {
        switch (_type)
        {
        case Type::StartPoint:
            return pCenterLine->getStartPoint();
        case Type::EndPoint:
            return pCenterLine->getEndPoint();
        case Type::MiddlePoint:
            return (pCenterLine->getStartPoint() + pCenterLine->getEndPoint()) / 2;
        default:
            break;
        }
    }
    assert(false);
    return wy::Vector2(0.0, 0.0);
}

bool SketchCenterLineGripGizmo::onBeginDrag(wydb::Database* pDb)
{
    assert(pDb);
    const wy3d::SketchCenterLine* pCenterLine = wy3d::SketchCenterLine::cast(pDb->getElement(_id));
    if (!pCenterLine)
    {
        assert(false);
        return false;
    }
    _startSPnt = pCenterLine->getStartPoint();
    _startEPnt = pCenterLine->getEndPoint();
    _startMPnt = (_startSPnt + _startEPnt) / 2;
    return true;
}

bool SketchCenterLineGripGizmo::onDragging(wydb::Transaction* pTrans, const wy::Vector2& curPos)
{
    assert(pTrans);
    wy3d::SketchCenterLine* pCenterLine = wy3d::SketchCenterLine::cast(pTrans->getElementForWrite(_id));
    if (pCenterLine)
    {
        switch (_type)
        {
        case Type::StartPoint:
            return wy::ErrorStatus::Ok == pCenterLine->setStartPoint(curPos);
        case Type::EndPoint:
            return wy::ErrorStatus::Ok == pCenterLine->setEndPoint(curPos);
        case Type::MiddlePoint:
        {
            wy::Vector2 delta = curPos - _startMPnt;
            wy::ErrorStatus error1 = pCenterLine->setStartPoint(_startSPnt + delta);
            wy::ErrorStatus error2 = pCenterLine->setEndPoint(_startEPnt + delta);
            return (wy::ErrorStatus::Ok == error1) && (wy::ErrorStatus::Ok == error2);
        }
        default:
            break;
        }
    }

    assert(false);
    return false;
}

SketchSnapContextSPtr SketchCenterLineGripGizmo::newDragContext() const
{
    switch (_type)
    {
    case Type::StartPoint:
        return std::make_shared<SketchDrawLineContext>(_id, _startEPnt);
    case Type::EndPoint:
        return std::make_shared<SketchDrawLineContext>(_id, _startSPnt);
    case Type::MiddlePoint:
        return std::make_shared<SketchLocateContext>(_id);
    default:
        return SketchEntityGripGizmo::newDragContext();
    }
}

} // namespace wy3dApp
