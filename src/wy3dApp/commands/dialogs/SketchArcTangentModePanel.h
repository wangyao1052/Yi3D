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

#ifndef WY3DAPP_SKETCH_ARC_TANGENT_MODE_PANEL_H
#define WY3DAPP_SKETCH_ARC_TANGENT_MODE_PANEL_H

#include "FloatingCmdPanel.h"
#include "wy3dAppDefs.h"

class QRadioButton;

namespace wy3dApp {

// How the far end of the tangent arc is defined, i.e. what the second step asks for.
enum class ArcTangentMode
{
    PassPoint = 0, // the arc passes through a coordinate point (default)
    Radius = 1     // the radius is given first, then the sweep angle
};

class SketchArcTangentModePanel : public FloatingCmdPanel
{
    Q_OBJECT
public:
    explicit SketchArcTangentModePanel(QWidget* parent = nullptr);
    virtual ~SketchArcTangentModePanel();

    void setMode(ArcTangentMode mode);
    ArcTangentMode getMode() const { return _mode; }

signals:
    // The mode already matches the clicked button when this arrives, and programmatic setMode()
    // does not emit, so a handler never sees its own change echoed back.
    void modeChanged(ArcTangentMode mode);

private:
    QRadioButton* _pRbPassPoint;
    QRadioButton* _pRbRadius;
    ArcTangentMode _mode;
};

}

#endif // WY3DAPP_SKETCH_ARC_TANGENT_MODE_PANEL_H
