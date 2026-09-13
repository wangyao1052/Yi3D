///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 Wang Yao <wangyao1052@163.com>
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

#include "SketchIntersectionCurvePanel.h"

#include <QApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QRadioButton>
#include <QVBoxLayout>

SketchIntersectionCurvePanel::SketchIntersectionCurvePanel(QWidget* parent)
    : FloatingCmdPanel(parent)
    , _pRbFit(nullptr)
    , _pRbInterpolate(nullptr)
{
    setObjectName("SketchIntersectionCurvePanel");
    setStyleSheet(
        "QWidget#SketchIntersectionCurvePanel{background:#e7e7e7;border:1px solid #b7b7b7;}"
        "QFrame#titleBar{background:#0f6d93;border:none;}"
        "QLabel#titleLabel{color:#ffffff;font-weight:600;padding-left:6px;}"
        "QRadioButton{color:#202020;background:transparent;}");

    this->setTitle(tr("Intersection Curve"));
    this->setFooterVisible(false);

    QWidget* pContent = this->contentWidget();
    QVBoxLayout* pLayout = new QVBoxLayout(pContent);
    pLayout->setContentsMargins(12, 10, 12, 10);
    pLayout->setSpacing(6);

    // The radios keep the focus away from the viewport's keyboard handling on purpose: they are
    // clicked, never typed into, and Esc has to keep reaching the command.
    const auto addMode = [pContent, pLayout](const QString& text, bool checked)
    {
        QRadioButton* pRadio = new QRadioButton(text, pContent);
        pRadio->setFocusPolicy(Qt::NoFocus);
        pRadio->setChecked(checked);
        pLayout->addWidget(pRadio);
        return pRadio;
    };

    _pRbFit = addMode(tr("Fit"), true);
    _pRbInterpolate = addMode(tr("Interpolate"), false);

    this->setMinimumWidth(180);
    this->adjustSize();

    // Tab is the keyboard way between the two: the panel goes up over the viewport and stays
    // there for the whole command, so it listens application wide rather than on one widget.
    // Consuming the key also keeps Qt's focus navigation from taking it first.
    qApp->installEventFilter(this);
}

SketchIntersectionCurvePanel::~SketchIntersectionCurvePanel()
{
    qApp->removeEventFilter(this);
}

bool SketchIntersectionCurvePanel::eventFilter(QObject* watched, QEvent* event)
{
    if (event && QEvent::KeyPress == event->type())
    {
        QKeyEvent* pKeyEvent = static_cast<QKeyEvent*>(event);
        if (pKeyEvent && Qt::Key_Tab == pKeyEvent->key())
        {
            // Both are set explicitly: auto exclusive radios leave the pair unchecked if the
            // checked one is simply unchecked.
            if (_pRbFit->isChecked())
            {
                _pRbInterpolate->setChecked(true);
            }
            else
            {
                _pRbFit->setChecked(true);
            }
            return true;
        }
    }
    return FloatingCmdPanel::eventFilter(watched, event);
}

bool SketchIntersectionCurvePanel::isFit() const
{
    return _pRbFit->isChecked();
}
