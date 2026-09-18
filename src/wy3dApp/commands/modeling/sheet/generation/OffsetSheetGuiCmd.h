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

#include <cstdint>
#include <memory>
#include <set>
#include <vector>
#include <wydbElementId.h>
#include "commands/OsgGuiCommand.h"
#include "commands/GuiCmdMakeElement.h"
#include "select/SelectPreview.h"
#include "select/SelectionSetHighlightor.h"

class OffsetSheetCmdPanel;
namespace wy3d { class OffsetSheet; }

class OffsetSheetGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(OffsetSheetGuiCmd, wy3dApp::OffsetSheetGuiCmd, OsgGuiCommand)
public:
    // 宿主种类: 由第一个选中的面决定; 片体模式下是哪个片体也就定死了
    enum class HostKind
    {
        Undefined = 0,
        Sheet     = 1,
        Solid     = 2,
    };

    OffsetSheetGuiCmd();
    virtual ~OffsetSheetGuiCmd();

protected:
    enum class Step
    {
        Undefined = 0,
        SetParameters = 1,
    };

    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void cleanup() override;

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseUp(const MouseEvent& event) override;

    // 上下文菜单
    virtual bool isContextMenuActionVisible_ClearSelection() const override;
    virtual void onContextMenuAction_ClearSelection() override;

private:
    class MakeOffsetSheet;
    class MakeSolidFaceOffsetSheet;

    bool isValidSheetSelectionSet(const wyap::SelectionSet& ss, wydb::ElementId& sheetId);
    bool isValidSheet(const wydb::ElementId& sheetId);

    bool createCmdPanel();
    void destroyCmdPanel();
    // 预选了一个顶层片体: 整个片体偏置
    bool startWithWholeSheet(const wydb::ElementId& sheetId);
    // 预选的面当已选面用: 第一个面定种类, 与它不一致的丢掉
    void adoptPreSelectedFaces(const wyap::SelectionSet& ss);
    // 一个面所属的宿主种类(既不是实体也不是片体时返回 Undefined)
    HostKind hostKindOf(const wyap::Selection& sel, wydb::ElementId& hostId);
    // 由已选面定宿主并把预览建起来
    bool resolveHostFromPickedFaces(const wyap::Selection& firstSel, unsigned int& errorCode);
    void gotoStep(Step step);
    void updatePrompt();
    void updatePickOption();
    bool applyTarget();
    void clearFaceSelections();
    // 形体改动会重建宿主的渲染数据, 面上的高亮与新面预览色跟着没了, 每次改完要重新挂上
    void refreshFaceColors();
    void clearHostSelection();
    void onDialogOffsetChanged(double value);
    void onDialogAccepted();
    void onDialogCanceled();

protected:
    Step _step;
    HostKind _hostKind;
    wydb::ElementId _hostId;
    int _targetMode;

    // 实体面预览算不出来时的错误码, 确认时用
    unsigned int _previewErrorCode;

    // 已选的目标面
    wyap::SelectionSet _faceSels;

    // 点选选项
    PointPickOption _pointPickOption;
    std::shared_ptr<MakeOffsetSheet> _pMakeOffsetSheet;
    std::shared_ptr<MakeSolidFaceOffsetSheet> _pMakeSolidSheet;
    // 预览
    SelectPreviewSPtr _pPreview;
    // 高亮
    SelectionSetHighlightorSPtr _pSelSetHighlightor_Faces;
    // 整片体入口的目标面(面级高亮, 代替元素级)
    SelectionSetHighlightorSPtr _pSelSetHighlightor_TargetFaces;
    // 命令生成的新面(天蓝预览色), 随命令消失
    SelectionSetHighlightorSPtr _pSelSetHighlightor_NewFaces;
    OffsetSheetCmdPanel* _pCmdPanel;
    double _offset;
};

#endif // WY3DAPP_OFFSET_SHEET_GUI_CMD_H
