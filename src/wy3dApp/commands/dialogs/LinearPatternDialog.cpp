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

#include "LinearPatternDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QRegExpValidator>
#include <wy3dImpl.h>
#include "utils/MessageBoxUtil.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

LinearPatternDialog::LinearPatternDialog(
    unsigned int count1st, double spacing1st,
    unsigned int count2nd, double spacing2nd, QWidget* parent)
    : QDialog(parent)
{
    this->setWindowTitle(tr("Linear Pattern"));

    QVBoxLayout* layout = new QVBoxLayout(this);

    // 方向1
    QGroupBox* groupBoxDir1st = new QGroupBox(tr("Direction 1"), this);
    {
        QGridLayout* gridLayout = new QGridLayout(groupBoxDir1st);
        {
            QLabel* label = new QLabel(this);
            label->setText(tr("Count"));
            _count1stEdit = new QLineEdit(this);
            _count1stEdit->setText(QString::number(count1st));
            {
                QRegExp regExp("[0-9]+");
                QRegExpValidator* validator = new QRegExpValidator(regExp, _count1stEdit);
                _count1stEdit->setValidator(validator);
            }
            gridLayout->addWidget(label, 0, 0);
            gridLayout->addWidget(_count1stEdit, 0, 1);
        }
        {
            QLabel* label = new QLabel(this);
            label->setText(tr("Spacing"));
            _spacing1stEdit = new QLineEdit(this);
            _spacing1stEdit->setText(QString::number(spacing1st));
            gridLayout->addWidget(label, 1, 0);
            gridLayout->addWidget(_spacing1stEdit, 1, 1);
        }
    }
    layout->addWidget(groupBoxDir1st);

    // 方向2
    QGroupBox* groupBoxDir2nd = new QGroupBox(tr("Direction 2"), this);
    {
        QGridLayout* gridLayout = new QGridLayout(groupBoxDir2nd);
        {
            QLabel* label = new QLabel(this);
            label->setText(tr("Count"));
            _count2ndEdit = new QLineEdit(this);
            _count2ndEdit->setText(QString::number(count2nd));
            {
                QRegExp regExp("[0-9]+");
                QRegExpValidator* validator = new QRegExpValidator(regExp, _count2ndEdit);
                _count2ndEdit->setValidator(validator);
            }
            gridLayout->addWidget(label, 0, 0);
            gridLayout->addWidget(_count2ndEdit, 0, 1);
        }
        {
            QLabel* label = new QLabel(this);
            label->setText(tr("Spacing"));
            _spacing2ndEdit = new QLineEdit(this);
            _spacing2ndEdit->setText(QString::number(spacing2nd));
            gridLayout->addWidget(label, 1, 0);
            gridLayout->addWidget(_spacing2ndEdit, 1, 1);
        }
    }
    layout->addWidget(groupBoxDir2nd);
   
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

void LinearPatternDialog::onOkBtnClicked()
{
    bool ok(false);

    // 方向1上的实例数量
    unsigned int count1st = _count1stEdit->text().toUInt(&ok);
    if (!ok)
    {
        MessageBoxUtil::showWarning_PleaseInputNumber();
        _count1stEdit->setFocus();
        return;
    }
    if (count1st <= 0 || count1st > wy3d::kMaxLinearPatternCount)
    {
        MessageBoxUtil::showWarning_InvalidValue(1, wy3d::kMaxLinearPatternCount);
        _count1stEdit->setFocus();
        return;
    }

    // 方向1上的实例间距
    double spacing1st = _spacing1stEdit->text().toDouble(&ok);
    if (!ok)
    {
        MessageBoxUtil::showWarning_PleaseInputNumber();
        _spacing1stEdit->setFocus();
        return;
    }
    if (spacing1st > wy3d::kMaxValue)
    {
        MessageBoxUtil::showWarning_InvalidValue(0, wy3d::kMaxValue);
        _spacing1stEdit->setFocus();
        return;
    }

    // 方向2上的实例数量
    unsigned int count2nd = _count2ndEdit->text().toUInt(&ok);
    if (!ok)
    {
        MessageBoxUtil::showWarning_PleaseInputNumber();
        _count2ndEdit->setFocus();
        return;
    }
    if (count2nd <= 0 || count2nd > wy3d::kMaxLinearPatternCount)
    {
        MessageBoxUtil::showWarning_InvalidValue(0, wy3d::kMaxLinearPatternCount);
        _count2ndEdit->setFocus();
        return;
    }

    // 方向2上的实例间距
    double spacing2nd = _spacing2ndEdit->text().toDouble(&ok);
    if (!ok)
    {
        MessageBoxUtil::showWarning_PleaseInputNumber();
        _spacing2ndEdit->setFocus();
        return;
    }
    if (spacing2nd > wy3d::kMaxValue)
    {
        MessageBoxUtil::showWarning_InvalidValue(0, wy3d::kMaxValue);
        _spacing2ndEdit->setFocus();
        return;
    }

    // 方向1和方向2上的数量必须至少有一个大于1
    if (count1st <= 1 && count2nd <= 1)
    {
        MessageBoxUtil::showWarning(tr("At least one of the count in Direction 1 and Direction 2 must be greater than 1."));
        return;
    }

    // 数据
    _count1st = count1st;
    _spacing1st = spacing1st;
    _count2nd = count2nd;
    _spacing2nd = spacing2nd;

    this->accept();
}

void LinearPatternDialog::onCancelBtnClicked()
{
    this->reject();
}

}
