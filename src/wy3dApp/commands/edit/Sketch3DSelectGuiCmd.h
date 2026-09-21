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

#ifndef WY3DAPP_SKETCH3D_SELECT_GUI_CMD_H
#define WY3DAPP_SKETCH3D_SELECT_GUI_CMD_H

#include "commands/SelectGuiCmd.h"

class SelectHandler;

class Sketch3DSelectGuiCmd : public SelectGuiCmd
{
    WYRX_DECLARE_MEMBERS(Sketch3DSelectGuiCmd, Sketch3DSelectGuiCmd, SelectGuiCmd)
public:
    Sketch3DSelectGuiCmd();
    virtual ~Sketch3DSelectGuiCmd();

    // Context menu
    virtual GuiCmdMenu* initContextMenu() override;

protected:
    GuiCmdSketch3DInfo _sketch3DInfo;
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;
    virtual void configureSelectOptions(GuiCmdSelectOptions& options) override;
    virtual void onStart_EnvSpecific() override;
    virtual void selectAll_Impl(wyap::SelectionSet& ss) override;
    virtual bool tryAddPositionGizmo_Impl(const wyap::SelectionSet& sels, std::list<wyap::GizmoSPtr>& gizmos) override;
    virtual void onKeyDown(const KeyEvent& event) override;
    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseDown(const MouseEvent& event) override;
    virtual void onLeftMouseUp(const MouseEvent& event) override;
    virtual void onEscapeKey() override;
    virtual void updateSelectTipAndLabel() override;

private:
    bool tryPickWorkPlane(double x, double y);
    void clearPendingWorkPlane();
    void setOverCurvedFace(bool value);
    // Would a click here select a sketch entity? Asks the selection channel itself.
    bool isEntityUnderPointer(double x, double y);
    SelectHandler* getSelectHandler() const;

private:
    // Pick of the faces and datum planes that may become the work plane
    PointPickOption _planePickOption;
    // Hover preview of the face under the pointer
    SelectPreviewSPtr _pPlanePreview;
    // Highlight of the face picked as the work plane, held until the drawing command takes over
    SelectionSetHighlightorSPtr _pCustomWorkPlaneHighlight;
    float _leftDownX;
    float _leftDownY;
    bool _isOverCurvedFace;
};

#endif // WY3DAPP_SKETCH3D_SELECT_GUI_CMD_H
