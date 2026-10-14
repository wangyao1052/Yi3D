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

#include <wy3dApp/OsgGuiCommand.h>
#include "commands/GuiCmdMakeElement.h"
#include "commands/modeling/sheet/generation/MakeNonParametricSheet.h"
#include "commands/transient/ValidSketchTransient.h"
#include "select/SelectionSetHighlightor.h"
#include <wy3dApp/SelectPreview.h>
#include <map>
#include <memory>
#include <vector>
#include <TopoDS_Edge.hxx>
#include <wy3dFilledSheet.h>
#include <wy3dSketch.h>
#include <wy3dSketch3D.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class MakeFilledSheet : public GuiCmdMakeElement
{
public:
    MakeFilledSheet(GuiCommand* pGuiCmd)
        : GuiCmdMakeElement(pGuiCmd), _pFilledSheet(nullptr) {}
    ~MakeFilledSheet() {}

    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

    bool init(const wydb::ElementId& sketchId, unsigned int& errorCode);

    // Any number of constraint sketches, one at a time. Each change runs in its own
    // sub-transaction merged back into the command's transaction group, so the whole command
    // stays a single undo step.
    // A non-zero errorCode means the change made the surface unbuildable and it has been rolled back
    bool addConstraintSketch(const wydb::ElementId& constraintSketchId, unsigned int& errorCode);
    bool removeConstraintSketch(const wydb::ElementId& constraintSketchId, unsigned int& errorCode);
    bool clearConstraintSketches(unsigned int& errorCode);

    const wy3d::FilledSheet* getFilledSheet() const { return _pFilledSheet; }

private:
    bool changeConstraintSketch(const wydb::ElementId& constraintSketchId, bool add);
    unsigned int getChainUpdateErrorCode() const;

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
        SelectEdges = 2,
        SelectConstraintCurves = 3,
    };
    virtual void cleanUp() override;
    virtual bool finishStep(Step step);
    virtual void gotoStep(Step step);

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseUp(const MouseEvent& event) override;
    virtual void onFeatureTreeItemClicked(const wydb::ElementId& id) override;

    virtual void onEscapeKey() override;
    virtual void onEnterKey() override;
    virtual void onSpaceKey() override;
    virtual void configureContextMenu(ContextMenu& menu) override;
    virtual void onCompleteSelection() override;
    virtual void onClearSelection() override;

private:
    bool collectPickedSketches(const wyap::SelectionSet& ss, std::vector<wydb::ElementId>& ids) const;
    bool isValidBoundarySketch(const wydb::ElementId& sketchId, QString& error);
    void preview(wydb::ElementId sketchId);

    bool isValidConstraintSketch(const wydb::ElementId& constraintSketchId, QString& error);
    bool isConstraintSketchValid(const wydb::ElementId& constraintSketchId);
    // The constraint sketches live on the feature; these only read them
    std::vector<wydb::ElementId> getConstraintSketchIds() const;
    bool hasConstraintSketch() const;
    bool isConstraintSketch(const wydb::ElementId& constraintSketchId) const;
    void toggleConstraintSketch(const wydb::ElementId& constraintSketchId);
    void rebuildHighlights();
    void activateConstraintSketches();
    void releaseConstraintSketches();

    // Extract the picked edges of Solid/Sheet elements
    bool collectPickedEdges(std::vector<TopoDS_Edge>& edges) const;

protected:
    Step _step;
    wydb::ElementId _sketchId;

    PointPickOption _sketchPickOption;
    PointPickOption _edgePickOption;
    PointPickOption _constraintSketchPickOption;

    std::shared_ptr<ValidSketchTransient> _pValidSketchPreview;
    std::shared_ptr<InvalidSketchToolTip> _pInvalidSketchTooltip;

    SelectPreview _edgePreview;
    SelectPreview _constraintSketchPreview;
    SelectionSetHighlightorSPtr _pSelSetHighlightor;

    // Preselected constraint sketches, only recognizable once the command reaches SelectConstraintCurves
    std::vector<wydb::ElementId> _preselectedConstraintSketchIds;
    // Constraint sketches this command has taken out of the Inactive state the scene gives them
    std::vector<wydb::ElementId> _activatedConstraintSketchIds;

    struct SketchValidInfo
    {
        bool valid;
        QString error;

        SketchValidInfo() : valid(true) {}
    };
    std::map<wydb::ElementId, SketchValidInfo> _sketchId2ValidInfo;
    // Constraint sketches are judged differently from boundaries, so they keep their own cache
    std::map<wydb::ElementId, SketchValidInfo> _constraintSketchId2ValidInfo;

    std::shared_ptr<MakeFilledSheet> _pMakeFilledSheet;
    std::shared_ptr<MakeNonParametricSheet> _pMakeNonParametricSheet;
};

} // namespace wy3dApp

#endif // WY3DAPP_FILLED_SHEET_GUI_CMD_H
