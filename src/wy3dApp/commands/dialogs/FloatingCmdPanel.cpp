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

#include "FloatingCmdPanel.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>

FloatingCmdPanel::FloatingCmdPanel(QWidget* parent)
    : QWidget(parent)
    , _parentOffset(8, 8)
    , _userMoved(false)
    , _pContent(nullptr)
    , _pTitleLabel(nullptr)
    , _pTitleBar(nullptr)
    , _pFooterLayout(nullptr)
    , _pOkButton(nullptr)
    , _pCancelButton(nullptr)
    , _dragging(false)
    , _dragOffset()
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setAttribute(Qt::WA_StyledBackground, true);

    // root layout: title bar + content area
    QVBoxLayout* pRootLayout = new QVBoxLayout(this);
    pRootLayout->setContentsMargins(0, 0, 0, 0);
    pRootLayout->setSpacing(0);

    // auto-create title bar with drag support
    _pTitleBar = new QFrame(this);
    _pTitleBar->setObjectName("titleBar");
    _pTitleBar->setFixedHeight(24);
    this->setTitleBarWidget(_pTitleBar);

    QHBoxLayout* pTitleLayout = new QHBoxLayout(_pTitleBar);
    pTitleLayout->setContentsMargins(4, 2, 4, 2);
    pTitleLayout->setSpacing(4);

    _pTitleLabel = new QLabel(_pTitleBar);
    _pTitleLabel->setObjectName("titleLabel");
    _pTitleLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    pTitleLayout->addWidget(_pTitleLabel, 1);

    pRootLayout->addWidget(_pTitleBar);

    // content area for subclasses
    _pContent = new QWidget(this);
    _pContent->setObjectName("content");
    pRootLayout->addWidget(_pContent, 1);

    // separator line
    QFrame* pSeparator = new QFrame(this);
    pSeparator->setObjectName("separator");
    pSeparator->setFixedHeight(1);
    pSeparator->setStyleSheet("QFrame#separator{background:#c3c3c3;border:none;}");
    pRootLayout->addWidget(pSeparator);

    // footer with OK/Cancel buttons
    QWidget* pFooter = new QWidget(this);
    _pFooterLayout = new QHBoxLayout(pFooter);
    _pFooterLayout->setContentsMargins(8, 8, 8, 8);
    _pFooterLayout->setSpacing(6);
    _pFooterLayout->addStretch(1);

    _pOkButton = new QPushButton(tr("OK"), pFooter);
    _pOkButton->setObjectName("okBtn");
    _pOkButton->setFocusPolicy(Qt::NoFocus);
    _pFooterLayout->addWidget(_pOkButton);

    _pCancelButton = new QPushButton(tr("Cancel"), pFooter);
    _pCancelButton->setObjectName("cancelBtn");
    _pCancelButton->setFocusPolicy(Qt::NoFocus);
    _pFooterLayout->addWidget(_pCancelButton);

    pRootLayout->addWidget(pFooter);

    QObject::connect(_pOkButton, &QPushButton::clicked,
        this, &FloatingCmdPanel::onOkClicked);
    QObject::connect(_pCancelButton, &QPushButton::clicked,
        this, &FloatingCmdPanel::onCancelClicked);

    if (parent)
    {
        parent->installEventFilter(this);
        if (QWidget* pWindow = parent->window())
        {
            if (pWindow != parent)
                pWindow->installEventFilter(this);
        }
    }
}

FloatingCmdPanel::~FloatingCmdPanel()
{
    if (QWidget* pParent = parentWidget())
    {
        pParent->removeEventFilter(this);
        if (QWidget* pWindow = pParent->window())
        {
            if (pWindow != pParent)
                pWindow->removeEventFilter(this);
        }
    }
}

void FloatingCmdPanel::setTitle(const QString& title)
{
    if (_pTitleLabel)
        _pTitleLabel->setText(title);
}

void FloatingCmdPanel::setTitleBarWidget(QWidget* pTitleBar)
{
    _pTitleBar = pTitleBar;
    if (_pTitleBar)
        _pTitleBar->installEventFilter(this);
}

bool FloatingCmdPanel::eventFilter(QObject* watched, QEvent* event)
{
    // drag support: title bar mouse events
    if (watched == _pTitleBar && event)
    {
        if (event->type() == QEvent::MouseButtonPress)
        {
            QMouseEvent* pMouseEvent = static_cast<QMouseEvent*>(event);
            if (pMouseEvent && pMouseEvent->button() == Qt::LeftButton)
            {
                _dragging = true;
                _dragOffset = pMouseEvent->globalPos() - frameGeometry().topLeft();
                return true;
            }
        }
        else if (event->type() == QEvent::MouseMove)
        {
            if (_dragging)
            {
                QMouseEvent* pMouseEvent = static_cast<QMouseEvent*>(event);
                if (pMouseEvent && (pMouseEvent->buttons() & Qt::LeftButton))
                {
                    QPoint topLeft = pMouseEvent->globalPos() - _dragOffset;
                    move(topLeft);
                    _userMoved = true;
                    if (QWidget* pParent = parentWidget())
                    {
                        _parentOffset = topLeft - pParent->mapToGlobal(QPoint(0, 0));
                        clampToParentBounds();
                        move(pParent->mapToGlobal(_parentOffset));
                    }
                    return true;
                }
            }
        }
        else if (event->type() == QEvent::MouseButtonRelease)
        {
            QMouseEvent* pMouseEvent = static_cast<QMouseEvent*>(event);
            if (pMouseEvent && pMouseEvent->button() == Qt::LeftButton)
            {
                _dragging = false;
                return true;
            }
        }
        else if (event->type() == QEvent::Leave)
        {
            _dragging = false;
        }
    }

    // follow parent window resize/move
    QWidget* pParent = parentWidget();
    QWidget* pWindow = pParent ? pParent->window() : nullptr;
    if ((watched == pParent || watched == pWindow) && event)
    {
        auto reposition = [this]()
        {
            QWidget* pParentWidget = parentWidget();
            if (!pParentWidget) return;

            if (_userMoved)
            {
                clampToParentBounds();
                move(pParentWidget->mapToGlobal(_parentOffset));
            }
            else
            {
                anchorToTopLeft();
            }
        };

        if (event->type() == QEvent::Move)
        {
            reposition(); // instant follow on move
        }
        else if (event->type() == QEvent::Resize ||
                 event->type() == QEvent::Show)
        {
            QTimer::singleShot(0, this, reposition); // defer for layout update
        }
    }
    return QWidget::eventFilter(watched, event);
}

void FloatingCmdPanel::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (_userMoved)
    {
        if (QWidget* pParent = parentWidget())
        {
            clampToParentBounds();
            move(pParent->mapToGlobal(_parentOffset));
        }
    }
    else
    {
        anchorToTopLeft();
    }
}

void FloatingCmdPanel::anchorToTopLeft()
{
    QWidget* pParent = parentWidget();
    if (!pParent) return;

    const int margin = 8;
    _parentOffset = QPoint(margin, margin);
    clampToParentBounds();
    move(pParent->mapToGlobal(_parentOffset));
}

void FloatingCmdPanel::clampToParentBounds()
{
    QWidget* pParent = parentWidget();
    if (!pParent) return;

    _parentOffset.setX(qBound(0, _parentOffset.x(),
        qMax(0, pParent->width() - width())));
    _parentOffset.setY(qBound(0, _parentOffset.y(),
        qMax(0, pParent->height() - height())));
}

void FloatingCmdPanel::onOkClicked()
{
    hide();
    emit accepted();
}

void FloatingCmdPanel::onCancelClicked()
{
    hide();
    emit canceled();
}
