///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 Wang Yao <wangyao1052@163.com>
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

#ifndef WY3DAPP_SKETCH3D_DRAW_GUI_CMD_H
#define WY3DAPP_SKETCH3D_DRAW_GUI_CMD_H

#include "commands/OsgGuiCommand.h"
#include <wy3dSketchPlane.h>
#include "snap/SnapObject.h"
#include "commands/GuiCommandMenu.h"
#include "utils/GuiCommandUtil.h"
#include <memory>

class Sketch3DPlaneLabel;
class Sketch3DSnapSystem;

// 3D草图工作平面:显示CSYS + 平面原点驻留吸附;支持移动原点与循环切换平面
class Sketch3DWorkingPlane
{
public:
    Sketch3DWorkingPlane();
    explicit Sketch3DWorkingPlane(const wy3d::SketchPlane& plane);
    ~Sketch3DWorkingPlane();

    Sketch3DWorkingPlane(const Sketch3DWorkingPlane&) = delete;
    Sketch3DWorkingPlane& operator=(const Sketch3DWorkingPlane&) = delete;

    void show();
    void moveTo(const wy::Vector3& pos);
    void switchToNext();

    const wy3d::SketchPlane& getWorkingPlane() const
    {
        return _workingPlane;
    }

    // 平面循环索引(0=XY, 1=YZ, 2=ZX,相对构造时的基架)
    unsigned int cycleIndex() const { return _i; }

private:
    void showImpl(const wy3d::SketchPlane& plane);

private:
    wy::Vector3 _xDir;
    wy::Vector3 _yDir;
    wy::Vector3 _zDir;
    wy3d::SketchPlane _workingPlane;
    bool _isValid;
    unsigned int _i;
    wyap::SnapObjectSPtr _pOriginSnapObject;

    std::unique_ptr<Sketch3DPlaneLabel> _pPlaneLabel;
};

class Sketch3DDrawGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(Sketch3DDrawGuiCmd, Sketch3DDrawGuiCmd, OsgGuiCommand)
public:
    Sketch3DDrawGuiCmd();
    virtual ~Sketch3DDrawGuiCmd();

    // 获取工作平面
    const wy3d::SketchPlane& getWorkingPlane() const;

    // 获取3D草图捕捉体系(当前活动环境为3D草图环境时有效,否则空)
    Sketch3DSnapSystem* getSketch3DSnapSystem() const;

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;

protected:

    // Space: switch the working plane (also used when the hover popup has focus)
    virtual void onSpaceKey() override;

    void moveWorkPlaneOriginTo(const wy::Vector3& pnt);

    virtual GuiCmdMenu* initContextMenu() override;

protected:
    GuiCmdSketch3DInfo _sketch3DInfo;
    std::unique_ptr<Sketch3DWorkingPlane> _pWorkPlane;
    SelectionSetHighlightorSPtr _pCustomWorkPlaneHighlight;
};

class Sketch3DDrawGuiCmdMenu : public GuiCmdMenu
{
    Q_OBJECT
public:
    explicit Sketch3DDrawGuiCmdMenu(Sketch3DDrawGuiCmd* pCmd) : GuiCmdMenu(pCmd) {}

protected:
    // 初始化客制化菜单项
    virtual bool initCustomHeaderActions(QMenu* menu) override;

private slots:
    void onViewNormalToWorkingPlane();
};

#endif // WY3DAPP_SKETCH3D_DRAW_GUI_CMD_H
