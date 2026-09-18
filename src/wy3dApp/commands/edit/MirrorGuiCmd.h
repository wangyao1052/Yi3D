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

#ifndef WY3DAPP_MIRROR_GUI_CMD_H
#define WY3DAPP_MIRROR_GUI_CMD_H

#include <wyapSelection.h>
#include <wy3dSketchPlane.h>

#include "commands/OsgGuiCommand.h"
#include "select/SelectPreview.h"
#include "select/SelectionSetHighlightor.h"

class MirrorGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(MirrorGuiCmd, wy3dApp::MirrorGuiCmd, OsgGuiCommand)
public:
    MirrorGuiCmd();

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;

protected:
    enum class Step
    {
        Undefined = 0,
        SelectSource = 1,
        SelectMirrorPlane = 2,
    };
    bool finishStep(Step step);
    void gotoStep(Step step);

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseUp(const MouseEvent& event) override;

    // 单击特征树上的元素
    virtual void onFeatureTreeItemClicked(const wydb::ElementId& id) override;

private:
    // 创建镜像
    bool createMirror(
        const wydb::ElementId& sourceId,
        const wy3d::SketchPlane& mirrorPlane,
        unsigned int& errorCode);

    // 创建镜像: 实体宿主(可镜像实体内的切割体)
    bool createMirrorOnSolid(
        const wydb::ElementId& ownerId,
        const wy3d::Solid* pSourceSolid,
        const wy3d::SketchPlane& mirrorPlane,
        unsigned int& errorCode);

    // 创建镜像: 片体宿主(镜像它自己, 副本追加到宿主形体上)
    bool createMirrorOnSheet(
        const wydb::ElementId& sheetId,
        const wy3d::SketchPlane& mirrorPlane,
        unsigned int& errorCode);

private:
    Step _step;
    wydb::ElementId _sourceId;
    wy3d::SketchPlane _mirrorPlane;

    // 点选选项
    PointPickOption _pointPickOption;

    // 源对象预览
    SelectPreviewSPtr _pSourcePreview;

    // 镜像面预览
    SelectPreviewSPtr _pMirrorPlanePreview;
};

#endif // WY3DAPP_MIRROR_GUI_CMD_H