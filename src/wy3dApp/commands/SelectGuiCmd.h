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

#ifndef WY3DAPP_SELECT_GUI_CMD_H
#define WY3DAPP_SELECT_GUI_CMD_H

#include <list>
#include <memory>
#include <wyVector3.h>
#include <wyapSelManager.h>
#include <wyapGizmoManager.h>
#include "commands/OsgGuiCommand.h"
#include "commands/GuiCommandMenu.h"

class ElementNode;
#include "commands/edit/PasteElements.h"

class SelectGuiCmdMenu : public GuiCmdMenu
{
public:
    explicit SelectGuiCmdMenu(GuiCommand* pCmd);

protected:
    virtual bool initCustomMiddleActions(QMenu* menu) override;

private:
    void onCopy();
    void onPaste();
};

class SelectGuiCmd : public OsgGuiCommand, public wyap::SelManagerReactor
{
    WYRX_DECLARE_MEMBERS(SelectGuiCmd, SelectGuiCmd, OsgGuiCommand)

    friend class SelectGuiCmdMenu;
public:
    SelectGuiCmd();
    virtual ~SelectGuiCmd();

    // Context menu
    virtual GuiCmdMenu* initContextMenu() override;

    // 选择集变更
    virtual void onSelectionChanged(
        const wyap::SelectionSet& addedSS,
        const wyap::SelectionSet& removedSS,
        const wyap::SelectionSet& currSS) override;

protected:
    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseDown(const MouseEvent& event) override;
    virtual void onKeyDown(const KeyEvent& event) override;
    virtual void onEscapeKey() override;

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;

    // 重置
    virtual void reset();

    // Enter键响应
    virtual void onEnterKey() override;
    // Space键响应
    virtual void onSpaceKey() override;

private:
    void selectAll();
    void copy();
    void erase();

    void updateSelectTipAndLabel();
    bool beginPaste(double x, double y);
    void endPaste(const wy::Vector3& pos);
    void cancelPaste();

    // 添加移动Gizmo
    bool tryAddElementPositionGizmo(const wyap::SelectionSet& sels);

    // 启用选择
    void enableSelect();
    // 禁用选择
    void disableSelect();

protected:
    struct PasteOp
    {
        SketchSnapContextSPtr pSnapContext;
        std::shared_ptr<PasteElements> pPasteElements;
        std::set<wydb::ElementId> excludeIds;
    };
    typedef std::shared_ptr<PasteOp> PasteOpSPtr;
    PasteOpSPtr _pPasteOp;
    // === 子类覆写的虚方法 ===

    // 配置选择(enableSelect)时的 pickMask & filter
    virtual void configureSelectOptions(GuiCmdSelectOptions& options) {}

    // 启动时环境特定操作
    virtual void onStart_EnvSpecific() {}

    // 全选实现(填充 ss)
    virtual void selectAll_Impl(wyap::SelectionSet& ss) {}

    // 添加位置 Gizmo(填充 gizmos). 返回 false 表示跳过
    virtual bool tryAddPositionGizmo_Impl(const wyap::SelectionSet& sels, std::list<wyap::GizmoSPtr>& gizmos) { return true; }

    // 粘贴时获取环境类型
    virtual GuiCmdEnvType getPasteEnvType() const { return GuiCmdEnvType::Modeling; }

    // 粘贴时计算位置
    virtual wy::Vector3 computePastePosition(double x, double y,
        const std::set<wydb::ElementId>& excludeIds) { return wy::Vector3(); }

    // 获取草图信息(草图子类覆写返回实际值，建模返回默认)
    virtual SketchSnapSystem* getSketchSnapSys() const { return nullptr; }
    virtual wydb::ElementId getSketchId() const { return wydb::ElementId::kNull; }
    virtual const wy3d::SketchPlane& getSketchPlane() const { static wy3d::SketchPlane s; return s; }
};

#endif // WY3DAPP_SELECT_GUI_CMD_H