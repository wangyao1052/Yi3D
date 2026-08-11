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

#ifndef WY3DAPP_FLOATING_CMD_PANEL_H
#define WY3DAPP_FLOATING_CMD_PANEL_H

#include <QPoint>
#include <QWidget>

class QFrame;
class QHBoxLayout;
class QLabel;
class QPushButton;
class QShowEvent;
class QEvent;

class FloatingCmdPanel : public QWidget
{
    Q_OBJECT
public:
    explicit FloatingCmdPanel(QWidget* parent = nullptr);
    ~FloatingCmdPanel();

    void setTitle(const QString& title);

    // returns the content area widget for subclasses to populate
    QWidget* contentWidget() const { return _pContent; }
    // returns the footer layout for subclasses to add extra widgets/buttons
    QHBoxLayout* footerLayout() const { return _pFooterLayout; }

signals:
    void accepted();
    void canceled();

protected:
    virtual bool eventFilter(QObject* watched, QEvent* event) override;
    virtual void showEvent(QShowEvent* event) override;

    void anchorToTopLeft();
    void clampToParentBounds();

private slots:
    void onOkClicked();
    void onCancelClicked();

private:
    void setTitleBarWidget(QWidget* pTitleBar);

protected:
    QPoint _parentOffset;
    bool _userMoved;

private:
    QWidget* _pContent;
    QLabel* _pTitleLabel;
    QWidget* _pTitleBar;
    QHBoxLayout* _pFooterLayout;
    QPushButton* _pOkButton;
    QPushButton* _pCancelButton;
    bool _dragging;
    QPoint _dragOffset;
};

#endif // WY3DAPP_FLOATING_CMD_PANEL_H
