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

#ifndef WY3DAPP_SKETCH3D_FILLET_GUI_CMD_H
#define WY3DAPP_SKETCH3D_FILLET_GUI_CMD_H

#include <memory>

#include <utils/wy3dSketch3DFilletAlgo.h>
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

#include "Sketch3DCurveRangeUtil.h"
#include "commands/OsgGuiCommand.h"
#include "commands/transient/Sketch3DCurveTransient.h"
#include "commands/transient/Sketch3DTrimExtendTransient.h"
#include "utils/GuiCommandUtil.h"

// What the two picks came to, by element: the algorithm's answer knows the geometry but not which
// entities it belongs to, and the command is the only layer that does.
struct Fillet3DData
{
    wydb::ElementId id1st;
    wydb::ElementId id2nd;

    wy3d::Sketch3DFilletData fillet;

    // Compared field by field rather than wholesale, because the frame has no equality of its own
    // and does not need one: the two curves decide it, and those are already in the comparison.
    bool operator==(const Fillet3DData& rhs) const
    {
        return id1st == rhs.id1st && id2nd == rhs.id2nd
            && fillet.startParam1st == rhs.fillet.startParam1st
            && fillet.endParam1st == rhs.fillet.endParam1st
            && fillet.startParam2nd == rhs.fillet.startParam2nd
            && fillet.endParam2nd == rhs.fillet.endParam2nd
            && fillet.filletCenter == rhs.fillet.filletCenter
            && fillet.filletRadius == rhs.fillet.filletRadius
            && fillet.filletStartAngle == rhs.fillet.filletStartAngle
            && fillet.filletEndAngle == rhs.fillet.filletEndAngle;
    }

    bool operator!=(const Fillet3DData& rhs) const
    {
        return !operator==(rhs);
    }
};

// Rounds the corner between two coplanar 3D sketch curves off with an arc.
//
// The pair is picked one curve per step, as the 2D command picks it, and the pair has to be
// coplanar for there to be a fillet at all - the curve under the cursor is then refused rather than
// quietly solved in some other plane. Whether a fillet of the given radius fits is the algorithm's
// answer, and it is taken as it comes.
class Sketch3DFilletGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(Sketch3DFilletGuiCmd, Sketch3DFilletGuiCmd, OsgGuiCommand)
public:
    Sketch3DFilletGuiCmd();
    virtual ~Sketch3DFilletGuiCmd();

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
    // Drops the second curve and the fillet, keeping the first curve picked.
    void clearSecondPreview();

    bool fillet(const Fillet3DData* pFilletData);

private:
    GuiCmdSketch3DInfo _sketch3DInfo;
    Step _step;
    // Radius, remembered across runs the way the 2D command remembers it.
    static double _R;

    PointPickOption _pointPickOption;

    // first curve
    wydb::ElementId _id1st;
    wy::Vector3 _pickPos1st;
    Sketch3DTrimExtendTransientSPtr _pCurveTransient1st;

    // second curve
    wydb::ElementId _id2nd;
    wy::Vector3 _pickPos2nd;
    Sketch3DTrimExtendTransientSPtr _pCurveTransient2nd;

    // fillet
    Sketch3DCurveTransientSPtr _pFilletTransient;
    std::shared_ptr<Fillet3DData> _pFilletData;
};

#endif // WY3DAPP_SKETCH3D_FILLET_GUI_CMD_H
