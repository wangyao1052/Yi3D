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

#ifndef WY3DAPP_ROTATE_GUI_CMD_H
#define WY3DAPP_ROTATE_GUI_CMD_H

#include <cfloat>
#include <memory>
#include <set>
#include <wyVector2.h>
#include <wyVector3.h>
#include <wyapSelManager.h>
#include <wyapSelection.h>
#include <wy3dApp/OsgGuiCommand.h>
#include "commands/GuiCommandMenu.h"
#include "snap/SketchSnapSystem.h"
#include "commands/edit/RotateElements.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class GuiCmdHoverInputPopupBase;
class GuiCmdHoverInputPopup1;
class GuiCmdHoverInputPopup2;
class RotateElements;
class RotateTransient;

class RotateGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_ABSTRACT_MEMBERS(RotateGuiCmd, wy3dApp::RotateGuiCmd, OsgGuiCommand)
public:
    RotateGuiCmd();
    virtual ~RotateGuiCmd();

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;

protected:
    enum class Step
    {
        Undefined = 0,
        SelectElements  = 1,
        SelectWorkingPlane = 2,
        SpecifyBasePnt  = 3,
        SpecifyBaseLineEndPnt = 4,
        SpecifyRotateAngle    = 5,
    };
    virtual void reset();
    bool finishStep(Step step);
    void gotoStep(Step step);

    void onFrame(double time) override;
    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseDown(const MouseEvent& event) override;

    // Enter键响应
    virtual void onEnterKey() override;
    // Space键响应
    virtual void onSpaceKey() override;

    // 上下文菜单
    virtual bool isContextMenuActionVisible_CompleteSelection() const override;
    virtual void onContextMenuAction_CompleteSelection() override;
    virtual bool isContextMenuActionVisible_ClearSelection() const override;
    virtual void onContextMenuAction_ClearSelection() override;

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
        double lastMouseX;
        double lastMouseY;
        double lastMouseMoveTime;
        wy::Vector2 point;
        double length;
        double angleDeg;

        HoverPopupState()
            : lastMouseX(DBL_MAX),
              lastMouseY(DBL_MAX),
              lastMouseMoveTime(-1.0),
              point(0.0, 0.0),
              length(0.0),
              angleDeg(0.0)
        {
            this->resetValue();
        }

        void resetValue()
        {
            point.set(0.0, 0.0);
            length = 0.0;
            angleDeg = 0.0;
        }
    };

protected:
    Step _step;
    wyap::SelectionSet _sels;
    PointPickOption _pickWorkPlnOption;
    wy3d::Plane _workPln;
    wy::Vector2 _basePnt2;
    wy::Vector3 _basePnt;
    wy::Vector3 _baseLineEndPnt;
    wy::Vector2 _baseLineEndPnt2;
    double _rotateAngle;
    struct RotateInteractionData // 旋转交互数据
    {
        bool isCCW; // 是否是逆时针
        double lastRadian; // 上一次弧度
        RotateInteractionData() : isCCW(true), lastRadian(0.0) {}
    } _rid;
    SketchSnapContextSPtr _pSnapContext;

    // 工作平面预览
    SelectPreview _workPlnPreview;
    // 工作平面原点捕捉对象
    wyap::SnapObjectSPtr _pWorkPlnOriginSnapObject;
    // 工作平面捕捉系统
    SketchSnapSystemSPtr _pWorkPlnSnapSystem;

    std::shared_ptr<RotateElements> _pRotateElements;
    std::shared_ptr<RotateTransient> _pRotateTransient;
    std::unique_ptr<GuiCmdHoverInputPopup2> _pXYPopup;
    std::unique_ptr<GuiCmdHoverInputPopup2> _pLengthAnglePopup;
    std::unique_ptr<GuiCmdHoverInputPopup1> _pAnglePopup;
    HoverPopupState _hoverPopupState;

    friend class RotateGuiCmdMenu;

protected:
    // === 子类必须覆写的纯虚方法 ===

    // 获取当前工作平面
    virtual const wy3d::Plane& getActivePlane() const = 0;

    // 获取当前捕捉系统
    virtual SketchSnapSystem* getActiveSnapSystem() const { return nullptr; }

    // 选择元素步骤完成后跳转的下一步
    virtual void gotoNextStepAfterSelectElements() {}

    // 配置选择元素步骤的 allowedElemTypes & filter
    virtual void configureSelectElementOptions(SelectOptions& options) {}

    // 环境特定启动
    virtual void onStart_EnvSpecific() {}

    // 环境特定清理
    virtual void cleanupEnvSpecific() {}

    // 获取草图捕捉系统(草图子类覆写返回实际值，建模返回nullptr)
    virtual SketchSnapSystem* getSketchSnapSys() const { return nullptr; }
};

} // namespace wy3dApp

#endif // WY3DAPP_ROTATE_GUI_CMD_H
