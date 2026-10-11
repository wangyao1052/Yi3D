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

#ifndef WY3DAPP_SKETCH_CHAMFER_GUI_CMD_H
#define WY3DAPP_SKETCH_CHAMFER_GUI_CMD_H

#include <wy3dApp/OsgGuiCommand.h>
#include <memory>
#include <wyVector2.h>
#include <wyVector3.h>
#include <wy3dVector2.h>
#include <wy3dVector3.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>
#include <wy3dSketchEllipse.h>
#include <wy3dSketchEllipseArc.h>

#include "commands/transient/SketchCurveTransient.h"

struct ChamferData
{
    // first curve
    wydb::ElementId id1st;
    double startParam1st;
    double endParam1st;

    // second curve
    wydb::ElementId id2nd;
    double startParam2nd;
    double endParam2nd;

    // 倒角线段起点
    wy::Vector2 chamferStartPnt;
    // 倒角线段终点
    wy::Vector2 chamferEndPnt;

    bool operator==(const ChamferData& rhs) const
    {
        return id1st == rhs.id1st
            && startParam1st == rhs.startParam1st
            && endParam1st == rhs.endParam1st
            && id2nd == rhs.id2nd
            && startParam2nd == rhs.startParam2nd
            && endParam2nd == rhs.endParam2nd
            && chamferStartPnt == rhs.chamferStartPnt
            && chamferEndPnt == rhs.chamferEndPnt;
    }

    bool operator!=(const ChamferData& rhs) const
    {
        return !operator==(rhs);
    }

    void swap()
    {
        std::swap(id1st, id2nd);
        std::swap(startParam1st, startParam2nd);
        std::swap(endParam1st, endParam2nd);
    }
};

class SketchChamferGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(SketchChamferGuiCmd, SketchChamferGuiCmd, OsgGuiCommand)
public:
    SketchChamferGuiCmd();
    virtual ~SketchChamferGuiCmd();

protected:
    GuiCmdSketchInfo _sketchInfo;
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;

protected:
    enum class Step
    {
        Undefined = 0,
        First = 1,
        Second = 2,
    };
    virtual void reset();
    void gotoStep(Step step);

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseDown(const MouseEvent& event) override;

private:
    void preview(wydb::ElementId id, const wy::Vector3& pickPos);

    std::shared_ptr<ChamferData> chamferPreview(
        wydb::ElementId id1st, const wy::Vector2& refPnt1,
        wydb::ElementId id2nd, const wy::Vector2& refPnt2);

    std::shared_ptr<ChamferData> chamferPreviewLineLine(
        const wy3d::SketchLine* pLine1st, double refParam1st,
        const wy3d::SketchLine* pLine2nd, double refParam2nd);

    bool chamfer(const ChamferData* pChamferData);
    bool chamferItem(wydb::Database* pDb, const wydb::ElementId& id, double startParam, double endParam);
    bool chamferLine(wydb::Database* pDb, const wy::Vector2& startPnt, const wy::Vector2& endPnt);

private:
    Step _step;        // 步骤
    static double _D1; // 倒角距离1
    static double _D2; // 倒角距离2

    // 点选选项
    PointPickOption _pointPickOption;

    // 第一条曲线
    wy::Vector2 _pickPos1st;
    std::shared_ptr<SketchCurveTransient> _pCurveTransient1st;
    // 第二条曲线
    wy::Vector2 _pickPos2nd;
    std::shared_ptr<SketchCurveTransient> _pCurveTransient2nd;
    // 圆角结果
    std::shared_ptr<SketchCurveTransient> _pChamferTransient;
    std::shared_ptr<ChamferData> _pChamferData;
};

#endif // WY3DAPP_SKETCH_CHAMFER_GUI_CMD_H