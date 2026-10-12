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

#ifndef WY3DAPP_SKETCH_ROTATE_GUI_CMD_H
#define WY3DAPP_SKETCH_ROTATE_GUI_CMD_H

#include <wy3dApp/OsgGuiCommand.h>
#include "commands/edit/RotateGuiCmd.h"
#include "wy3dAppDefs.h"

namespace wy3dApp {

class SketchRotateGuiCmd : public RotateGuiCmd
{
    WYRX_DECLARE_MEMBERS(SketchRotateGuiCmd, wy3dApp::SketchRotateGuiCmd, RotateGuiCmd)
public:
    SketchRotateGuiCmd();
    virtual ~SketchRotateGuiCmd();

protected:
    GuiCmdSketchInfo _sketchInfo;
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual const wy3d::Plane& getActivePlane() const override;
    virtual SketchSnapSystem* getActiveSnapSystem() const override;
    virtual void gotoNextStepAfterSelectElements() override;
    virtual void configureSelectElementOptions(GuiCmdSelectOptions& options) override;
    SketchSnapSystem* getSketchSnapSys() const override { return _sketchInfo.pSketchSnapSys; }
};

}

#endif // WY3DAPP_SKETCH_ROTATE_GUI_CMD_H
