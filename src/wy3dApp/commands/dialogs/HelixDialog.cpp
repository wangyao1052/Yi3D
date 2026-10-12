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

#include "HelixDialog.h"
#include <cfloat>
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
#include "utils/MessageBoxUtil.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

HelixDialog::HelixDialog(double pitch, double turns, double startAngle,
    bool isClockWise, QWidget* parent)
    : QDialog(parent), _pitch(pitch), _turns(turns), _startAngle(startAngle),
      _isClockWise(isClockWise)
{
    this->setWindowTitle(tr("Helix"));

    QVBoxLayout* layout = new QVBoxLayout(this);

    QGridLayout* gridLayout = new QGridLayout();
    int row(-1);
    // 第一行:螺距
    {
        ++row;
        //
        QLabel* label = new QLabel(this);
        label->setText(tr("Pitch"));
        gridLayout->addWidget(label, row, 0);
        //
        _pitchEdit = new QLineEdit(this);
        _pitchEdit->setText(QString::number(_pitch));
        gridLayout->addWidget(_pitchEdit, row, 1);
    }
    // 第二行:圈数
    {
        ++row;
        //
        QLabel* label = new QLabel(this);
        label->setText(tr("Turns"));
        gridLayout->addWidget(label, row, 0);
        //
        _turnsEdit = new QLineEdit(this);
        _turnsEdit->setText(QString::number(_turns));
        gridLayout->addWidget(_turnsEdit, row, 1);
    }
    // 第三行:起始角度
    {
        ++row;
        //
        QLabel* label = new QLabel(this);
        label->setText(tr("Start Angle"));
        gridLayout->addWidget(label, row, 0);
        //
        _startAngleEdit = new QLineEdit(this);
        _startAngleEdit->setText(QString::number(_startAngle));
        gridLayout->addWidget(_startAngleEdit, row, 1);
    }
    // 第四行:顺时针/逆时针
    {
        ++row;
        //
        QLabel* label = new QLabel(this);
        label->setText(tr("Handedness"));
        gridLayout->addWidget(label, row, 0);
        //
        _cwRadio = new QRadioButton(this);
        _cwRadio->setText(tr("Clockwise"));
        _ccwRadio = new QRadioButton(this);
        _ccwRadio->setText(tr("Counter-Clockwise"));
        if (_isClockWise) _cwRadio->setChecked(true);
        else _ccwRadio->setChecked(true);
        QHBoxLayout* layout = new QHBoxLayout();
        layout->addWidget(_cwRadio);
        layout->addWidget(_ccwRadio);
        gridLayout->addLayout(layout, row, 1);
    }
    layout->addLayout(gridLayout);

    // 创建OK和Cancel按钮
    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* okBtn = new QPushButton(tr("Ok"), this);
    QPushButton* cancelBtn = new QPushButton(tr("Cancel"), this);
    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);
    layout->addLayout(btnLayout);

    // 信号槽
    this->connect(okBtn, SIGNAL(clicked()), this, SLOT(onOkBtnClicked()));
    this->connect(cancelBtn, SIGNAL(clicked()), this, SLOT(onCancelBtnClicked()));

    // 固定大小
    this->adjustSize();
    this->setFixedSize(this->size());
}

void HelixDialog::onOkBtnClicked()
{
    // 螺距
    if (!this->getValue(_pitchEdit, wy3d::kMinValue, wy3d::kMaxHelixPitch, _pitch))
    {
        return;
    }
    // 圈数
    if (!this->getValue(_turnsEdit, wy3d::kMinValue, wy3d::kMaxHelixTurns, _turns))
    {
        return;
    }
    // 起始角度
    if (!this->getValue(_startAngleEdit, -DBL_MAX, DBL_MAX, _startAngle))
    {
        return;
    }
    // 顺时针/逆时针
    if (_cwRadio->isChecked())
    {
        _isClockWise = true;
    }
    else
    {
        assert(_ccwRadio->isChecked());
        _isClockWise = false;
    }

    this->accept();
}

void HelixDialog::onCancelBtnClicked()
{
    this->reject();
}

bool HelixDialog::getValue(QLineEdit* pEdit, double allowMin, double allowMax, double& value)
{
    assert(pEdit);

    bool ok(false);
    value = pEdit->text().toDouble(&ok);
    if (!ok)
    {
        MessageBoxUtil::showWarning_PleaseInputNumber();
        pEdit->setFocus();
        pEdit->selectAll();
        return false;
    }

    if (value < allowMin || value > allowMax)
    {
        MessageBoxUtil::showWarning_InvalidValue(allowMin, allowMax);
        pEdit->setFocus();
        pEdit->selectAll();
        return false;
    }

    return true;
}

void HelixDialog::keyPressEvent(QKeyEvent* event)
{
    switch (event->key())
    {
    case Qt::Key_Escape:
    {
        return this->onCancelBtnClicked(); // Cancel
    }
    break;
    }

    QWidget::keyPressEvent(event);
}

}
