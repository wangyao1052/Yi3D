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

#ifndef WY3DAPP_SKETCH_DRAW_CENTER_LINE_GUI_CMD_H
#define WY3DAPP_SKETCH_DRAW_CENTER_LINE_GUI_CMD_H

#include <wy3dApp/OsgGuiCommand.h>
#include <cfloat>
#include <memory>
#include <set>
#include <wyVector2.h>
#include <wy3dVector2.h>
#include <wydbElementId.h>
#include <wy3dSketchCenterLine.h>

class MakeSketchCenterLine;
class GuiCmdHoverInputPopup2;

class SketchDrawCenterLineGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(SketchDrawCenterLineGuiCmd, SketchDrawCenterLineGuiCmd, OsgGuiCommand)
public:
    SketchDrawCenterLineGuiCmd();
    virtual ~SketchDrawCenterLineGuiCmd();

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
    virtual void cleanup() override;
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

private:
    struct HoverPopupState
    {
        // 鼠标信息
        double lastMouseX;
        double lastMouseY;
        double lastMouseMoveTime;

        // 数值信息
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
    wy::Vector2 _endPnt;
    SketchSnapContextSPtr _pSnapContext;

    // 悬停浮窗
    std::unique_ptr<GuiCmdHoverInputPopup2> _pXYPopup;
    std::unique_ptr<GuiCmdHoverInputPopup2> _pLengthAnglePopup;
    HoverPopupState _hoverPopupState;

    // 创建中心线
    std::shared_ptr<MakeSketchCenterLine> _pMakeSketchLine;
};

class MakeSketchCenterLine : public GuiCmdMakeElement
{
public:
    MakeSketchCenterLine(GuiCommand* pGuiCmd) : GuiCmdMakeElement(pGuiCmd), _pSketchLine(nullptr) {}
    ~MakeSketchCenterLine() {}

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
    wy3d::SketchCenterLine* _pSketchLine;
};

#endif // WY3DAPP_SKETCH_DRAW_CENTER_LINE_GUI_CMD_H
