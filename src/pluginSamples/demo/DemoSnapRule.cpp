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

#include "DemoSnapRule.h"

#include <cmath>

#include <wy3dSketch.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchLine.h>

std::vector<DemoSnapCandidate> collectSnapCandidates(
    const wydb::Database* pDb, const wydb::ElementId& sketchId,
    const std::set<wydb::ElementId>& excludeIds)
{
    std::vector<DemoSnapCandidate> candidates;
    if (!pDb) return candidates;

    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(sketchId));
    if (!pSketch) return candidates;

    for (auto iter = pSketch->createIterator(); !iter.isDone(); iter.moveNext())
    {
        const wydb::ElementId id = iter.current();
        if (excludeIds.cend() != excludeIds.find(id)) continue;

        const wydb::Element* pElem = pDb->getElement(id);
        if (const wy3d::SketchCircle* pCircle = wy3d::SketchCircle::cast(pElem))
        {
            candidates.push_back({ pCircle->getCenter(), id, wy3dApp::Snap2dType::CenterPoint });
        }
        else if (const wy3d::SketchLine* pLine = wy3d::SketchLine::cast(pElem))
        {
            candidates.push_back({ pLine->getStartPoint(), id, wy3dApp::Snap2dType::EndPoint });
            candidates.push_back({ pLine->getEndPoint(), id, wy3dApp::Snap2dType::EndPoint });
        }
    }

    return candidates;
}

std::shared_ptr<DemoSnapRule> DemoSnapRule::instance()
{
    static std::shared_ptr<DemoSnapRule> s_instance(new DemoSnapRule());
    return s_instance;
}

const std::type_info& DemoSnapRule::getContextType() const
{
    return typeid(DemoSnapContext);
}

void DemoSnapRule::resetRecording()
{
    _callCount = 0;
    _lastScaleScreenX = 0.0;
    _lastScaleScreenY = 0.0;
    _lastHitPoint = wy::Vector2();
    _lastKind = wy3dApp::Snap2dType::Undefined;
    _lastHighlightId = wydb::ElementId::kNull;
}

bool DemoSnapRule::recordHit(const wy::Vector2& point, const wydb::ElementId& highlightId,
    wy3dApp::Snap2dType snapType, wy3dApp::Snap2dRuleHit& hit)
{
    hit.point = point;
    hit.highlightId = highlightId;
    hit.snapType = snapType;

    _lastHitPoint = point;
    _lastHighlightId = highlightId;
    _lastKind = snapType;

    return true;
}

bool DemoSnapRule::snapNoPicked(
    const wy3dApp::Snap2dContext* pContext,
    const wydb::Database* pDb,
    const wy::Vector2& pos,
    double scaleScreenX, double scaleScreenY,
    const std::set<wydb::ElementId>& excludeIds,
    wy3dApp::Snap2dRuleHit& hit)
{
    ++_callCount;
    _lastScaleScreenX = scaleScreenX;
    _lastScaleScreenY = scaleScreenY;

    const DemoSnapContext* pDemoContext = dynamic_cast<const DemoSnapContext*>(pContext);
    if (!pDemoContext || !pDb) return false;

    // 10-pixel tolerances converted to plane units
    const double tolX = std::fabs(10.0 * scaleScreenX);
    const double tolY = std::fabs(10.0 * scaleScreenY);

    // Origin: the X/Y axes crossing
    if (std::fabs(pos.x()) <= tolX && std::fabs(pos.y()) <= tolY)
    {
        return this->recordHit(wy::Vector2::kZero, wydb::ElementId::kNull,
            wy3dApp::Snap2dType::Origin, hit);
    }

    // Closest circle center or line end point
    const std::vector<DemoSnapCandidate> candidates =
        collectSnapCandidates(pDb, pDemoContext->getSketchId(), excludeIds);
    const DemoSnapCandidate* pClosest = nullptr;
    double closestDistSq = 0.0;
    for (const DemoSnapCandidate& candidate : candidates)
    {
        const double dx = candidate.point.x() - pos.x();
        const double dy = candidate.point.y() - pos.y();
        if (std::fabs(dx) > tolX || std::fabs(dy) > tolY) continue;

        const double distSq = dx * dx + dy * dy;
        if (!pClosest || distSq < closestDistSq)
        {
            pClosest = &candidate;
            closestDistSq = distSq;
        }
    }
    if (pClosest)
    {
        return this->recordHit(pClosest->point, pClosest->id, pClosest->snapType, hit);
    }

    // Points on the X axis (Y locked) and on the Y axis (X locked)
    if (std::fabs(pos.y()) <= tolY)
    {
        return this->recordHit(wy::Vector2(pos.x(), 0.0), wydb::ElementId::kNull,
            wy3dApp::Snap2dType::AlignY, hit);
    }
    if (std::fabs(pos.x()) <= tolX)
    {
        return this->recordHit(wy::Vector2(0.0, pos.y()), wydb::ElementId::kNull,
            wy3dApp::Snap2dType::AlignX, hit);
    }

    return false;
}
