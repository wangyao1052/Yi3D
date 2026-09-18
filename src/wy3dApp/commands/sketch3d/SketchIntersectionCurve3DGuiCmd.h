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

#ifndef WY3DAPP_SKETCH_INTERSECTION_CURVE3D_GUI_CMD_H
#define WY3DAPP_SKETCH_INTERSECTION_CURVE3D_GUI_CMD_H

#include "commands/OsgGuiCommand.h"
#include "select/SelectPreview.h"
#include "utils/GuiCommandUtil.h"
#include <wydbElementId.h>
#include <utils/wy3dSketch3DIntersectionUtil.h>
#include <utils/wy3dSketch3DEdgeUtil.h>

class SketchIntersectionCurvePanel;

// Draws sketch curves where two picked items meet. An item is a face of a solid or a sheet,
// or a datum plane. The first pick is held for the rest of the command and every pick after it
// is intersected with that one, the curve appearing as soon as the pick is made. The first item
// is never handed back: holding a different one means running the command again. Snapshot
// semantics: no reference to the sources is kept, the entities do not follow later changes of
// the model.
class SketchIntersectionCurve3DGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(SketchIntersectionCurve3DGuiCmd, SketchIntersectionCurve3DGuiCmd, OsgGuiCommand)

public:
    SketchIntersectionCurve3DGuiCmd();
    virtual ~SketchIntersectionCurve3DGuiCmd();

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;

protected:
    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseUp(const MouseEvent& event) override;

private:
    bool resolveSource(const wyap::Selection& sel, wy3d::Sketch3DIntersectionUtil::Source& source) const;

    // Holds the first item: keeps it highlighted until the command ends, and drops datum planes
    // from every later pick when it is itself a plane.
    void holdFirst(const wyap::Selection& sel, const wy3d::Sketch3DIntersectionUtil::Source& source);
    void setAllowPlane(bool allow);

    void applyPair(const wyap::Selection& second, const wy3d::Sketch3DIntersectionUtil::Source& source);

    // The interpolation / fit choice, offered on a floating panel for as long as the command
    // runs. A missing panel is not fatal: the mode then falls back to fit.
    void createPanel();
    void destroyPanel();

    void setStepOneTip() const;
    void setStepTwoTip(bool firstIsPlane) const;
    void reportFailure(wy3d::Sketch3DIntersectionUtil::Result result) const;
    void reportCreateFailure(wy3d::Sketch3DEdgeUtil::Result result) const;

private:
    GuiCmdSketch3DInfo _sketch3DInfo;
    PointPickOption _pointPickOption;
    SelectPreviewSPtr _pPreview;                 // 悬停高亮预览
    SelectionSetHighlightorSPtr _pHeldHighlight; // 已持有的第一项保持高亮
    SketchIntersectionCurvePanel* _pPanel;       // interpolation / fit switch, owned

    // The held first item, i.e. the anchor of the fan: picked once, then read at every later
    // pick for the rest of the command.
    bool _hasFirst;
    wyap::Selection _firstSelection;
    wy3d::Sketch3DIntersectionUtil::Source _firstSource;
};

#endif // WY3DAPP_SKETCH_INTERSECTION_CURVE3D_GUI_CMD_H
