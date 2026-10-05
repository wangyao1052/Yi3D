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

#include "SketchArcTangentModePanel.h"

#include <QRadioButton>
#include <QVBoxLayout>

SketchArcTangentModePanel::SketchArcTangentModePanel(QWidget* parent)
    : FloatingCmdPanel(parent)
    , _pRbPassPoint(nullptr)
    , _pRbRadius(nullptr)
    , _mode(ArcTangentMode::PassPoint)
{
    // The viewport keeps the keyboard for the whole command (Esc, typed values), so clicking the
    // panel must not activate it: the buttons still toggle, the window never activates.
    this->setWindowFlag(Qt::WindowDoesNotAcceptFocus, true);

    setObjectName("SketchArcTangentModePanel");
    setStyleSheet(
        "QWidget#SketchArcTangentModePanel{background:#e7e7e7;border:1px solid #b7b7b7;}"
        "QFrame#titleBar{background:#0f6d93;border:none;}"
        "QLabel#titleLabel{color:#ffffff;font-weight:600;padding-left:6px;}"
        "QRadioButton{color:#202020;background:transparent;}");

    this->setFooterVisible(false);
    this->setTitle(tr("Tangent Arc"));

    QWidget* pContent = this->contentWidget();
    QVBoxLayout* pLayout = new QVBoxLayout(pContent);
    pLayout->setContentsMargins(12, 12, 12, 12);
    pLayout->setSpacing(8);

    // Clicked, never typed into, for the same reason as the window flag above.
    _pRbPassPoint = new QRadioButton(tr("End Point"), pContent);
    _pRbRadius = new QRadioButton(tr("Radius & Angle"), pContent);
    _pRbPassPoint->setChecked(true);
    for (QRadioButton* pRadio : { _pRbPassPoint, _pRbRadius })
    {
        pRadio->setFocusPolicy(Qt::NoFocus);
        pLayout->addWidget(pRadio);
    }
    pLayout->addStretch(1);

    QObject::connect(_pRbPassPoint, &QRadioButton::clicked, this, [this]() { _mode = ArcTangentMode::PassPoint; emit modeChanged(_mode); });
    QObject::connect(_pRbRadius, &QRadioButton::clicked, this, [this]() { _mode = ArcTangentMode::Radius; emit modeChanged(_mode); });
}

SketchArcTangentModePanel::~SketchArcTangentModePanel()
{
}

void SketchArcTangentModePanel::setMode(ArcTangentMode mode)
{
    _mode = mode;
    _pRbPassPoint->setChecked(ArcTangentMode::PassPoint == mode);
    _pRbRadius->setChecked(ArcTangentMode::Radius == mode);
}
