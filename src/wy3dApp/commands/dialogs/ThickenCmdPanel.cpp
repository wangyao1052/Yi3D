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

#include "ThickenCmdPanel.h"

#include <cassert>
#include <cmath>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QToolTip>
#include <QVBoxLayout>

#include <wy3dThicken.h>
#include <wy3dImpl.h>

static constexpr double kDefaultThickness = 2.0;

static QString formatDouble(double v)
{
    QString s = QString::number(v, 'f', 2);
    while (s.contains('.') && (s.endsWith('0') || s.endsWith('.')))
        s.chop(1);
    return s;
}

ThickenCmdPanel::ThickenCmdPanel(QWidget* parent)
    : FloatingCmdPanel(parent)
    , _pThicknessEdit(nullptr)
    , _pDirectionCombo(nullptr)
    , _lastValidThickness(kDefaultThickness)
{
    setObjectName("ThickenCmdPanel");
    setStyleSheet(
        "QWidget#ThickenCmdPanel{background:#e7e7e7;border:1px solid #b7b7b7;}"
        "QFrame#titleBar{background:#0f6d93;border:none;}"
        "QLabel#titleLabel{color:#ffffff;font-weight:600;padding-left:6px;}"
        "QLabel#fieldLabel{color:#202020;background:transparent;}"
        "QLineEdit{min-height:22px;border:1px solid #7a7a7a;padding:0 4px;background:#ffffff;}"
        "QComboBox{min-height:22px;border:1px solid #7a7a7a;padding:0 4px;background:#ffffff;}"
        "QPushButton#okBtn{min-width:66px;min-height:24px;border:1px solid #0f6d93;background:#0f6d93;color:#ffffff;}"
        "QPushButton#okBtn:hover{background:#117aa3;}"
        "QPushButton#okBtn:pressed{background:#0b5b7a;}"
        "QPushButton#cancelBtn{min-width:66px;min-height:24px;border:1px solid #0f6d93;background:#ffffff;color:#0f6d93;}"
        "QPushButton#cancelBtn:hover{background:#f2f9ff;}"
        "QPushButton#cancelBtn:pressed{background:#eef6fb;}");

    setTitle(tr("Thicken"));

    QWidget* pBody = contentWidget();
    QVBoxLayout* pBodyLayout = new QVBoxLayout(pBody);
    pBodyLayout->setContentsMargins(12, 12, 12, 12);
    pBodyLayout->setSpacing(10);

    // thickness
    QLabel* pThicknessLabel = new QLabel(tr("Thickness:"), pBody);
    pThicknessLabel->setObjectName("fieldLabel");
    pBodyLayout->addWidget(pThicknessLabel);

    _pThicknessEdit = new QLineEdit(formatDouble(kDefaultThickness), pBody);
    _pThicknessEdit->selectAll();
    pBodyLayout->addWidget(_pThicknessEdit);

    // direction
    QLabel* pDirectionLabel = new QLabel(tr("Direction:"), pBody);
    pDirectionLabel->setObjectName("fieldLabel");
    pBodyLayout->addWidget(pDirectionLabel);

    _pDirectionCombo = new QComboBox(pBody);
    _pDirectionCombo->addItem(tr("One Side"));
    _pDirectionCombo->addItem(tr("Symmetric"));
    pBodyLayout->addWidget(_pDirectionCombo);

    pBodyLayout->addStretch(1);

    setMinimumWidth(240);
    adjustSize();

    // connections
    QObject::connect(_pThicknessEdit, &QLineEdit::editingFinished,
        this, &ThickenCmdPanel::onThicknessEditChanged);
    QObject::connect(_pDirectionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &ThickenCmdPanel::onDirectionComboChanged);
}

void ThickenCmdPanel::setThicknessValue(double value)
{
    _pThicknessEdit->blockSignals(true);
    _pThicknessEdit->setText(formatDouble(value));
    _pThicknessEdit->blockSignals(false);
}

void ThickenCmdPanel::onThicknessEditChanged()
{
    bool ok = false;
    double value = _pThicknessEdit->text().toDouble(&ok);
    if (!ok || std::fabs(value) < wy3d::kMinValue ||
        std::fabs(value) > wy3d::kMaxValue)
    {
        _pThicknessEdit->setText(
            formatDouble(_lastValidThickness));
        QToolTip::showText(_pThicknessEdit->mapToGlobal(QPoint(0, _pThicknessEdit->height())),
            tr("Invalid value, must be between %1 and %2.")
                .arg(wy3d::kMinValue).arg(wy3d::kMaxValue),
            _pThicknessEdit);
        return;
    }
    _lastValidThickness = value;
    QToolTip::hideText();
    emit thicknessChanged(value);
}

void ThickenCmdPanel::onDirectionComboChanged(int index)
{
    emit directionChanged(index);
}
