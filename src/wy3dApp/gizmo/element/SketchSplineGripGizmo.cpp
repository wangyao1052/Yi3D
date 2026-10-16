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

#include "SketchSplineGripGizmo.h"
#include <cassert>
#include <wyVector2.h>
#include <wy3dSketchSpline.h>
#include <wy3dApp/Application.h>
#include "gizmo/renderer/SketchSplineCtrolPointsPathOsgRenderer.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

SplineCtrolPointsPathGhostGizmo::SplineCtrolPointsPathGhostGizmo(const wy3d::SketchSpline* pSpline)
    : GhostGizmo(pSpline)
{
    _pRenderer = std::make_unique<SketchSplineCtrolPointsPathOsgRenderer>(this);
}

SketchSplineGripGizmo::SketchSplineGripGizmo(const wy3d::SketchSpline* pSketchSpline, unsigned int index)
    : SketchEntityGripGizmo(pSketchSpline), _index(index)
{
    assert(pSketchSpline);
}

wy::Vector2 SketchSplineGripGizmo::getGripPosition() const
{
    const wy3d::SketchSpline* pSketchSpline = wy3d::SketchSpline::cast(this->getSketchEntity());
    if (!pSketchSpline)
    {
        assert(false);
        return wy::Vector2(0.0, 0.0);
    }
    const std::vector<wy::Vector2>& points = pSketchSpline->getPoints();
    if (_index >= points.size())
    {
        assert(false);
        return wy::Vector2(0.0, 0.0);
    }
    return points[_index];
}

bool SketchSplineGripGizmo::onBeginDrag(wydb::Database* pDb)
{
    assert(pDb);
    const wy3d::SketchSpline* pSketchSpline = wy3d::SketchSpline::cast(pDb->getElement(_id));
    if (!pSketchSpline)
    {
        assert(false);
        return false;
    }
    return true;
}

bool SketchSplineGripGizmo::onDragging(wydb::Transaction* pTrans, const wy::Vector2& curPos)
{
    assert(pTrans);
    wy3d::SketchSpline* pSketchSpline = wy3d::SketchSpline::cast(pTrans->getElementForWrite(_id));
    if (!pSketchSpline)
    {
        assert(false);
        return false;
    }
    const std::vector<wy::Vector2>& points = pSketchSpline->getPoints();
    if (_index >= points.size())
    {
        assert(false);
        return false;
    }
    std::vector<wy::Vector2> newPoints = points;
    newPoints[_index] = curPos;
    if (wy::ErrorStatus::Ok == pSketchSpline->setPoints(newPoints))
    {
        return true;
    }
    else
    {
        return false;
    }
}

SketchSnapContextSPtr SketchSplineGripGizmo::newDragContext() const
{
    return std::make_shared<SketchLocateContext>();
}

} // namespace wy3dApp
