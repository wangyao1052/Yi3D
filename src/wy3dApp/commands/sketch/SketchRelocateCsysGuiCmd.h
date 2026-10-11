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

#ifndef WY3DAPP_SKETCH_RELOCATE_CSYS_CMD_H
#define WY3DAPP_SKETCH_RELOCATE_CSYS_CMD_H

#include <wy3dApp/OsgGuiCommand.h>
#include <wyVector2.h>
#include "commands/transient/SketchBasicTransient.h"

class SketchRelocateCsysGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(SketchRelocateCsysGuiCmd, SketchRelocateCsysGuiCmd, OsgGuiCommand)
public:
    SketchRelocateCsysGuiCmd();
    virtual ~SketchRelocateCsysGuiCmd();

protected:
    GuiCmdSketchInfo _sketchInfo;
    virtual wyap::CmdExecution::StartResult onStart() override;

protected:
    enum class Step
    {
        Undefined = 0,
        SpecifyOrigin = 1,
        SpecifyRotationAngle = 2,
    };
    bool finishStep(Step step);
    void gotoStep(Step step);

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseDown(const MouseEvent& event) override;

private:
    bool relocateSketchCsys(const wy3d::Plane& plane, const wy::Vector2& origin, double angle);

private:
    Step _step;
    wy::Vector2 _origin;
    double _angle;
    SketchSnapContextSPtr _pSnapContext;
    SketchCsysTransientSPtr _pCsysTransient;
};

#endif // WY3DAPP_SKETCH_RELOCATE_CSYS_CMD_H