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

#ifndef WY3DAPP_EXTRUSION_GUI_CMD_H
#define WY3DAPP_EXTRUSION_GUI_CMD_H

#include <cfloat>
#include "commands/OsgGuiCommand.h"
#include <map>
#include <wyVector3.h>
#include <wy3dVector3.h>
#include <wy3dExtrusion.h>
#include <wy3dSketch.h>
#include "commands/transient/ValidSketchTransient.h"
#include "select/SelectPreview.h"

class MakeExtrusion;
class GuiCmdHoverInputPopup2_2ndTabLabel;

class ExtrudeGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(ExtrudeGuiCmd, wy3dApp::ExtrudeGuiCmd, OsgGuiCommand)
public:
    ExtrudeGuiCmd();
    virtual ~ExtrudeGuiCmd();

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
        SpecifySolidToCut = 3,
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

    virtual std::shared_ptr<MakeExtrusion> newMakeExtrusion();

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

    // 点选选项
    PointPickOption _pointPickOption;

    // 预览&提示
    std::shared_ptr<ValidSketchTransient> _pValidSketchPreview;
    std::shared_ptr<InvalidSketchToolTip> _pInvalidSketchTooltip;

    // 草图信息
    struct SketchValidInfo
    {
        bool valid;
        QString error;

        SketchValidInfo() : valid(true) {}
    };
    std::map<wydb::ElementId, SketchValidInfo> _sketchId2ValidInfo;

    // 创建拉伸体
    std::shared_ptr<MakeExtrusion> _pMakeExtrusion;
    std::unique_ptr<GuiCmdHoverInputPopup2_2ndTabLabel> _pDepthPopup;
    HoverPopupState _hoverPopupState;
};

class ExtrudeCutGuiCmd : public ExtrudeGuiCmd
{
    WYRX_DECLARE_MEMBERS(ExtrudeCutGuiCmd, wy3dApp::ExtrudeCutGuiCmd, ExtrudeGuiCmd)
public:
    virtual void reset();
    virtual bool finishStep(Step step) override;
    virtual void gotoStep(Step step) override;

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseDown(const MouseEvent& event) override;
    virtual void onLeftMouseUp(const MouseEvent& event) override;

    virtual std::shared_ptr<MakeExtrusion> newMakeExtrusion() override;

private:
    // 获取用户选择的要切除的实体
    const wy3d::Solid* getSolidToCut() const;

private:
    // 预览
    SelectPreviewSPtr _pSolidToCutPreview;
};

class MakeExtrusion : public GuiCmdMakeElement
{
public:
    MakeExtrusion(GuiCommand* pGuiCmd, bool isCut)
        : GuiCmdMakeElement(pGuiCmd), _isCut(isCut), _pExtrusion(nullptr), _workPlnNormal(0.0, 0.0, 1.0), _direction(wy3d::ExtrusionDirection::OneSide) {}
    ~MakeExtrusion() {}

    // 收集创建的元素ID
    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

    // 获取工作平面法向
    virtual wy::Vector3 getWorkingPlaneNormal() const override
    {
        return _workPlnNormal;
    }

    // 创建
    bool init(const wydb::ElementId& sketchId, unsigned int& errorCode);
    // 更新
    bool update(double depth);
    // 更新方向
    bool setDirection(wy3d::ExtrusionDirection direction);
    // 切除实体
    bool cutSolid(const wy3d::Solid* pConstSolidToCut, unsigned int& errorCode);

private:
    bool _isCut;
    wy3d::Extrusion* _pExtrusion;
    wy::Vector3 _workPlnNormal;
    wy3d::ExtrusionDirection _direction;
};

#endif // WY3DAPP_EXTRUSION_GUI_CMD_H
