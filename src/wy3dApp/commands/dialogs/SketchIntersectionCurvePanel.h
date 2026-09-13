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

#ifndef WY3DAPP_SKETCH_INTERSECTION_CURVE_PANEL_H
#define WY3DAPP_SKETCH_INTERSECTION_CURVE_PANEL_H

#include "FloatingCmdPanel.h"

class QRadioButton;

// Whether the intersector may leave the points it computed, i.e. wy3d::Sketch3DIntersectionUtil's
// two modes, put in front of the user while the command runs. There is nothing to confirm, so
// the state is read at every pair and the footer stays hidden.
class SketchIntersectionCurvePanel : public FloatingCmdPanel
{
    Q_OBJECT
public:
    explicit SketchIntersectionCurvePanel(QWidget* parent = nullptr);
    virtual ~SketchIntersectionCurvePanel();

    bool isFit() const;

protected:
    virtual bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QRadioButton* _pRbFit;
    QRadioButton* _pRbInterpolate;
};

#endif // WY3DAPP_SKETCH_INTERSECTION_CURVE_PANEL_H
