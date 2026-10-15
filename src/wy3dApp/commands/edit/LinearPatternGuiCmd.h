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

#ifndef WY3DAPP_LINEAR_PATTERN_GUI_CMD_H
#define WY3DAPP_LINEAR_PATTERN_GUI_CMD_H

#include <wy3dApp/OsgGuiCommand.h>
#include <wy3dApp/SelectPreview.h>
#include "select/SelectionSetHighlightor.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class LinearPatternGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(LinearPatternGuiCmd, wy3dApp::LinearPatternGuiCmd, OsgGuiCommand)
public:
    LinearPatternGuiCmd();
    virtual ~LinearPatternGuiCmd();

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;

protected:
    enum class Step
    {
        Undefined = 0,
        SelectSource = 1,
        SpecifyWorkingPlane = 2,
        InputPatternData = 3,
    };
    bool finishStep(Step step);
    void gotoStep(Step step);

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseUp(const MouseEvent& event) override;

    // 单击特征树上的元素
    virtual void onFeatureTreeItemClicked(const wydb::ElementId& id) override;

private:
    // 线性阵列
    bool createLinearPattern(
        unsigned int count1st, double spacing1st,
        unsigned int count2nd, double spacing2nd,
        unsigned int& errorCode);

private:
    Step _step;
    wydb::ElementId _sourceId;

    // 点选选项
    PointPickOptions _pointPickOption;

    // 预览
    SelectPreview _sourcePreview;

    // 工作平面
    wy3d::Plane _workPln;
    // 工作面预览
    SelectPreview _workPlnPreview;
};

} // namespace wy3dApp

#endif // WY3DAPP_LINEAR_PATTERN_GUI_CMD_H
