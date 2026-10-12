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

#ifndef WY3DAPP_COINCIDENT_DATUM_PLN_CMD_H
#define WY3DAPP_COINCIDENT_DATUM_PLN_CMD_H

#include <wyapSelection.h>
#include <wy3dPlane.h>
#include <wy3dDatumPlane.h>
#include <wy3dApp/OsgGuiCommand.h>
#include "commands/datumPlane/MakeDatumPlane.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class CoincidentDatumPlnCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(CoincidentDatumPlnCmd, wy3dApp::CoincidentDatumPlnCmd, OsgGuiCommand)
public:
    CoincidentDatumPlnCmd();
    ~CoincidentDatumPlnCmd();

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;

protected:
    enum class Step
    {
        Undefined = 0,
        SelectDatumPlaneOrFace = 1,
    };
    bool finishStep(Step step);
    void gotoStep(Step step);

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseUp(const MouseEvent& event) override;

private:
    Step _step;
    wy3d::Plane _plane;

    // 点选选项
    PointPickOption _pointPickOption;
    // 预览
    SelectPreview _preview;
    // 创建基准面
    std::shared_ptr<MakeDatumPlane> _pMakeDatumPlane;
};

}

#endif // WY3DAPP_COINCIDENT_DATUM_PLN_CMD_H
