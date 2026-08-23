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
#include <QWidget>
#include <QElapsedTimer>
#include <QPoint>
#include <QRect>
#include <functional>

class QLabel;
class QLineEdit;

class GuiCmdHoverInputPopupBase : public QWidget
{
public:
    explicit GuiCmdHoverInputPopupBase(QWidget* parent = nullptr);
    virtual ~GuiCmdHoverInputPopupBase();

    void setAcceptHandler(const std::function<void()>& handler);
    void setCancelHandler(const std::function<void()>& handler);
    void setDefaultOffset(const QPoint& offset);
    void showAtGlobal(const QPoint& globalPos);

protected:
    virtual bool eventFilter(QObject* watched, QEvent* event) override;
    virtual void focusInput() = 0;
    void onAccept() const;
    void onCancel() const;

private:
    QRect computeBoundaryRectGlobal() const;

private:
    std::function<void()> _acceptHandler;
    std::function<void()> _cancelHandler;
    QPoint _defaultOffset;
    QElapsedTimer _renderAreaReentryTimer;
    bool _cursorOutsideRenderArea;
};

class GuiCmdHoverInputPopup1 : public GuiCmdHoverInputPopupBase
{
public:
    GuiCmdHoverInputPopup1(
        const QString& label,
        const QString& valueSample,
        QWidget* parent = nullptr);
    virtual ~GuiCmdHoverInputPopup1();

    void setValue(const QString& value);
    void setValue(double value, int precision = 2);
    QString getRowText() const;

protected:
    virtual bool eventFilter(QObject* watched, QEvent* event) override;
    virtual void focusInput() override;

private:
    void applyPresetWidths();

private:
    QLabel* _pRowLabel;
    QLineEdit* _pRowEdit;
    QString _valueSample;
    bool _focusInput;
};

class GuiCmdHoverInputPopup2 : public GuiCmdHoverInputPopupBase
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

class GuiCmdHoverInputPopup3 : public GuiCmdHoverInputPopupBase
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
class GuiCmdHoverInputPopup2_2ndTabLabel : public GuiCmdHoverInputPopupBase
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

#endif // WY3DAPP_GUI_CMD_HOVER_INPUT_POPUP_H
