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

#ifndef WY3DAPP_LOFTED_SHEET_GUI_CMD_H
#define WY3DAPP_LOFTED_SHEET_GUI_CMD_H

#include <vector>
#include <map>
#include <wy3dLoftedSheet.h>

#include "commands/OsgGuiCommand.h"
#include "commands/transient/ValidSketchTransient.h"
#include "select/SelectionSetHighlightor.h"

class MakeLoftedSheet;

class LoftedSheetGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(LoftedSheetGuiCmd, wy3dApp::LoftedSheetGuiCmd, OsgGuiCommand)
public:
    LoftedSheetGuiCmd();

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;

protected:
    virtual void cleanup() override;

    enum class Step
    {
        Undefined = 0,
        SelectProfiles = 1,
    };
    virtual bool finishStep(Step step);
    virtual void gotoStep(Step step);

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseUp(const MouseEvent& event) override;
    virtual void onFeatureTreeItemClicked(const wydb::ElementId& id) override;

    // Enter键响应
    virtual void onEnterKey() override;
    // Space键响应
    virtual void onSpaceKey() override;

    // 上下文菜单
    virtual bool isContextMenuActionVisible_CompleteSelection() const override;
    virtual void onContextMenuAction_CompleteSelection() override;
    virtual bool isContextMenuActionVisible_ClearSelection() const override;
    virtual void onContextMenuAction_ClearSelection() override;

protected:
    // 获取选择的轮廓草图
    static std::vector<wydb::ElementId> getSelectedProfiles(const SelectionSetHighlightor& profilesHighlightor);
    // 是否是有效的轮廓
    bool isValidSketchSelectionSet(const wyap::SelectionSet& ss, std::vector<wydb::ElementId>& profileIds);
    bool isValidProfile(const wydb::ElementId& profileId, QString& error);
    // 预览
    void preview(wydb::ElementId sketchId);

protected:
    Step _step;
    std::vector<wydb::ElementId> _profileIds;

    // 点选选项
    PointPickOption _pointPickOption;

    // 预览&提示
    SelectPreviewSPtr _pProfilePreview;
    std::shared_ptr<InvalidSketchToolTip> _pInvalidSketchTooltip;
    // 轮廓高亮对象
    SelectionSetHighlightor _profilesHighlightor;

    // 草图有效性信息
    struct SketchValidInfo
    {
        bool valid;
        QString error;

        SketchValidInfo() : valid(true) {}
    };
    std::map<wydb::ElementId, SketchValidInfo> _sketchId2ValidInfo;

    // 创建放样曲面
    std::shared_ptr<MakeLoftedSheet> _pMakeLoftedSheet;
};

class MakeLoftedSheet : public GuiCmdMakeElement
{
public:
    MakeLoftedSheet(GuiCommand* pGuiCmd)
        : GuiCmdMakeElement(pGuiCmd), _pLoftedSheet(nullptr) {}

    // 收集创建的元素ID
    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

    // 创建
    bool create(const std::vector<wydb::ElementId>& profileIds, unsigned int& errorCode);

private:
    wy3d::LoftedSheet* _pLoftedSheet;
};

#endif // WY3DAPP_LOFTED_SHEET_GUI_CMD_H
