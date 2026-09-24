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

#ifndef WY3DAPP_SKETCH_PROJECT_CURVE_PANEL_H
#define WY3DAPP_SKETCH_PROJECT_CURVE_PANEL_H

#include "FloatingCmdPanel.h"

class QCheckBox;
class QLabel;

// SolidWorks' IProjectionCurveFeatureData carries exactly two flags, Reverse and Bidirectional,
// and this panel is both of them: there is no direction to pick, because the projection runs along
// the source sketch's own plane normal.
//
// It also carries the OK and Cancel buttons, and the line that says whether there is a projection
// to confirm - a projection that lands nowhere leaves nothing on the screen to tell it apart from
// one that has not been asked for yet, and OK stays dead until there is something behind it.
class SketchProjectCurvePanel : public FloatingCmdPanel
{
    Q_OBJECT
public:
    // What the panel says about the projection, which is also what decides whether OK can be
    // pressed. One state per step, so the line asks for the one thing the command is waiting for
    // rather than for both at once.
    enum class Projection
    {
        NoSketch,     // step one: the sketch has not been picked yet
        NoTarget,     // step two: the sketch is picked, the face or datum plane is not
        NoProjection, // both are picked, and the sketch does not land on the target
        Ready,        // there is a curve waiting to be confirmed
    };

    explicit SketchProjectCurvePanel(QWidget* parent = nullptr);
    virtual ~SketchProjectCurvePanel();

    bool isReverse() const;
    bool isBidirectional() const;

    // Whether there is a direction on screen for Tab to flip: the arrow only appears once the sketch
    // is picked, and before that Tab would be flipping something invisible. The command says when.
    void setDirectionAvailable(bool available);
    bool isDirectionAvailable() const;

    void setProjection(Projection projection);

signals:
    // Both flags have already been reconciled with each other by the time this arrives, so a
    // handler can read them both without waiting for a second signal.
    void optionsChanged();

protected:
    virtual bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onBidirectionalToggled(bool checked);

private:
    QCheckBox* _pCbReverse;
    QCheckBox* _pCbBidirectional;
    QLabel* _pStatus;
    bool _isDirectionAvailable;
};

#endif // WY3DAPP_SKETCH_PROJECT_CURVE_PANEL_H
