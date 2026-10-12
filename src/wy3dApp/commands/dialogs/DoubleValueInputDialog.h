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

#ifndef WY3DAPP_DOUBLE_VALUE_INPUT_DIALOG_H
#define WY3DAPP_DOUBLE_VALUE_INPUT_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <wy3dImpl.h>
#include "wy3dAppDefs.h"

namespace wy3dApp {

class DoubleValueLineEdit : public QLineEdit
{
    Q_OBJECT
public:
    explicit DoubleValueLineEdit(QWidget* parent = nullptr);

protected:
    virtual void keyPressEvent(QKeyEvent* event) override;
};

class DoubleValueInputDialog : public QDialog
{
    Q_OBJECT
public:
    struct Options
    {
        bool allowNegative; // 是否允许负数
        double allowMin;    // 允许输入的最小值
        double allowMax;    // 允许输入的最大值

        Options()
            : allowNegative(false), allowMin(wy3d::kMinValue), allowMax(wy3d::kMaxValue)
        {}
    };
public:
    DoubleValueInputDialog(
        double defaultValue,
        const QString& title,
        const QString& valueLabel,
        const Options& options = Options(),
        QWidget* parent = nullptr);

    DoubleValueInputDialog(
        double defaultValue1st,
        double defaultValue2nd,
        const QString& title,
        const QString& valueLabel1st,
        const QString& valueLabel2nd,
        const Options& options = Options(),
        QWidget* parent = nullptr);

    // 重载设置首选大小
    // 对布局特别有用
    virtual QSize sizeHint() const override
    {
        return QSize(200, 120);
    }

    // 获取值
    double getValue() const
    {
        return _value;
    }
    double getValue2nd() const
    {
        return _value2nd;
    }

protected:
    virtual void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onOkBtnClicked();
    void onCancelBtnClicked();

private:
    void init(const QString& title, const QString& valueLabel, bool hasValue2nd, const QString& valueLabel2nd);

private:
    DoubleValueLineEdit* _valueEdit;
    DoubleValueLineEdit* _valueEdit2nd;
    double _value;
    double _value2nd;
    Options _options;
};

}

#endif // WY3DAPP_DOUBLE_VALUE_INPUT_DIALOG_H
