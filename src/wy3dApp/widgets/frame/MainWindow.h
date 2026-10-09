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

#pragma once

#include <QtWidgets/QMainWindow>
#include <QAction>
#include <QMenuBar>
#include <QToolBar>
#include <QMenu>
#include <QStatusBar>
#include <QLabel>
#include <QComboBox>
#include <QDockWidget>
#include <QEvent>
#include <vector>
#include <wydbDatabase.h>
#include <wyapDocManager.h>

class Document;
class DockPanelManager;
class OutputWidget;
class GuiCmdActionMgr;
class MainWindowUI;
class ViewWidgetContainer;

class MainWindow :
    public QMainWindow,
    public wyap::DocManagerReactor
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = Q_NULLPTR);
    ~MainWindow();

    ViewWidgetContainer* getViewWidgetContainer() const;

    // 状态栏提示标签
    QLabel* getStatusBarTipsLabel() const
    {
        return _tipsLabel;
    }

    // Document manager reactor functions.
    virtual void onDocumentDeactivated(wyap::Document* pDeactivatedDoc) override;
    virtual void onDocumentActivated(wyap::Document* pActivatedDoc) override;

private:
    // 初始化状态栏
    void initStatusBar();
    // 初始化可停靠窗口
    void initDockableWidgets();

    std::vector<wyap::Document*> getModifiedDocuments() const;
    bool confirmReturnToSave(
        const std::vector<wyap::Document*>& modifiedDocs) const;
    void closeAllDocumentsForExit();

protected:
    virtual void closeEvent(QCloseEvent* event) override;

private:
    // status bar
    QLabel* _tipsLabel;
    // 停靠面板管理器
    DockPanelManager* _pDockPanelManager;

    // UI
    MainWindowUI* _pUI;

public:
    friend class MainWindowUI;
};

