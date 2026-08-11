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

#ifndef WY3DAPP_THICKEN_GUI_CMD_H
#define WY3DAPP_THICKEN_GUI_CMD_H

#include <memory>
#include "commands/OsgGuiCommand.h"
#include "commands/GuiCmdMakeElement.h"
#include "select/SelectPreview.h"

class ThickenCmdPanel;
namespace wy3d { class Thicken; }

class MakeThicken : public GuiCmdMakeElement
{
public:
    MakeThicken(GuiCommand* pGuiCmd)
        : GuiCmdMakeElement(pGuiCmd), _pThicken(nullptr) {}
    ~MakeThicken() {}

    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

    bool init(const wydb::ElementId& sheetId, unsigned int& errorCode);
    bool update(double thickness, int direction);

private:
    wy3d::Thicken* _pThicken;
};

class ThickenGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(ThickenGuiCmd, wy3dApp::ThickenGuiCmd, OsgGuiCommand)
public:
    ThickenGuiCmd();
    virtual ~ThickenGuiCmd();

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;

protected:
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
    void onDialogThicknessChanged(double value);
    void onDialogDirectionChanged(int direction);
    void onDialogAccepted();
    void onDialogCanceled();

protected:
    wydb::ElementId _sheetId;
    PointPickOption _pointPickOption;

    std::shared_ptr<MakeThicken> _pMakeThicken;
    SelectPreviewSPtr _pSheetPreview;
    ThickenCmdPanel* _pCmdPanel;
    double _thickness;
    int _direction;
};

#endif // WY3DAPP_THICKEN_GUI_CMD_H
