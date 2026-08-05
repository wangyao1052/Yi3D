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

#ifndef WY3DAPP_MAKE_PRIMITIVE_GUI_CMD_H
#define WY3DAPP_MAKE_PRIMITIVE_GUI_CMD_H

#include "commands/OsgGuiCommand.h"
#include <wy3dVector3.h>
#include <wyapSelManager.h>
#include <wy3dBox.h>
#include "select/SelectPreview.h"
#include "commands/GuiCommandMenu.h"

class MakePrimitiveGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(MakePrimitiveGuiCmd, MakePrimitiveGuiCmd, OsgGuiCommand)
public:
    MakePrimitiveGuiCmd();
    virtual ~MakePrimitiveGuiCmd();

    // 获取工作步骤
    unsigned int getStep() const { return _step; }
    // 获取工作平面
    const wy3d::SketchPlane& getWorkingPlane() const { return _workPln; }

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;

protected:
    enum Step
    {
        Undefined = 0,
        SpecifyWorkingPlane = 1,
        SpecifyPnt1 = 2,
        SpecifyPnt2 = 3,
        SpecifyPnt3 = 4,
        SpecifyPnt4 = 5,
    };
    virtual void reset();
    virtual bool finishStep(unsigned int step);
    virtual void gotoStep(unsigned int step);
    virtual void gotoStepImpl(unsigned int step);

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseDown(const MouseEvent& event) override;

    // 上下文菜单
    virtual GuiCmdMenu* initContextMenu() override;

    // 单击特征树上的元素
    virtual void onFeatureTreeItemClicked(const wydb::ElementId& id) override;

protected:
    // 步骤
    unsigned int _step;
    // 点选选项
    PointPickOption _pickOption;
    // 捕捉排除元素
    std::set<wydb::ElementId> _excludeIds;
    // 工作平面
    wy3d::SketchPlane _workPln;
    // 工作面预览
    SelectPreviewSPtr _pWorkPlnPreview;
    // 工作平面原点捕捉对象
    wyap::SnapObjectSPtr _pWorkPlnOriginSnapObject;
};

class MakePrimitiveGuiCmdMenu : public GuiCmdMenu
{
    Q_OBJECT
public:
    explicit MakePrimitiveGuiCmdMenu(MakePrimitiveGuiCmd* pCmd) : GuiCmdMenu(pCmd) {}

protected:
    // 初始化客制化菜单项
    virtual bool initCustomHeaderActions(QMenu* menu) override;

private slots:
    void onViewNormalToWorkingPlane();
};

#endif // WY3DAPP_MAKE_PRIMITIVE_GUI_CMD_H