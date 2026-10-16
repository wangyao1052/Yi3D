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

#ifndef WY3DAPP_MODELING_ROTATE_GUI_CMD_H
#define WY3DAPP_MODELING_ROTATE_GUI_CMD_H

#include "commands/edit/RotateGuiCmd.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class ModelingRotateGuiCmd : public RotateGuiCmd
{
    WYRX_DECLARE_MEMBERS(ModelingRotateGuiCmd, wy3dApp::ModelingRotateGuiCmd, RotateGuiCmd)
public:
    ModelingRotateGuiCmd();
    virtual ~ModelingRotateGuiCmd();

protected:
    virtual const wy3d::Plane& getActivePlane() const override;
    virtual Snap2dSystem* getActiveSnapSystem() const override;
    virtual void gotoNextStepAfterSelectElements() override;
    virtual void configureSelectElementOptions(SelectOptions& options) override;
    virtual void onStart_EnvSpecific() override;
    virtual void cleanupEnvSpecific() override;
};

} // namespace wy3dApp

#endif // WY3DAPP_MODELING_ROTATE_GUI_CMD_H
