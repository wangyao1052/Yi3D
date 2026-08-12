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

#ifndef WY3DAPP_THICKEN_CMD_PANEL_H
#define WY3DAPP_THICKEN_CMD_PANEL_H

#include "FloatingCmdPanel.h"

class QLineEdit;
class QComboBox;

class ThickenCmdPanel : public FloatingCmdPanel
{
    Q_OBJECT
public:
    explicit ThickenCmdPanel(QWidget* parent = nullptr);

    void setThicknessValue(double value);

signals:
    void thicknessChanged(double value);
    void directionChanged(int direction);

private slots:
    void onThicknessEditChanged();
    void onDirectionComboChanged(int index);

private:
    QLineEdit* _pThicknessEdit;
    QComboBox* _pDirectionCombo;
    double _lastValidThickness;
};

#endif // WY3DAPP_THICKEN_CMD_PANEL_H
