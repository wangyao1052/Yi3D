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

#include "SketchPolygonDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QRegExpValidator>
#include <QMouseEvent>
#include <QKeyEvent>
#include "wy3dAppDefs.h"

namespace wy3dApp {

PolygonSidesLineEdit::PolygonSidesLineEdit(QWidget* parent) : QLineEdit(parent)
{
}

void PolygonSidesLineEdit::keyPressEvent(QKeyEvent* event)
{
    switch (event->key())
    {
    case Qt::Key_I:      // 切换内接多边形
    case Qt::Key_C:      // 切换外接多边形
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

SketchPolygonDialog::SketchPolygonDialog(unsigned int numSides, unsigned int minSides, unsigned int maxSides,
    QWidget* parent) : QDialog(parent), _numSides(0), _minSides(minSides), _maxSides(maxSides)
{
    this->setWindowTitle(tr("Sketch Polygon"));

    QVBoxLayout* layout = new QVBoxLayout(this);

    QGridLayout* gridLayout = new QGridLayout();
    // 第一行:多边形边数
    {
        //
        QLabel* label = new QLabel(this);
        label->setText(tr("Sides"));
        gridLayout->addWidget(label, 0, 0);
        //
        _sidesEdit = new PolygonSidesLineEdit(this);
        _sidesEdit->setText(QString::number(numSides));
        _sidesEdit->selectAll();
        {
            QRegExp regExp("[0-9]+");
            QRegExpValidator* validator = new QRegExpValidator(regExp, _sidesEdit);
            _sidesEdit->setValidator(validator);
        }
        gridLayout->addWidget(_sidesEdit, 0, 1);
    }
    // 第二行:选项
    {
        // 选项
        QLabel* label = new QLabel(this);
        label->setText(tr("Option"));
        gridLayout->addWidget(label, 1, 0);
        // 内接多边形
        _inscribedRadio = new QRadioButton(this);
        _inscribedRadio->setText(tr("Inscribed polygon"));
        _inscribedRadio->setChecked(true);
        gridLayout->addWidget(_inscribedRadio, 1, 1);
    }
    // 第三行:外接多边形
    {
        // 外接多边形
        _circumscribedRadio = new QRadioButton(this);
        _circumscribedRadio->setText(tr("Circumscribed polygon"));
        gridLayout->addWidget(_circumscribedRadio, 2, 1);
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

void SketchPolygonDialog::onOkBtnClicked()
{
    bool ok(false);
    unsigned int numSides = _sidesEdit->text().toUInt(&ok);
    if (!ok)
    {
        return;
    }
    if (numSides < _minSides || numSides > _maxSides)
    {
        QMessageBox::warning(this, tr("Warning"), tr("Number of sides must be an integer between 3 and 100."));
        return;
    }

    _numSides = numSides;
    this->accept();
}

void SketchPolygonDialog::onCancelBtnClicked()
{
    this->reject();
}

void SketchPolygonDialog::keyPressEvent(QKeyEvent* event)
{
    switch (event->key())
    {
    case Qt::Key_I:
    {
        if (!_inscribedRadio->isChecked())
        {
            _inscribedRadio->setChecked(true);
        }
    }
    break;

    case Qt::Key_C:
    {
        if (!_circumscribedRadio->isChecked())
        {
            _circumscribedRadio->setChecked(true);
        }
    }
    break;

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

bool SketchPolygonDialog::isInscribedPolygon() const
{
    return _inscribedRadio->isChecked();
}

}
