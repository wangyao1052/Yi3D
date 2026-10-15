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

#ifndef WY3DAPP_LOFT_GUI_CMD_H
#define WY3DAPP_LOFT_GUI_CMD_H

#include <vector>
#include <map>
#include <wy3dLoft.h>

#include <wy3dApp/OsgGuiCommand.h>
#include "commands/transient/ValidSketchTransient.h"
#include "select/SelectionSetHighlightor.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class MakeLoft;

class LoftGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(LoftGuiCmd, wy3dApp::LoftGuiCmd, OsgGuiCommand)
public:
    LoftGuiCmd();

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;

protected:
    virtual void cleanUp() override;

    enum class Step
    {
        Undefined = 0,
        SelectProfiles    = 1,
        SpecifySolidToCut = 2,
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
    virtual void configureContextMenu(ContextMenu& menu) override;
    virtual void onCompleteSelectionMenuItem() override;
    virtual void onClearSelectionMenuItem() override;

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
    PointPickOptions _pointPickOption;

    // 预览&提示
    SelectPreview _profilePreview;
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

    // 创建放样体
    std::shared_ptr<MakeLoft> _pMakeLoft;
};

class LoftCutGuiCmd : public LoftGuiCmd
{
    WYRX_DECLARE_MEMBERS(LoftCutGuiCmd, wy3dApp::LoftCutGuiCmd, LoftGuiCmd)
public:
    virtual void cleanUp() override;
    virtual bool finishStep(Step step) override;
    virtual void gotoStep(Step step) override;

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseUp(const MouseEvent& event) override;

private:
    // 获取用户选择的要切除的实体
    const wy3d::Solid* getSolidToCut() const;

private:
    // 预览
    SelectPreview _solidToCutPreview;
};

class MakeLoft : public GuiCmdMakeElement
{
public:
    MakeLoft(GuiCommand* pGuiCmd, bool isCut)
        : GuiCmdMakeElement(pGuiCmd), _isCut(isCut), _pLoft(nullptr) {}

    // 收集创建的元素ID
    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

    // 创建
    bool create(const std::vector<wydb::ElementId>& profileIds, unsigned int& errorCode);
    // 切除实体
    bool cutSolid(const wy3d::Solid* pConstSolidToCut, unsigned int& errorCode);

private:
    bool _isCut;
    wy3d::Loft* _pLoft;
};

} // namespace wy3dApp

#endif // WY3DAPP_LOFT_GUI_CMD_H
