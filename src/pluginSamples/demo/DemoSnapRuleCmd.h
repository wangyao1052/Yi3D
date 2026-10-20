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

#ifndef DEMO_SNAP_RULE_CMD_H
#define DEMO_SNAP_RULE_CMD_H

#include <wyapCmdExecution.h>

#include <wy3dApp/OsgGuiCommand.h>

// Self test for the Snap2dRule injection channel: registers the demo rule on
// start, so hovering near the sketch X axis shows the rule's locked point, and
// the next left click runs the scripted assertions once, reporting on stdout as
// DEMO_SNAPRULE|PASS|... / DEMO_SNAPRULE|FAIL|...
class DemoSnapRuleCmd : public wy3dApp::OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(DemoSnapRuleCmd, com.yi3d.demo::DemoSnapRuleCmd, wy3dApp::OsgGuiCommand)
public:
    DemoSnapRuleCmd();
    ~DemoSnapRuleCmd() override;

protected:
    wyap::CmdExecution::StartResult onStart() override;
    void onEnd() override;
    void onAbort(wyap::CmdExecution::AbortCause cause) override;
    void onMouseMove(const wy3dApp::MouseEvent& event) override;

private:
    wy3dApp::GuiCmdSketchInfo _sketchInfo;
    wy3dApp::Snap2dContextSPtr _pContext;
};

#endif // DEMO_SNAP_RULE_CMD_H
