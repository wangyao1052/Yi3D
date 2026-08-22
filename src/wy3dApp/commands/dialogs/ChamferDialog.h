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

#ifndef WY3DAPP_CHAMFER_DIALOG_H
#define WY3DAPP_CHAMFER_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <wy3dChamfer.h>

class QLabel;

class ChamferDistanceLineEdit : public QLineEdit
{
    Q_OBJECT
public:
    explicit ChamferDistanceLineEdit(QWidget* parent = nullptr);

protected:
    virtual void keyPressEvent(QKeyEvent* event) override;
};

class ChamferDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ChamferDialog(double distance1, double distance2, double angle,
        wy3d::ChamferType chamferType, bool isFlipped, QWidget* parent = nullptr);

    // 重载设置首选大小
    // 对布局特别有用
    virtual QSize sizeHint() const override
    {
        return QSize(260, 180);
    }

    // 获取第一倒角距离
    double getDistance1() const
    {
        return _distance1;
    }

    // 获取第二倒角距离
    double getDistance2() const
    {
        return _distance2;
    }

    // 获取倒角角度 (度)
    double getAngle() const
    {
        return _angle;
    }

    // 获取倒角类型
    wy3d::ChamferType getChamferType() const
    {
        return _chamferType;
    }

    // 获取是否翻转方向
    bool isFlipped() const
    {
        return _isFlipped;
    }

protected:
    virtual void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onOkBtnClicked();
    void onCancelBtnClicked();
    void onTypeChanged(int index);

private:
    void updateModeWidgets();

private:
    QComboBox* _typeCombo;
    ChamferDistanceLineEdit* _distanceEdit;
    ChamferDistanceLineEdit* _distance2Edit;
    ChamferDistanceLineEdit* _angleEdit;
    QLabel* _distance2Label;
    QLabel* _angleLabel;
    QCheckBox* _flipCheckBox;
    double _distance1;
    double _distance2;
    double _angle;   // 度 (对话框按度工作)
    wy3d::ChamferType _chamferType;
    bool _isFlipped;
};

#endif // WY3DAPP_CHAMFER_DIALOG_H
