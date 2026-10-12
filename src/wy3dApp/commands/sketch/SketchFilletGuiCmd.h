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

#ifndef WY3DAPP_SKETCH_FILLET_GUI_CMD_H
#define WY3DAPP_SKETCH_FILLET_GUI_CMD_H

#include <wy3dApp/OsgGuiCommand.h>
#include <memory>
#include <wyVector2.h>
#include <wyVector3.h>
#include <wy3dVector2.h>
#include <wy3dVector3.h>
#include <wydbElementId.h>
#include "commands/transient/SketchCurveTransient.h"

#include <wy3dSketchLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>
#include <wy3dSketchEllipse.h>
#include <wy3dSketchEllipseArc.h>
#include <wy3dSketchSpline.h>
#include "wy3dAppDefs.h"

namespace wy3dApp {

struct FilletData
{
    // first curve
    wydb::ElementId id1st;
    double startParam1st;
    double endParam1st;

    // second curve
    wydb::ElementId id2nd;
    double startParam2nd;
    double endParam2nd; 

    // 圆角圆心
    wy::Vector2 filletCenter;
    // 圆角半径
    double filletRadius;
    // 圆角起始角度
    double filletStartAngle;
    // 圆角终止角度
    double filletEndAngle;

    bool operator==(const FilletData& rhs) const
    {
        return id1st == rhs.id1st
            && startParam1st == rhs.startParam1st
            && endParam1st == rhs.endParam1st
            && id2nd == rhs.id2nd
            && startParam2nd == rhs.startParam2nd
            && endParam2nd == rhs.endParam2nd
            && filletCenter == rhs.filletCenter
            && filletRadius == rhs.filletRadius
            && filletStartAngle == rhs.filletStartAngle
            && filletEndAngle == rhs.filletEndAngle;
    }

    bool operator!=(const FilletData& rhs) const
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

class SketchFilletGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(SketchFilletGuiCmd, wy3dApp::SketchFilletGuiCmd, OsgGuiCommand)
public:
    SketchFilletGuiCmd();
    virtual ~SketchFilletGuiCmd();

protected:
    GuiCmdSketchInfo _sketchInfo;
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;

protected:
    enum class Step
    {
        First = 1,
        Second = 2,
    };
    virtual void reset();
    void gotoStep(Step step);

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseDown(const MouseEvent& event) override;

private:
    void preview(wydb::ElementId id, const wy::Vector3& pickPos);

    std::shared_ptr<FilletData> filletPreview(
        wydb::ElementId id1st, const wy::Vector2& refPnt1,
        wydb::ElementId id2nd, const wy::Vector2& refPnt2);

    // line vs line
    std::shared_ptr<FilletData> filletPreviewLineLine(
        const wy3d::SketchLine* pLine1st, double paramPickPosOnLine1st,
        const wy3d::SketchLine* pLine2nd, double paramPickPosOnLine2nd);

    // line vs arc
    std::shared_ptr<FilletData> filletPreviewLineArc(
        const wy3d::SketchLine* pLine, double paramPickPosOnLine,
        const wy3d::SketchArc* pArc, double paramPickPosOnArc,
        bool isSecondPickPosMajor = true);
    // arc vs line
    std::shared_ptr<FilletData> filletPreviewArcLine(
        const wy3d::SketchArc* pArc, double paramPickPosOnArc,
        const wy3d::SketchLine* pLine, double paramPickPosOnLine);

    // line vs circle
    std::shared_ptr<FilletData> filletPreviewLineCircle(
        const wy3d::SketchLine* pLine, double paramPickPosOnLine,
        const wy3d::SketchCircle* pCircle, double paramPickPosOnCircle,
        bool isSecondPickPosMajor = true);
    // circle vs line
    std::shared_ptr<FilletData> filletPreviewCircleLine(
        const wy3d::SketchCircle* pCircle, double paramPickPosOnCircle,
        const wy3d::SketchLine* pLine, double paramPickPosOnLine);

    // line vs spline
    std::shared_ptr<FilletData> filletPreviewLineSpline(
        const wy3d::SketchLine* pLine, double paramPickPosOnLine,
        const wy3d::SketchSpline* pSpline, double paramPickPosOnSpline,
        bool isSecondPickPosMajor = true);
    // spline vs line
    std::shared_ptr<FilletData> filletPreviewSplineLine(
        const wy3d::SketchSpline* pSpline, double paramPickPosOnSpline,
        const wy3d::SketchLine* pLine, double paramPickPosOnLine);

    // circle vs circle
    std::shared_ptr<FilletData> filletPreviewCircleCircle(
        const wy3d::SketchCircle* pCircle1st, double paramPickPos1st,
        const wy3d::SketchCircle* pCircle2nd, double paramPickPos2nd);

    // circle vs arc
    std::shared_ptr<FilletData> filletPreviewCircleArc(
        const wy3d::SketchCircle* pCircle, double paramPickPosOnCircle,
        const wy3d::SketchArc* pArc, double paramPickPosOnArc,
        bool isSecondPickPosMajor = true);
    // arc vs circle
    std::shared_ptr<FilletData> filletPreviewArcCircle(
        const wy3d::SketchArc* pArc, double paramPickPosOnArc,
        const wy3d::SketchCircle* pCircle, double paramPickPosOnCircle);

    // circle vs spline
    std::shared_ptr<FilletData> filletPreviewCircleSpline(
        const wy3d::SketchCircle* pCircle, double paramPickPosOnCircle,
        const wy3d::SketchSpline* pSpline, double paramPickPosOnSpline,
        bool isSecondPickPosMajor = true);
    // spline vs circle
    std::shared_ptr<FilletData> filletPreviewSplineCircle(
        const wy3d::SketchSpline* pSpline, double paramPickPosOnSpline,
        const wy3d::SketchCircle* pCircle, double paramPickPosOnCircle);

    // arc vs arc
    std::shared_ptr<FilletData> filletPreviewArcArc(
        const wy3d::SketchArc* pArc1, double paramPickPos1,
        const wy3d::SketchArc* pArc2, double paramPickPos2);

    // arc vs spline
    std::shared_ptr<FilletData> filletPreviewArcSpline(
        const wy3d::SketchArc* pArc, double paramPickPosOnArc,
        const wy3d::SketchSpline* pSpline, double paramPickPosOnSpline,
        bool isSecondPickPosMajor = true);
    // spline vs arc
    std::shared_ptr<FilletData> filletPreviewSplineArc(
        const wy3d::SketchSpline* pSpline, double paramPickPosOnSpline,
        const wy3d::SketchArc* pArc, double paramPickPosOnArc);

    // spline vs spline
    std::shared_ptr<FilletData> filletPreviewSplineSpline(
        const wy3d::SketchSpline* pSpline1st, double paramPickPosOnSpline1st,
        const wy3d::SketchSpline* pSpline2nd, double paramPickPosOnSpline2nd);

    const wy3d::SketchCurve* getSketchCurve(const wydb::ElementId& id) const;

    bool fillet(const FilletData* pFilletData);
    bool filletItem(wydb::Database* pDb, const wydb::ElementId& id, double startParam, double endParam);
    bool filletArc(wydb::Database* pDb, const wy::Vector2& center, double radius, double startAngle, double endAngle);

private:
    Step _step; // 步骤
    static double _R;  // 圆角半径
    // 点选选项
    PointPickOption _pointPickOption;
    // 第一条曲线
    wy::Vector2 _pickPos1st;
    std::shared_ptr<SketchCurveTransient> _pCurveTransient1st;
    // 第二条曲线
    wy::Vector2 _pickPos2nd;
    std::shared_ptr<SketchCurveTransient> _pCurveTransient2nd;
    // 圆角结果
    std::shared_ptr<SketchCurveTransient> _pFilletTransient;
    std::shared_ptr<FilletData> _pFilletData;
};

}

#endif // WY3DAPP_SKETCH_FILLET_GUI_CMD_H
