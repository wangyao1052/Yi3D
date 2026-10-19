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

#ifndef DEMO_SNAP_RULE_H
#define DEMO_SNAP_RULE_H

#include <memory>
#include <set>
#include <vector>

#include <wyVector2.h>
#include <wydbElementId.h>
#include <wy3dApp/Snap2dType.h>
#include <wy3dApp/Snap2dContext.h>
#include <wy3dApp/Snap2dRule.h>

// A point the demo rule can snap to: a circle center or a line end point.
struct DemoSnapCandidate
{
    wy::Vector2 point;
    wydb::ElementId id;
    wy3dApp::Snap2dType snapType;
};

// Scan the sketch's curves for demo snap candidates. Also used by the self test
// to pick the probe positions.
std::vector<DemoSnapCandidate> collectSnapCandidates(
    const wydb::Database* pDb, const wydb::ElementId& sketchId,
    const std::set<wydb::ElementId>& excludeIds = {});

// Snap context carrying the sketch whose geometry the demo rule scans.
class DemoSnapContext : public wy3dApp::Snap2dContext
{
    WYRX_DECLARE_ABSTRACT_MEMBERS(DemoSnapContext, com.yi3d.demo::DemoSnapContext, wy3dApp::Snap2dContext)
public:
    explicit DemoSnapContext(const wydb::ElementId& sketchId) : _sketchId(sketchId) {}

    const wydb::ElementId& getSketchId() const { return _sketchId; }

private:
    wydb::ElementId _sketchId;
};

// Demo rule with five snap behaviors, in priority order: the sketch origin
// (Origin), the closest circle center (CenterPoint) or line end
// point (EndPoint), then points on the X axis (AlignY) and on the Y axis
// (AlignX). The origin also reports both axis alignments as extra hits.
// Tolerances are 10 pixels converted with the screen scale the same way the
// built-in rules do it.
class DemoSnapRule : public wy3dApp::Snap2dRule
{
public:
    // The plugin keeps one rule instance; re-registering the same instance is
    // tolerated by the self test, so command re-runs must not lose the state.
    static std::shared_ptr<DemoSnapRule> instance();

    virtual const wyrx::ClassInfo* getContextClass() const override;

    virtual bool snapNoPicked(
        const wy3dApp::Snap2dContext* pContext,
        const wydb::Database* pDb,
        const wy::Vector2& pos,
        double scaleScreenX, double scaleScreenY,
        const std::set<wydb::ElementId>& excludeIds,
        wy::Vector2& retPosition,
        std::vector<wy3dApp::Snap2dHit>& hits) override;

    // Self-test readback.
    void resetRecording();
    int getCallCount() const { return _callCount; }
    double getLastScaleScreenX() const { return _lastScaleScreenX; }
    double getLastScaleScreenY() const { return _lastScaleScreenY; }
    const wy::Vector2& getLastHitPoint() const { return _lastHitPoint; }
    wy3dApp::Snap2dType getLastKind() const { return _lastKind; }
    const wydb::ElementId& getLastElementId() const { return _lastElementId; }

private:
    DemoSnapRule() = default;

    bool recordHit(const wy::Vector2& snappedPosition, const wy::Vector2& hitPoint,
        const wydb::ElementId& elementId, wy3dApp::Snap2dType snapType,
        wy::Vector2& retPosition, std::vector<wy3dApp::Snap2dHit>& hits);
    void appendHit(const wy::Vector2& point, const wydb::ElementId& elementId,
        wy3dApp::Snap2dType snapType, std::vector<wy3dApp::Snap2dHit>& hits) const;

private:
    int _callCount = 0;
    double _lastScaleScreenX = 0.0;
    double _lastScaleScreenY = 0.0;
    wy::Vector2 _lastHitPoint;
    wy3dApp::Snap2dType _lastKind = wy3dApp::Snap2dType::Undefined;
    wydb::ElementId _lastElementId;
};

#endif // DEMO_SNAP_RULE_H
