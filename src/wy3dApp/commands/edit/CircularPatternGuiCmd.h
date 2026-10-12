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

#ifndef WY3DAPP_CIRCULAR_PATTERN_GUI_CMD_H
#define WY3DAPP_CIRCULAR_PATTERN_GUI_CMD_H

#include <wyVector3.h>
#include <wy3dApp/OsgGuiCommand.h>
#include <wy3dApp/SelectPreview.h>
#include "select/SelectionSetHighlightor.h"
#include "wy3dAppDefs.h"

namespace wy3dApp {

class CircularPatternGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(CircularPatternGuiCmd, wy3dApp::CircularPatternGuiCmd, OsgGuiCommand)
public:
    CircularPatternGuiCmd();
    virtual ~CircularPatternGuiCmd();

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;

protected:
    enum class Step
    {
        Undefined = 0,
        SelectSource = 1,
        SpecifyCylindricalSurface = 2,
        InputPatternData = 3,
    };
    bool finishStep(Step step);
    void gotoStep(Step step);

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseUp(const MouseEvent& event) override;

    // 单击特征树上的元素
    virtual void onFeatureTreeItemClicked(const wydb::ElementId& id) override;

private:
    // 圆周阵列
    bool createCircularPattern(
        double totalAngle, unsigned int instanceCount, bool isCCW,
        unsigned int& errorCode);

private:
    Step _step;
    wydb::ElementId _sourceId;
    wyap::Selection _cylindricalSurfaceSel;

    // 点选选项
    PointPickOption _pointPickOption;

    // 阵列源对象预览
    SelectPreview _sourcePreview;
    // 圆柱面预览
    SelectPreview _cylindricalSurfacePreview;
    // 圆柱面高亮
    SelectionSetHighlightorSPtr _pCylindricalSurfaceHighlightor;

    // 圆周阵列中心点
    wy::Vector3 _centerPoint;
    // 圆周阵列轴向
    wy::Vector3 _axisDirection;
};

}

#endif // WY3DAPP_CIRCULAR_PATTERN_GUI_CMD_H
