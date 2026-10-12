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

#ifndef WY3DAPP_MODELING_MOVE_GUI_CMD_H
#define WY3DAPP_MODELING_MOVE_GUI_CMD_H

#include "commands/edit/MoveGuiCmd.h"
#include "wy3dAppDefs.h"

namespace wy3dApp {

class ModelingMoveGuiCmd : public MoveGuiCmd
{
    WYRX_DECLARE_MEMBERS(ModelingMoveGuiCmd, wy3dApp::ModelingMoveGuiCmd, MoveGuiCmd)
public:
    ModelingMoveGuiCmd();
    virtual ~ModelingMoveGuiCmd();

protected:
    virtual void gotoNextStepAfterSelectElements() override;
    virtual void configureSelectElementOptions(GuiCmdSelectOptions& options) override;
    virtual void onMouseMove_SpecifyStartPnt(double x, double y) override;
    virtual void onMouseMove_SpecifyEndPnt(double x, double y) override;
    virtual void onLeftMouseDown_SpecifyStartPnt(double x, double y) override;
    virtual void onLeftMouseDown_SpecifyEndPnt(double x, double y) override;
    virtual void syncStartPntData() override;
    virtual void syncEndPntData() override;
    virtual void showCoordinatePopup() override;
    virtual void handlePopupEnterKey() override;
    virtual bool executeMove() override;
    virtual void onStart_EnvSpecific() override;
    virtual void cleanupEnvSpecific() override;

private:
    wy::Vector3 snapInWorkPlane(double x, double y, const wy::Vector3& pos3d) const;
};

}

#endif // WY3DAPP_MODELING_MOVE_GUI_CMD_H
