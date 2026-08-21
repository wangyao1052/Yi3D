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

#ifndef WY3DAPP_SWEPT_SHEET_GUI_CMD_H
#define WY3DAPP_SWEPT_SHEET_GUI_CMD_H

#include "commands/OsgGuiCommand.h"
#include <map>
#include <wy3dSweptSheet.h>
#include "commands/transient/ValidSketchTransient.h"
#include "select/SelectionSetHighlightor.h"

class MakeSweptSheet;

class SweptSheetGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(SweptSheetGuiCmd, wy3dApp::SweptSheetGuiCmd, OsgGuiCommand)
public:
    SweptSheetGuiCmd();
    virtual ~SweptSheetGuiCmd();

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;

protected:
    enum class Step
    {
        Undefined = 0,
        SelectPath = 1,
        SelectProfile = 2,
    };
    virtual void cleanup();
    virtual bool finishStep(Step step);
    virtual void gotoStep(Step step);

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseUp(const MouseEvent& event) override;
    virtual void onFeatureTreeItemClicked(const wydb::ElementId& id) override;

private:
    using IsValidSketchFuncPtr = bool (SweptSheetGuiCmd::*)(const wydb::ElementId& pathId, QString& error);
    bool isValidSketchSelectionSet(const wyap::SelectionSet& ss,
        wydb::ElementId& pathId, wydb::ElementId& profileId);
    bool isValidPath(const wydb::ElementId& pathId, QString& error);
    bool isValidProfile(const wydb::ElementId& profileId, QString& error);

    void mouseMovePreview(
        double x, double y,
        std::shared_ptr<ValidSketchTransient>& pSketchPreview,
        IsValidSketchFuncPtr isValidSketchFunc,
        const wydb::ElementId& excludeId);
    void preview(wydb::ElementId sketchId,
        std::shared_ptr<ValidSketchTransient>& pSketchPreview,
        IsValidSketchFuncPtr isValidSketchFunc);

protected:
    Step _step;
    wydb::ElementId _pathId;
    wydb::ElementId _profileId;

    // 点选选项
    PointPickOption _pointPickOption;

    // 预览&提示
    std::shared_ptr<ValidSketchTransient> _pPathPreview;
    std::shared_ptr<ValidSketchTransient> _pProfilePreview;
    std::shared_ptr<InvalidSketchToolTip> _pInvalidSketchTooltip;

    // 路径临时高亮对象
    SelectionSetHighlightorSPtr _pPathHighlightor;

    // 草图信息
    struct SketchValidInfo
    {
        bool valid;
        QString error;

        SketchValidInfo() : valid(true) {}
    };
    std::map<wydb::ElementId, SketchValidInfo> _sketchId2ValidInfo;

    // 创建扫掠曲面
    std::shared_ptr<MakeSweptSheet> _pMakeSweptSheet;
};

class MakeSweptSheet : public GuiCmdMakeElement
{
public:
    MakeSweptSheet(GuiCommand* pGuiCmd)
        : GuiCmdMakeElement(pGuiCmd), _pSweptSheet(nullptr) {}
    ~MakeSweptSheet() {}

    // 收集创建的元素ID
    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

    // 创建
    bool create(const wydb::ElementId& pathId, const wydb::ElementId& profileId, unsigned int& errorCode);

private:
    wy3d::SweptSheet* _pSweptSheet;
};

#endif // WY3DAPP_SWEPT_SHEET_GUI_CMD_H
