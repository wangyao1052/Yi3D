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

#ifndef WY3DAPP_SKETCH_DRAW_RECTANGLE_GUI_CMD_H
#define WY3DAPP_SKETCH_DRAW_RECTANGLE_GUI_CMD_H

#include <wy3dApp/OsgGuiCommand.h>
#include <cfloat>
#include <memory>
#include <set>
#include <wyVector2.h>
#include <wy3dVector2.h>
#include <wydbElementId.h>
#include <wy3dSketchLine.h>

class MakeSketchRectangle;
class GuiCmdHoverInputPopup2;

class SketchDrawRectangleGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(SketchDrawRectangleGuiCmd, SketchDrawRectangleGuiCmd, OsgGuiCommand)
public:
    SketchDrawRectangleGuiCmd();
    virtual ~SketchDrawRectangleGuiCmd();

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
        double width;

        HoverPopupState() :
            lastMouseX(DBL_MAX),
            lastMouseY(DBL_MAX),
            lastMouseMoveTime(-1.0),
            point(),
            length(0.0),
            width(0.0)
        {
            this->resetValue();
        }

        void resetValue()
        {
            point.set(0.0, 0.0);
            length = 0.0;
            width = 0.0;
        }
    };

private:
    Step _step;
    wy::Vector2 _startPnt;
    wy::Vector2 _endPnt;
    SketchSnapContextSPtr _pSnapContext;
    // 悬停浮窗
    std::unique_ptr<GuiCmdHoverInputPopup2> _pXYPopup;
    std::unique_ptr<GuiCmdHoverInputPopup2> _pLengthWidthPopup;
    HoverPopupState _hoverPopupState;
    std::shared_ptr<MakeSketchRectangle> _pMakeSketchRectangle;
};

class MakeSketchRectangle : public GuiCmdMakeElement
{
public:
    enum class Mode
    {
        CornerRect = 0,
        CenterRect = 1,
    };

public:
    MakeSketchRectangle(GuiCommand* pGuiCmd, Mode mode, wydb::ElementId sketchId) : GuiCmdMakeElement(pGuiCmd), _mode(mode),
        _sketchId(sketchId),
        _pSketchLine1st(nullptr), _pSketchLine2nd(nullptr), _pSketchLine3rd(nullptr), _pSketchLine4th(nullptr)
    {}
    ~MakeSketchRectangle() {}

    // 收集创建的元素ID
    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

    // 创建
    bool init(const wy::Vector2& startPnt);
    // 更新
    bool update(const wy::Vector2& endPnt);

private:
    bool checkValid(
        Mode mode,
        const wy::Vector2& startPnt,
        const wy::Vector2& endPnt) const;

    void computeRectEndPoints(
        Mode mode,
        const wy::Vector2& startPnt,
        const wy::Vector2& endPnt,
        wy::Vector2& pnt1,
        wy::Vector2& pnt2,
        wy::Vector2& pnt3,
        wy::Vector2& pnt4) const;

private:
    Mode _mode;
    wydb::ElementId _sketchId;
    wy::Vector2 _startPnt;
    wy3d::SketchLine* _pSketchLine1st;
    wy3d::SketchLine* _pSketchLine2nd;
    wy3d::SketchLine* _pSketchLine3rd;
    wy3d::SketchLine* _pSketchLine4th;
};

#endif // WY3DAPP_SKETCH_DRAW_RECTANGLE_GUI_CMD_H
