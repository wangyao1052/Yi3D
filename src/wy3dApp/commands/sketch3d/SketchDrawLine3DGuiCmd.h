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

#ifndef WY3DAPP_SKETCH_DRAW_LINE3D_GUI_CMD_H
#define WY3DAPP_SKETCH_DRAW_LINE3D_GUI_CMD_H

#include "commands/sketch3d/Sketch3DDrawGuiCmd.h"
#include <cfloat>
#include <memory>
#include <set>
#include <utility>
#include <wyVector3.h>
#include <wydbElementId.h>
#include <wy3dSketchLine3D.h>

class Sketch3DSnapContext;

class GuiCmdHoverInputPopup2;
class GuiCmdHoverInputPopup3;
class MakeSketchLine3D;

class SketchDrawLine3DGuiCmd : public Sketch3DDrawGuiCmd
{
    WYRX_DECLARE_MEMBERS(SketchDrawLine3DGuiCmd, SketchDrawLine3DGuiCmd, Sketch3DDrawGuiCmd)
public:
    SketchDrawLine3DGuiCmd();
    virtual ~SketchDrawLine3DGuiCmd();

protected:
    enum class Step
    {
        SpecifyStartPnt = 1,
        SpecifyEndPnt = 2,
    };
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void cleanup() override;
    virtual void onEscapeKey() override;
    bool finishStep(unsigned int step);
    void gotoStep(unsigned int step);

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseDown(const MouseEvent& event) override;
    void onFrame(double time) override;

private:
    std::set<wydb::ElementId> getSnapExcludeIds() const;
    std::pair<wy::Vector3, bool> computePoint3d(double x, double y);

    void initializePopups();
    void showPopup();
    void hidePopup();
    void tryShowPopupOnHover(double time);
    void onPopupEnterKey();
    void onPopupEscapeKey();
    void onPopupSpaceKey();
    void simulateMouseMoveFromPopup();

private:
    struct HoverPopupState
    {
        double lastMouseX;
        double lastMouseY;
        double lastMouseMoveTime;

        wy::Vector3 point;
        double length;
        double angleDeg;
        bool snapped;

        HoverPopupState()
            : lastMouseX(DBL_MAX)
            , lastMouseY(DBL_MAX)
            , lastMouseMoveTime(-1.0)
            , point()
            , length(0.0)
            , angleDeg(0.0)
            , snapped(false)
        {
            this->resetValue();
        }

        void resetValue()
        {
            point.set(0.0, 0.0, 0.0);
            length = 0.0;
            angleDeg = 0.0;
            snapped = false;
        }
    };

private:
    unsigned int _step;
    wy::Vector3 _startPnt;
    wy::Vector3 _endPnt;
    std::shared_ptr<MakeSketchLine3D> _pMakeSketchLine3D;
    // 捕捉上下文(起点步=Locate,终点步=DrawLine,仿2D命令的_pSnapContext)
    std::shared_ptr<Sketch3DSnapContext> _pSnapContext;

    std::unique_ptr<GuiCmdHoverInputPopup3> _pXYZPopup;
    std::unique_ptr<GuiCmdHoverInputPopup2> _pLengthAnglePopup;
    HoverPopupState _hoverPopupState;
};

class MakeSketchLine3D : public GuiCmdMakeElement
{
public:
    MakeSketchLine3D(GuiCommand* pGuiCmd) : GuiCmdMakeElement(pGuiCmd), _pSketchLine3D(nullptr) {}
    ~MakeSketchLine3D() {}

    // 收集创建的元素ID
    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

    wydb::ElementId getId() const
    {
        return _pSketchLine3D ? _pSketchLine3D->getId() : wydb::ElementId::kNull;
    }

    // 创建
    bool init(const wy::Vector3& startPnt, wydb::ElementId sketch3dId);
    // 更新
    bool update(const wy::Vector3& endPnt);

private:
    wy3d::SketchLine3D* _pSketchLine3D;
};

#endif // WY3DAPP_SKETCH_DRAW_LINE3D_GUI_CMD_H
