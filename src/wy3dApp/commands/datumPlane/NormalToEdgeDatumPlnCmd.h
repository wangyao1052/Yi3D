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

#ifndef WY3DAPP_NORMAL_TO_EDGE_DATUM_PLN_CMD_H
#define WY3DAPP_NORMAL_TO_EDGE_DATUM_PLN_CMD_H

#include <cfloat>
#include <Geom_Curve.hxx>
#include <memory>
#include <wyVector3.h>
#include <wyapSelection.h>
#include <wy3dPlane.h>
#include <wy3dDatumPlane.h>
#include "commands/OsgGuiCommand.h"
#include "commands/datumPlane/MakeDatumPlane.h"
#include "commands/transient/BasicTransient.h"

class GuiCmdHoverInputPopup1;

class NormalToEdgeDatumPlnCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(NormalToEdgeDatumPlnCmd, NormalToEdgeDatumPlnCmd, OsgGuiCommand)
public:
    NormalToEdgeDatumPlnCmd();
    ~NormalToEdgeDatumPlnCmd();

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void cleanup() override;
    virtual void reset();
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;

protected:
    enum class Step
    {
        Undefined = 0,
        SelectEdge = 1,
        SpecifyDistance = 2,
    };
    virtual bool finishStep(Step step);
    void gotoStep(Step step);

    void onFrame(double time) override;
    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseDown(const MouseEvent& event) override;
    virtual void onLeftMouseUp(const MouseEvent& event) override;

private:
    void initializePopups();
    void showPopup();
    void hidePopup();
    void tryShowPopupOnHover(double time);
    void onPopupEnterKey();
    void onPopupEscapeKey();
    void simulateMouseMoveFromPopup();

private:
    enum class CurveType
    {
        Line = 0,
        Circle = 1,
        Arc = 2,
        Ellipse = 3,
        EllipseArc = 4,
        Spline = 5,
        Helix = 6,
        Undefined = 100,
    };
    enum class ParamType
    {
        Length = 0,
        Radian = 1,
        ParametricRadian = 2,
        Undefined = 100,
    };
    struct CurveInfo
    {
        CurveType curveType;
        ParamType paramType;

        Handle(Geom_Curve) geomCurve;
        double param;
        wy::Vector3 initPoint;

        wy3d::Plane refPlane;

        // 额外参数
        double extraA;
        double extraB;

        CurveInfo() : curveType(CurveType::Undefined), paramType(ParamType::Undefined),
            geomCurve(nullptr), param(0.0), refPlane(wy3d::Plane::kInvalid),
            extraA(0.0), extraB(0.0)
        {}
    };
    struct HoverPopupState
    {
        double lastMouseX;
        double lastMouseY;
        double lastMouseMoveTime;
        double value;
        int sign;

        HoverPopupState()
            : lastMouseX(DBL_MAX)
            , lastMouseY(DBL_MAX)
            , lastMouseMoveTime(-1.0)
            , value(0.0)
            , sign(1)
        {}

        void resetValue()
        {
            value = 0.0;
            sign = 1;
        }
    };

    double convertParamValueToKernel(
        double param, ParamType paramType,
        double extraA = 0.0, double extraB = 0.0) const;
    double convertParamValueToGui(
        double param, ParamType paramType,
        double extraA = 0.0, double extraB = 0.0) const;
    QString getParamTypeLabelStr(ParamType paramType) const;

    bool extractCurveInfo(const wyap::Selection& sel, CurveInfo& curveInfo);
    bool extractCurveInfoImpl(const wyap::Selection& sel, CurveInfo& curveInfo);
    bool computePlane(Handle(Geom_Curve) geomCurve, double param, wy3d::Plane& plane);
    
protected:
    Step _step;
    CurveInfo _curveInfo;
    wy3d::Plane _plane;

    // 点选选项
    PointPickOption _pointPickOption;
    // 预览
    SelectPreview _preview;
    // 高亮
    SelectionSetHighlightorSPtr _pSelSetHighlightor;
    // 起点
    PointTransientSPtr _pEdgeStartPntTransient;
    // 创建基准面
    std::shared_ptr<MakeDatumPlane> _pMakeDatumPlane;

    std::unique_ptr<GuiCmdHoverInputPopup1> _pDistancePopup;
    HoverPopupState _hoverPopupState;
};

#endif 
