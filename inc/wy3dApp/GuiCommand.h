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

#ifndef WY3DAPP_GUI_CMDS_H
#define WY3DAPP_GUI_CMDS_H

#include <set>
#include <cassert>

#include <QString>
#include <QCoreApplication>
#include <QLabel>

#include <wyVector2.h>
#include <wyVector3.h>
#include <wy3dVector2.h>
#include <wy3dVector3.h>
#include <wydbElementId.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapCmdExecution.h>
#include "snap/SnapSystemBase.h"
#include <wy3dPlane.h>
#include <wy3dSelectionType.h>
#include <wy3dApp/GuiCmdTransient.h>
#include "widgets/CursorType.h"
#include <osgGA/GUIEventAdapter>
#include "snap/SketchSnapContext.h"
#include "scene/RenderConst.h"

#include "select/SelectMode.h"
#include <wy3dApp/PointPickOption.h>
#include "select/SelectFilterFunctor.h"
#include <wy3dApp/SelectPreview.h>
#include "select/SelectionSetHighlightor.h"

#include <wy3dApp/GuiCmdControls.h>
#include <wy3dApp/GuiCommandUtil.h>

class GuiCmdOsgEventHandler;
class GuiCmdMakeElement;
class GuiCommand;
class SketchSnapSystem;
class GuiCmdMenu;
class GuiEventDispatcher;

#include "commands/GuiCmdMakeElement.h"
#include "wy3dAppDefs.h"

// 命令选择选项
struct GuiCmdSelectOptions
{
    // 点选
    bool pointSelect;
    // 框选
    bool boxSelect;
    // 拾取掩码
    unsigned int pickMask;
    // 选择类型
    wy3d::SelectionType selectionType;
    // 前置选择过滤函数
    SelectPreFilterFunctorSPtr preFilter;
    // 选择过滤函数
    SelectFilterFunctorSPtr filter;
    // 预览
    bool preview;
    // 选择模式:全量还是增量
    SelectMode selectMode;

    GuiCmdSelectOptions() : pointSelect(false), boxSelect(false), pickMask(PICK_MASK),
        selectionType(wy3d::SelectionType::Element), preview(true), selectMode(SelectMode::Full)
    {}
};

// 鼠标事件
struct MouseEvent
{
    float x, y;
    double time;
};

// 按键码
namespace KeyCode {
    constexpr int Enter     = osgGA::GUIEventAdapter::KEY_Return;
    constexpr int KP_Enter  = osgGA::GUIEventAdapter::KEY_KP_Enter;
    constexpr int Escape    = osgGA::GUIEventAdapter::KEY_Escape;
    constexpr int Space     = osgGA::GUIEventAdapter::KEY_Space;
    constexpr int Delete    = osgGA::GUIEventAdapter::KEY_Delete;
    constexpr int Backspace = osgGA::GUIEventAdapter::KEY_BackSpace;
    // Ctrl+字母: ASCII 控制码(1~26), 通用标准, 非 OSG 专用
    constexpr int CtrlA = 1;
    constexpr int CtrlB = 2;
    constexpr int CtrlC = 3;
    constexpr int CtrlD = 4;
    constexpr int CtrlE = 5;
    constexpr int CtrlF = 6;
    constexpr int CtrlG = 7;
    constexpr int CtrlH = 8;
    constexpr int CtrlI = 9;
    constexpr int CtrlJ = 10;
    constexpr int CtrlK = 11;
    constexpr int CtrlL = 12;
    constexpr int CtrlM = 13;
    constexpr int CtrlN = 14;
    constexpr int CtrlO = 15;
    constexpr int CtrlP = 16;
    constexpr int CtrlQ = 17;
    constexpr int CtrlR = 18;
    constexpr int CtrlS = 19;
    constexpr int CtrlT = 20;
    constexpr int CtrlU = 21;
    constexpr int CtrlV = 22;
    constexpr int CtrlW = 23;
    constexpr int CtrlX = 24;
    constexpr int CtrlY = 25;
    constexpr int CtrlZ = 26;
}

// 修饰键掩码
namespace KeyMod {
    constexpr int Shift = osgGA::GUIEventAdapter::MODKEY_SHIFT;
    constexpr int Ctrl  = osgGA::GUIEventAdapter::MODKEY_CTRL;
    constexpr int Alt   = osgGA::GUIEventAdapter::MODKEY_ALT;
}

// 键盘事件
struct KeyEvent
{
    int key;
    int modKeyMask;
    double time;
};

class WY3DAPP_EXPORT GuiCommand : public wyap::InteractiveCmdExecution
{
    WYRX_DECLARE_ABSTRACT_MEMBERS(GuiCommand, GuiCommand, wyap::InteractiveCmdExecution)
public:
    GuiCommand();
    virtual ~GuiCommand();

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;

    // 清理数据
    virtual void cleanup() {}
    // Esc键响应
    virtual void onEscapeKey() { this->requestAbort(AbortCause::UserCancel); }
    // Enter键响应
    virtual void onEnterKey() {}
    // Space键响应
    virtual void onSpaceKey() {}
    // 帧事件
    virtual void onFrame(double time) {}

    // 鼠标响应
    virtual void onLeftMouseDown(const MouseEvent& event) {}
    virtual void onLeftMouseUp(const MouseEvent& event) {}
    virtual void onMiddleMouseDown(const MouseEvent& event) {}
    virtual void onMiddleMouseUp(const MouseEvent& event) {}
    virtual void onRightMouseDown(const MouseEvent& event) {}
    virtual void onRightMouseUp(const MouseEvent& event);
    virtual void onMouseMove(const MouseEvent& event) {}
    virtual void onLeftMouseDoubleClicked(const MouseEvent& event) {}
    virtual void onRightMouseDoubleClicked(const MouseEvent& event) {}
    // 键盘响应
    virtual void onKeyDown(const KeyEvent& event);
    virtual void onKeyUp(const KeyEvent& event) {}

    // 上下文菜单
    virtual GuiCmdMenu* initContextMenu();
    virtual bool isContextMenuActionVisible_CompleteSelection() const { return false; }
    virtual void onContextMenuAction_CompleteSelection() {}
    virtual bool isContextMenuActionVisible_ClearSelection() const { return false; }
    virtual void onContextMenuAction_ClearSelection() {}
    // 控件
    virtual GuiCmdControlsSPtr initControls() { return nullptr; }
    // 单击特征树上的元素
    virtual void onFeatureTreeItemClicked(const wydb::ElementId& id) { return; }

    // 屏幕坐标 → 窗口坐标（纯虚，由渲染引擎子类实现）
    virtual void screenToWindowPos(double screenX, double screenY,
        double& windowX, double& windowY) const = 0;

protected:
    void configSelect(GuiCmdSelectOptions options);

    virtual wyap::Selection pointPick(
        double x, double y,
        const PointPickOption& option) = 0;

    virtual std::pair<wy::Vector3, wyap::SnapResultSPtr> computePosition3d(
        double x, double y,
        const wy3d::Plane& sketchPlane,
        const std::set<wydb::ElementId>& excludeIds,
        bool snap = true) = 0;

    virtual wy::Vector2 computePosition2d(
        double x, double y,
        const wy3d::Plane& sketchPlane,
        const std::set<wydb::ElementId>& excludeIds,
        std::shared_ptr<SketchSnapContext> pSnapContext = nullptr,
        SketchSnapSystem* pSketchSnapSys = nullptr,
        bool snap3d = true) = 0;

    virtual wy::Vector2 computePosition2dWithoutSnap(
        double x, double y,
        const wy3d::Plane& sketchPlane) = 0;

    virtual bool computeHeight(double x, double y,
        const wy::Vector3& basePnt, double& height,
        const GuiCmdMakeElement* pMakeElement = nullptr) = 0;
    virtual bool computeHeight(double x, double y,
        const osg::Vec3d& basePnt, double& height,
        const GuiCmdMakeElement* pMakeElement = nullptr) = 0;
    virtual bool computeHeight2(double x, double y,
        const wy3d::Plane& workPln,
        const wy::Vector2& basePnt,
        const std::set<wydb::ElementId>& excludeIds,
        double& height) = 0;

    virtual bool computeRotationAngle(double x, double y,
        const wy3d::Plane& workPln,
        const wy::Vector2& basis,
        const std::set<wydb::ElementId>& excludeIds,
        double& rotationAngle) = 0;

protected:
    // 命令选项
    GuiCmdSelectOptions _options;
    // 捕捉排除项
    std::set<wydb::ElementId> _snapExcludeIds;
    // 上下文菜单
    GuiCmdMenu* _pContextMenu;
    // 控件
    GuiCmdControlsSPtr _pControls;

    friend class GuiCmdMenu;
    friend class FeatureTreeWidget;
};

#endif // WY3DAPP_GUI_CMDS_H
