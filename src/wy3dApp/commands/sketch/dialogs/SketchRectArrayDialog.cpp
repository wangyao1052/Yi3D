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

#include "SketchRectArrayDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QRegExpValidator>
#include "wy3dAppDefs.h"

namespace wy3dApp {

SketchRectArrayDialog::SketchRectArrayDialog(unsigned int cols, unsigned int rows, QWidget* parent)
    : QDialog(parent)
{
    this->setWindowTitle(tr("Sketch Rectangular Array"));

    QVBoxLayout* layout = new QVBoxLayout(this);

    QGridLayout* gridLayout = new QGridLayout();
    {
        QLabel* label = new QLabel(this);
        label->setText(tr("Columns"));
        _colsEdit = new QLineEdit(this);
        _colsEdit->setText(QString::number(cols));
        _colsEdit->selectAll();
        {
            QRegExp regExp("[0-9]+");
            QRegExpValidator* validator = new QRegExpValidator(regExp, _colsEdit);
            _colsEdit->setValidator(validator);
        }
        gridLayout->addWidget(label, 1, 1);
        gridLayout->addWidget(_colsEdit, 1, 2);
    }
    {
        QLabel* label = new QLabel(this);
        label->setText(tr("Rows"));
        _rowsEdit = new QLineEdit(this);
        _rowsEdit->setText(QString::number(rows));
        _rowsEdit->selectAll();
        {
            QRegExp regExp("[0-9]+");
            QRegExpValidator* validator = new QRegExpValidator(regExp, _rowsEdit);
            _rowsEdit->setValidator(validator);
        }
        gridLayout->addWidget(label, 2, 1);
        gridLayout->addWidget(_rowsEdit, 2, 2);
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

void SketchRectArrayDialog::onOkBtnClicked()
{
    bool ok(false);
    unsigned int cols = _colsEdit->text().toUInt(&ok);
    if (!ok) return;
    unsigned int rows = _rowsEdit->text().toUInt(&ok);
    if (!ok) return;

    if (cols == 0 || rows == 0)
    {
        QMessageBox::warning(this, tr("Warning"), tr("The number of rows and columns in a rectangular array must be non-zero."));
        return;
    }
    if (cols == 1 && rows == 1)
    {
        QMessageBox::warning(this, tr("Warning"), tr("The number of rows and columns in a rectangular array cannot both be 1 at the same time."));
        return;
    }
    if (cols * rows > 10000)
    {
        QMessageBox::warning(this, tr("Warning"), tr("The number of elements in a rectangular array must be less than or equal to 10,000."));
        return;
    }

    _cols = cols;
    _rows = rows;
    this->accept();
}

void SketchRectArrayDialog::onCancelBtnClicked()
{
    this->reject();
}

}
