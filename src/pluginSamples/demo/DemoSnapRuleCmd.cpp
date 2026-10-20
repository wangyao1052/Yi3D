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
#include <memory>

#include <QCoreApplication>

#include <wy3dApp/Application.h>
#include <wy3dApp/GuiCommandUtil.h>
#include <wy3dApp/Snap2dSystem.h>

#include "DemoSnapRule.h"

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

    const wy::ErrorStatus error = _sketchInfo.pSnap2dSys->addRule(
        DemoSnapContext::classInfo(), DemoSnapRule::instance());
    if (wy::ErrorStatus::Ok != error && wy::ErrorStatus::DuplicateKey != error)
    {
        return wyap::CmdExecution::StartResult::Rejected;
    }

    // One context instance across events: the snap system then keeps its result
    // and only moves the point, instead of rebuilding the display each move.
    _pContext = std::make_shared<DemoSnapContext>(_sketchInfo.sketchId);

    wy3dApp::Application::instance().getStatusBar()->setTips(QCoreApplication::translate("DemoSnapRuleCmd",
        "Demo snap rule: hover near the origin, the X/Y axes, circle centers or line end points."));

    return wyap::CmdExecution::StartResult::Succeeded;
}

void DemoSnapRuleCmd::onEnd()
{
    if (_sketchInfo.pSnap2dSys)
    {
        _sketchInfo.pSnap2dSys->removeRule(DemoSnapContext::classInfo());
    }

    __baseClass::onEnd();
}

void DemoSnapRuleCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    if (_sketchInfo.pSnap2dSys)
    {
        _sketchInfo.pSnap2dSys->removeRule(DemoSnapContext::classInfo());
    }

    __baseClass::onAbort(cause);
}

void DemoSnapRuleCmd::onMouseMove(const wy3dApp::MouseEvent& event)
{
    // Return value unused: the call's purpose is the snap display side effect.
    this->computePoint2d(event.x, event.y, {}, _pContext, _sketchInfo.pSnap2dSys);
}
