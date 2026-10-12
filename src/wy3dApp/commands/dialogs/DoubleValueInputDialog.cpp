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

#include "DoubleValueInputDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QRegExpValidator>
#include <QMouseEvent>
#include <QKeyEvent>
#include <wy3dImpl.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

DoubleValueLineEdit::DoubleValueLineEdit(QWidget* parent) : QLineEdit(parent)
{
}

void DoubleValueLineEdit::keyPressEvent(QKeyEvent* event)
{
    switch (event->key())
    {
    case Qt::Key_Enter:  // 确定
    case Qt::Key_Return: // 确定
    case Qt::Key_Escape: // 取消
    {
        event->ignore(); // 由父对话框处理
    }
    break;

    default:
    {
        QLineEdit::keyPressEvent(event);
    }
    }
}

DoubleValueInputDialog::DoubleValueInputDialog(
    double defaultValue,
    const QString& title,
    const QString& valueLabel,
    const Options& options,
    QWidget* parent) : QDialog(parent), _value(defaultValue), _value2nd(0.0), _options(options), _valueEdit(nullptr), _valueEdit2nd(nullptr)
{
    this->init(title, valueLabel, false, "");
}

DoubleValueInputDialog::DoubleValueInputDialog(
    double defaultValue1st,
    double defaultValue2nd,
    const QString& title,
    const QString& valueLabel1st,
    const QString& valueLabel2nd,
    const Options& options,
    QWidget* parent) : QDialog(parent), _value(defaultValue1st), _value2nd(defaultValue2nd), _options(options), _valueEdit(nullptr), _valueEdit2nd(nullptr)
{
    this->init(title, valueLabel1st, true, valueLabel2nd);
}

void DoubleValueInputDialog::init(const QString& title, const QString& valueLabel, bool hasValue2nd, const QString& valueLabel2nd)
{
    this->setWindowTitle(title);

    auto newDoubleValueLineEdit = [this](double value) -> DoubleValueLineEdit*
    {
        DoubleValueLineEdit* valueEdit = new DoubleValueLineEdit(this);
        valueEdit->setText(QString::number(value));
        valueEdit->selectAll();
        {
            // 匹配正的浮点数
            // 可匹配的示例：0、0.5、5.0、5.、100.01、100.00
            // 不可匹配的示例: -5、.5、00.5
            QString pattern = "^(0|[1-9]\\d*)(\\.\\d*)?$";

            // 允许输入负数
            if (_options.allowNegative)
            {
                pattern = "^-?(0|[1-9]\\d*)(\\.\\d*)?$"; // 匹配正负浮点数(加了-?)
            }

            // 正则表达式
            QRegExp regExp(pattern);
            QRegExpValidator* validator = new QRegExpValidator(regExp, valueEdit);
            valueEdit->setValidator(validator);
        }

        return valueEdit;
    };

    // 排布
    QVBoxLayout* layout = new QVBoxLayout(this);
    // {
    // 1.数值输入区
    QGridLayout* gridLayout = new QGridLayout();
    {
        // 标签
        QLabel* label = new QLabel(this);
        label->setText(valueLabel);
        gridLayout->addWidget(label, 0, 0);

        // 浮点数编辑框
        _valueEdit = newDoubleValueLineEdit(_value);

        gridLayout->addWidget(_valueEdit, 0, 1);
    }

    if (hasValue2nd)
    {
        // 标签
        QLabel* label = new QLabel(this);
        label->setText(valueLabel2nd);
        gridLayout->addWidget(label, 1, 0);

        // 浮点数编辑框
        _valueEdit2nd = newDoubleValueLineEdit(_value2nd);

        gridLayout->addWidget(_valueEdit2nd, 1, 1);
    }

    layout->addLayout(gridLayout);

    // 2.确认按钮+取消按钮
    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* okBtn = new QPushButton(tr("Ok"), this);
    QPushButton* cancelBtn = new QPushButton(tr("Cancel"), this);
    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);
    layout->addLayout(btnLayout);
    // }

    // 信号槽
    this->connect(okBtn, SIGNAL(clicked()), this, SLOT(onOkBtnClicked()));
    this->connect(cancelBtn, SIGNAL(clicked()), this, SLOT(onCancelBtnClicked()));

    // 固定大小
    this->adjustSize();
    this->setFixedSize(this->size());
}

void DoubleValueInputDialog::onOkBtnClicked()
{
    bool ok(false);
    double value = _valueEdit->text().toDouble(&ok);
    if (!ok)
    {
        return;
    }
    if (value < _options.allowMin || value > _options.allowMax)
    {
        QMessageBox::warning(this, tr("Warning"),
            tr("Please enter a number that is greater than or equal to ") +
            QString::number(_options.allowMin) +
            tr(" and less than or equal to ") +
            QString::number(_options.allowMax) +
            tr("."));
        _valueEdit->setFocus();
        return;
    }
    _value = value;

    if (_valueEdit2nd)
    {
        double value2nd = _valueEdit2nd->text().toDouble(&ok);
        if (!ok)
        {
            return;
        }
        if (value2nd < _options.allowMin || value2nd > _options.allowMax)
        {
            QMessageBox::warning(this, tr("Warning"),
                tr("Please enter a number that is greater than or equal to ") +
                QString::number(_options.allowMin) +
                tr(" and less than or equal to ") +
                QString::number(_options.allowMax) +
                tr("."));
            _valueEdit2nd->setFocus();
            return;
        }
        _value2nd = value2nd;
    }

    this->accept();
}

void DoubleValueInputDialog::onCancelBtnClicked()
{
    this->reject();
}

void DoubleValueInputDialog::keyPressEvent(QKeyEvent* event)
{
    switch (event->key())
    {
    case Qt::Key_Enter:
    case Qt::Key_Return:
    {
        return this->onOkBtnClicked(); // Ok
    }
    break;

    case Qt::Key_Escape:
    {
        return this->onCancelBtnClicked(); // Cancel
    }
    break;
    }

    QWidget::keyPressEvent(event);
}

}
