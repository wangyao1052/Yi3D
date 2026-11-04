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

#include "GuiCmdHoverInputPopup.h"

#include <QApplication>
#include <QButtonGroup>
#include <QEvent>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QRadioButton>
#include <QTimer>
#include <algorithm>
#include <cassert>

#include <wy3dApp/Application.h>
#include "application/ApplicationImpl.h"
#include "widgets/frame/MainWindow.h"
#include "widgets/frame/ViewWidget.h"
#include "widgets/frame/ViewWidgetContainer.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

namespace
{
constexpr int kLabelMinWidthPx = 32;
constexpr int kLabelExtraPaddingPx = 16;
constexpr int kValueMinWidthPx = 32;
constexpr int kValueExtraPaddingPx = 16;

bool isInViewWidget(QObject* watched)
{
    QWidget* pWidget = qobject_cast<QWidget*>(watched);
    for (QWidget* w = pWidget; w; w = w->parentWidget())
    {
        if (qobject_cast<ViewWidget*>(w) != nullptr)
        {
            return true;
        }
    }
    return false;
}

ViewWidget* activeViewWidget()
{
    Application& app = ApplicationImpl::instance();
    MainWindow* pMainWindow = app.getMainWindow();
    wyap::Document* pActiveDoc = app.getActiveDocument();
    if (!pMainWindow || !pActiveDoc)
    {
        return nullptr;
    }

    ViewWidgetContainer* pViewWidgetContainer = pMainWindow->getViewWidgetContainer();
    if (!pViewWidgetContainer)
    {
        return nullptr;
    }
    return pViewWidgetContainer->getViewWidget(pActiveDoc);
}
}

static void selectAllAndAnchorLeft(QLineEdit* pLineEdit)
{
    auto applySelection = [](QLineEdit* pEdit)
    {
        if (!pEdit)
        {
            return;
        }

        const int textLength = pEdit->text().size();
        if (textLength > 0)
        {
            pEdit->setSelection(0, textLength);
            pEdit->setCursorPosition(textLength);
            pEdit->home(true);
        }
        else
        {
            pEdit->setCursorPosition(0);
        }
    };

    if (!pLineEdit)
    {
        return;
    }

    pLineEdit->setFocus();
    applySelection(pLineEdit);

    QPointer<QLineEdit> guard(pLineEdit);
    QTimer::singleShot(0, pLineEdit, [guard, applySelection]()
    {
        if (!guard)
        {
            return;
        }
        applySelection(guard.data());
    });
}

static void clearSelectionAndAnchorLeft(QLineEdit* pLineEdit)
{
    if (!pLineEdit)
    {
        return;
    }
    pLineEdit->deselect();
    pLineEdit->setCursorPosition(0);
}

GuiCmdHoverInputPopupBase::GuiCmdHoverInputPopupBase(QWidget* parent)
    : QWidget(parent),
    _acceptHandler(),
    _cancelHandler(),
    _spaceKeyHandler()
{
    this->setAttribute(Qt::WA_DeleteOnClose, false);
    this->setAttribute(Qt::WA_StyledBackground, true);
    _timer.setSingleShot(true);
    _timer.setInterval(0);
    connect(&_timer, &QTimer::timeout, this, [this]() { this->onTimer(); });
}

GuiCmdHoverInputPopupBase::~GuiCmdHoverInputPopupBase()
{
    if (qApp)
    {
        qApp->removeEventFilter(this);
    }
}

void GuiCmdHoverInputPopupBase::setAcceptHandler(const std::function<void()>& handler)
{
    _acceptHandler = handler;
}

void GuiCmdHoverInputPopupBase::setCancelHandler(const std::function<void()>& handler)
{
    _cancelHandler = handler;
}

void GuiCmdHoverInputPopupBase::setSpaceKeyHandler(const std::function<void()>& handler)
{
    _spaceKeyHandler = handler;
}

void GuiCmdHoverInputPopupBase::setTabKeyHandler(const std::function<void()>& handler)
{
    _tabKeyHandler = handler;
}

void GuiCmdHoverInputPopupBase::showAtGlobal(const QPoint& globalPos, int xOffset, int yOffset)
{
    _pendingPosition = globalPos;
    _offset.setX(xOffset);
    _offset.setY(yOffset);
    if (!_timer.isActive())
    {
        _timer.start();
    }
}

void GuiCmdHoverInputPopupBase::setVisible(bool visible)
{
    if (!visible)
    {
        _timer.stop();
    }
    QWidget::setVisible(visible);
}

void GuiCmdHoverInputPopupBase::onTimer()
{
    // Keep the popup attached to the cursor, hide it outside the render area,
    // and only take keyboard focus when it transitions from hidden to visible.
    // Re-activating on every call would steal focus and reset the selected text
    // each frame.
    if (QApplication::applicationState() != Qt::ApplicationActive)
    {
        if (this->isVisible())
        {
            this->hide();
        }
        return;
    }

    const QRect boundaryRectGlobal = this->computeBoundaryRectGlobal();
    if (!boundaryRectGlobal.isValid() || !boundaryRectGlobal.contains(_pendingPosition))
    {
        if (this->isVisible())
        {
            this->hide();
        }
        return;
    }

    this->adjustSize();
    QPoint popupPos = _pendingPosition + _offset;

    if (popupPos.x() + this->width() > boundaryRectGlobal.right())
    {
        popupPos.setX(_pendingPosition.x() - _offset.x() - this->width());
    }
    if (popupPos.y() + this->height() > boundaryRectGlobal.bottom())
    {
        popupPos.setY(_pendingPosition.y() - _offset.y() - this->height());
    }

    if (popupPos.x() < boundaryRectGlobal.left())
    {
        popupPos.setX(boundaryRectGlobal.left());
    }
    if (popupPos.y() < boundaryRectGlobal.top())
    {
        popupPos.setY(boundaryRectGlobal.top());
    }

    QWidget* pParent = this->parentWidget();
    if (!pParent)
    {
        return;
    }

    // The popup is a child overlay, so move() is in parent coordinates. Keep it
    // where it was while the user is editing; the view is frozen then.
    const QPoint localPos = pParent->mapFromGlobal(popupPos);
    if (!_userEdited && this->pos() != localPos)
    {
        this->move(localPos);
    }

    if (!this->isVisible())
    {
        this->show();
        this->raise();
        this->focusInput();
    }
    else
    {
        // Panning/zooming the view with the mouse moves keyboard focus to the
        // 3D view. Take it back once the interaction ends, otherwise typing in
        // the always-visible popup would stop working.
        this->restoreFocusFromRenderArea(boundaryRectGlobal);
    }
}

void GuiCmdHoverInputPopupBase::restoreFocusFromRenderArea(const QRect& renderRectGlobal)
{
    if (QApplication::activePopupWidget() ||
        QApplication::mouseButtons() != Qt::NoButton)
    {
        return;
    }

    QWidget* pFocus = QApplication::focusWidget();
    if (!pFocus || pFocus == this || this->isAncestorOf(pFocus))
    {
        return;
    }

    // Only take focus back from the render area itself (the OSG view). Panels,
    // dialogs and menus live outside the render rect or in another window, so
    // they keep their focus.
    QWidget* pMainWindow = this->parentWidget();
    if (!pMainWindow || pFocus->window() != pMainWindow)
    {
        return;
    }
    const QRect focusRect(pFocus->mapToGlobal(QPoint(0, 0)), pFocus->size());
    if (!renderRectGlobal.contains(focusRect.center()))
    {
        return;
    }

    this->focusInput();
}

bool GuiCmdHoverInputPopupBase::eventFilter(QObject* watched, QEvent* event)
{
    if (!event)
    {
        return QWidget::eventFilter(watched, event);
    }

    switch (event->type())
    {
    case QEvent::ApplicationDeactivate:
    {
        this->hide();
        break;
    }

    case QEvent::Enter:
    {
        QWidget* pWidget = qobject_cast<QWidget*>(watched);
        const bool inPopup =
            pWidget && (pWidget == this || this->isAncestorOf(pWidget));

        // Qt also sends Enter to the ancestors of the entered widget on the
        // path from the common ancestor. Entering the view's container chain
        // on the way back into the view must not dismiss the popup.
        ViewWidget* pActiveView = activeViewWidget();
        const bool relatesToView =
            isInViewWidget(watched) ||
            (pActiveView && pWidget && pWidget->isAncestorOf(pActiveView));

        if (!inPopup && !relatesToView)
        {
            this->hide();
        }
        break;
    }

    case QEvent::MouseMove:
    case QEvent::Wheel:
    {
        // While the user is typing, the 3D view must not follow the mouse:
        // values, preview geometry and zoom stay frozen.
        if (_userEdited && !_hideOnFocusLoss && this->isVisible() &&
            isInViewWidget(watched))
        {
            return true;
        }
        break;
    }

    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick:
    {
        // Click-to-open popups (the dimension editor) dismiss when the user
        // clicks anywhere outside them. The click itself is not consumed.
        if (_hideOnFocusLoss && this->isVisible())
        {
            QWidget* pWidget = qobject_cast<QWidget*>(watched);
            if (!pWidget || (pWidget != this && !this->isAncestorOf(pWidget)))
            {
                this->onCancel();
                this->hide();
            }
        }

        // AutoCAD-style: the click ends the text-entry session and is still
        // delivered to the view, so it places a point, selects, opens the
        // command menu, etc. as usual. The typed value is abandoned.
        if (_userEdited && !_hideOnFocusLoss && this->isVisible() &&
            isInViewWidget(watched))
        {
            // The click ends the text-entry session.
            _userEdited = false;
        }
        break;
    }

    case QEvent::KeyPress:
    {
        // Keep Tab inside the popup (as a child widget the default chain would
        // continue into the main window), and keep Space reserved for the
        // command while the popup has keyboard focus.
        if (this->handleKeyPress(watched, static_cast<QKeyEvent*>(event)))
        {
            return true;
        }
        break;
    }

    case QEvent::ShortcutOverride:
    {
        // When focus is in the popup's QLineEdit, QLineEdit accepts the
        // ShortcutOverride for Undo/Redo, bypassing the shortcut system and
        // turning Ctrl+Z/Ctrl+Y into text undo/redo inside the line edit.
        // Filter it out here (leaving it unaccepted) so the application-level
        // Undo/Redo shortcuts (QAction) can trigger.
        if (this->isVisible())
        {
            QKeyEvent* pKeyEvent = static_cast<QKeyEvent*>(event);
            if (pKeyEvent->modifiers() == Qt::ControlModifier &&
                (pKeyEvent->key() == Qt::Key_Z || pKeyEvent->key() == Qt::Key_Y))
            {
                return true;
            }
        }
        break;
    }

    default:
        break;
    }

    return QWidget::eventFilter(watched, event);
}

bool GuiCmdHoverInputPopupBase::handleKeyPress(QObject* watched, QKeyEvent* event)
{
    if (!event || !this->isVisible())
    {
        return false;
    }

    QLineEdit* pLineEdit = qobject_cast<QLineEdit*>(watched);
    if (!pLineEdit || !this->isAncestorOf(pLineEdit))
    {
        return false;
    }

    const int key = event->key();
    if (key == Qt::Key_Tab || key == Qt::Key_Backtab)
    {
        // Shift+Tab arrives either as Key_Backtab or as Key_Tab+Shift,
        // depending on the platform.
        const bool backward =
            key == Qt::Key_Backtab || (event->modifiers() & Qt::ShiftModifier);

        // Forward Tab may be claimed by the popup (e.g. to toggle the
        // extrusion direction); otherwise - and always for backward Tab -
        // cycle the rows.
        if (!backward && _tabKeyHandler)
        {
            _tabKeyHandler();
        }
        else
        {
            this->focusNextRow(backward);
        }
        return true;
    }

    if (key == Qt::Key_Space && event->modifiers() == Qt::NoModifier)
    {
        if (_spaceKeyHandler)
        {
            _spaceKeyHandler();
        }
        return true;
    }
    return false;
}

void GuiCmdHoverInputPopupBase::onAccept() const
{
    if (_acceptHandler)
    {
        _acceptHandler();
    }
}

void GuiCmdHoverInputPopupBase::onCancel() const
{
    if (_cancelHandler)
    {
        _cancelHandler();
    }
}

void GuiCmdHoverInputPopupBase::showEvent(QShowEvent* event)
{
    qApp->installEventFilter(this);
    QWidget::showEvent(event);
}

void GuiCmdHoverInputPopupBase::hideEvent(QHideEvent* event)
{
    qApp->removeEventFilter(this);
    // A hidden popup no longer owns the user's in-progress input.
    _userEdited = false;
    QWidget::hideEvent(event);
}

void GuiCmdHoverInputPopupBase::trackUserEditing(QLineEdit* pEdit)
{
    if (!pEdit)
    {
        return;
    }
    QObject::connect(pEdit, &QLineEdit::textEdited, this, [this](const QString&)
    {
        _userEdited = true;
    });
}

void GuiCmdHoverInputPopupBase::updateLineEditValue(QLineEdit* pEdit, const QString& value,
                                                    bool overwriteUserInput)
{
    if (overwriteUserInput)
    {
        _userEdited = false;
    }
    if (!pEdit || _userEdited)
    {
        return;
    }
    if (pEdit->text() == value)
    {
        return;
    }
    pEdit->setText(value);
    if (pEdit->hasFocus())
    {
        // Keep the whole value selected so the next typed digits replace it.
        // selectAll() is synchronous; selectAllAndAnchorLeft() defers through a
        // timer and must not be used on this per-frame path.
        pEdit->selectAll();
    }
    else
    {
        clearSelectionAndAnchorLeft(pEdit);
    }
}

QRect GuiCmdHoverInputPopupBase::computeBoundaryRectGlobal() const
{
    ViewWidget* pViewWidget = activeViewWidget();
    if (!pViewWidget)
    {
        return QRect();
    }

    return pViewWidget->getRenderAreaGlobalRect();
}

void GuiCmdHoverInputPopupBase::focusNextRow(bool backward)
{
    const QList<QLineEdit*> edits = this->findChildren<QLineEdit*>();
    QList<QLineEdit*> focusable;
    for (QLineEdit* pEdit : edits)
    {
        if (pEdit && pEdit->isVisibleTo(this) && pEdit->isEnabled() &&
            pEdit->focusPolicy() != Qt::NoFocus)
        {
            focusable.append(pEdit);
        }
    }
    if (focusable.isEmpty())
    {
        return;
    }

    QWidget* pFocus = QApplication::focusWidget();
    int index = -1;
    for (int i = 0; i < focusable.size(); ++i)
    {
        if (focusable.at(i) == pFocus)
        {
            index = i;
            break;
        }
    }

    const int count = focusable.size();
    const int next = backward ? (index <= 0 ? count - 1 : index - 1)
                              : ((index + 1) % count);
    focusable.at(next)->setFocus(Qt::TabFocusReason);
}

GuiCmdHoverInputPopup1::GuiCmdHoverInputPopup1(
    const QString& label,
    const QString& valueSample,
    QWidget* parent)
    : GuiCmdHoverInputPopupBase(parent),
    _pGridLayout(nullptr),
    _pRowLabel(nullptr),
    _pRowEdit(nullptr),
    _valueSample(valueSample),
    _focusInput(true)
{
    this->setObjectName("GuiCmdHoverInputPopup1");
    this->setStyleSheet(
        "QWidget#GuiCmdHoverInputPopup1{background:#d7d7d7;border:none;}"
        "QWidget#tableWidget{background:#e4e4e4;border:1px solid #8a8a8a;}"
        "QLabel#cellName{color:#202020;border-right:1px solid #8a8a8a;padding:2px 4px;}"
        "QLineEdit#cellValue{color:#202020;background:#ffffff;border:1px solid #8a8a8a;padding:2px 4px;selection-background-color:#0078d7;selection-color:#ffffff;}");

    QHBoxLayout* pLayout = new QHBoxLayout(this);
    pLayout->setContentsMargins(0, 0, 0, 0);
    pLayout->setSpacing(0);
    pLayout->setSizeConstraint(QLayout::SetFixedSize);

    QWidget* pTableWidget = new QWidget(this);
    pTableWidget->setObjectName("tableWidget");
    pTableWidget->setContentsMargins(0, 0, 0, 0);
    QGridLayout* pGridLayout = new QGridLayout(pTableWidget);
    pGridLayout->setContentsMargins(0, 0, 0, 0);
    pGridLayout->setHorizontalSpacing(0);
    pGridLayout->setVerticalSpacing(0);
    pGridLayout->setSizeConstraint(QLayout::SetFixedSize);
    _pGridLayout = pGridLayout;

    _pRowLabel = new QLabel(label, pTableWidget);
    _pRowLabel->setObjectName("cellName");
    _pRowLabel->setAlignment(Qt::AlignCenter);
    _pRowEdit = new QLineEdit(pTableWidget);
    _pRowEdit->setObjectName("cellValue");
    _pRowEdit->installEventFilter(this);
    this->trackUserEditing(_pRowEdit);

    this->applyPresetWidths();

    pGridLayout->addWidget(_pRowLabel, 0, 0);
    pGridLayout->addWidget(_pRowEdit, 0, 1);
    pGridLayout->setColumnStretch(0, 0);
    pGridLayout->setColumnStretch(1, 0);
    pLayout->addWidget(pTableWidget);

    QObject::connect(_pRowEdit, &QLineEdit::returnPressed, [this]()
    {
        this->onAccept();
    });
}

GuiCmdHoverInputPopup1::~GuiCmdHoverInputPopup1()
{
}

void GuiCmdHoverInputPopup1::setValue(const QString& value, bool overwriteUserInput)
{
    this->updateLineEditValue(_pRowEdit, value, overwriteUserInput);
}

void GuiCmdHoverInputPopup1::setValue(double value, int precision, bool overwriteUserInput)
{
    this->setValue(QString::number(value, 'f', precision), overwriteUserInput);
}

QString GuiCmdHoverInputPopup1::getRowText() const
{
    return _pRowEdit->text();
}

void GuiCmdHoverInputPopup1::selectAllValue()
{
    selectAllAndAnchorLeft(_pRowEdit);
}

bool GuiCmdHoverInputPopup1::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == _pRowEdit && event && event->type() == QEvent::FocusIn)
    {
        QPointer<QLineEdit> guard(_pRowEdit);
        QTimer::singleShot(0, _pRowEdit, [guard]()
        {
            if (!guard)
            {
                return;
            }
            selectAllAndAnchorLeft(guard.data());
        });
    }
    else if (watched == _pRowEdit && event && event->type() == QEvent::FocusOut)
    {
        QPointer<QLineEdit> guard(_pRowEdit);
        QTimer::singleShot(0, _pRowEdit, [guard]()
        {
            if (!guard)
            {
                return;
            }
            clearSelectionAndAnchorLeft(guard.data());
        });
    }
    else if (watched == _pRowEdit && event && event->type() == QEvent::KeyPress)
    {
        QKeyEvent* pKeyEvent = dynamic_cast<QKeyEvent*>(event);
        if (pKeyEvent && pKeyEvent->key() == Qt::Key_Escape)
        {
            this->onCancel();
            return true;
        }
    }
    return GuiCmdHoverInputPopupBase::eventFilter(watched, event);
}

void GuiCmdHoverInputPopup1::focusInput()
{
    if (_focusInput)
    {
        selectAllAndAnchorLeft(_pRowEdit);
    }
}

void GuiCmdHoverInputPopup1::applyPresetWidths()
{
    QFontMetrics labelMetrics(_pRowLabel->font());
    int labelWidth = std::max(kLabelMinWidthPx, labelMetrics.horizontalAdvance(_pRowLabel->text()) + kLabelExtraPaddingPx);
    _pRowLabel->setMinimumWidth(labelWidth);
    _pRowLabel->setMaximumWidth(labelWidth);

    QFontMetrics valueMetrics(_pRowEdit->font());
    int valueWidth = std::max(kValueMinWidthPx, valueMetrics.horizontalAdvance(_valueSample) + kValueExtraPaddingPx);
    _pRowEdit->setMinimumWidth(valueWidth);
    _pRowEdit->setMaximumWidth(valueWidth);
}

GuiCmdHoverInputPopup2::GuiCmdHoverInputPopup2(
    const QString& row1Label,
    const QString& row2Label,
    const QString& valueSample,
    QWidget* parent)
    : GuiCmdHoverInputPopupBase(parent),
    _pRow1Label(nullptr),
    _pRow2Label(nullptr),
    _pRow1Edit(nullptr),
    _pRow2Edit(nullptr),
    _valueSample(valueSample),
    _focusFirstRow(true)
{
    this->setObjectName("GuiCmdHoverInputPopup2");
    this->setStyleSheet(
        "QWidget#GuiCmdHoverInputPopup2{background:#d7d7d7;border:none;}"
        "QWidget#tableWidget{background:#e4e4e4;border:1px solid #8a8a8a;}"
        "QLabel#cellNameTop{color:#202020;border-right:1px solid #8a8a8a;border-bottom:1px solid #8a8a8a;padding:2px 4px;}"
        "QLabel#cellNameBottom{color:#202020;border-right:1px solid #8a8a8a;padding:2px 4px;}"
        "QLineEdit#cellValueTop{color:#202020;background:#ffffff;border-top:1px solid #8a8a8a;border-right:1px solid #8a8a8a;border-bottom:1px solid #8a8a8a;border-left:1px solid #8a8a8a;padding:2px 4px;selection-background-color:#0078d7;selection-color:#ffffff;}"
        "QLineEdit#cellValueBottom{color:#202020;background:#ffffff;border-top:none;border-right:1px solid #8a8a8a;border-bottom:1px solid #8a8a8a;border-left:1px solid #8a8a8a;padding:2px 4px;selection-background-color:#0078d7;selection-color:#ffffff;}");

    QHBoxLayout* pLayout = new QHBoxLayout(this);
    pLayout->setContentsMargins(0, 0, 0, 0);
    pLayout->setSpacing(0);
    pLayout->setSizeConstraint(QLayout::SetFixedSize);

    QWidget* pTableWidget = new QWidget(this);
    pTableWidget->setObjectName("tableWidget");
    pTableWidget->setContentsMargins(0, 0, 0, 0);
    QGridLayout* pGridLayout = new QGridLayout(pTableWidget);
    pGridLayout->setContentsMargins(0, 0, 0, 0);
    pGridLayout->setHorizontalSpacing(0);
    pGridLayout->setVerticalSpacing(0);
    pGridLayout->setSizeConstraint(QLayout::SetFixedSize);

    _pRow1Label = new QLabel(row1Label, pTableWidget);
    _pRow1Label->setObjectName("cellNameTop");
    _pRow1Label->setAlignment(Qt::AlignCenter);
    _pRow2Label = new QLabel(row2Label, pTableWidget);
    _pRow2Label->setObjectName("cellNameBottom");
    _pRow2Label->setAlignment(Qt::AlignCenter);
    _pRow1Edit = new QLineEdit(pTableWidget);
    _pRow1Edit->setObjectName("cellValueTop");
    _pRow2Edit = new QLineEdit(pTableWidget);
    _pRow2Edit->setObjectName("cellValueBottom");
    _pRow1Edit->installEventFilter(this);
    _pRow2Edit->installEventFilter(this);
    this->trackUserEditing(_pRow1Edit);
    this->trackUserEditing(_pRow2Edit);

    this->applyPresetWidths();

    pGridLayout->addWidget(_pRow1Label, 0, 0);
    pGridLayout->addWidget(_pRow1Edit, 0, 1);
    pGridLayout->addWidget(_pRow2Label, 1, 0);
    pGridLayout->addWidget(_pRow2Edit, 1, 1);
    pGridLayout->setColumnStretch(0, 0);
    pGridLayout->setColumnStretch(1, 0);
    pLayout->addWidget(pTableWidget);

    QObject::connect(_pRow1Edit, &QLineEdit::returnPressed, [this]()
    {
        this->onAccept();
    });
    QObject::connect(_pRow2Edit, &QLineEdit::returnPressed, [this]()
    {
        this->onAccept();
    });
}

GuiCmdHoverInputPopup2::~GuiCmdHoverInputPopup2()
{
}

void GuiCmdHoverInputPopup2::setValues(const QString& row1Value, const QString& row2Value)
{
    this->updateLineEditValue(_pRow1Edit, row1Value);
    this->updateLineEditValue(_pRow2Edit, row2Value);
}

void GuiCmdHoverInputPopup2::setValues(double row1Value, double row2Value, int precision)
{
    this->setValues(
        QString::number(row1Value, 'f', precision),
        QString::number(row2Value, 'f', precision));
}

QString GuiCmdHoverInputPopup2::getRow1Text() const
{
    return _pRow1Edit->text();
}

QString GuiCmdHoverInputPopup2::getRow2Text() const
{
    return _pRow2Edit->text();
}

bool GuiCmdHoverInputPopup2::eventFilter(QObject* watched, QEvent* event)
{
    if ((watched == _pRow1Edit || watched == _pRow2Edit) && event && event->type() == QEvent::FocusIn)
    {
        QLineEdit* pEdit = (watched == _pRow1Edit) ? _pRow1Edit : _pRow2Edit;
        QPointer<QLineEdit> guard(pEdit);
        QTimer::singleShot(0, pEdit, [guard]()
        {
            if (!guard)
            {
                return;
            }
            selectAllAndAnchorLeft(guard.data());
        });
    }
    else if ((watched == _pRow1Edit || watched == _pRow2Edit) && event && event->type() == QEvent::FocusOut)
    {
        QLineEdit* pEdit = (watched == _pRow1Edit) ? _pRow1Edit : _pRow2Edit;
        QPointer<QLineEdit> guard(pEdit);
        QTimer::singleShot(0, pEdit, [guard]()
        {
            if (!guard)
            {
                return;
            }
            clearSelectionAndAnchorLeft(guard.data());
        });
    }
    else if ((watched == _pRow1Edit || watched == _pRow2Edit) && event && event->type() == QEvent::KeyPress)
    {
        QKeyEvent* pKeyEvent = dynamic_cast<QKeyEvent*>(event);
        if (pKeyEvent && pKeyEvent->key() == Qt::Key_Escape)
        {
            this->onCancel();
            return true;
        }
    }
    return GuiCmdHoverInputPopupBase::eventFilter(watched, event);
}

void GuiCmdHoverInputPopup2::focusInput()
{
    if (_focusFirstRow)
    {
        selectAllAndAnchorLeft(_pRow1Edit);
    }
    else
    {
        selectAllAndAnchorLeft(_pRow2Edit);
    }
}

void GuiCmdHoverInputPopup2::applyPresetWidths()
{
    QFontMetrics labelMetrics(_pRow1Label->font());
    int labelTextWidth = std::max(
        labelMetrics.horizontalAdvance(_pRow1Label->text()),
        labelMetrics.horizontalAdvance(_pRow2Label->text()));
    int labelWidth = std::max(kLabelMinWidthPx, labelTextWidth + kLabelExtraPaddingPx);
    _pRow1Label->setMinimumWidth(labelWidth);
    _pRow2Label->setMinimumWidth(labelWidth);
    _pRow1Label->setMaximumWidth(labelWidth);
    _pRow2Label->setMaximumWidth(labelWidth);

    QFontMetrics valueMetrics(_pRow1Edit->font());
    int valueWidth = std::max(kValueMinWidthPx, valueMetrics.horizontalAdvance(_valueSample) + kValueExtraPaddingPx);
    _pRow1Edit->setMinimumWidth(valueWidth);
    _pRow2Edit->setMinimumWidth(valueWidth);
    _pRow1Edit->setMaximumWidth(valueWidth);
    _pRow2Edit->setMaximumWidth(valueWidth);
}

GuiCmdHoverInputPopup3::GuiCmdHoverInputPopup3(
    const QString& row1Label,
    const QString& row2Label,
    const QString& row3Label,
    const QString& valueSample,
    QWidget* parent)
    : GuiCmdHoverInputPopupBase(parent),
    _pRow1Label(nullptr),
    _pRow2Label(nullptr),
    _pRow3Label(nullptr),
    _pRow1Edit(nullptr),
    _pRow2Edit(nullptr),
    _pRow3Edit(nullptr),
    _valueSample(valueSample),
    _focusRowIndex(0)
{
    this->setObjectName("GuiCmdHoverInputPopup3");
    this->setStyleSheet(
        "QWidget#GuiCmdHoverInputPopup3{background:#d7d7d7;border:none;}"
        "QWidget#tableWidget{background:#e4e4e4;border:1px solid #8a8a8a;}"
        "QLabel#cellNameTop{color:#202020;border-right:1px solid #8a8a8a;border-bottom:1px solid #8a8a8a;padding:2px 4px;}"
        "QLabel#cellNameMiddle{color:#202020;border-right:1px solid #8a8a8a;border-bottom:1px solid #8a8a8a;padding:2px 4px;}"
        "QLabel#cellNameBottom{color:#202020;border-right:1px solid #8a8a8a;padding:2px 4px;}"
        "QLineEdit#cellValueTop{color:#202020;background:#ffffff;border-top:1px solid #8a8a8a;border-right:1px solid #8a8a8a;border-bottom:1px solid #8a8a8a;border-left:1px solid #8a8a8a;padding:2px 4px;selection-background-color:#0078d7;selection-color:#ffffff;}"
        "QLineEdit#cellValueMiddle{color:#202020;background:#ffffff;border-top:none;border-right:1px solid #8a8a8a;border-bottom:1px solid #8a8a8a;border-left:1px solid #8a8a8a;padding:2px 4px;selection-background-color:#0078d7;selection-color:#ffffff;}"
        "QLineEdit#cellValueBottom{color:#202020;background:#ffffff;border-top:none;border-right:1px solid #8a8a8a;border-bottom:1px solid #8a8a8a;border-left:1px solid #8a8a8a;padding:2px 4px;selection-background-color:#0078d7;selection-color:#ffffff;}");

    QHBoxLayout* pLayout = new QHBoxLayout(this);
    pLayout->setContentsMargins(0, 0, 0, 0);
    pLayout->setSpacing(0);
    pLayout->setSizeConstraint(QLayout::SetFixedSize);

    QWidget* pTableWidget = new QWidget(this);
    pTableWidget->setObjectName("tableWidget");
    pTableWidget->setContentsMargins(0, 0, 0, 0);
    QGridLayout* pGridLayout = new QGridLayout(pTableWidget);
    pGridLayout->setContentsMargins(0, 0, 0, 0);
    pGridLayout->setHorizontalSpacing(0);
    pGridLayout->setVerticalSpacing(0);
    pGridLayout->setSizeConstraint(QLayout::SetFixedSize);

    _pRow1Label = new QLabel(row1Label, pTableWidget);
    _pRow1Label->setObjectName("cellNameTop");
    _pRow1Label->setAlignment(Qt::AlignCenter);
    _pRow2Label = new QLabel(row2Label, pTableWidget);
    _pRow2Label->setObjectName("cellNameMiddle");
    _pRow2Label->setAlignment(Qt::AlignCenter);
    _pRow3Label = new QLabel(row3Label, pTableWidget);
    _pRow3Label->setObjectName("cellNameBottom");
    _pRow3Label->setAlignment(Qt::AlignCenter);
    _pRow1Edit = new QLineEdit(pTableWidget);
    _pRow1Edit->setObjectName("cellValueTop");
    _pRow2Edit = new QLineEdit(pTableWidget);
    _pRow2Edit->setObjectName("cellValueMiddle");
    _pRow3Edit = new QLineEdit(pTableWidget);
    _pRow3Edit->setObjectName("cellValueBottom");
    _pRow1Edit->installEventFilter(this);
    _pRow2Edit->installEventFilter(this);
    _pRow3Edit->installEventFilter(this);
    this->trackUserEditing(_pRow1Edit);
    this->trackUserEditing(_pRow2Edit);
    this->trackUserEditing(_pRow3Edit);

    this->applyPresetWidths();

    pGridLayout->addWidget(_pRow1Label, 0, 0);
    pGridLayout->addWidget(_pRow1Edit, 0, 1);
    pGridLayout->addWidget(_pRow2Label, 1, 0);
    pGridLayout->addWidget(_pRow2Edit, 1, 1);
    pGridLayout->addWidget(_pRow3Label, 2, 0);
    pGridLayout->addWidget(_pRow3Edit, 2, 1);
    pGridLayout->setColumnStretch(0, 0);
    pGridLayout->setColumnStretch(1, 0);
    pLayout->addWidget(pTableWidget);

    QObject::connect(_pRow1Edit, &QLineEdit::returnPressed, [this]()
    {
        this->onAccept();
    });
    QObject::connect(_pRow2Edit, &QLineEdit::returnPressed, [this]()
    {
        this->onAccept();
    });
    QObject::connect(_pRow3Edit, &QLineEdit::returnPressed, [this]()
    {
        this->onAccept();
    });
}

GuiCmdHoverInputPopup3::~GuiCmdHoverInputPopup3()
{
}

void GuiCmdHoverInputPopup3::setValues(
    const QString& row1Value,
    const QString& row2Value,
    const QString& row3Value)
{
    this->updateLineEditValue(_pRow1Edit, row1Value);
    this->updateLineEditValue(_pRow2Edit, row2Value);
    this->updateLineEditValue(_pRow3Edit, row3Value);
}

void GuiCmdHoverInputPopup3::setValues(
    double row1Value,
    double row2Value,
    double row3Value,
    int precision)
{
    this->setValues(
        QString::number(row1Value, 'f', precision),
        QString::number(row2Value, 'f', precision),
        QString::number(row3Value, 'f', precision));
}

QString GuiCmdHoverInputPopup3::getRow1Text() const
{
    return _pRow1Edit->text();
}

QString GuiCmdHoverInputPopup3::getRow2Text() const
{
    return _pRow2Edit->text();
}

QString GuiCmdHoverInputPopup3::getRow3Text() const
{
    return _pRow3Edit->text();
}

bool GuiCmdHoverInputPopup3::eventFilter(QObject* watched, QEvent* event)
{
    if ((watched == _pRow1Edit || watched == _pRow2Edit || watched == _pRow3Edit) &&
        event && event->type() == QEvent::FocusIn)
    {
        QLineEdit* pEdit = _pRow1Edit;
        if (watched == _pRow2Edit)
        {
            pEdit = _pRow2Edit;
        }
        else if (watched == _pRow3Edit)
        {
            pEdit = _pRow3Edit;
        }
        QPointer<QLineEdit> guard(pEdit);
        QTimer::singleShot(0, pEdit, [guard]()
        {
            if (!guard)
            {
                return;
            }
            selectAllAndAnchorLeft(guard.data());
        });
    }
    else if ((watched == _pRow1Edit || watched == _pRow2Edit || watched == _pRow3Edit) &&
        event && event->type() == QEvent::FocusOut)
    {
        QLineEdit* pEdit = _pRow1Edit;
        if (watched == _pRow2Edit)
        {
            pEdit = _pRow2Edit;
        }
        else if (watched == _pRow3Edit)
        {
            pEdit = _pRow3Edit;
        }
        QPointer<QLineEdit> guard(pEdit);
        QTimer::singleShot(0, pEdit, [guard]()
        {
            if (!guard)
            {
                return;
            }
            clearSelectionAndAnchorLeft(guard.data());
        });
    }
    else if ((watched == _pRow1Edit || watched == _pRow2Edit || watched == _pRow3Edit) &&
        event && event->type() == QEvent::KeyPress)
    {
        QKeyEvent* pKeyEvent = dynamic_cast<QKeyEvent*>(event);
        if (pKeyEvent && pKeyEvent->key() == Qt::Key_Escape)
        {
            this->onCancel();
            return true;
        }
    }
    return GuiCmdHoverInputPopupBase::eventFilter(watched, event);
}

void GuiCmdHoverInputPopup3::focusInput()
{
    QLineEdit* pEdit = _pRow1Edit;
    if (_focusRowIndex == 1)
    {
        pEdit = _pRow2Edit;
    }
    else if (_focusRowIndex == 2)
    {
        pEdit = _pRow3Edit;
    }

    selectAllAndAnchorLeft(pEdit);
}

void GuiCmdHoverInputPopup3::applyPresetWidths()
{
    QFontMetrics labelMetrics(_pRow1Label->font());
    int labelTextWidth = std::max(
        std::max(
            labelMetrics.horizontalAdvance(_pRow1Label->text()),
            labelMetrics.horizontalAdvance(_pRow2Label->text())),
        labelMetrics.horizontalAdvance(_pRow3Label->text()));
    int labelWidth = std::max(kLabelMinWidthPx, labelTextWidth + kLabelExtraPaddingPx);
    _pRow1Label->setMinimumWidth(labelWidth);
    _pRow2Label->setMinimumWidth(labelWidth);
    _pRow3Label->setMinimumWidth(labelWidth);
    _pRow1Label->setMaximumWidth(labelWidth);
    _pRow2Label->setMaximumWidth(labelWidth);
    _pRow3Label->setMaximumWidth(labelWidth);

    QFontMetrics valueMetrics(_pRow1Edit->font());
    int valueWidth = std::max(kValueMinWidthPx, valueMetrics.horizontalAdvance(_valueSample) + kValueExtraPaddingPx);
    _pRow1Edit->setMinimumWidth(valueWidth);
    _pRow2Edit->setMinimumWidth(valueWidth);
    _pRow3Edit->setMinimumWidth(valueWidth);
    _pRow1Edit->setMaximumWidth(valueWidth);
    _pRow2Edit->setMaximumWidth(valueWidth);
    _pRow3Edit->setMaximumWidth(valueWidth);
}

GuiCmdHoverInputPopup2_2ndTabLabel::GuiCmdHoverInputPopup2_2ndTabLabel(
    const QString& row1Label,
    const QString& row2Label,
    const QString& valueSample,
    QWidget* parent)
    : GuiCmdHoverInputPopupBase(parent),
    _pRow1Label(nullptr),
    _pRow2Label(nullptr),
    _pRowEdit(nullptr),
    _pDirectionEdit(nullptr),
    _valueSample(valueSample),
    _directionToggleHandler()
{
    this->setObjectName("GuiCmdHoverInputPopup2_2ndTabLabel");
    this->setStyleSheet(
        "QWidget#GuiCmdHoverInputPopup2_2ndTabLabel{background:#d7d7d7;border:none;}"
        "QWidget#tableWidget{background:#e4e4e4;border:1px solid #8a8a8a;}"
        "QLabel#cellNameTop{color:#202020;border-right:1px solid #8a8a8a;border-bottom:1px solid #8a8a8a;padding:2px 4px;}"
        "QLabel#cellNameBottom{color:#202020;border-right:1px solid #8a8a8a;padding:2px 4px;}"
        "QLineEdit#cellValueTop{color:#202020;background:#ffffff;border-top:1px solid #8a8a8a;border-right:1px solid #8a8a8a;border-bottom:1px solid #8a8a8a;border-left:1px solid #8a8a8a;padding:2px 4px;selection-background-color:#0078d7;selection-color:#ffffff;}"
        "QLineEdit#cellValueBottom{color:#202020;background:#e4e4e4;border-top:none;border-right:1px solid #8a8a8a;border-bottom:1px solid #8a8a8a;border-left:1px solid #8a8a8a;padding:2px 4px;}");

    QHBoxLayout* pLayout = new QHBoxLayout(this);
    pLayout->setContentsMargins(0, 0, 0, 0);
    pLayout->setSpacing(0);
    pLayout->setSizeConstraint(QLayout::SetFixedSize);

    QWidget* pTableWidget = new QWidget(this);
    pTableWidget->setObjectName("tableWidget");
    pTableWidget->setContentsMargins(0, 0, 0, 0);
    QGridLayout* pGridLayout = new QGridLayout(pTableWidget);
    pGridLayout->setContentsMargins(0, 0, 0, 0);
    pGridLayout->setHorizontalSpacing(0);
    pGridLayout->setVerticalSpacing(0);
    pGridLayout->setSizeConstraint(QLayout::SetFixedSize);

    _pRow1Label = new QLabel(row1Label, pTableWidget);
    _pRow1Label->setObjectName("cellNameTop");
    _pRow1Label->setAlignment(Qt::AlignCenter);
    _pRow2Label = new QLabel(row2Label, pTableWidget);
    _pRow2Label->setObjectName("cellNameBottom");
    _pRow2Label->setAlignment(Qt::AlignCenter);
    _pRowEdit = new QLineEdit(pTableWidget);
    _pRowEdit->setObjectName("cellValueTop");
    _pDirectionEdit = new QLineEdit(pTableWidget);
    _pDirectionEdit->setObjectName("cellValueBottom");
    _pDirectionEdit->setAlignment(Qt::AlignCenter);
    _pDirectionEdit->setReadOnly(true);
    _pRowEdit->installEventFilter(this);
    _pDirectionEdit->installEventFilter(this);
    this->trackUserEditing(_pRowEdit);

    this->applyPresetWidths();

    pGridLayout->addWidget(_pRow1Label, 0, 0);
    pGridLayout->addWidget(_pRowEdit, 0, 1);
    pGridLayout->addWidget(_pRow2Label, 1, 0);
    pGridLayout->addWidget(_pDirectionEdit, 1, 1);
    pGridLayout->setColumnStretch(0, 0);
    pGridLayout->setColumnStretch(1, 0);
    pLayout->addWidget(pTableWidget);

    QObject::connect(_pRowEdit, &QLineEdit::returnPressed, [this]()
    {
        this->onAccept();
    });
}

GuiCmdHoverInputPopup2_2ndTabLabel::~GuiCmdHoverInputPopup2_2ndTabLabel()
{
}

void GuiCmdHoverInputPopup2_2ndTabLabel::setValue(const QString& value)
{
    this->updateLineEditValue(_pRowEdit, value);
}

void GuiCmdHoverInputPopup2_2ndTabLabel::setValue(double value, int precision)
{
    this->setValue(QString::number(value, 'f', precision));
}

QString GuiCmdHoverInputPopup2_2ndTabLabel::getRowText() const
{
    return _pRowEdit->text();
}

void GuiCmdHoverInputPopup2_2ndTabLabel::setDirectionLabel(const QString& text)
{
    // Read-only row: always reflect the current direction, even while the user
    // has frozen the value row by typing.
    if (_pDirectionEdit->text() == text)
    {
        return;
    }
    _pDirectionEdit->setText(text);
    clearSelectionAndAnchorLeft(_pDirectionEdit);
}

void GuiCmdHoverInputPopup2_2ndTabLabel::setDirectionToggleHandler(const std::function<void()>& handler)
{
    _directionToggleHandler = handler;
    // Tab in this popup toggles the direction row instead of cycling rows.
    this->setTabKeyHandler(handler);
}

bool GuiCmdHoverInputPopup2_2ndTabLabel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == _pRowEdit && event && event->type() == QEvent::FocusIn)
    {
        QPointer<QLineEdit> guard(_pRowEdit);
        QTimer::singleShot(0, _pRowEdit, [guard]()
        {
            if (!guard)
            {
                return;
            }
            selectAllAndAnchorLeft(guard.data());
        });
    }
    else if (watched == _pRowEdit && event && event->type() == QEvent::FocusOut)
    {
        QPointer<QLineEdit> guard(_pRowEdit);
        QTimer::singleShot(0, _pRowEdit, [guard]()
        {
            if (!guard)
            {
                return;
            }
            clearSelectionAndAnchorLeft(guard.data());
        });
    }
    else if ((watched == _pRowEdit || watched == _pDirectionEdit) &&
        event && event->type() == QEvent::KeyPress)
    {
        QKeyEvent* pKeyEvent = dynamic_cast<QKeyEvent*>(event);
        if (pKeyEvent && pKeyEvent->key() == Qt::Key_Escape)
        {
            this->onCancel();
            return true;
        }
    }
    return GuiCmdHoverInputPopupBase::eventFilter(watched, event);
}

void GuiCmdHoverInputPopup2_2ndTabLabel::focusInput()
{
    selectAllAndAnchorLeft(_pRowEdit);
}

void GuiCmdHoverInputPopup2_2ndTabLabel::applyPresetWidths()
{
    QFontMetrics labelMetrics(_pRow1Label->font());
    int labelTextWidth = std::max(
        labelMetrics.horizontalAdvance(_pRow1Label->text()),
        labelMetrics.horizontalAdvance(_pRow2Label->text()));
    int labelWidth = std::max(kLabelMinWidthPx, labelTextWidth + kLabelExtraPaddingPx);
    _pRow1Label->setMinimumWidth(labelWidth);
    _pRow2Label->setMinimumWidth(labelWidth);
    _pRow1Label->setMaximumWidth(labelWidth);
    _pRow2Label->setMaximumWidth(labelWidth);

    QFontMetrics valueMetrics(_pRowEdit->font());
    int valueWidth = std::max(kValueMinWidthPx, valueMetrics.horizontalAdvance(_valueSample) + kValueExtraPaddingPx);
    _pRowEdit->setMinimumWidth(valueWidth);
    _pRowEdit->setMaximumWidth(valueWidth);
    _pDirectionEdit->setMinimumWidth(valueWidth);
    _pDirectionEdit->setMaximumWidth(valueWidth);
}

GuiCmdHoverInputPopup1_ChoiceRow::GuiCmdHoverInputPopup1_ChoiceRow(
    const QString& label,
    const QString& valueSample,
    const QStringList& choices,
    QWidget* parent)
    : GuiCmdHoverInputPopup1(label, valueSample, parent)
    , _choices()
    , _choiceHandler()
{
    assert(!choices.isEmpty());

    // The choice row takes the first row and the label/value row moves below
    // it; removeWidget is explicit because a freshly built popup has not been
    // laid out yet, so addWidget would not relocate the items by itself
    _pGridLayout->removeWidget(_pRowLabel);
    _pGridLayout->removeWidget(_pRowEdit);
    _pGridLayout->addWidget(_pRowLabel, 1, 0);
    _pGridLayout->addWidget(_pRowEdit, 1, 1);

    QWidget* pChoiceWidget = new QWidget(_pGridLayout->parentWidget());
    QHBoxLayout* pChoiceLayout = new QHBoxLayout(pChoiceWidget);
    pChoiceLayout->setContentsMargins(6, 4, 6, 4);
    pChoiceLayout->setSpacing(8);

    QButtonGroup* pChoiceGroup = new QButtonGroup(pChoiceWidget);
    for (int i = 0; i < choices.size(); ++i)
    {
        QRadioButton* pChoice = new QRadioButton(choices.at(i), pChoiceWidget);
        // Clicking a radio must not pull the keyboard focus off the value box:
        // Esc is handled on the edit, and a focused radio would let it through
        pChoice->setFocusPolicy(Qt::NoFocus);
        pChoiceLayout->addWidget(pChoice);
        pChoiceGroup->addButton(pChoice, i);
        _choices.push_back(pChoice);
    }
    pChoiceLayout->addStretch(1);
    if (!_choices.empty()) _choices.front()->setChecked(true);
    // Only a user click switches the choice; programmatic setChecked stays silent
    connect(pChoiceGroup, &QButtonGroup::idClicked, this, [this](int id)
    {
        if (_choiceHandler) _choiceHandler(id);
    });

    _pGridLayout->addWidget(pChoiceWidget, 0, 0, 1, 2);
    this->adjustSize();
}

GuiCmdHoverInputPopup1_ChoiceRow::~GuiCmdHoverInputPopup1_ChoiceRow()
{
}

void GuiCmdHoverInputPopup1_ChoiceRow::setChoice(int index)
{
    const int count = static_cast<int>(_choices.size());
    if (0 > index || count <= index) return;
    _choices.at(index)->setChecked(true);
}

void GuiCmdHoverInputPopup1_ChoiceRow::setChoiceHandler(const std::function<void(int)>& handler)
{
    _choiceHandler = handler;
}

} // namespace wy3dApp
