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

#ifndef WY3DAPP_FILLET_GUI_CMD_H
#define WY3DAPP_FILLET_GUI_CMD_H

#include <vector>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include "commands/OsgGuiCommand.h"
#include <wy3dFillet.h>
#include "select/SelectPreview.h"
#include "select/SelectionSetHighlightor.h"
#include "commands/GuiCommandMenu.h"

class FilletGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(FilletGuiCmd, wy3dApp::FilletGuiCmd, OsgGuiCommand)
public:
    FilletGuiCmd();
    virtual ~FilletGuiCmd();

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;

    virtual void cleanup() override { this->reset(); }

protected:
    enum class Step
    {
        Undefined = 0,
        SelectEdges = 1,
        InputFilletRadius = 2,
    };
    virtual void reset();
    bool finishStep(Step step);
    void gotoStep(Step step);

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseUp(const MouseEvent& event) override;

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
    // 创建圆角
    bool createFillet(unsigned int& errorCode);

    // 建立宿主拓扑缓存(仅在宿主变化时重建): 悬停很频繁, 不能每帧重建
    bool ensureHostTopo(const wydb::ElementId& hostId);
    // 检查选择项: 通过时 outSels 为要加入选择集的项(片体上的面展开成边)
    bool resolveFilletPick(const wyap::Selection& sel, std::vector<wyap::Selection>& outSels);
    // 全部已选中则一起取消, 否则一起选中
    void toggleSelections(const std::vector<wyap::Selection>& sels);

    // 悬停预览: 不可圆角的边/面不高亮, 光标变禁止(照拉伸命令)
    void updateHoverPreview(double x, double y);

private:
    Step _step;
    wyap::SelectionSet _sels;
    double _radius;

    // 点选选项
    PointPickOption _pointPickOption;

    // 预览
    SelectPreviewSPtr _pPreview;
    // 高亮
    SelectionSetHighlightorSPtr _pSelSetHighlightor;

    // 宿主拓扑缓存
    wydb::ElementId _hostId;
    bool _hostIsSheet;
    TopTools_IndexedMapOfShape _hostFaces;
    TopTools_IndexedMapOfShape _hostEdges;
    TopTools_IndexedDataMapOfShapeListOfShape _hostEdgeFaces;

    friend class FilletGuiCmdMenu;
};

#endif // WY3DAPP_FILLET_GUI_CMD_H
