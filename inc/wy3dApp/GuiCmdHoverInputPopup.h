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

#ifndef WY3DAPP_GUI_CMD_HOVER_INPUT_POPUP_H
#define WY3DAPP_GUI_CMD_HOVER_INPUT_POPUP_H

#include <QString>
#include <QStringList>
#include <QWidget>
#include <QTimer>
#include <QPoint>
#include <QRect>
#include <functional>
#include <vector>
#include <wy3dApp/wy3dAppDefs.h>

class QButtonGroup;
class QGridLayout;
class QHideEvent;
class QShowEvent;
class QKeyEvent;
class QLabel;
class QLineEdit;
class QRadioButton;

namespace wy3dApp {

class WY3DAPP_EXPORT GuiCmdHoverInputPopupBase : public QWidget
{
public:
    explicit GuiCmdHoverInputPopupBase(QWidget* parent = nullptr);
    virtual ~GuiCmdHoverInputPopupBase();

    void setAcceptHandler(const std::function<void()>& handler);
    void setCancelHandler(const std::function<void()>& handler);
    void setSpaceKeyHandler(const std::function<void()>& handler);
    // Optional Tab handler. Without it Tab cycles the popup's rows.
    void setTabKeyHandler(const std::function<void()>& handler);
    void showAtGlobal(const QPoint& globalPos, int xOffset = 36, int yOffset = 36 );

    // Hides the popup immediately. Cancels a show/position update that was
    // deferred out of the OSG frame callback (see showAtGlobal()).
    void setVisible(bool visible) override;

    // Opt-in: dismiss the popup when the user clicks anywhere outside it, as
    // the click-to-open dimension editor does. Hover-summoned command popups
    // keep it false and are dismissed by their command instead.
    void setHideOnFocusLoss(bool hide) { _hideOnFocusLoss = hide; }

protected:
    virtual bool eventFilter(QObject* watched, QEvent* event) override;
    virtual void showEvent(QShowEvent* event) override;
    virtual void hideEvent(QHideEvent* event) override;
    virtual bool handleKeyPress(QObject* watched, QKeyEvent* event);

    virtual void focusInput() = 0;
    void onAccept() const;
    void onCancel() const;

    // Live (per-frame) value updates: once the user edits any row, programmatic
    // updates are frozen for the whole popup until it is hidden or reset.
    void trackUserEditing(QLineEdit* pEdit);
    void updateLineEditValue(QLineEdit* pEdit, const QString& value,
                             bool overwriteUserInput = false);

private:
    void onTimer();
    QRect computeBoundaryRectGlobal() const;
    void restoreFocusFromRenderArea(const QRect& renderRectGlobal);
    void focusNextRow(bool backward);

private:
    std::function<void()> _acceptHandler;
    std::function<void()> _cancelHandler;
    std::function<void()> _spaceKeyHandler;
    std::function<void()> _tabKeyHandler;

    QPoint _pendingPosition;
    QPoint _offset;

    // Pending show/position update, applied between paints. A running timer is
    // exactly "an update is still due"; hide() cancels it by stopping the timer.
    QTimer _timer;
    bool _hideOnFocusLoss = false;
    bool _userEdited = false;
};

class WY3DAPP_EXPORT GuiCmdHoverInputPopup1 : public GuiCmdHoverInputPopupBase
{
public:
    GuiCmdHoverInputPopup1(
        const QString& label,
        const QString& valueSample,
        QWidget* parent = nullptr);
    virtual ~GuiCmdHoverInputPopup1();

    void setValue(const QString& value, bool overwriteUserInput = false);
    void setValue(double value, int precision = 2, bool overwriteUserInput = false);
    QString getRowText() const;
    // setValue() leaves the text unselected with the caret at the front; call
    // this when the next typed digits should replace the whole value
    void selectAllValue();

protected:
    virtual bool eventFilter(QObject* watched, QEvent* event) override;
    virtual void focusInput() override;

    // Subclasses that add a row above the label/value row move these widgets
    // inside the grid; see GuiCmdHoverInputPopup1_ChoiceRow
    QGridLayout* _pGridLayout;
    QLabel* _pRowLabel;
    QLineEdit* _pRowEdit;

private:
    void applyPresetWidths();

private:
    QString _valueSample;
    bool _focusInput;
};

// One radio per entry of the choice list, in a row above the label/value row;
// the button group id is the entry index. Clicking a radio reports the new
// index through the handler; programmatic setChoice() stays silent.
class WY3DAPP_EXPORT GuiCmdHoverInputPopup1_ChoiceRow : public GuiCmdHoverInputPopup1
{
public:
    GuiCmdHoverInputPopup1_ChoiceRow(
        const QString& label,
        const QString& valueSample,
        const QStringList& choices,
        QWidget* parent = nullptr);
    virtual ~GuiCmdHoverInputPopup1_ChoiceRow();

    void setChoice(int index);
    void setChoiceHandler(const std::function<void(int)>& handler);

private:
    std::vector<QRadioButton*> _choices;
    std::function<void(int)> _choiceHandler;
};

class WY3DAPP_EXPORT GuiCmdHoverInputPopup2 : public GuiCmdHoverInputPopupBase
{
public:
    GuiCmdHoverInputPopup2(
        const QString& row1Label,
        const QString& row2Label,
        const QString& valueSample,
        QWidget* parent = nullptr);
    virtual ~GuiCmdHoverInputPopup2();

    void setValues(const QString& row1Value, const QString& row2Value);
    void setValues(double row1Value, double row2Value, int precision = 2);
    QString getRow1Text() const;
    QString getRow2Text() const;

protected:
    virtual bool eventFilter(QObject* watched, QEvent* event) override;
    virtual void focusInput() override;

private:
    void applyPresetWidths();

private:
    QLabel* _pRow1Label;
    QLabel* _pRow2Label;
    QLineEdit* _pRow1Edit;
    QLineEdit* _pRow2Edit;
    QString _valueSample;
    bool _focusFirstRow;
};

class WY3DAPP_EXPORT GuiCmdHoverInputPopup3 : public GuiCmdHoverInputPopupBase
{
public:
    GuiCmdHoverInputPopup3(
        const QString& row1Label,
        const QString& row2Label,
        const QString& row3Label,
        const QString& valueSample,
        QWidget* parent = nullptr);
    virtual ~GuiCmdHoverInputPopup3();

    void setValues(
        const QString& row1Value,
        const QString& row2Value,
        const QString& row3Value);
    void setValues(
        double row1Value,
        double row2Value,
        double row3Value,
        int precision = 2);
    QString getRow1Text() const;
    QString getRow2Text() const;
    QString getRow3Text() const;

protected:
    virtual bool eventFilter(QObject* watched, QEvent* event) override;
    virtual void focusInput() override;

private:
    void applyPresetWidths();

private:
    QLabel* _pRow1Label;
    QLabel* _pRow2Label;
    QLabel* _pRow3Label;
    QLineEdit* _pRow1Edit;
    QLineEdit* _pRow2Edit;
    QLineEdit* _pRow3Edit;
    QString _valueSample;
    int _focusRowIndex;
};

// Two-row popup: row 1 takes a value (QLineEdit), row 2 is a read-only
// QLineEdit showing the current direction; pressing Tab toggles the direction
// via the toggle handler
class WY3DAPP_EXPORT GuiCmdHoverInputPopup2_2ndTabLabel : public GuiCmdHoverInputPopupBase
{
public:
    GuiCmdHoverInputPopup2_2ndTabLabel(
        const QString& row1Label,
        const QString& row2Label,
        const QString& valueSample,
        QWidget* parent = nullptr);
    virtual ~GuiCmdHoverInputPopup2_2ndTabLabel();

    void setValue(const QString& value);
    void setValue(double value, int precision = 2);
    QString getRowText() const;

    void setDirectionLabel(const QString& text);
    void setDirectionToggleHandler(const std::function<void()>& handler);

protected:
    virtual bool eventFilter(QObject* watched, QEvent* event) override;
    virtual void focusInput() override;

private:
    void applyPresetWidths();

private:
    QLabel* _pRow1Label;
    QLabel* _pRow2Label;
    QLineEdit* _pRowEdit;
    QLineEdit* _pDirectionEdit;
    QString _valueSample;
    std::function<void()> _directionToggleHandler;
};

} // namespace wy3dApp

#endif // WY3DAPP_GUI_CMD_HOVER_INPUT_POPUP_H
