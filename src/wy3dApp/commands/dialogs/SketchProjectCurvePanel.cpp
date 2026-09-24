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

#include "SketchProjectCurvePanel.h"

#include <QApplication>
#include <QCheckBox>
#include <QEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QVBoxLayout>

SketchProjectCurvePanel::SketchProjectCurvePanel(QWidget* parent)
    : FloatingCmdPanel(parent)
    , _pCbReverse(nullptr)
    , _pCbBidirectional(nullptr)
    , _pStatus(nullptr)
    , _isDirectionAvailable(false)
{
    // Enter is how a projection gets confirmed, and it reaches the command through the viewport's
    // keyboard handling, so the panel must never take the keyboard away when it is clicked. On
    // Windows this is WS_EX_NOACTIVATE: the boxes still toggle, the window never activates.
    this->setWindowFlag(Qt::WindowDoesNotAcceptFocus, true);

    setObjectName("SketchProjectCurvePanel");
    setStyleSheet(
        "QWidget#SketchProjectCurvePanel{background:#e7e7e7;border:1px solid #b7b7b7;}"
        "QFrame#titleBar{background:#0f6d93;border:none;}"
        "QLabel#titleLabel{color:#ffffff;font-weight:600;padding-left:6px;}"
        "QLabel#fieldLabel{color:#202020;background:transparent;}"
        "QCheckBox{color:#202020;background:transparent;}"
        "QCheckBox:disabled{color:#909090;}"
        "QPushButton#okBtn{min-width:66px;min-height:24px;border:1px solid #0f6d93;background:#0f6d93;color:#ffffff;}"
        "QPushButton#okBtn:hover{background:#117aa3;}"
        "QPushButton#okBtn:pressed{background:#0b5b7a;}"
        "QPushButton#cancelBtn{min-width:66px;min-height:24px;border:1px solid #0f6d93;background:#ffffff;color:#0f6d93;}"
        "QPushButton#cancelBtn:hover{background:#f2f9ff;}"
        "QPushButton#cancelBtn:pressed{background:#eef6fb;}");

    this->setTitle(tr("Project Curve"));
    // The footer is here because the result is not always readable off the screen: a projection
    // that lands nowhere and one that has not been asked for yet look the same without it.
    this->setFooterVisible(true);

    QWidget* pContent = this->contentWidget();
    QVBoxLayout* pLayout = new QVBoxLayout(pContent);
    pLayout->setContentsMargins(12, 12, 12, 12);
    pLayout->setSpacing(10);

    // Clicked, never typed into, for the same reason as the window flag above.
    _pCbReverse = new QCheckBox(tr("Reverse"), pContent);
    _pCbReverse->setFocusPolicy(Qt::NoFocus);
    pLayout->addWidget(_pCbReverse);

    _pCbBidirectional = new QCheckBox(tr("Bidirectional"), pContent);
    _pCbBidirectional->setFocusPolicy(Qt::NoFocus);
    pLayout->addWidget(_pCbBidirectional);

    _pStatus = new QLabel(pContent);
    _pStatus->setObjectName("fieldLabel");
    pLayout->addWidget(_pStatus);

    pLayout->addStretch(1);

    this->setMinimumWidth(240);
    this->adjustSize();

    // The reconciler goes first so that optionsChanged always sees settled flags.
    QObject::connect(_pCbBidirectional, &QCheckBox::toggled,
        this, &SketchProjectCurvePanel::onBidirectionalToggled);
    QObject::connect(_pCbReverse, &QCheckBox::toggled, this, &SketchProjectCurvePanel::optionsChanged);
    QObject::connect(_pCbBidirectional, &QCheckBox::toggled, this, &SketchProjectCurvePanel::optionsChanged);

    this->setProjection(Projection::NoSketch);

    // Tab is the keyboard way to the Reverse box, as it is in the intersection panel: the panel sits
    // over the viewport for the whole command and the viewport keeps the keyboard, so it listens
    // application wide rather than on one widget. Consuming the key also keeps Qt's focus navigation
    // from taking it first.
    qApp->installEventFilter(this);
}

SketchProjectCurvePanel::~SketchProjectCurvePanel()
{
    qApp->removeEventFilter(this);
}

bool SketchProjectCurvePanel::eventFilter(QObject* watched, QEvent* event)
{
    if (event && QEvent::KeyPress == event->type())
    {
        QKeyEvent* pKeyEvent = static_cast<QKeyEvent*>(event);
        if (pKeyEvent && Qt::Key_Tab == pKeyEvent->key())
        {
            // Held down, the key repeats, and every repeat would be another projection: one flip per
            // press. Bidirectional leaves no direction to flip, which is what greys the box out.
            if (_isDirectionAvailable && !pKeyEvent->isAutoRepeat() && _pCbReverse->isEnabled())
                _pCbReverse->setChecked(!_pCbReverse->isChecked());
            return true;
        }
    }
    return FloatingCmdPanel::eventFilter(watched, event);
}

void SketchProjectCurvePanel::onBidirectionalToggled(bool checked)
{
    // Taking both sides leaves reverse with nothing left to change, so it greys out rather than
    // sitting there doing nothing. Its check state is left alone: turning bidirectional back off
    // has to find the user's own choice where they left it.
    _pCbReverse->setEnabled(!checked);
}

void SketchProjectCurvePanel::setProjection(Projection projection)
{
    switch (projection)
    {
    case Projection::Ready:
        _pStatus->setText(tr("Projection curve computed."));
        break;
    case Projection::NoProjection:
        _pStatus->setText(tr("No projection curve."));
        break;
    case Projection::NoTarget:
        _pStatus->setText(tr("Select the face or datum plane to project onto."));
        break;
    default:
        _pStatus->setText(tr("Select the sketch to project."));
        break;
    }

    // Nothing to confirm until there is a curve, so OK stays dead until there is one.
    this->setOkEnabled(Projection::Ready == projection);
}

bool SketchProjectCurvePanel::isReverse() const
{
    return _pCbReverse->isChecked();
}

bool SketchProjectCurvePanel::isBidirectional() const
{
    return _pCbBidirectional->isChecked();
}

void SketchProjectCurvePanel::setDirectionAvailable(bool available)
{
    _isDirectionAvailable = available;
}

bool SketchProjectCurvePanel::isDirectionAvailable() const
{
    return _isDirectionAvailable;
}
