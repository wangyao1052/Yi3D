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

#ifndef WY3DAPP_HELIX_GUI_CMD_H
#define WY3DAPP_HELIX_GUI_CMD_H

#include <wy3dApp/OsgGuiCommand.h>
#include <map>
#include <memory>
#include <wy3dHelix.h>
#include "commands/transient/ValidSketchTransient.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class HelixGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(HelixGuiCmd, wy3dApp::HelixGuiCmd, OsgGuiCommand)
public:
    HelixGuiCmd();
    virtual ~HelixGuiCmd();

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;

protected:
    enum class Step
    {
        Undefined = 0,
        SelectSketch = 1,
        InputHelixData = 2,
    };
    virtual void reset();
    virtual bool finishStep(Step step);
    virtual void gotoStep(Step step);

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseUp(const MouseEvent& event) override;
    virtual void onFeatureTreeItemClicked(const wydb::ElementId& id) override;

private:
    bool isValidSketchSelectionSet(const wyap::SelectionSet& ss, wydb::ElementId& sketchId);
    bool isValidSketch(const wydb::ElementId& sketchId, QString& error);
    void preview(wydb::ElementId sketchId);

    bool createHelix(
        const wydb::ElementId& sketchId,
        double pitch,
        double turns,
        double startAngle,
        bool isClockWise,
        unsigned int& errorCode);

protected:
    Step _step;
    wydb::ElementId _sketchId;

    // 点选选项
    PointPickOption _pointPickOption;

    // 预览&提示
    std::shared_ptr<ValidSketchTransient> _pValidSketch;
    std::shared_ptr<InvalidSketchToolTip> _pInvalidSketchTooltip;

    // 草图信息
    struct SketchValidInfo
    {
        bool valid;
        QString error;

        SketchValidInfo() : valid(true) {}
    };
    std::map<wydb::ElementId, SketchValidInfo> _sketchId2ValidInfo;
};

}

#endif // WY3DAPP_HELIX_GUI_CMD_H
