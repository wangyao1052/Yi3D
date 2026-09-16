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

#ifndef WY3DAPP_SPLIT_FACE_GUI_CMD_H
#define WY3DAPP_SPLIT_FACE_GUI_CMD_H

#include <vector>
#include <wydbElementId.h>
#include "commands/OsgGuiCommand.h"
#include "select/SelectPreview.h"
#include "select/SelectionSetHighlightor.h"

// Divides faces of a solid along 3D sketch curves: pick the faces, then pick the sketch whose
// curves run across them, and the curves become edges of the body with the faces split along
// them. The curves are the ones the Intersection Curve command leaves behind, so the two chain.
// The whole sketch is the tool, the way a profile sketch is consumed whole, so picking it is the
// whole of the second step - the feature is made there and then, no confirm key. One run makes
// one feature, hence one undo step.
class SplitFaceGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(SplitFaceGuiCmd, wy3dApp::SplitFaceGuiCmd, OsgGuiCommand)
public:
    SplitFaceGuiCmd();

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;

protected:
    enum class Step
    {
        Undefined = 0,
        SelectFaces = 1,
        SelectSketch = 2,
    };
    virtual void cleanup() override;
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
    // 拾取作为分割工具的3D草图: 拾取成功即记下草图, 由调用方随即执行
    bool pickSketch(const wyap::Selection& sel);

    // 创建分割面
    bool createSplitFace(
        const wyap::SelectionSet& faceSels,
        const wydb::ElementId& sketchId,
        unsigned int& errorCode);

private:
    Step _step;
    wyap::SelectionSet _faceSels;
    // 作为分割工具的3D草图
    wydb::ElementId _sketchId;

    // 点选选项
    PointPickOption _pointPickOption;
    // 预览
    SelectPreviewSPtr _pPreview;
    // 高亮
    SelectionSetHighlightorSPtr _pSelSetHighlightor_Faces;
};

#endif // WY3DAPP_SPLIT_FACE_GUI_CMD_H
