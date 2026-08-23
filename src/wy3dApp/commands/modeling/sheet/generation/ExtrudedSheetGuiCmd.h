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

#ifndef WY3DAPP_EXTRUDED_SHEET_GUI_CMD_H
#define WY3DAPP_EXTRUDED_SHEET_GUI_CMD_H

#include <cfloat>
#include "commands/OsgGuiCommand.h"
#include "commands/GuiCmdMakeElement.h"
#include <map>
#include <wyVector3.h>
#include <wy3dVector3.h>
#include <wy3dExtrudedSheet.h>
#include <wy3dSketch.h>
#include "commands/transient/ValidSketchTransient.h"

class GuiCmdHoverInputPopup2_2ndTabLabel;

class MakeExtrudedSheet : public GuiCmdMakeElement
{
public:
    MakeExtrudedSheet(GuiCommand* pGuiCmd)
        : GuiCmdMakeElement(pGuiCmd), _pExtrudedSheet(nullptr), _workPlnNormal(0.0, 0.0, 1.0), _direction(wy3d::ExtrusionDirection::OneSide) {}
    ~MakeExtrudedSheet() {}

    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

    virtual wy::Vector3 getWorkingPlaneNormal() const override
    {
        return _workPlnNormal;
    }

    bool init(const wydb::ElementId& sketchId, unsigned int& errorCode);
    bool update(double depth);
    bool setDirection(wy3d::ExtrusionDirection direction);

private:
    wy3d::ExtrudedSheet* _pExtrudedSheet;
    wy::Vector3 _workPlnNormal;
    wy3d::ExtrusionDirection _direction;
};

class ExtrudedSheetGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(ExtrudedSheetGuiCmd, wy3dApp::ExtrudedSheetGuiCmd, OsgGuiCommand)
public:
    ExtrudedSheetGuiCmd();
    virtual ~ExtrudedSheetGuiCmd();

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;

protected:
    enum class Step
    {
        Undefined = 0,
        SelectSketch = 1,
        SpecifyDepth = 2,
    };
    virtual void cleanup() override;
    virtual void reset();
    virtual bool finishStep(Step step);
    virtual void gotoStep(Step step);

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseDown(const MouseEvent& event) override;
    virtual void onLeftMouseUp(const MouseEvent& event) override;
    virtual void onFeatureTreeItemClicked(const wydb::ElementId& id) override;
    void onFrame(double time) override;

private:
    bool isValidSketchSelectionSet(const wyap::SelectionSet& ss, wydb::ElementId& sketchId);
    bool isValidSketch(const wydb::ElementId& sketchId, QString& error);
    void preview(wydb::ElementId sketchId);

protected:
    void initializePopups();
    void showPopup();
    void hidePopup();
    void tryShowPopupOnHover(double time);
    void onPopupEnterKey();
    void onPopupEscapeKey();
    void simulateMouseMoveFromPopup();
    void updateDirectionLabel();

protected:
    struct HoverPopupState
    {
        double lastMouseX;
        double lastMouseY;
        double lastMouseMoveTime;
        double depth;
        int depthSign;

        HoverPopupState()
            : lastMouseX(DBL_MAX)
            , lastMouseY(DBL_MAX)
            , lastMouseMoveTime(-1.0)
            , depth(0.0)
            , depthSign(1)
        {}

        void resetValue()
        {
            depth = 0.0;
            depthSign = 1;
        }
    };

protected:
    Step _step;
    wydb::ElementId _sketchId;
    wy::Vector3 _pickPos;
    double _depth;
    wy3d::ExtrusionDirection _direction;

    PointPickOption _pointPickOption;

    std::shared_ptr<ValidSketchTransient> _pValidSketchPreview;
    std::shared_ptr<InvalidSketchToolTip> _pInvalidSketchTooltip;

    struct SketchValidInfo
    {
        bool valid;
        QString error;

        SketchValidInfo() : valid(true) {}
    };
    std::map<wydb::ElementId, SketchValidInfo> _sketchId2ValidInfo;

    std::shared_ptr<MakeExtrudedSheet> _pMakeExtrudedSheet;
    std::unique_ptr<GuiCmdHoverInputPopup2_2ndTabLabel> _pDepthPopup;
    HoverPopupState _hoverPopupState;
};

#endif // WY3DAPP_EXTRUDED_SHEET_GUI_CMD_H
