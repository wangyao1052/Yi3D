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
#include <QCursor>
#include <QEvent>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QTimer>
#include <algorithm>

#include "application/Application.h"
#include "widgets/frame/MainWindow.h"
#include "widgets/frame/ViewWidget.h"
#include "widgets/frame/ViewWidgetContainer.h"

namespace
{
constexpr int kLabelMinWidthPx = 32;
constexpr int kLabelExtraPaddingPx = 16;
constexpr int kValueMinWidthPx = 32;
constexpr int kValueExtraPaddingPx = 16;
constexpr qint64 kRenderAreaReentryDelayMs = 450;
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
    _defaultOffset(36, 36),
    _renderAreaReentryTimer(),
    _cursorOutsideRenderArea(false)
{
    this->setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    this->setAttribute(Qt::WA_DeleteOnClose, false);
    this->setAttribute(Qt::WA_StyledBackground, true);
    qApp->installEventFilter(this);
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

void GuiCmdHoverInputPopupBase::setDefaultOffset(const QPoint& offset)
{
    _defaultOffset = offset;
}

void GuiCmdHoverInputPopupBase::showAtGlobal(const QPoint& globalPos)
{
    if (QApplication::applicationState() != Qt::ApplicationActive)
    {
        _cursorOutsideRenderArea = true;
        _renderAreaReentryTimer.invalidate();
        if (this->isVisible())
        {
            this->hide();
        }
        return;
    }

    if (_renderAreaReentryTimer.isValid() &&
        _renderAreaReentryTimer.elapsed() < kRenderAreaReentryDelayMs)
    {
        return;
    }

    const QRect boundaryRectGlobal = this->computeBoundaryRectGlobal();
    if (!boundaryRectGlobal.isValid() || !boundaryRectGlobal.contains(globalPos))
    {
        _cursorOutsideRenderArea = true;
        _renderAreaReentryTimer.invalidate();
        if (this->isVisible())
        {
            this->hide();
        }
        return;
    }

    if (_cursorOutsideRenderArea)
    {
        _cursorOutsideRenderArea = false;
        _renderAreaReentryTimer.restart();
        return;
    }

    this->adjustSize();
    QPoint popupPos = globalPos + _defaultOffset;

    if (popupPos.x() + this->width() > boundaryRectGlobal.right())
    {
        popupPos.setX(globalPos.x() - _defaultOffset.x() - this->width());
    }
    if (popupPos.y() + this->height() > boundaryRectGlobal.bottom())
    {
        popupPos.setY(globalPos.y() - _defaultOffset.y() - this->height());
    }

    if (popupPos.x() < boundaryRectGlobal.left())
    {
        popupPos.setX(boundaryRectGlobal.left());
    }
    if (popupPos.y() < boundaryRectGlobal.top())
    {
        popupPos.setY(boundaryRectGlobal.top());
    }

    this->move(popupPos);
    this->show();
    this->raise();
    this->activateWindow();
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
        _cursorOutsideRenderArea = true;
        _renderAreaReentryTimer.invalidate();
        this->hide();
        break;
    }

    case QEvent::Enter:
    case QEvent::ApplicationActivate:
    {
        const QPoint cursorPos = QCursor::pos();
        const QRect renderRectGlobal = this->computeBoundaryRectGlobal();
        const bool cursorInRenderArea =
            renderRectGlobal.isValid() && renderRectGlobal.contains(cursorPos);
        const bool cursorInPopup =
            this->isVisible() && this->frameGeometry().contains(cursorPos);
        if (cursorInRenderArea && _cursorOutsideRenderArea)
        {
            _cursorOutsideRenderArea = false;
            _renderAreaReentryTimer.restart();
        }
        else if (!cursorInRenderArea && !cursorInPopup)
        {
            _cursorOutsideRenderArea = true;
            _renderAreaReentryTimer.invalidate();
            this->hide();
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

QRect GuiCmdHoverInputPopupBase::computeBoundaryRectGlobal() const
{
    Application& app = Application::instance();
    MainWindow* pMainWindow = app.getMainWindow();
    wyap::Document* pActiveDoc = app.getActiveDocument();
    if (!pMainWindow || !pActiveDoc)
    {
        return QRect();
    }

    ViewWidgetContainer* pViewWidgetContainer = pMainWindow->getViewWidgetContainer();
    if (!pViewWidgetContainer)
    {
        return QRect();
    }

    ViewWidget* pViewWidget = pViewWidgetContainer->getViewWidget(pActiveDoc);
    if (!pViewWidget)
    {
        return QRect();
    }

    return pViewWidget->getRenderAreaGlobalRect();
}

GuiCmdHoverInputPopup1::GuiCmdHoverInputPopup1(
    const QString& label,
    const QString& valueSample,
    QWidget* parent)
    : GuiCmdHoverInputPopupBase(parent),
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

    _pRowLabel = new QLabel(label, pTableWidget);
    _pRowLabel->setObjectName("cellName");
    _pRowLabel->setAlignment(Qt::AlignCenter);
    _pRowEdit = new QLineEdit(pTableWidget);
    _pRowEdit->setObjectName("cellValue");
    _pRowEdit->installEventFilter(this);

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

void GuiCmdHoverInputPopup1::setValue(const QString& value)
{
    _pRowEdit->setText(value);
    clearSelectionAndAnchorLeft(_pRowEdit);
}

void GuiCmdHoverInputPopup1::setValue(double value, int precision)
{
    this->setValue(QString::number(value, 'f', precision));
}

QString GuiCmdHoverInputPopup1::getRowText() const
{
    return _pRowEdit->text();
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
    _pRow1Edit->setText(row1Value);
    clearSelectionAndAnchorLeft(_pRow1Edit);
    _pRow2Edit->setText(row2Value);
    clearSelectionAndAnchorLeft(_pRow2Edit);
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
    _pRow1Edit->setText(row1Value);
    clearSelectionAndAnchorLeft(_pRow1Edit);
    _pRow2Edit->setText(row2Value);
    clearSelectionAndAnchorLeft(_pRow2Edit);
    _pRow3Edit->setText(row3Value);
    clearSelectionAndAnchorLeft(_pRow3Edit);
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
    _pRowEdit->setText(value);
    clearSelectionAndAnchorLeft(_pRowEdit);
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
    _pDirectionEdit->setText(text);
    clearSelectionAndAnchorLeft(_pDirectionEdit);
}

void GuiCmdHoverInputPopup2_2ndTabLabel::setDirectionToggleHandler(const std::function<void()>& handler)
{
    _directionToggleHandler = handler;
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
        if (pKeyEvent && pKeyEvent->key() == Qt::Key_Tab && _directionToggleHandler)
        {
            _directionToggleHandler();
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
