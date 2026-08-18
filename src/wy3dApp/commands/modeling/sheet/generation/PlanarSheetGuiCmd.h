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

#ifndef WY3DAPP_PLANAR_SHEET_GUI_CMD_H
#define WY3DAPP_PLANAR_SHEET_GUI_CMD_H

#include "commands/OsgGuiCommand.h"
#include "commands/GuiCmdMakeElement.h"
#include "select/SelectionSetHighlightor.h"
#include "select/SelectPreview.h"
#include <map>
#include <vector>
#include <TopoDS_Edge.hxx>
#include <wyVector3.h>
#include <wy3dVector3.h>
#include <wy3dPlanarSheet.h>
#include <wy3dNonParametricSheet.h>
#include <wy3dSketch.h>
#include "commands/transient/ValidSketchTransient.h"

class MakePlanarSheet : public GuiCmdMakeElement
{
public:
    MakePlanarSheet(GuiCommand* pGuiCmd)
        : GuiCmdMakeElement(pGuiCmd), _pPlanarSheet(nullptr), _workPlnNormal(0.0, 0.0, 1.0) {}
    ~MakePlanarSheet() {}

    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

    virtual wy::Vector3 getWorkingPlaneNormal() const override
    {
        return _workPlnNormal;
    }

    bool init(const wydb::ElementId& sketchId, unsigned int& errorCode);

private:
    wy3d::PlanarSheet* _pPlanarSheet;
    wy::Vector3 _workPlnNormal;
};

class MakeNonParametricSheet : public GuiCmdMakeElement
{
public:
    MakeNonParametricSheet(GuiCommand* pGuiCmd)
        : GuiCmdMakeElement(pGuiCmd), _pNonParametricSheet(nullptr) {}
    ~MakeNonParametricSheet() {}

    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

    bool init(const TopoDS_Shape& shape, unsigned int& errorCode);

private:
    wy3d::NonParametricSheet* _pNonParametricSheet;
};

class PlanarSheetGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(PlanarSheetGuiCmd, wy3dApp::PlanarSheetGuiCmd, OsgGuiCommand)
public:
    PlanarSheetGuiCmd();
    virtual ~PlanarSheetGuiCmd();

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;

protected:
    enum class Step
    {
        Undefined = 0,
        SelectSketch = 1,
        SelectEdges = 2,
    };
    virtual void cleanup() override;
    virtual void reset();
    virtual bool finishStep(Step step);
    virtual void gotoStep(Step step);

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseDown(const MouseEvent& event) override;
    virtual void onLeftMouseUp(const MouseEvent& event) override;
    virtual void onFeatureTreeItemClicked(const wydb::ElementId& id) override;

    virtual void onEscapeKey() override;
    virtual bool isContextMenuActionVisible_CompleteSelection() const override;
    virtual void onContextMenuAction_CompleteSelection() override;
    virtual bool isContextMenuActionVisible_ClearSelection() const override;
    virtual void onContextMenuAction_ClearSelection() override;

private:
    bool isValidSketchSelectionSet(const wyap::SelectionSet& ss, wydb::ElementId& sketchId);
    bool isValidSketch(const wydb::ElementId& sketchId, QString& error);
    void preview(wydb::ElementId sketchId);

    // 从已选边提取 TopoDS_Edge 序列（支持跨实体）
    bool collectPickedEdges(std::vector<TopoDS_Edge>& edges) const;
    // 每次选择变更后校验: 闭合+共面 → 自动创建
    void tryAutoFinishEdgeSelection();
    // 右键 Complete: 强制围合
    bool completeEdgeSelection(unsigned int& errorCode);

protected:
    Step _step;
    wydb::ElementId _sketchId;

    PointPickOption _sketchPickOption;
    PointPickOption _edgePickOption;

    std::shared_ptr<ValidSketchTransient> _pValidSketchPreview;
    std::shared_ptr<InvalidSketchToolTip> _pInvalidSketchTooltip;

    SelectPreviewSPtr _pEdgePreview;
    SelectionSetHighlightorSPtr _pSelSetHighlightor;

    struct SketchValidInfo
    {
        bool valid;
        QString error;

        SketchValidInfo() : valid(true) {}
    };
    std::map<wydb::ElementId, SketchValidInfo> _sketchId2ValidInfo;

    std::shared_ptr<MakePlanarSheet> _pMakePlanarSheet;
    std::shared_ptr<MakeNonParametricSheet> _pMakeNonParametricSheet;
};

#endif // WY3DAPP_PLANAR_SHEET_GUI_CMD_H
