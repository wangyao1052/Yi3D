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

#include "ViewportOverlayBar.h"

#include <cassert>

#include <QAction>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QShowEvent>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace
{
constexpr int kIconSize = 48;
constexpr int kButtonSize = 54;
// Same margin and same corner FloatingCmdPanel anchors with.
constexpr int kMargin = 8;
constexpr int kPadding = 4;
} // namespace

ViewportOverlayBar::ViewportOverlayBar(QWidget* pViewportWidget)
    : QWidget(pViewportWidget)
    , _pLayout(nullptr)
{
    this->setObjectName("viewportOverlayBar");
    this->setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    // The viewport keeps the keyboard. The drawing shortcuts are window shortcuts of the main
    // window, so activating the bar would make the main window inactive and take every one of
    // them down with it. On Windows this is WS_EX_NOACTIVATE -- the buttons still click.
    this->setWindowFlag(Qt::WindowDoesNotAcceptFocus, true);
    this->setAttribute(Qt::WA_ShowWithoutActivating, true);
    // The panel is translucent, so the viewport stays readable through it.
    this->setAttribute(Qt::WA_TranslucentBackground, true);
    // Tooltips on a window that never activates are off by default.
    this->setAttribute(Qt::WA_AlwaysShowToolTips, true);
    this->setAttribute(Qt::WA_StyledBackground, true);
    this->setFocusPolicy(Qt::NoFocus);
    this->setStyleSheet(
        "QWidget#viewportOverlayBar{background:transparent;border:none;}"
        "QFrame#panel{background:rgba(246,246,246,204);border:1px solid rgba(150,150,150,170);}"
        "QToolButton{background:transparent;border:1px solid transparent;border-radius:2px;}"
        "QToolButton:hover{background:rgba(15,109,147,56);border:1px solid rgba(15,109,147,140);}"
        "QToolButton:pressed{background:rgba(15,109,147,115);border:1px solid #0b5b7a;}"
        "QToolButton:checked{background:rgba(15,109,147,90);border:1px solid #0f6d93;}");

    // The panel is a child rather than this widget itself: a translucent top-level window does
    // not paint its own style sheet background, but it does paint a child's.
    QVBoxLayout* pRootLayout = new QVBoxLayout(this);
    pRootLayout->setContentsMargins(0, 0, 0, 0);
    pRootLayout->setSpacing(0);

    QFrame* pPanel = new QFrame(this);
    pPanel->setObjectName("panel");
    pRootLayout->addWidget(pPanel);

    _pLayout = new QHBoxLayout(pPanel);
    _pLayout->setContentsMargins(kPadding, kPadding, kPadding, kPadding);
    _pLayout->setSpacing(kPadding);

    if (pViewportWidget)
    {
        pViewportWidget->installEventFilter(this);
        QWidget* pWindow = pViewportWidget->window();
        if (pWindow && pWindow != pViewportWidget)
            pWindow->installEventFilter(this);
    }
}

ViewportOverlayBar::~ViewportOverlayBar()
{
    if (QWidget* pParent = parentWidget())
    {
        pParent->removeEventFilter(this);
        QWidget* pWindow = pParent->window();
        if (pWindow && pWindow != pParent)
            pWindow->removeEventFilter(this);
    }
}

void ViewportOverlayBar::addAction(QAction* pAction)
{
    if (!pAction)
    {
        assert(false);
        return;
    }

    QToolButton* pButton = new QToolButton(_pLayout->parentWidget());
    pButton->setDefaultAction(pAction);
    pButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    pButton->setIconSize(QSize(kIconSize, kIconSize));
    pButton->setFixedSize(kButtonSize, kButtonSize);
    pButton->setFocusPolicy(Qt::NoFocus);
    _pLayout->addWidget(pButton);
    this->adjustSize();
}

bool ViewportOverlayBar::eventFilter(QObject* watched, QEvent* event)
{
    QWidget* pParent = parentWidget();
    QWidget* pWindow = pParent ? pParent->window() : nullptr;
    if ((watched == pParent || (pWindow && watched == pWindow)) && event)
    {
        if (QEvent::Move == event->type())
        {
            this->anchorToTopLeft(); // instant follow on move
        }
        else if (QEvent::Resize == event->type() ||
                 QEvent::Show == event->type())
        {
            // deferred for layout update; this as context so a destroyed bar cannot be
            // reached by a pending call
            QTimer::singleShot(0, this, [this]() { this->anchorToTopLeft(); });
        }
    }
    return QWidget::eventFilter(watched, event);
}

void ViewportOverlayBar::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    this->anchorToTopLeft();
}

void ViewportOverlayBar::anchorToTopLeft()
{
    QWidget* pParent = parentWidget();
    if (!pParent) return;

    this->move(pParent->mapToGlobal(QPoint(kMargin, kMargin)));
}
