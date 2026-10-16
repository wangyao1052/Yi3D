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

#ifndef WY3DAPP_SKETCH_DRAW_ARC_BY_3POINTS_GUI_CMD_H
#define WY3DAPP_SKETCH_DRAW_ARC_BY_3POINTS_GUI_CMD_H

#include <wy3dApp/OsgGuiCommand.h>
#include <cfloat>
#include <memory>
#include <set>
#include <wyVector2.h>
#include <wy3dVector2.h>
#include <wydbElementId.h>
#include <wy3dSketchArc.h>
#include "commands/transient/BasicTransient.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class MakeSketchArcBy3Points;
class GuiCmdHoverInputPopup2;

class SketchDrawArcBy3PointsGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(SketchDrawArcBy3PointsGuiCmd, wy3dApp::SketchDrawArcBy3PointsGuiCmd, OsgGuiCommand)
public:
    SketchDrawArcBy3PointsGuiCmd();
    virtual ~SketchDrawArcBy3PointsGuiCmd();

protected:
    GuiCmdSketchInfo _sketchInfo;
    virtual wyap::CmdExecution::StartResult onStart() override;

protected:
    enum class Step
    {
        Undefined = 0,
        SpecifyPoint1st = 1,
        SpecifyPoint2nd = 2,
        SpecifyPoint3rd = 3,
    };
    virtual void cleanUp() override;
    virtual void reset();
    virtual void onEscapeKey() override;
    bool finishStep(Step step);
    void gotoStep(Step step);

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseDown(const MouseEvent& event) override;
    void onFrame(double time) override;

private:
    void initializePopups();
    void showPopup();
    void hidePopup();
    void tryShowPopupOnHover(double time);
    void onPopupEnterKey();
    void onPopupEscapeKey();
    void simulateMouseMoveFromPopup();

    // 更新圆心标记
    void updateCenterPointTrasient();

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
    // 步骤
    Step _step;
    // 数据
    wy::Vector2 _pnt1st;
    wy::Vector2 _pnt2nd;
    wy::Vector2 _pnt3rd;
    // 创建的圆弧ID
    wydb::ElementId _sketchArcId;
    // 草图捕捉上下文
    Snap2dContextSPtr _pSnapContext;
    // 捕捉排除项
    std::set<wydb::ElementId> _snapExcludeIds;
    // 圆心标记
    CenterPointTransientSPtr _pCenterPointTransient;

    std::unique_ptr<GuiCmdHoverInputPopup2> _pXYPopup;
    HoverPopupState _hoverPopupState;

    std::shared_ptr<MakeSketchArcBy3Points> _pMakeSketchArc;
};

class MakeSketchArcBy3Points : public GuiCmdMakeElement
{
public:
    MakeSketchArcBy3Points(GuiCommand* pGuiCmd) : GuiCmdMakeElement(pGuiCmd), _pSketchArc(nullptr) {}

    // 收集创建的元素ID
    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

    // 初始化
    bool init(const wy::Vector2& pnt1, wydb::ElementId sketchId, const wy3d::SketchArc*& pOutSketchArc);
    // 更新
    bool update(const wy::Vector2& pnt1, const wy::Vector2& pnt2, const wy::Vector2& pnt3, bool useThirdPoint = true);

private:
    wy3d::SketchArc* _pSketchArc;
};

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_DRAW_ARC_BY_3POINTS_GUI_CMD_H
