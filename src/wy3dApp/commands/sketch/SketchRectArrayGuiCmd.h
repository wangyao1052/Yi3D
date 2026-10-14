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

#ifndef WY3DAPP_SKETCH_RECT_ARRAY_GUI_CMD_H
#define WY3DAPP_SKETCH_RECT_ARRAY_GUI_CMD_H

#include <wy3dApp/OsgGuiCommand.h>
#include <cfloat>
#include <memory>
#include <set>
#include <vector>

#include <osg/PositionAttitudeTransform>
#include <wyVector2.h>
#include <wyapSelection.h>
#include <wy3dVector2.h>
#include <wy3dVector3.h>

#include "commands/GuiCommandMenu.h"

#include "commands/sketch/SketchRectArrayElements.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class LineTransient;
class GuiCmdHoverInputPopup1;

class SketchRectArrayGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(SketchRectArrayGuiCmd, wy3dApp::SketchRectArrayGuiCmd, OsgGuiCommand)
public:
    SketchRectArrayGuiCmd();
    virtual ~SketchRectArrayGuiCmd();

    static bool isValidRowsCols(unsigned int cols, unsigned int rows);

protected:
    GuiCmdSketchInfo _sketchInfo;
    virtual wyap::CmdExecution::StartResult onStart() override;

protected:
    enum class Step
    {
        Undefined = 0,
        Step1_SelectElements = 1,
        Step2_SpecifyRowsCols = 2,
        Step3_SpecifyColumnSpacing_StartPnt = 3,
        Step4_SpecifyColumnSpacing_EndPnt = 4,
        Step5_SpecifyRowSpacing_StartPnt = 5,
        Step6_SpecifyRowSpacing_EndPnt = 6
    };

    virtual void cleanUp() override;
    virtual void reset();
    virtual void onEscapeKey() override;
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
    void computeDefaultColRowSpacing(const std::set<wydb::ElementId>& ids, double& colSpacing, double& rowSpacing) const;

    void initializePopups();
    void showPopup();
    void hidePopup();
    GuiCmdHoverInputPopup1* getActivePopup() const;
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
        double value;
        int colSign;
        int rowSign;

        HoverPopupState()
            : lastMouseX(DBL_MAX),
            lastMouseY(DBL_MAX),
            lastMouseMoveTime(-1.0),
            value(0.0),
            colSign(1),
            rowSign(1)
        {
        }

        void resetValue()
        {
            value = 0.0;
            colSign = 1;
            rowSign = 1;
        }
    };

private:
    Step _step;
    std::set<wydb::ElementId> _ids;
    unsigned int _cols;
    unsigned int _rows;
    wy::Vector2 _colSpacingStartPnt;
    double _colSpacing;
    wy::Vector2 _rowSpacingStartPnt;
    double _rowSpacing;
    SketchSnapContextSPtr _pSnapContext;

    std::shared_ptr<SketchRectArrayElements> _pRectArray;
    std::shared_ptr<LineTransient> _pLineTransient;

    std::unique_ptr<GuiCmdHoverInputPopup1> _pColSpacingPopup;
    std::unique_ptr<GuiCmdHoverInputPopup1> _pRowSpacingPopup;
    HoverPopupState _hoverPopupState;

    friend class SketchRectArrayGuiCmdMenu;
};

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_RECT_ARRAY_GUI_CMD_H
