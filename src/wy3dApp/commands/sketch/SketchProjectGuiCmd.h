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

#ifndef WY3DAPP_SKETCH_PROJECT_GUI_CMD_H
#define WY3DAPP_SKETCH_PROJECT_GUI_CMD_H

#include <wy3dApp/OsgGuiCommand.h>
#include <wy3dApp/SelectPreview.h>
#include <memory>
#include <wydbElementId.h>
#include <wy3dPlane.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class SketchProjectGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(SketchProjectGuiCmd, wy3dApp::SketchProjectGuiCmd, OsgGuiCommand)

public:
    SketchProjectGuiCmd();
    virtual ~SketchProjectGuiCmd();

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;

protected:
    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseUp(const MouseEvent& event) override;

private:
    bool projectEdge(const wyap::Selection& sel);

    void showDegenerateWarning();
    void showNullCurveWarning();
    void showUnsupportedTypeWarning();

private:
    GuiCmdSketchInfo _sketchInfo;
    PointPickOption _pointPickOption;
    SelectPreview _preview;  // 悬停高亮预览
};

}

#endif // WY3DAPP_SKETCH_PROJECT_GUI_CMD_H
