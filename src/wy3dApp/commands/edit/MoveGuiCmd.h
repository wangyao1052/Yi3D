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

#ifndef WY3DAPP_MOVE_GUI_CMD_H
#define WY3DAPP_MOVE_GUI_CMD_H

#include <cfloat>
#include <wyVector2.h>
#include <wyVector3.h>
#include <wyapSelManager.h>
#include <wyapSelection.h>
#include <wy3dApp/OsgGuiCommand.h>
#include "commands/GuiCommandMenu.h"
#include "snap/SketchSnapSystem.h"
#include "commands/edit/MoveElements.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class MoveElemens;
class LineTransient;
class GuiCmdHoverInputPopup2;
class GuiCmdHoverInputPopup3;

class MoveGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(MoveGuiCmd, wy3dApp::MoveGuiCmd, OsgGuiCommand)
public:
    MoveGuiCmd();
    virtual ~MoveGuiCmd();

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;

protected:
    enum class Step
    {
        Undefined = 0,
        SelectElements     = 1,
        SelectWorkingPlane = 2,
        SpecifyStartPnt    = 3,
        SpecifyEndPnt      = 4,
    };
    bool finishStep(Step step);
    void gotoStep(Step step);

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseDown(const MouseEvent& event) override;
    void onFrame(double time) override;

    // Enter键响应
    virtual void onEnterKey() override;
    // Space键响应
    virtual void onSpaceKey() override;

    // 上下文菜单
    virtual bool isContextMenuActionVisible_CompleteSelection() const override;
    virtual void onContextMenuAction_CompleteSelection() override;
    virtual bool isContextMenuActionVisible_ClearSelection() const override;
    virtual void onContextMenuAction_ClearSelection() override;

private:
    wy::Vector3 snapInWorkPlane(
        const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& aa,
        const wy::Vector3& pos3d) const;
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
        wy::Vector3 point;
        wy::Vector3 vector;

        HoverPopupState()
            : lastMouseX(DBL_MAX),
              lastMouseY(DBL_MAX),
              lastMouseMoveTime(-1.0),
              point(0.0, 0.0, 0.0),
              vector(0.0, 0.0, 0.0)
        {
            this->resetValue();
        }

        void resetValue()
        {
            point.set(0.0, 0.0, 0.0);
            vector.set(0.0, 0.0, 0.0);
        }
    };

protected:
    Step _step;
    wyap::SelectionSet _sels;
    PointPickOption _pickWorkPlnOption;
    wy3d::Plane _workPln;
    wy::Vector2 _startPnt2d;
    wy::Vector3 _startPnt;
    wy::Vector2 _moveVec2d;
    wy::Vector3 _moveVec;
    SketchSnapContextSPtr _pSnapContext;

    // 工作面预览
    SelectPreview _workPlnPreview;
    // 工作平面原点捕捉对象
    wyap::SnapObjectSPtr _pWorkPlnOriginSnapObject;
    // 工作平面捕捉系统
    SketchSnapSystemSPtr _pWorkPlnSnapSystem;
    
    std::shared_ptr<MoveElemens> _pMoveElements;
    std::shared_ptr<LineTransient> _pLineTransient;
    std::unique_ptr<GuiCmdHoverInputPopup2> _pXYPopup;
    std::unique_ptr<GuiCmdHoverInputPopup2> _pLengthAnglePopup;
    std::unique_ptr<GuiCmdHoverInputPopup3> _pXYZPopup;
    HoverPopupState _hoverPopupState;

    friend class MoveGuiCmdMenu;

protected:
    // === 子类必须覆写的纯虚方法 ===

    // 选择元素步骤完成后跳转的下一步
    virtual void gotoNextStepAfterSelectElements() {}

    // 配置选择元素步骤的 allowedElemTypes & filter
    virtual void configureSelectElementOptions(SelectOptions& options) {}

    // 鼠标移动:指定起点
    virtual void onMouseMove_SpecifyStartPnt(double x, double y) {}

    // 鼠标移动:指定终点
    virtual void onMouseMove_SpecifyEndPnt(double x, double y) {}

    // 左键按下:指定起点
    virtual void onLeftMouseDown_SpecifyStartPnt(double x, double y) {}

    // 左键按下:指定终点
    virtual void onLeftMouseDown_SpecifyEndPnt(double x, double y) {}

    // 同步数据(SpecifyStartPnt完成后)
    virtual void syncStartPntData() {}

    // 同步数据(SpecifyEndPnt完成后)
    virtual void syncEndPntData() {}

    // 显示坐标输入弹窗
    virtual void showCoordinatePopup() {}

    // 处理弹窗 Enter 键
    virtual void handlePopupEnterKey() {}

    // 执行移动
    virtual bool executeMove() { return false; }

    // 环境特定启动
    virtual void onStart_EnvSpecific() {}

    // 环境特定清理
    virtual void cleanupEnvSpecific() {}

protected:
    // 获取草图捕捉系统(草图子类覆写返回实际值，建模返回nullptr)
    virtual SketchSnapSystem* getSketchSnapSys() const { return nullptr; }
};

} // namespace wy3dApp

#endif // WY3DAPP_MOVE_GUI_CMD_H
