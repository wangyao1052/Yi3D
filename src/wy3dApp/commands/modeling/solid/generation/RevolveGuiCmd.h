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

#ifndef WY3DAPP_REVOLUTION_GUI_CMD_H
#define WY3DAPP_REVOLUTION_GUI_CMD_H

#include "commands/OsgGuiCommand.h"
#include <map>
#include <memory>
#include <wy3dRevolution.h>
#include "commands/transient/ValidSketchTransient.h"

class MakeRevolution;

class RevolveGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(RevolveGuiCmd, wy3dApp::RevolveGuiCmd, OsgGuiCommand)
public:
    RevolveGuiCmd();
    virtual ~RevolveGuiCmd();

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;

protected:
    enum class Step
    {
        Undefined = 0,
        SelectSketch = 1,
        SelectAxisCurve = 2,
        SpecifySolidToCut = 3,
    };
    virtual void cleanup() override;
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

    // 点选选项
    PointPickOption _pointPickOption;

    // 预览&提示
    std::shared_ptr<ValidSketchTransient> _pValidSketch;
    std::shared_ptr<InvalidSketchToolTip> _pInvalidSketchTooltip;
    SelectPreviewSPtr _pAxisCurvePreview;

    // 草图信息
    struct SketchValidInfo
    {
        bool valid;
        QString error;

        SketchValidInfo() : valid(true) {}
    };
    std::map<wydb::ElementId, SketchValidInfo> _sketchId2ValidInfo;

    // 创建旋转体
    std::shared_ptr<MakeRevolution> _pMakeRevolution;
};

class RevolveCutGuiCmd : public RevolveGuiCmd
{
    WYRX_DECLARE_MEMBERS(RevolveCutGuiCmd, wy3dApp::RevolveCutGuiCmd, RevolveGuiCmd)
public:
    virtual void reset();
    virtual bool finishStep(Step step) override;
    virtual void gotoStep(Step step) override;

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseUp(const MouseEvent& event) override;

private:
    // 获取用户选择的要切除的实体
    const wy3d::Solid* getSolidToCut() const;

private:
    // 预览
    SelectPreviewSPtr _pSolidToCutPreview;
};

class MakeRevolution : public GuiCmdMakeElement
{
public:
    MakeRevolution(GuiCommand* pGuiCmd, bool isCut)
        : GuiCmdMakeElement(pGuiCmd), _isCut(isCut), _pRevolution(nullptr) {}
    ~MakeRevolution() {}

    // 收集创建的元素ID
    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

    // 创建
    bool create(const wydb::ElementId& sketchId, const wydb::ElementId& axisCurveId, unsigned int& errorCode);
    // 切除实体
    bool cutSolid(const wy3d::Solid* pConstSolidToCut, unsigned int& errorCode);

private:
    bool _isCut;
    wy3d::Revolution* _pRevolution;
};

#endif // WY3DAPP_REVOLUTION_GUI_CMD_H
