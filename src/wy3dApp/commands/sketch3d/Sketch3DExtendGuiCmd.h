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

#ifndef WY3DAPP_SKETCH3D_EXTEND_GUI_CMD_H
#define WY3DAPP_SKETCH3D_EXTEND_GUI_CMD_H

#include <memory>

#include <utils/wy3dSketch3DExtendGraph.h>
#include <wydbDatabase.h>
#include <wydbElementId.h>
#include <wydbTransaction.h>
#include <wy3dSketch3D.h>
#include <wy3dSketchArc3D.h>
#include <wy3dSketchCurve3D.h>
#include <wy3dSketchEllipseArc3D.h>
#include <wy3dSketchLine3D.h>
#include <wy3dSketchSpline3D.h>

#include "commands/OsgGuiCommand.h"
#include "commands/transient/Sketch3DTrimExtendTransient.h"
#include "utils/GuiCommandUtil.h"

// Grows the end of a 3D sketch curve that the cursor points at, out to the nearest crossing its own
// extension would meet.
class Sketch3DExtendGuiCmd : public OsgGuiCommand, public wydb::DatabaseReactor
{
    WYRX_DECLARE_MEMBERS(Sketch3DExtendGuiCmd, Sketch3DExtendGuiCmd, OsgGuiCommand)
public:
    Sketch3DExtendGuiCmd();
    virtual ~Sketch3DExtendGuiCmd();

    virtual void onDatabaseChanged(
        const wydb::Database* pDatabase,
        const wydb::Transaction* pTransaction,
        const wydb::DatabaseChangeInfo& changeInfo) override;

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseUp(const MouseEvent& event) override;

private:
    // Runs the pick and keeps the preview in step with it.
    void pickExtendSegment(const wydb::ElementId& id, const wy::Vector3& pickPos3d);

    bool extend(const wydb::ElementId curveId,
        const wy3d::Sketch3DExtendKnot& startKnot, const wy3d::Sketch3DExtendKnot& endKnot);

    bool initExtendGraph();

private:
    GuiCmdSketch3DInfo _sketch3DInfo;
    std::unique_ptr<wy3d::Sketch3DExtendGraph> _pExtendGraph;
    // The stretch the cursor is on, kept so that moving inside one stretch does not rebuild the
    // preview and so that a click knows what to grow.
    Sketch3DTrimExtendTransientSPtr _pTransient;
    wydb::ElementId _pickedId;
    wy3d::Sketch3DExtendSegment _pickedSegment;

    PointPickOption _pointPickOption;
};

#endif // WY3DAPP_SKETCH3D_EXTEND_GUI_CMD_H
