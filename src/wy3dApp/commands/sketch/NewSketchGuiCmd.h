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

#ifndef WY3DAPP_NEW_SKETCH_GUI_CMD_H
#define WY3DAPP_NEW_SKETCH_GUI_CMD_H

#include <wyapSelManager.h>
#include <wy3dPlane.h>

#include <wy3dApp/OsgGuiCommand.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class NewSketchGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(NewSketchGuiCmd, wy3dApp::NewSketchGuiCmd, OsgGuiCommand)
public:
    NewSketchGuiCmd();
    ~NewSketchGuiCmd();

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;
    virtual void cleanup() override;

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
    virtual void onFeatureTreeItemClicked(const wydb::ElementId& id) override;

private:
    bool perform(const wyap::Selection& sel);

private:
    Step _step;
    wy3d::Plane _plane;

    // 点选选项
    PointPickOption _pointPickOption;
    // 预览
    SelectPreview _preview;
};

}

#endif // WY3DAPP_NEW_SKETCH_GUI_CMD_H
