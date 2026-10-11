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

#ifndef WY3DAPP_SKETCH_DRAW_SPLINE_GUI_CMD_H
#define WY3DAPP_SKETCH_DRAW_SPLINE_GUI_CMD_H

#include <wy3dApp/OsgGuiCommand.h>
#include <cfloat>
#include <list>
#include <memory>
#include <set>
#include <wyVector2.h>
#include <wy3dVector2.h>
#include <wydbElementId.h>
#include <wy3dSketchSpline.h>
#include "commands/transient/BasicTransient.h"

class MakeSketchSpline;
class GuiCmdHoverInputPopup2;

class SketchDrawSplineGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(SketchDrawSplineGuiCmd, SketchDrawSplineGuiCmd, OsgGuiCommand)
public:
    SketchDrawSplineGuiCmd();
    virtual ~SketchDrawSplineGuiCmd();

protected:
    GuiCmdSketchInfo _sketchInfo;
    virtual wyap::CmdExecution::StartResult onStart() override;

    enum class Step
    {
        Undefined = 0,
        SpecifyStartPnt = 1,
        SpecifyNextPnt  = 2,
    };
    virtual void cleanup() override;
    virtual void reset();
    virtual void onEscapeKey() override;
    bool finishStep(Step step);
    void gotoStep(Step step);

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseDown(const MouseEvent& event) override;
    virtual void onLeftMouseDoubleClicked(const MouseEvent& event) override;
    void onFrame(double time) override;

    // 是否闭合
    virtual bool isClosed() const { return false; }
    // 尝试修正nextPoint
    virtual wy::Vector2 tryReviseNextPoint(
        const std::vector<wy::Vector2>& points,
        const wy::Vector2& nextPoint) const
    { return nextPoint; }
    // 是否允许将 nextPoint 作为首尾闭合点
    virtual bool isAllowedClosurePoint(
        const std::vector<wy::Vector2>& points,
        const wy::Vector2& nextPoint) const
    { return false; }
    // 是否为不允许的重复点
    bool isDisallowedDuplicatePoint(
        const std::vector<wy::Vector2>& points,
        const wy::Vector2& nextPoint) const;

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

        wy::Vector2 point;

        HoverPopupState() :
            lastMouseX(DBL_MAX),
            lastMouseY(DBL_MAX),
            lastMouseMoveTime(-1.0),
            point()
        {
            this->resetValue();
        }

        void resetValue()
        {
            point.set(0.0, 0.0);
        }
    };

private:
    // 尝试添加起点为捕捉对象
    void tryAddStartPntAsResidentSnapObject();

protected:
    // 模式
    wy3d::SplineMode _mode;
    // 步骤
    Step _step;
    // 起始点
    wy::Vector2 _startPoint;
    // 下一点
    wy::Vector2 _nextPoint;
    // 插值点
    std::vector<wy::Vector2> _fitPoints;
    // 草图捕捉上下文
    SketchSnapContextSPtr _pSnapContext;
    // 插值点或控制点
    std::list<PointTransientSPtr> _pointTransients;
    // 控制点路径线
    std::list<LineTransientSPtr> _pathTransients;
    LineTransientSPtr _pActivePathTransient;
    // 创建样条曲线
    std::shared_ptr<MakeSketchSpline> _pMakeSketchSpline;
    // 悬停浮窗
    std::unique_ptr<GuiCmdHoverInputPopup2> _pXYPopup;
    HoverPopupState _hoverPopupState;

    // added by wangyao 2025.08.14 {
    // 样条曲线起点捕捉对象
    // 参照SolidWorks在特定条件下才会创建:
    // <1>插值样条曲线:第>=4个点才支持捕捉起点
    // <2>控制点样条曲线:第>=5个点才支持捕捉起点
    wyap::SnapObjectSPtr _pStartPointSnapObject;
    // }
};

class SketchDrawSplineGuiCmd_FitPoints : public SketchDrawSplineGuiCmd
{
    WYRX_DECLARE_MEMBERS(SketchDrawSplineGuiCmd_FitPoints, SketchDrawSplineGuiCmd_FitPoints, SketchDrawSplineGuiCmd)
public:
    SketchDrawSplineGuiCmd_FitPoints() : SketchDrawSplineGuiCmd()
    {
        _mode = wy3d::SplineMode::InterpolationPoints;
    }

protected:
    virtual bool isClosed() const override;
    virtual wy::Vector2 tryReviseNextPoint(
        const std::vector<wy::Vector2>& points,
        const wy::Vector2& nextPoint) const override;
    virtual bool isAllowedClosurePoint(
        const std::vector<wy::Vector2>& points,
        const wy::Vector2& nextPoint) const override;
};

class SketchDrawSplineGuiCmd_ControlPoints : public SketchDrawSplineGuiCmd
{
    WYRX_DECLARE_MEMBERS(SketchDrawSplineGuiCmd_ControlPoints, SketchDrawSplineGuiCmd_ControlPoints, SketchDrawSplineGuiCmd)
public:
    SketchDrawSplineGuiCmd_ControlPoints() : SketchDrawSplineGuiCmd()
    {
        _mode = wy3d::SplineMode::ControlPoints;
    }

protected:
    virtual bool isClosed() const override;
    virtual wy::Vector2 tryReviseNextPoint(
        const std::vector<wy::Vector2>& points,
        const wy::Vector2& nextPoint) const override;
    virtual bool isAllowedClosurePoint(
        const std::vector<wy::Vector2>& points,
        const wy::Vector2& nextPoint) const override;
};

class MakeSketchSpline : public GuiCmdMakeElement
{
public:
    MakeSketchSpline(GuiCommand* pGuiCmd, wy3d::SplineMode mode) : GuiCmdMakeElement(pGuiCmd),
        _mode(mode), _pSketchSpline(nullptr)
    {
        _pnts.reserve(20);
    }

    // 收集创建的元素ID
    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

    wydb::ElementId getId() const
    {
        return _pSketchSpline ? _pSketchSpline->getId() : wydb::ElementId::kNull;
    }

    // 创建
    bool init(const wy::Vector2& startPnt, wydb::ElementId sketchId);
    // 更新
    bool update(const std::vector<wy::Vector2>& fitPoints);
    bool update(const std::vector<wy::Vector2>& fitPoints, const wy::Vector2& nextPnt);

private:
    // 模式
    wy3d::SplineMode _mode;
    // 样条曲线
    wy3d::SketchSpline* _pSketchSpline;
    // 点集合
    std::vector<wy::Vector2> _pnts;
};

#endif // WY3DAPP_SKETCH_DRAW_SPLINE_GUI_CMD_H
