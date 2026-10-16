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

#ifndef WY3DAPP_SKETCH_DRAW_CIRCLE_BY3POINTS_GUI_CMD_H
#define WY3DAPP_SKETCH_DRAW_CIRCLE_BY3POINTS_GUI_CMD_H

#include <wy3dApp/OsgGuiCommand.h>
#include <cfloat>
#include <memory>
#include <set>
#include <wyVector2.h>
#include <wy3dVector2.h>
#include <wydbElementId.h>
#include <wy3dSketchCircle.h>
#include "commands/transient/BasicTransient.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class MakeSketchCircleBy3Points;
class GuiCmdHoverInputPopup2;

class SketchDrawCircleBy3PointsGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(SketchDrawCircleBy3PointsGuiCmd, wy3dApp::SketchDrawCircleBy3PointsGuiCmd, OsgGuiCommand)
public:
    SketchDrawCircleBy3PointsGuiCmd();
    ~SketchDrawCircleBy3PointsGuiCmd();

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

private:
    struct HoverPopupState
    {
        // 鼠标信息
        double lastMouseX;
        double lastMouseY;
        double lastMouseMoveTime;

        // 数值信息
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
    // 创建的圆ID
    wydb::ElementId _sketchCircleId;
    // 草图捕捉上下文
    Snap2dContextSPtr _pSnapContext;
    // 捕捉排除项
    std::set<wydb::ElementId> _snapExcludeIds;
    // 点
    PointTransientSPtr _pFirstPoint;
    PointTransientSPtr _pSecondPoint;
    // 创建草绘圆
    std::shared_ptr<MakeSketchCircleBy3Points> _pMakeSketchCircle;
    // 悬停浮窗
    std::unique_ptr<GuiCmdHoverInputPopup2> _pXYPopup;
    HoverPopupState _hoverPopupState;
};

class MakeSketchCircleBy3Points : public GuiCmdMakeElement
{
public:
    MakeSketchCircleBy3Points(GuiCommand* pGuiCmd) : GuiCmdMakeElement(pGuiCmd), _pSketchCircle(nullptr) {}

    // 收集创建的元素ID
    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

    wydb::ElementId getId() const
    {
        return _pSketchCircle ? _pSketchCircle->getId() : wydb::ElementId::kNull;
    }

    // 初始化
    bool init(const wy::Vector2& pnt1, wydb::ElementId sketchId, const wy3d::SketchCircle*& pOutSketchCircle);
    // 更新
    bool update(const wy::Vector2& pnt1, const wy::Vector2& pnt2, const wy::Vector2& pnt3, bool useThirdPoint = true);

private:
    wy3d::SketchCircle* _pSketchCircle;
};

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_DRAW_CIRCLE_BY3POINTS_GUI_CMD_H
