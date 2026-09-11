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

#include "SketchDrawCenterRectangle3DGuiCmd.h"

#include <QCoreApplication>
#include <QString>

SketchDrawCenterRectangle3DGuiCmd::SketchDrawCenterRectangle3DGuiCmd() : SketchDrawRectangle3DGuiCmd()
{
    _mode = MakeSketchRectangle3D::Mode::CenterRect;
}

SketchDrawCenterRectangle3DGuiCmd::~SketchDrawCenterRectangle3DGuiCmd()
{
}

QString SketchDrawCenterRectangle3DGuiCmd::getStartTip() const
{
    return QCoreApplication::translate("SketchDrawCenterRectangle3DGuiCmd",
        "Specify the rectangle center point; you can directly input the coordinate values. "
        "Press Space to switch the drawing plane.");
}

QString SketchDrawCenterRectangle3DGuiCmd::getEndTip() const
{
    return QCoreApplication::translate("SketchDrawCenterRectangle3DGuiCmd",
        "Specify the rectangle corner point or input length & width. "
        "Press Space to switch the drawing plane.");
}
