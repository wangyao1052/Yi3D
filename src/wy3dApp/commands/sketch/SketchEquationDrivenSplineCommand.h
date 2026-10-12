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

#ifndef WY3DAPP_SKETCH_EQUATION_DRIVEN_SPLINE_COMMAND_H
#define WY3DAPP_SKETCH_EQUATION_DRIVEN_SPLINE_COMMAND_H

#include <vector>
#include <wyVector2.h>
#include <wy3dVector2.h>
#include <wydbElementId.h>
#include <wyapCmdExecution.h>
#include <wy3dPlane.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class SketchEquationDrivenSplineCommand : public wyap::ImmediateCmdExecution
{
    WYRX_DECLARE_MEMBERS(SketchEquationDrivenSplineCommand, wy3dApp::SketchEquationDrivenSplineCommand, wyap::ImmediateCmdExecution)
public:
    SketchEquationDrivenSplineCommand() : wyap::ImmediateCmdExecution(), _sketchId(wydb::ElementId::kNull) {};

protected:
    virtual int run() override;

private:
    bool makeSpline(
        const wydb::ElementId& sketchId,
        const std::vector<wy::Vector2>& fitPoints);

private:
    wy3d::Plane _sketchPlane;
    wydb::ElementId _sketchId;
};

}

#endif // WY3DAPP_SKETCH_EQUATION_DRIVEN_SPLINE_COMMAND_H
