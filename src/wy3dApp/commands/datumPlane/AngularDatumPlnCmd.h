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

#ifndef WY3DAPP_ANGULAR_DATUM_PLN_CMD_H
#define WY3DAPP_ANGULAR_DATUM_PLN_CMD_H

#include <cfloat>
#include <memory>
#include <wyVector2.h>
#include <wyapSelection.h>
#include <wy3dSketchPlane.h>
#include <wy3dDatumPlane.h>
#include "commands/OsgGuiCommand.h"
#include "commands/datumPlane/MakeDatumPlane.h"
#include "commands/transient/SketchCurveTransient.h"

class GuiCmdHoverInputPopup1;

class AngularDatumPlnCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(AngularDatumPlnCmd, AngularDatumPlnCmd, OsgGuiCommand)
public:
    AngularDatumPlnCmd();
    ~AngularDatumPlnCmd();

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void initStep2ndTip();
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;
    virtual void cleanup() override;
    virtual void reset();

protected:
    enum class Step
    {
        Undefined = 0,
        SelectDatumPlaneOrFace = 1,
        SpecifyRotationAxis = 2,
        SpecifyAngle = 3,
    };
    virtual bool finishStep(Step step);
    void gotoStep(Step step);

    void onFrame(double time) override;
    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseDown(const MouseEvent& event) override;
    virtual void onLeftMouseUp(const MouseEvent& event) override;

protected:
    // 求出旋转轴线段
    // datumPlnId --- 基准面ID
    // lineOrigin,lineDir --- 基准面上的直线原点和方向向量
    // axisLineStart,axisLineEnd --- [out]旋转轴线段起始点&终止点
    static bool computeRotateAxisLineSegment(
        const wydb::ElementId& datumPlnId,
        const wy::Vector2& lineOrigin, const wy::Vector2& lineDir,
        wy::Vector2& axisLineStart, wy::Vector2& axisLineEnd);

    // 绕X轴旋转平面
    wy3d::SketchPlane rotatePlaneAroundXAxis(double angle) const;

private:
    void initializePopups();
    void showPopup();
    void hidePopup();
    void tryShowPopupOnHover(double time);
    void onPopupEnterKey();
    void onPopupEscapeKey();
    void simulateMouseMoveFromPopup();

private:
    struct HoverPopupState
    {
        double lastMouseX;
        double lastMouseY;
        double lastMouseMoveTime;
        double angle;
        int angleSign;

        HoverPopupState()
            : lastMouseX(DBL_MAX)
            , lastMouseY(DBL_MAX)
            , lastMouseMoveTime(-1.0)
            , angle(0.0)
            , angleSign(1)
        {}

        void resetValue()
        {
            angle = 0.0;
            angleSign = 1;
        }
    };

protected:
    Step _step;
    wy3d::SketchPlane _plane;
    QString _step2ndTip;
    // 交线
    wy::Vector2 _intersectLineOrigin;
    wy::Vector2 _intersectLineDir;
    // 旋转轴线
    wy::Vector2 _rotateAxisLineStart;
    wy::Vector2 _rotateAxisLineEnd;
    // 旋转角度平面
    wy3d::SketchPlane _rotateAnglePlane;
    // 旋转角度
    double _angle;

    // 捕捉排除项
    std::set<wydb::ElementId> _snapExcludeIds;
    // 点选选项
    PointPickOption _pointPickOption;
    // 预览
    SelectPreview _preview;
    // 高亮
    SelectionSetHighlightorSPtr _pSelSetHighlightor;
    // 创建基准面
    std::shared_ptr<MakeDatumPlane> _pMakeDatumPlane;
    // 旋转轴线
    SketchCurveTransientSPtr _pRotateAxisTransient;
    std::unique_ptr<GuiCmdHoverInputPopup1> _pAnglePopup; // 悬停输入浮窗
    HoverPopupState _hoverPopupState; // 悬停状态
};

class PerpendicularDatumPlnCmd : public AngularDatumPlnCmd
{
    WYRX_DECLARE_MEMBERS(PerpendicularDatumPlnCmd, PerpendicularDatumPlnCmd, AngularDatumPlnCmd)
protected:
    virtual void initStep2ndTip() override;
    virtual bool finishStep(Step step) override;
};

class ThroughAxisDatumPlnCmd : public AngularDatumPlnCmd
{
    WYRX_DECLARE_MEMBERS(ThroughAxisDatumPlnCmd, ThroughAxisDatumPlnCmd, AngularDatumPlnCmd)
protected:
    virtual void initStep2ndTip() override;
    virtual bool finishStep(Step step) override;
};

#endif 
