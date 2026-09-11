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

#ifndef WY3DAPP_SKETCH_DRAW_CENTER_RECTANGLE3D_GUI_CMD_H
#define WY3DAPP_SKETCH_DRAW_CENTER_RECTANGLE3D_GUI_CMD_H

#include "commands/sketch3d/SketchDrawRectangle3DGuiCmd.h"

// 中心式矩形:中心 + 角点
class SketchDrawCenterRectangle3DGuiCmd : public SketchDrawRectangle3DGuiCmd
{
    WYRX_DECLARE_MEMBERS(SketchDrawCenterRectangle3DGuiCmd, SketchDrawCenterRectangle3DGuiCmd, SketchDrawRectangle3DGuiCmd)
public:
    SketchDrawCenterRectangle3DGuiCmd();
    virtual ~SketchDrawCenterRectangle3DGuiCmd();

protected:
    virtual QString getStartTip() const override;
    virtual QString getEndTip() const override;
};

#endif // WY3DAPP_SKETCH_DRAW_CENTER_RECTANGLE3D_GUI_CMD_H
