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

#ifndef WY3DAPP_SKETCH3D_CHAMFER_GUI_CMD_H
#define WY3DAPP_SKETCH3D_CHAMFER_GUI_CMD_H

#include <memory>

#include <utils/wy3dSketch3DChamferAlgo.h>
#include <wydbDatabase.h>
#include <wydbElementId.h>
#include <wydbTransaction.h>
#include <wy3dSketch3D.h>
#include <wy3dSketchCurve3D.h>
#include <wy3dSketchLine3D.h>

#include "Sketch3DCurveRangeUtil.h"
#include "commands/OsgGuiCommand.h"
#include "commands/transient/BasicTransient.h"
#include "commands/transient/Sketch3DTrimExtendTransient.h"
#include "utils/GuiCommandUtil.h"

// What the two picks came to, by element: the algorithm's answer knows the geometry but not which
// entities it belongs to, and the command is the only layer that does.
struct Chamfer3DData
{
    wydb::ElementId id1st;
    wydb::ElementId id2nd;

    wy3d::Sketch3DChamferData chamfer;

    bool operator==(const Chamfer3DData& rhs) const
    {
        return id1st == rhs.id1st && id2nd == rhs.id2nd
            && chamfer.startParam1st == rhs.chamfer.startParam1st
            && chamfer.endParam1st == rhs.chamfer.endParam1st
            && chamfer.startParam2nd == rhs.chamfer.startParam2nd
            && chamfer.endParam2nd == rhs.chamfer.endParam2nd
            && chamfer.chamferStartPnt == rhs.chamfer.chamferStartPnt
            && chamfer.chamferEndPnt == rhs.chamfer.chamferEndPnt;
    }

    bool operator!=(const Chamfer3DData& rhs) const
    {
        return !operator==(rhs);
    }
};

// Cuts the corner between two straight 3D sketch curves off with a straight segment.
//
// Straight curves only, as in 2D: the distances are laid off along the two directions from the
// corner where their supports meet, which a curve with no single direction cannot offer.
class Sketch3DChamferGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(Sketch3DChamferGuiCmd, Sketch3DChamferGuiCmd, OsgGuiCommand)
public:
    Sketch3DChamferGuiCmd();
    virtual ~Sketch3DChamferGuiCmd();

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;

protected:
    enum class Step
    {
        First = 1,
        Second = 2,
    };

    void reset();
    void gotoStep(Step step);

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseUp(const MouseEvent& event) override;

private:
    void preview(const wydb::ElementId& id, const wy::Vector3& pickPos);
    // Drops the second curve and the chamfer, keeping the first curve picked.
    void clearSecondPreview();

    bool chamfer(const Chamfer3DData* pChamferData);

private:
    GuiCmdSketch3DInfo _sketch3DInfo;
    Step _step;
    // The two distances, remembered across runs the way the 2D command remembers them.
    static double _D1;
    static double _D2;

    PointPickOption _pointPickOption;

    // first curve
    wydb::ElementId _id1st;
    wy::Vector3 _pickPos1st;
    Sketch3DTrimExtendTransientSPtr _pCurveTransient1st;

    // second curve
    wydb::ElementId _id2nd;
    wy::Vector3 _pickPos2nd;
    Sketch3DTrimExtendTransientSPtr _pCurveTransient2nd;

    // chamfer
    LineTransientSPtr _pChamferTransient;
    std::shared_ptr<Chamfer3DData> _pChamferData;
};

#endif // WY3DAPP_SKETCH3D_CHAMFER_GUI_CMD_H
