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
#include "SketchPointGripGizmo.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

SketchPointGripGizmo::SketchPointGripGizmo(const wy3d::SketchPoint* pSketchPoint)
    : SketchEntityGripGizmo(pSketchPoint), _position()
{
    assert(pSketchPoint);
}

wy::Vector2 SketchPointGripGizmo::getGripPosition() const
{
    const wy3d::SketchPoint* pSketchPoint = wy3d::SketchPoint::cast(this->getSketchEntity());
    if (pSketchPoint)
    {
        return pSketchPoint->getPosition();
    }
    else
    {
        assert(false);
        return wy::Vector2(0.0, 0.0);
    }
}

bool SketchPointGripGizmo::onBeginDrag(wydb::Database* pDb)
{
    assert(pDb);
    const wy3d::SketchPoint* pSketchPoint = wy3d::SketchPoint::cast(pDb->getElement(getFirstElementId()));
    if (!pSketchPoint)
    {
        assert(false);
        return false;
    }
    _position = pSketchPoint->getPosition();
    return true;
}

bool SketchPointGripGizmo::onDragging(wydb::Transaction* pTrans, const wy::Vector2& curPos)
{
    assert(pTrans);
    wy3d::SketchPoint* pSketchPoint = wy3d::SketchPoint::cast(pTrans->getElementForWrite(getFirstElementId()));
    if (pSketchPoint)
    {
        return wy::ErrorStatus::Ok == pSketchPoint->setPosition(curPos);
    }
    else
    {
        assert(false);
        return false;
    }
}

Snap2dContextSPtr SketchPointGripGizmo::newDragContext() const
{
    return std::make_shared<Locate2dContext>();
}

} // namespace wy3dApp
