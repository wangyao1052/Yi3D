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

#ifndef WY3DAPP_GUI_CMD_MENU_H
#define WY3DAPP_GUI_CMD_MENU_H

#include <QMenu>
#include <QObject>
#include <QAction>

class GuiCommand;

// 交互命令上下文菜单
class GuiCmdMenu : public QObject
{
    Q_OBJECT
public:
    explicit GuiCmdMenu(GuiCommand* pCmd);
    virtual ~GuiCmdMenu() {}

    // 弹出菜单
    void exec(const QPoint& pos);

protected:
    // Custom menu actions — header (before Complete Selection / Clear Selection).
    // Returns true if any actions were added.
    virtual bool initCustomHeaderActions(QMenu* menu);

    // Custom menu actions — middle (after sketch env actions, before Property / WCS).
    // Returns true if any actions were added.
    virtual bool initCustomMiddleActions(QMenu* menu);

    // Custom menu actions — footer (after Property / WCS).
    // Returns true if any actions were added.
    virtual bool initCustomFooterActions(QMenu* menu);

private slots:
    // 完成选择
    void onCompleteSelection();
    // 清除选择
    void onClearSelection();
    
    // 属性框
    void onPropertyWidgetToggled(bool checked);
    // 世界坐标系
    void onWCSToggled(bool checked);

private:
    // 初始化
    void init(QMenu* menu);

protected:
    // 命令
    GuiCommand* _pCmd;
};

class GuiCmdQMenu : public QMenu
{
    Q_OBJECT
public:
    explicit GuiCmdQMenu(QWidget* parent = nullptr);

private slots:
    void installEventFilter();
    void removeEventFilter();

protected:
    virtual bool eventFilter(QObject* obj, QEvent* event) override;
};

#endif // WY3DAPP_GUI_CMD_MENU_H