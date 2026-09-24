///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 Wang Yao <wangyao1052@163.com>
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

#ifndef WY3DAPP_SKETCH_PROJECT_CURVE3D_GUI_CMD_H
#define WY3DAPP_SKETCH_PROJECT_CURVE3D_GUI_CMD_H

#include <memory>
#include <vector>

#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>

#include <utils/wy3dSketch3DEdgeUtil.h>
#include <utils/wy3dSketch3DProjectionUtil.h>

#include "commands/OsgGuiCommand.h"
#include "select/SelectPreview.h"
#include "utils/GuiCommandUtil.h"

class SketchProjectCurvePanel;
class Sketch3DDirectionTransient;

// Projects a sketch onto a face or a datum plane: SolidWorks' projected curve, sketch-on-face mode.
// The source is a 2D sketch picked whole, the direction is that sketch's own plane normal, and the
// two flags are the ones IProjectionCurveFeatureData carries - reverse and bidirectional.
//
// The projection is only computed, never created, until Enter: the pick draws a preview and every
// change of a flag recomputes it, so the two-way switch can be tried out for free and Esc leaves
// nothing behind. One sketch and one target per run of the command.
//
// Snapshot semantics, like the intersection and include commands: no reference to the sources is
// kept, and the entities do not follow later changes of the sketch or the model.
class SketchProjectCurve3DGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(SketchProjectCurve3DGuiCmd, SketchProjectCurve3DGuiCmd, OsgGuiCommand)

public:
    SketchProjectCurve3DGuiCmd();
    virtual ~SketchProjectCurve3DGuiCmd();

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;

protected:
    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseUp(const MouseEvent& event) override;

    virtual void onEnterKey() override;
    virtual void onSpaceKey() override;
    virtual bool isContextMenuActionVisible_CompleteSelection() const override;
    virtual void onContextMenuAction_CompleteSelection() override;
    virtual bool isContextMenuActionVisible_ClearSelection() const override;
    virtual void onContextMenuAction_ClearSelection() override;

private:
    enum class Step
    {
        SelectSketch,
        SelectTarget,
    };

    // What the projection runs into. A face of a solid or a sheet is bounded, so the result is
    // trimmed to it; a datum plane stands for an unbounded plane, so the whole projection comes
    // back. Keeping the two apart here is what lets the core layer be handed either one.
    struct ProjectionTarget
    {
        bool isPlane = false;
        TopoDS_Face face;
        wy3d::SketchPlane plane;
    };

    // Rebuilt at every step change: what is worth picking in one step is not in the other.
    void setPickForStep(Step step);

    bool takeSourceSketch(const wyap::Selection& sel);
    bool takeTarget(const wyap::Selection& sel);

    // Takes the picked target back, which is the one thing a single target pick has to be able to
    // undo: the highlight goes, and with the preview gone Enter has nothing left to confirm.
    void clearTarget();

    // The arrow that says which way the sweep runs - the one piece of the feature the user cannot
    // read off anything else, since the direction is the sketch plane's own normal. Rebuilt
    // whenever the flags change, from the same two flags the projection is built from.
    void updateDirectionArrow();

    // The one and only place a preview is computed. A new target and a toggled flag run the same
    // code, so the two can never come to different answers about what the projection is.
    void recomputePreview();

    // Tells the panel whether there is a curve to confirm, which is also what enables its OK.
    void updatePanelState();

    // The transaction. The geometry is already in hand by then - what the user saw in the preview
    // is exactly what gets created.
    bool commit();

    // The projection switch, offered on a floating panel for as long as the command runs. A
    // missing panel is not fatal: the flags then fall back to the plain forward sweep.
    void createPanel();
    void destroyPanel();

    void setStepOneTip() const;
    void setStepTwoTip() const;
    void reportFailure(wy3d::Sketch3DProjectionUtil::Result result) const;
    void reportCreateFailure(wy3d::Sketch3DEdgeUtil::Result result) const;

private:
    GuiCmdSketch3DInfo _sketch3DInfo;
    PointPickOption _pointPickOption;
    SelectPreviewSPtr _pPreview;                 // 悬停高亮预览
    SelectionSetHighlightorSPtr _pSourceHighlight; // 已选中的源草图保持高亮
    SelectionSetHighlightorSPtr _pTargetHighlight; // 已选中的投影目标保持高亮
    SketchProjectCurvePanel* _pPanel;            // reverse / bidirectional switches, owned

    Step _step;
    // Read once when the sketch is picked and reused by every recompute: the sweep margin and the
    // source edges do not change when a flag does.
    wy3d::SketchPlane _sourcePlane;
    std::vector<TopoDS_Edge> _sourceEdges;
    wy::Vector3 _arrowOrigin; // middle of the source curves, where the direction arrow is planted

    // Says which way the sweep runs. Goes up with the source sketch and follows the flags.
    std::shared_ptr<Sketch3DDirectionTransient> _pDirectionArrow;

    bool _hasTarget;
    ProjectionTarget _target;

    // What Enter would create. Empty means there is nothing to confirm, which is also how a
    // failed recompute clears the way for another target to be picked.
    std::vector<TopoDS_Edge> _previewEdges;
    GuiCmdTransientSPtr _pProjectionPreview;
};

#endif // WY3DAPP_SKETCH_PROJECT_CURVE3D_GUI_CMD_H
