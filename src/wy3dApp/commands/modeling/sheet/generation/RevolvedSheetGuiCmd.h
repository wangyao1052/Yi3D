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

#ifndef WY3DAPP_REVOLVED_SHEET_GUI_CMD_H
#define WY3DAPP_REVOLVED_SHEET_GUI_CMD_H

#include <wy3dApp/OsgGuiCommand.h>
#include <map>
#include <memory>
#include <wy3dRevolvedSheet.h>
#include "commands/transient/ValidSketchTransient.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class MakeRevolvedSheet;

class RevolvedSheetGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(RevolvedSheetGuiCmd, wy3dApp::RevolvedSheetGuiCmd, OsgGuiCommand)
public:
    RevolvedSheetGuiCmd();
    virtual ~RevolvedSheetGuiCmd();

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;

    virtual void cleanUp() override;

protected:
    enum class Step
    {
        Undefined = 0,
        SelectSketch = 1,
        SelectAxisCurve = 2,
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

protected:
    Step _step;
    wydb::ElementId _sketchId;
    wydb::ElementId _axisCurveId;

    PointPickOptions _pointPickOption;

    std::shared_ptr<ValidSketchTransient> _pValidSketch;
    std::shared_ptr<InvalidSketchToolTip> _pInvalidSketchTooltip;
    SelectPreview _axisCurvePreview;

    struct SketchValidInfo
    {
        bool valid;
        QString error;

        SketchValidInfo() : valid(true) {}
    };
    std::map<wydb::ElementId, SketchValidInfo> _sketchId2ValidInfo;

    std::shared_ptr<MakeRevolvedSheet> _pMakeRevolvedSheet;
};

class MakeRevolvedSheet : public GuiCmdMakeElement
{
public:
    MakeRevolvedSheet(GuiCommand* pGuiCmd)
        : GuiCmdMakeElement(pGuiCmd), _pRevolvedSheet(nullptr) {}
    ~MakeRevolvedSheet() {}

    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

    bool create(const wydb::ElementId& sketchId, const wydb::ElementId& axisCurveId, unsigned int& errorCode);

private:
    wy3d::RevolvedSheet* _pRevolvedSheet;
};

} // namespace wy3dApp

#endif // WY3DAPP_REVOLVED_SHEET_GUI_CMD_H
