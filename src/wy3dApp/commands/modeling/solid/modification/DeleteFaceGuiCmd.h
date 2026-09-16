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

#ifndef WY3DAPP_DELETE_FACE_GUI_CMD_H
#define WY3DAPP_DELETE_FACE_GUI_CMD_H

#include <wydbElementId.h>
#include "commands/OsgGuiCommand.h"
#include "select/SelectPreview.h"
#include "select/SelectionSetHighlightor.h"

// Deletes faces from a sheet: pick the faces and the sheet is left with an opening where they
// were. Only sheets can be picked - a sheet that loses a face is still a sheet, while a solid
// would have to turn into one. One run makes one feature, hence one undo step. The companion of
// the Split Face command: split a face, then delete the piece that is not wanted.
class DeleteFaceGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(DeleteFaceGuiCmd, wy3dApp::DeleteFaceGuiCmd, OsgGuiCommand)
public:
    DeleteFaceGuiCmd();

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;

protected:
    enum class Step
    {
        Undefined = 0,
        SelectFaces = 1,
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
    // 创建删除面
    bool createDeleteFace(const wyap::SelectionSet& faceSels, unsigned int& errorCode);

private:
    Step _step;
    wyap::SelectionSet _faceSels;

    // 点选选项
    PointPickOption _pointPickOption;
    // 预览
    SelectPreviewSPtr _pPreview;
    // 高亮
    SelectionSetHighlightorSPtr _pSelSetHighlightor_Faces;
};

#endif // WY3DAPP_DELETE_FACE_GUI_CMD_H
