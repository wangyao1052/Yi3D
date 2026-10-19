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

#ifndef WY3DAPP_SKETCH_DRAW_LINE_GUI_CMD_H
#define WY3DAPP_SKETCH_DRAW_LINE_GUI_CMD_H

#include <wy3dApp/OsgGuiCommand.h>
#include <cfloat>
#include <memory>
#include <set>
#include <wyVector2.h>
#include <wy3dVector2.h>
#include <wydbElementId.h>
#include <wy3dSketchLine.h>
#include <wy3dApp/Snap2dContext.h>
#include <wy3dApp/wy3dAppDefs.h>
#include "snap/Snap2dContextInternal.h"

namespace wy3dApp {

class MakeSketchLine;
class GuiCmdHoverInputPopup2;

class SketchDrawLineGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(SketchDrawLineGuiCmd, wy3dApp::SketchDrawLineGuiCmd, OsgGuiCommand)
public:
    SketchDrawLineGuiCmd();
    virtual ~SketchDrawLineGuiCmd();

protected:
    GuiCmdSketchInfo _sketchInfo;
    virtual wyap::CmdExecution::StartResult onStart() override;

protected:
    enum class Step
    {
        Undefined = 0,
        SpecifyStartPnt = 1,
        SpecifyEndPnt = 2,
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
    std::set<wydb::ElementId> getSnapExcludeIds() const;

    void initializePopups();
    void showPopup();
    void hidePopup();
    GuiCmdHoverInputPopup2* getActivePopup() const;
    void tryShowPopupOnHover(double time);
    void onPopupEnterKey();
    void onPopupEscapeKey();
    void simulateMouseMoveFromPopup();

    // 计算捕捉起点切线
    SnapStartTangent computeSnapStartTangent() const;

private:
    struct HoverPopupState
    {
        // 鼠标信息
        double lastMouseX;
        double lastMouseY;
        double lastMouseMoveTime;

        // 值信息
        wy::Vector2 point;
        double length;
        double angleDeg;

        HoverPopupState() :
            lastMouseX(DBL_MAX),
            lastMouseY(DBL_MAX),
            lastMouseMoveTime(-1.0),
            point(),
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

private:
    Step _step;
    wy::Vector2 _startPnt;
    SnapStartTangent _startTangent;
    wy::Vector2 _endPnt;
    Snap2dContextSPtr _pSnapContext;
    // 悬停浮窗
    std::unique_ptr<GuiCmdHoverInputPopup2> _pXYPopup;
    std::unique_ptr<GuiCmdHoverInputPopup2> _pLengthAnglePopup;
    HoverPopupState _hoverPopupState;
    // 创建SketchLine
    std::shared_ptr<MakeSketchLine> _pMakeSketchLine;
};

class MakeSketchLine : public GuiCmdMakeElement
{
public:
    MakeSketchLine(GuiCommand* pGuiCmd) : GuiCmdMakeElement(pGuiCmd), _pSketchLine(nullptr) {}
    ~MakeSketchLine() {}

    // 收集创建的元素ID
    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

    wydb::ElementId getId() const
    {
        return _pSketchLine ? _pSketchLine->getId() : wydb::ElementId::kNull;
    }

    // 创建
    bool init(const wy::Vector2& startPnt, wydb::ElementId sketchId);
    // 更新
    bool update(const wy::Vector2& endPnt);

private:
    wy3d::SketchLine* _pSketchLine;
};

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_DRAW_LINE_GUI_CMD_H
