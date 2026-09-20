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

#ifndef WY3DAPP_SKETCH3D_TRIM_GUI_CMD_H
#define WY3DAPP_SKETCH3D_TRIM_GUI_CMD_H

#include <map>
#include <memory>

#include <utils/wy3dSketch3DTrimGraph.h>
#include <wydbDatabase.h>
#include <wydbElementId.h>
#include <wydbTransaction.h>
#include <wy3dSketch3D.h>
#include <wy3dSketchArc3D.h>
#include <wy3dSketchCircle3D.h>
#include <wy3dSketchCurve3D.h>
#include <wy3dSketchEllipse3D.h>
#include <wy3dSketchEllipseArc3D.h>
#include <wy3dSketchLine3D.h>
#include <wy3dSketchSpline3D.h>

#include "commands/OsgGuiCommand.h"
#include "commands/transient/Sketch3DTrimExtendTransient.h"
#include "utils/GuiCommandUtil.h"

// Cuts the piece of a 3D sketch curve under the cursor away at the crossings on either side of it.
class Sketch3DTrimGuiCmd : public OsgGuiCommand, public wydb::DatabaseReactor
{
    WYRX_DECLARE_MEMBERS(Sketch3DTrimGuiCmd, Sketch3DTrimGuiCmd, OsgGuiCommand)
public:
    Sketch3DTrimGuiCmd();
    virtual ~Sketch3DTrimGuiCmd();

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
    void pickTrimSegment(const wydb::ElementId& id, const wy::Vector3& pickPos3d);

    bool trim(const wydb::ElementId curveId,
        const wy3d::Sketch3DTrimKnot& startKnot, const wy3d::Sketch3DTrimKnot& endKnot);

    bool trimLine(wydb::Transaction* pTrans, wy3d::Sketch3D* pSketch3D, wy3d::SketchLine3D* pLine,
        const wy3d::Sketch3DTrimKnot& startKnot, const wy3d::Sketch3DTrimKnot& endKnot);
    bool trimCircle(wydb::Transaction* pTrans, wy3d::Sketch3D* pSketch3D, wy3d::SketchCircle3D* pCircle,
        const wy3d::Sketch3DTrimKnot& startKnot, const wy3d::Sketch3DTrimKnot& endKnot);
    bool trimArc(wydb::Transaction* pTrans, wy3d::Sketch3D* pSketch3D, wy3d::SketchArc3D* pArc,
        const wy3d::Sketch3DTrimKnot& startKnot, const wy3d::Sketch3DTrimKnot& endKnot);
    bool trimEllipse(wydb::Transaction* pTrans, wy3d::Sketch3D* pSketch3D, wy3d::SketchEllipse3D* pEllipse,
        const wy3d::Sketch3DTrimKnot& startKnot, const wy3d::Sketch3DTrimKnot& endKnot);
    bool trimEllipseArc(wydb::Transaction* pTrans, wy3d::Sketch3D* pSketch3D,
        wy3d::SketchEllipseArc3D* pEllipseArc,
        const wy3d::Sketch3DTrimKnot& startKnot, const wy3d::Sketch3DTrimKnot& endKnot);
    bool trimSpline(wydb::Transaction* pTrans, wy3d::Sketch3D* pSketch3D, wy3d::SketchSpline3D* pSpline,
        const wy3d::Sketch3DTrimKnot& startKnot, const wy3d::Sketch3DTrimKnot& endKnot);

private:
    GuiCmdSketch3DInfo _sketch3DInfo;
    std::unique_ptr<wy3d::Sketch3DTrimGraph> _pTrimGraph;
    // The piece the cursor is on, kept so that moving inside one piece does not rebuild the preview
    // and so that a click knows what to cut.
    Sketch3DTrimExtendTransientSPtr _pTransient;
    wydb::ElementId _pickedId;
    wy3d::Sketch3DTrimSegment _pickedSegment;
    // The curve a piece was cut from, so the reactor can hang the piece's node off the original's
    // when an undo brings it back.
    std::map<wydb::ElementId, wydb::ElementId> _id2Parent;

    PointPickOption _pointPickOption;
};

#endif // WY3DAPP_SKETCH3D_TRIM_GUI_CMD_H
