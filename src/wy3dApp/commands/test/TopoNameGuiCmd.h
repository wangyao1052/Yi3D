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

#ifndef WY3DAPP_TOPONAME_GUI_CMD_H
#define WY3DAPP_TOPONAME_GUI_CMD_H

#include <TopAbs_ShapeEnum.hxx>

#include <wy3dApp/OsgGuiCommand.h>
#include <wy3dApp/SelectPreview.h>
#include "select/SelectionSetHighlightor.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class TopoNameGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(TopoNameGuiCmd, wy3dApp::TopoNameGuiCmd, OsgGuiCommand)
public:
    TopoNameGuiCmd();
    virtual ~TopoNameGuiCmd();

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;

protected:
    virtual void reset();

    virtual void onMouseMove(const MouseEvent& event) override;

    // Context menu
    virtual void onBuildContextMenu(ContextMenu& menu) override;
    virtual void onContextMenuAction_CompleteSelection() override;
    virtual void onContextMenuAction_ClearSelection() override;

private:
    void showTopoName(const wyap::Selection& sel);

    void addSelection(TopAbs_ShapeEnum shapeType, const std::string& topoName);
    void onFindEdgesByTopoName();
    void onFindFacesByTopoName();

private:
    // 点选选项
    PointPickOption _pointPickOption;

    // 预览
    SelectPreview _preview;
    // 高亮
    SelectionSetHighlightorSPtr _pSelSetHighlightor;
};

} // namespace wy3dApp

#endif // WY3DAPP_TOPONAME_GUI_CMD_H
