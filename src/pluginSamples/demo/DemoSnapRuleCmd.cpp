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

#include "DemoSnapRuleCmd.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

#include <QCoreApplication>

#include <wyVector2.h>
#include <wy3dApp/Application.h>
#include <wy3dApp/GuiCommandUtil.h>
#include <wy3dApp/Snap2dSystem.h>

#include "DemoSnapRule.h"

namespace {

const wy::Vector2 kFarPos(503.0, 503.0);
// The axis probe sits this many pixels from the origin, clear of point candidates.
const double kAxisOffsetPixels = 50.0;

bool isSamePoint(const wy::Vector2& lhs, const wy::Vector2& rhs)
{
    return lhs.x() == rhs.x() && lhs.y() == rhs.y();
}

bool hasCandidateNear(const std::vector<DemoSnapCandidate>& candidates,
    const wy::Vector2& point, double tolX, double tolY)
{
    for (const DemoSnapCandidate& candidate : candidates)
    {
        if (std::fabs(candidate.point.x() - point.x()) <= tolX
            && std::fabs(candidate.point.y() - point.y()) <= tolY)
        {
            return true;
        }
    }

    return false;
}

const DemoSnapCandidate* firstCandidate(const std::vector<DemoSnapCandidate>& candidates,
    wy3dApp::Snap2dType snapType)
{
    for (const DemoSnapCandidate& candidate : candidates)
    {
        if (snapType == candidate.snapType) return &candidate;
    }

    return nullptr;
}

} // namespace

DemoSnapRuleCmd::DemoSnapRuleCmd()
    : wy3dApp::OsgGuiCommand()
{
}

DemoSnapRuleCmd::~DemoSnapRuleCmd()
{
}

wyap::CmdExecution::StartResult DemoSnapRuleCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = __baseClass::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    _sketchInfo = wy3dApp::GuiCommandUtil::initSketchInfo();
    if (!_sketchInfo.pSnap2dSys)
    {
        return wyap::CmdExecution::StartResult::Rejected;
    }

    const wy::ErrorStatus error = _sketchInfo.pSnap2dSys->addRule(DemoSnapRule::instance());
    if (wy::ErrorStatus::Ok != error && wy::ErrorStatus::DuplicateKey != error)
    {
        std::cout << "DEMO_SNAPRULE|FAIL|add-rule=" << static_cast<int>(error) << std::endl;
        return wyap::CmdExecution::StartResult::Rejected;
    }

    // One context instance across events: the snap system then keeps its result
    // and only moves the point, instead of rebuilding the display each move.
    _pContext = std::make_shared<DemoSnapContext>(_sketchInfo.sketchId);

    wy3dApp::Application::instance().getStatusBar()->setTips(QCoreApplication::translate("DemoSnapRuleCmd",
        "Demo snap rule: hover near the origin, the X/Y axes, circle centers or line end points; click to run the self test."));

    return wyap::CmdExecution::StartResult::Succeeded;
}

void DemoSnapRuleCmd::onMouseMove(const wy3dApp::MouseEvent& event)
{
    // Return value unused: the call's purpose is the snap display side effect.
    this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, {}, _pContext, _sketchInfo.pSnap2dSys, false);
}

void DemoSnapRuleCmd::onLeftMouseUp(const wy3dApp::MouseEvent& event)
{
    if (_tested) return;
    _tested = true;

    // Stay active after the test: the status tip keeps showing the result and
    // hovering keeps demonstrating the rule. Ending here would immediately
    // restore the selection command's tip.
    const bool isPass = this->runSelfTest(event.x, event.y);

    if (isPass)
        wy3dApp::Application::instance().getStatusBar()->setTips(QCoreApplication::translate("DemoSnapRuleCmd", "Snap rule self test: PASS"));
    else
        wy3dApp::Application::instance().getStatusBar()->setTips(QCoreApplication::translate("DemoSnapRuleCmd", "Snap rule self test: FAIL"));
}

bool DemoSnapRuleCmd::runSelfTest(double x, double y)
{
    osgViewer::View* pView = this->getOsgView();
    if (!pView || !_sketchInfo.pSnap2dSys)
    {
        std::cout << "DEMO_SNAPRULE|FAIL|no-view-or-snap2d-system" << std::endl;
        return false;
    }

    DemoSnapRule* pRule = DemoSnapRule::instance().get();
    pRule->resetRecording();

    const std::vector<DemoSnapCandidate> candidates = collectSnapCandidates(
        wy3dApp::Application::instance().getActiveDatabase(), _sketchInfo.sketchId);

    const auto snapAt = [this, x, y, pView](const wy::Vector2& pos)
    {
        return _sketchInfo.pSnap2dSys->snap(x, y, pView, pos, {}, _pContext);
    };

    // Origin
    const bool isOriginHit = (nullptr != snapAt(wy::Vector2::kZero))
        && (wy3dApp::Snap2dType::Origin == pRule->getLastKind())
        && isSamePoint(wy::Vector2::kZero, pRule->getLastHitPoint());

    // Points on the X axis (AlignY) and on the Y axis (AlignX)
    const double tolX = std::fabs(10.0 * pRule->getLastScaleScreenX());
    const double tolY = std::fabs(10.0 * pRule->getLastScaleScreenY());

    double axisOffsetX = kAxisOffsetPixels * pRule->getLastScaleScreenX();
    while (hasCandidateNear(candidates, wy::Vector2(axisOffsetX, 0.0), tolX, tolY))
        axisOffsetX += kAxisOffsetPixels * pRule->getLastScaleScreenX();
    const bool isAxisXHit = (nullptr != snapAt(wy::Vector2(axisOffsetX, 0.0)))
        && (wy3dApp::Snap2dType::AlignY == pRule->getLastKind())
        && isSamePoint(wy::Vector2(axisOffsetX, 0.0), pRule->getLastHitPoint());

    double axisOffsetY = kAxisOffsetPixels * pRule->getLastScaleScreenY();
    while (hasCandidateNear(candidates, wy::Vector2(0.0, axisOffsetY), tolX, tolY))
        axisOffsetY += kAxisOffsetPixels * pRule->getLastScaleScreenY();
    const bool isAxisYHit = (nullptr != snapAt(wy::Vector2(0.0, axisOffsetY)))
        && (wy3dApp::Snap2dType::AlignX == pRule->getLastKind())
        && isSamePoint(wy::Vector2(0.0, axisOffsetY), pRule->getLastHitPoint());

    // Circle center and line end point, when the sketch has them
    const DemoSnapCandidate* pCenter = firstCandidate(candidates, wy3dApp::Snap2dType::CenterPoint);
    const bool isCenterSkipped = (nullptr == pCenter);
    const bool isCenterHit = !isCenterSkipped
        && (nullptr != snapAt(pCenter->point))
        && (wy3dApp::Snap2dType::CenterPoint == pRule->getLastKind())
        && isSamePoint(pCenter->point, pRule->getLastHitPoint())
        && (pCenter->id == pRule->getLastHighlightId());

    const DemoSnapCandidate* pEnd = firstCandidate(candidates, wy3dApp::Snap2dType::EndPoint);
    const bool isEndSkipped = (nullptr == pEnd);
    const bool isEndHit = !isEndSkipped
        && (nullptr != snapAt(pEnd->point))
        && (wy3dApp::Snap2dType::EndPoint == pRule->getLastKind())
        && isSamePoint(pEnd->point, pRule->getLastHitPoint())
        && (pEnd->id == pRule->getLastHighlightId());

    // Far away: nothing snaps
    const bool isFarMiss = (nullptr == snapAt(kFarPos));

    const bool isPass = isOriginHit && isAxisXHit && isAxisYHit
        && (isCenterSkipped || isCenterHit)
        && (isEndSkipped || isEndHit)
        && isFarMiss;

    std::cout << "DEMO_SNAPRULE|" << (isPass ? "PASS" : "FAIL")
        << "|origin=" << (isOriginHit ? 1 : 0)
        << "|axisX=" << (isAxisXHit ? 1 : 0)
        << "|axisY=" << (isAxisYHit ? 1 : 0)
        << "|center=" << (isCenterSkipped ? "skip" : (isCenterHit ? "1" : "0"))
        << "|end=" << (isEndSkipped ? "skip" : (isEndHit ? "1" : "0"))
        << "|far=" << (isFarMiss ? 0 : 1)
        << "|calls=" << pRule->getCallCount()
        << "|scaleY=" << pRule->getLastScaleScreenY()
        << std::endl;

    return isPass;
}
