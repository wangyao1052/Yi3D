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

#ifndef WY3DAPP_OFFSET_SHEET_GUI_CMD_H
#define WY3DAPP_OFFSET_SHEET_GUI_CMD_H

#include <memory>
#include "commands/OsgGuiCommand.h"
#include "commands/GuiCmdMakeElement.h"
#include "select/SelectPreview.h"

class OffsetSheetCmdPanel;
namespace wy3d { class OffsetSheet; }

class MakeOffsetSheet : public GuiCmdMakeElement
{
public:
    MakeOffsetSheet(GuiCommand* pGuiCmd)
        : GuiCmdMakeElement(pGuiCmd), _pOffsetSheet(nullptr) {}
    ~MakeOffsetSheet() {}

    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

    bool init(const wydb::ElementId& sheetId, unsigned int& errorCode);
    bool update(double offset);

private:
    wy3d::OffsetSheet* _pOffsetSheet;
};

class OffsetSheetGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(OffsetSheetGuiCmd, wy3dApp::OffsetSheetGuiCmd, OsgGuiCommand)
public:
    OffsetSheetGuiCmd();
    virtual ~OffsetSheetGuiCmd();

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void cleanup() override;

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseUp(const MouseEvent& event) override;
    virtual void onFeatureTreeItemClicked(const wydb::ElementId& id) override;

private:
    bool isValidSheetSelectionSet(const wyap::SelectionSet& ss, wydb::ElementId& sheetId);
    bool isValidSheet(const wydb::ElementId& sheetId);

    bool createCmdPanel();
    void destroyCmdPanel();
    void onSheetSelected();
    void onDialogOffsetChanged(double value);
    void onDialogAccepted();
    void onDialogCanceled();

protected:
    wydb::ElementId _sheetId;
    PointPickOption _pointPickOption;
    std::shared_ptr<MakeOffsetSheet> _pMakeOffsetSheet;
    SelectPreviewSPtr _pSheetPreview;
    OffsetSheetCmdPanel* _pCmdPanel;
    double _offset;
};

#endif // WY3DAPP_OFFSET_SHEET_GUI_CMD_H
