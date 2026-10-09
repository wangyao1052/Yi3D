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

#ifndef DEMO_PARAMETERS_CMD_H
#define DEMO_PARAMETERS_CMD_H

#include <wydbElementId.h>

#include <wyapCmdExecution.h>

#include "commands/OsgGuiCommand.h"
#include "select/PointPick.h"
#include "select/SelectPreview.h"

// Demonstrates picking a sketch element and listing all its parameters.
class DemoParametersCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(DemoParametersCmd, com.yi3d.demo::DemoParametersCmd, OsgGuiCommand)
public:
    DemoParametersCmd();
    ~DemoParametersCmd() override;

protected:
    wyap::CmdExecution::StartResult onStart() override;
    void cleanup() override;
    void onMouseMove(const MouseEvent& event) override;
    void onLeftMouseUp(const MouseEvent& event) override;

private:
    void showParameters(const wydb::ElementId& elementId);

private:
    PointPickOption _pointPickOption;
    SelectPreview _hoverPreview;
};

#endif // DEMO_PARAMETERS_CMD_H
