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

#ifndef WY3DAPP_FILLED_SHEET_GUI_CMD_H
#define WY3DAPP_FILLED_SHEET_GUI_CMD_H

#include "commands/OsgGuiCommand.h"
#include "commands/GuiCmdMakeElement.h"
#include "commands/transient/ValidSketchTransient.h"
#include <map>
#include <memory>
#include <wy3dFilledSheet.h>
#include <wy3dSketch.h>
#include <wy3dSketch3D.h>

class MakeFilledSheet : public GuiCmdMakeElement
{
public:
    MakeFilledSheet(GuiCommand* pGuiCmd)
        : GuiCmdMakeElement(pGuiCmd), _pFilledSheet(nullptr) {}
    ~MakeFilledSheet() {}

    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

    bool init(const wydb::ElementId& sketchId, unsigned int& errorCode);

private:
    wy3d::FilledSheet* _pFilledSheet;
};

class FilledSheetGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(FilledSheetGuiCmd, wy3dApp::FilledSheetGuiCmd, OsgGuiCommand)
public:
    FilledSheetGuiCmd();
    virtual ~FilledSheetGuiCmd();

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;

protected:
    enum class Step
    {
        Undefined = 0,
        SelectSketch = 1,
    };
    virtual void cleanup() override;
    virtual bool finishStep(Step step);
    virtual void gotoStep(Step step);

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseUp(const MouseEvent& event) override;
    virtual void onFeatureTreeItemClicked(const wydb::ElementId& id) override;

private:
    bool isValidSketchSelectionSet(const wyap::SelectionSet& ss, wydb::ElementId& sketchId);
    bool isValidBoundarySketch(const wydb::ElementId& sketchId, QString& error);
    void preview(wydb::ElementId sketchId);

protected:
    Step _step;
    wydb::ElementId _sketchId;

    PointPickOption _sketchPickOption;

    std::shared_ptr<ValidSketchTransient> _pValidSketchPreview;
    std::shared_ptr<InvalidSketchToolTip> _pInvalidSketchTooltip;

    struct SketchValidInfo
    {
        bool valid;
        QString error;

        SketchValidInfo() : valid(true) {}
    };
    std::map<wydb::ElementId, SketchValidInfo> _sketchId2ValidInfo;

    std::shared_ptr<MakeFilledSheet> _pMakeFilledSheet;
};

#endif // WY3DAPP_FILLED_SHEET_GUI_CMD_H
