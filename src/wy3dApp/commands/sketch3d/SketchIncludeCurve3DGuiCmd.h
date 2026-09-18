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

#ifndef WY3DAPP_SKETCH_INCLUDE_CURVE3D_GUI_CMD_H
#define WY3DAPP_SKETCH_INCLUDE_CURVE3D_GUI_CMD_H

#include "commands/OsgGuiCommand.h"
#include "select/SelectPreview.h"
#include "utils/GuiCommandUtil.h"
#include <map>
#include <string>
#include <utility>
#include <wydbElementId.h>
#include <utils/wy3dSketch3DEdgeUtil.h>

// Takes a curve that already exists somewhere in the model (an edge of a solid or sheet,
// a curve of a 2D sketch, a curve of another 3D sketch) and copies it into the 3D sketch
// being edited. Snapshot semantics: no reference is kept, the entity does not follow
// later changes of the source.
class SketchIncludeCurve3DGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(SketchIncludeCurve3DGuiCmd, SketchIncludeCurve3DGuiCmd, OsgGuiCommand)

public:
    SketchIncludeCurve3DGuiCmd();
    virtual ~SketchIncludeCurve3DGuiCmd();

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;

protected:
    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseUp(const MouseEvent& event) override;

private:
    bool includeModelEdge(const wyap::Selection& sel);
    bool includeSketchCurve(const wyap::Selection& sel);
    bool includeSketchEntity3D(const wyap::Selection& sel);

    // Deduplication and transaction start. Returns nullptr when the curve was already
    // included, or when the transaction cannot be started.
    wydb::Transaction* beginInclude(const wyap::Selection& sel);
    // Appends the converted entity to the edited 3D sketch, ends the transaction, and
    // reports a conversion failure. One pick is one undo step.
    bool finishInclude(const wyap::Selection& sel, wydb::Transaction* pTrans,
        wy3d::Sketch3DEdgeUtil::Result result, wy3d::SketchEntity3D* pEntity);

    void reportFailure(wy3d::Sketch3DEdgeUtil::Result result) const;

private:
    GuiCmdSketch3DInfo _sketch3DInfo;
    PointPickOption _pointPickOption;
    SelectPreviewSPtr _pPreview;  // 悬停高亮预览
    // Included curves of this command run: element id + sub path -> entity id. A curve is
    // included only once, unless its entity has been undone in between.
    std::map<std::pair<wydb::ElementId, std::string>, wydb::ElementId> _includedCurves;
};

#endif // WY3DAPP_SKETCH_INCLUDE_CURVE3D_GUI_CMD_H
