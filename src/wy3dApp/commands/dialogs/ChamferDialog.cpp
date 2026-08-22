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

#include "ChamferDialog.h"
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

ChamferDistanceLineEdit::ChamferDistanceLineEdit(QWidget* parent) : QLineEdit(parent)
{
}

void ChamferDistanceLineEdit::keyPressEvent(QKeyEvent* event)
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

ChamferDialog::ChamferDialog(double distance1, double distance2, double angle,
    wy3d::ChamferType chamferType, bool isFlipped, QWidget* parent)
    : QDialog(parent), _distance1(std::fabs(distance1)), _distance2(std::fabs(distance2)),
      _angle(angle), _chamferType(chamferType), _isFlipped(isFlipped)
{
    this->setWindowTitle(tr("Chamfer"));

    QVBoxLayout* layout = new QVBoxLayout(this);

    QGridLayout* gridLayout = new QGridLayout();
    // 第0行: 倒角类型
    {
        //
        QLabel* label = new QLabel(this);
        label->setText(tr("Type"));
        gridLayout->addWidget(label, 0, 0);
        //
        _typeCombo = new QComboBox(this);
        _typeCombo->addItem(tr("Equal distance"));
        _typeCombo->addItem(tr("Distance-Distance"));
        _typeCombo->addItem(tr("Distance-Angle"));
        _typeCombo->setCurrentIndex(static_cast<int>(_chamferType));
        gridLayout->addWidget(_typeCombo, 0, 1);
    }
    // 第1行: 倒角距离 (三种模式共有)
    {
        //
        QLabel* label = new QLabel(this);
        label->setText(tr("Distance 1"));
        gridLayout->addWidget(label, 1, 0);
        //
        _distanceEdit = new ChamferDistanceLineEdit(this);
        _distanceEdit->setText(QString::number(_distance1));
        _distanceEdit->selectAll();
        {
            // 可匹配的示例：0、0.5、5.0、100.01
            // 不可匹配的示例: - 5、.5、00.5
            QRegExp regExp("^(0|[1-9]\\d*)(\\.\\d+)?$");
            QRegExpValidator* validator = new QRegExpValidator(regExp, _distanceEdit);
            _distanceEdit->setValidator(validator);
        }
        gridLayout->addWidget(_distanceEdit, 1, 1);
    }
    // 第2行: 第二倒角距离 (仅双距离模式可见)
    {
        //
        _distance2Label = new QLabel(this);
        _distance2Label->setText(tr("Distance 2"));
        gridLayout->addWidget(_distance2Label, 2, 0);
        //
        _distance2Edit = new ChamferDistanceLineEdit(this);
        _distance2Edit->setText(QString::number(_distance2));
        QRegExp regExp("^(0|[1-9]\\d*)(\\.\\d+)?$");
        QRegExpValidator* validator = new QRegExpValidator(regExp, _distance2Edit);
        _distance2Edit->setValidator(validator);
        gridLayout->addWidget(_distance2Edit, 2, 1);
    }
    // 第3行: 倒角角度 (仅距离+角度模式可见)
    {
        //
        _angleLabel = new QLabel(this);
        _angleLabel->setText(tr("Angle"));
        gridLayout->addWidget(_angleLabel, 3, 0);
        //
        _angleEdit = new ChamferDistanceLineEdit(this);
        _angleEdit->setText(QString::number(_angle));
        QRegExp regExp("^(0|[1-9]\\d*)(\\.\\d+)?$");
        QRegExpValidator* validator = new QRegExpValidator(regExp, _angleEdit);
        _angleEdit->setValidator(validator);
        gridLayout->addWidget(_angleEdit, 3, 1);
    }
    // 第4行: 翻转方向
    {
        _flipCheckBox = new QCheckBox(this);
        _flipCheckBox->setText(tr("Flip Direction"));
        _flipCheckBox->setChecked(_isFlipped);
        gridLayout->addWidget(_flipCheckBox, 4, 0, 1, 2);
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
    this->connect(_typeCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(onTypeChanged(int)));
    this->connect(okBtn, SIGNAL(clicked()), this, SLOT(onOkBtnClicked()));
    this->connect(cancelBtn, SIGNAL(clicked()), this, SLOT(onCancelBtnClicked()));

    // 固定大小 (以所有行可见时的尺寸为准; 隐藏行由布局自动折叠, 模式切换时尺寸稳定)
    this->adjustSize();
    this->setFixedSize(this->size());

    // 初始化模式相关控件状态
    this->updateModeWidgets();
}

void ChamferDialog::onTypeChanged(int index)
{
    _chamferType = static_cast<wy3d::ChamferType>(index);
    this->updateModeWidgets();
}

void ChamferDialog::updateModeWidgets()
{
    const bool showDistance2 = (wy3d::ChamferType::DistanceDistance == _chamferType);
    const bool showAngle = (wy3d::ChamferType::DistanceAngle == _chamferType);
    _distance2Label->setVisible(showDistance2);
    _distance2Edit->setVisible(showDistance2);
    _angleLabel->setVisible(showAngle);
    _angleEdit->setVisible(showAngle);
    // 仅两距/角度模式可翻转 (等距对称, 翻转无意义)
    _flipCheckBox->setEnabled(wy3d::ChamferType::EqualDistance != _chamferType);
}

void ChamferDialog::onOkBtnClicked()
{
    bool ok(false);
    double distance1 = _distanceEdit->text().toDouble(&ok);
    if (!ok)
    {
        return;
    }
    if (distance1 < wy3d::kMinValue || distance1 > wy3d::kMaxValue)
    {
        QMessageBox::warning(this, tr("Warning"), tr(
            "Please enter a number that is greater than or equal to 0.001 and less than or equal to 1000000."));
        return;
    }

    if (wy3d::ChamferType::DistanceDistance == _chamferType)
    {
        double distance2 = _distance2Edit->text().toDouble(&ok);
        if (!ok || distance2 < wy3d::kMinValue || distance2 > wy3d::kMaxValue)
        {
            QMessageBox::warning(this, tr("Warning"), tr(
                "Please enter a second distance that is greater than or equal to 0.001 and less than or equal to 1000000."));
            return;
        }
        _distance2 = distance2;
    }

    if (wy3d::ChamferType::DistanceAngle == _chamferType)
    {
        double angle = _angleEdit->text().toDouble(&ok);
        if (!ok || angle <= 0.0 || angle >= 180.0)
        {
            QMessageBox::warning(this, tr("Warning"), tr(
                "Please enter an angle that is greater than 0 and less than 180 degrees."));
            return;
        }
        _angle = angle;
    }

    _distance1 = distance1;
    _isFlipped = _flipCheckBox->isChecked();
    this->accept();
}

void ChamferDialog::onCancelBtnClicked()
{
    this->reject();
}

void ChamferDialog::keyPressEvent(QKeyEvent* event)
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
