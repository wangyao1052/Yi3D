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

#ifndef WY3DAPP_SKETCH_DRAW_ARC_GUI_CMD_H
#define WY3DAPP_SKETCH_DRAW_ARC_GUI_CMD_H

#include "commands/OsgGuiCommand.h"
#include <cfloat>
#include <memory>
#include <set>
#include <wyVector2.h>
#include <wy3dVector2.h>
#include <wydbElementId.h>
#include <wy3dSketchArc.h>
#include "commands/transient/BasicTransient.h"

class MakeSketchArc;
class SketchCircleTransient;
class GuiCmdHoverInputPopupBase;
class GuiCmdHoverInputPopup1;
class GuiCmdHoverInputPopup2;

class SketchDrawArcGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(SketchDrawArcGuiCmd, SketchDrawArcGuiCmd, OsgGuiCommand)
public:
    SketchDrawArcGuiCmd();
    virtual ~SketchDrawArcGuiCmd();

protected:
    GuiCmdSketchInfo _sketchInfo;
    virtual wyap::CmdExecution::StartResult onStart() override;

protected:
    enum class Step
    {
        Undefined = 0,
        SpecifyCenterPnt = 1,
        SpecifyStartPoint = 2,
        SpecifyEndPoint = 3
    };
    virtual void cleanup() override;
    virtual void reset();
    virtual void onEscapeKey() override;
    bool finishStep(Step step);
    void gotoStep(Step step);

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseDown(const MouseEvent& event) override;
    void onFrame(double time) override;

private:
    std::set<wydb::ElementId> getSnapExcludeIds() const;

    void initializePopups();
    void showPopup();
    void hidePopup();
    GuiCmdHoverInputPopupBase* getActivePopup() const;
    void tryShowPopupOnHover(double time);
    void onPopupEnterKey();
    void onPopupEscapeKey();
    void simulateMouseMoveFromPopup();

private:
    struct HoverPopupState
    {
        // 鼠标信息
        double lastMouseX;
        double lastMouseY;
        double lastMouseMoveTime;

        // 值信息
        wy::Vector2 point;
        double radius;
        double startAngleDeg;
        double totalAngleDeg;

        HoverPopupState() :
            lastMouseX(DBL_MAX),
            lastMouseY(DBL_MAX),
            lastMouseMoveTime(-1.0),
            point(),
            radius(0.0),
            startAngleDeg(0.0),
            totalAngleDeg(0.0)
        {
            this->resetValue();
        }

        void resetValue()
        {
            point.set(0.0, 0.0);
            radius = 0.0;
            startAngleDeg = 0.0;
            totalAngleDeg = 0.0;
        }
    };

private:
    Step _step;
    wy::Vector2 _centerPnt;
    wy::Vector2 _startPnt;
    double _radius;
    double _startAngle;
    double _totalAngle;
    SketchSnapContextSPtr _pSnapContext;

    // 圆
    std::shared_ptr<SketchCircleTransient> _pCircleTransient;
    // 圆心标记
    CenterPointTransientSPtr _pCenterPointTransient;
    // 起点半径线
    LineTransientSPtr _pStartRadiusLineTransient;
    // 终点半径线
    LineTransientSPtr _pEndRadiusLineTransient;

    std::unique_ptr<GuiCmdHoverInputPopup2> _pXYPopup;
    std::unique_ptr<GuiCmdHoverInputPopup2> _pRadiusAnglePopup;
    std::unique_ptr<GuiCmdHoverInputPopup1> _pSweepAnglePopup;
    HoverPopupState _hoverPopupState;

    std::shared_ptr<MakeSketchArc> _pMakeSketchArc;
};

class MakeSketchArc : public GuiCmdMakeElement
{
public:
    MakeSketchArc(GuiCommand* pGuiCmd) : GuiCmdMakeElement(pGuiCmd), _pSketchArc(nullptr) {}
    ~MakeSketchArc() {}

    // 收集创建的元素ID
    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

    wydb::ElementId getId() const
    {
        return _pSketchArc ? _pSketchArc->getId() : wydb::ElementId::kNull;
    }

    // 创建
    bool init(const wy::Vector2& centerPnt, double radius, double startAngle, wydb::ElementId sketchId);
    // 更新
    bool update(double totalAngle);

private:
    wy3d::SketchArc* _pSketchArc;
};

#endif // WY3DAPP_SKETCH_DRAW_ARC_GUI_CMD_H