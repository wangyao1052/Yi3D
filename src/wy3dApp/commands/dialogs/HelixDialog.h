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

#ifndef WY3DAPP_HELIX_DIALOG_H
#define WY3DAPP_HELIX_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QRadioButton>
#include <QCheckBox>
#include "wy3dAppDefs.h"

namespace wy3dApp {

class HelixDialog : public QDialog
{
    Q_OBJECT

public:
    HelixDialog(double pitch, double turns, double startAngle, bool isClockWise,
        QWidget* parent = nullptr);

    // 重载设置首选大小
    // 对布局特别有用
    virtual QSize sizeHint() const override
    {
        return QSize(250, 100);
    }

    // 获取螺距
    double getPitch() const { return _pitch; }

    // 获取圈数
    double getTurns() const { return _turns; }

    // 获取起始角度
    double getStartAngle() const { return _startAngle; }

    // 获取是否是顺时针
    bool isClockWise() const { return _isClockWise; }

protected:
    virtual void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onOkBtnClicked();
    void onCancelBtnClicked();

private:
    bool getValue(QLineEdit* pEdit, double allowMin, double allowMax, double& value);

private:
    // 螺距
    QLineEdit* _pitchEdit;
    double _pitch;

    // 圈数
    QLineEdit* _turnsEdit;
    double _turns;

    // 起始角度
    QLineEdit* _startAngleEdit;
    double _startAngle;

    // 顺时针/逆时针
    QRadioButton* _cwRadio;
    QRadioButton* _ccwRadio;
    bool _isClockWise;
};

}

#endif // WY3DAPP_HELIX_DIALOG_H
