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

#include "SketchPolarArrayDialog.h"
#include <wy3dMath.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QButtonGroup>
#include <QMessageBox>
#include <QRegExpValidator>
#include "utils/MessageBoxUtil.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

SketchPolarArrayDialog::SketchPolarArrayDialog(
    const QString& title,
    double totalAngle, unsigned int count, bool isCCW,
    const Options& options, QWidget* parent)
    : QDialog(parent), _options(options)
{
    if (title.isEmpty())
        this->setWindowTitle(tr("Sketch Polar Array"));
    else
        this->setWindowTitle(title);

    QVBoxLayout* layout = new QVBoxLayout(this);

    QGridLayout* gridLayout = new QGridLayout();
    {
        QLabel* label = new QLabel(this);
        label->setText(tr("Total Angle"));
        _totalAngleEdit = new QLineEdit(this);
        _totalAngleEdit->setText(QString::number(totalAngle));
        {
            QRegExp regExp("^\\d+(\\.\\d+)?$");
            QRegExpValidator* validator = new QRegExpValidator(regExp, _totalAngleEdit);
            _totalAngleEdit->setValidator(validator);
        }
        gridLayout->addWidget(label, 1, 1);
        gridLayout->addWidget(_totalAngleEdit, 1, 2);
    }
    {
        QLabel* label = new QLabel(this);
        label->setText(tr("Count"));
        _countEdit = new QLineEdit(this);
        _countEdit->setText(QString::number(count));
        _countEdit->selectAll();
        {
            QRegExp regExp("[0-9]+");
            QRegExpValidator* validator = new QRegExpValidator(regExp, _countEdit);
            _countEdit->setValidator(validator);
        }
        gridLayout->addWidget(label, 2, 1);
        gridLayout->addWidget(_countEdit, 2, 2);
    }
    {
        QLabel* label = new QLabel(this);
        label->setText(tr("Direction"));
        _ccwRadio = new QRadioButton(tr("CCW"), this);
        _cwRadio = new QRadioButton(tr("CW"), this);
        {
            QButtonGroup* buttonGroup = new QButtonGroup(this);
            buttonGroup->addButton(_ccwRadio);
            buttonGroup->addButton(_cwRadio);
            if (isCCW) _ccwRadio->setChecked(true);
            else _cwRadio->setChecked(true);
        }
        QHBoxLayout* radioLayout = new QHBoxLayout();
        radioLayout->addWidget(_ccwRadio);
        radioLayout->addWidget(_cwRadio);
        gridLayout->addWidget(label, 3, 1);
        gridLayout->addLayout(radioLayout, 3, 2);
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

void SketchPolarArrayDialog::onOkBtnClicked()
{
    bool ok(false);

    // 总角度
    double totalAngle = _totalAngleEdit->text().toDouble(&ok);
    if (!ok)
    {
        MessageBoxUtil::showWarning_PleaseInputNumber();
        _totalAngleEdit->setFocus();
        return;
    }
    if (totalAngle < _options.minTotalAngle || totalAngle > _options.maxTotalAngle)
    {
        MessageBoxUtil::showWarning_InvalidValue(_options.minTotalAngle, _options.maxTotalAngle);
        _totalAngleEdit->setFocus();
        return;
    }

    // 实例数量
    unsigned int count = _countEdit->text().toUInt(&ok);
    if (!ok)
    {
        MessageBoxUtil::showWarning_PleaseInputNumber();
        _countEdit->setFocus();
        return;
    }
    if (count < _options.minCount || count > _options.maxCount)
    {
        MessageBoxUtil::showWarning_InvalidValue(_options.minCount, _options.maxCount);
        _countEdit->setFocus();
        return;
    }

    // 逆时针
    bool isCCW = _ccwRadio->isChecked() ? true : false;

    // 结果
    _totalAngle = wy3d::degreesToRadians(totalAngle);
    _count = count;
    _isCCW = isCCW;
    this->accept();
}

void SketchPolarArrayDialog::onCancelBtnClicked()
{
    this->reject();
}

} // namespace wy3dApp
