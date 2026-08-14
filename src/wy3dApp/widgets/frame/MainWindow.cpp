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

#include "MainWindow.h"
#include "ViewWidgetContainer.h"
#include "application/Application.h"
#include "application/AutoSave.h"
#include "widgets/panels/DockPanelManager.h"
#include "OsgUtils.h"
#include "scene/Scene.h"
#include "utils/MessageBoxUtil.h"

#include <wyapCommand.h>
#include <wyapCmdStack.h>

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <cassert>

#include <QCoreApplication>
#include <QSettings>
#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QHBoxLayout>
#include <QThread>
#include <QTime>
#include <QProcess>
#include <QObject>
#include <QCloseEvent>

#include <osgDB/ReadFile>

#include <wyapSceneManager.h>
#include "view/OsgView.h"
#include "view/ViewUtil.h"

#include <wy3dMath.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dDatumPlane.h>
#include <wy3dBox.h>
#include <wy3dCylinder.h>
#include <wy3dSphere.h>
#include <wy3dSketch.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>
#include <wy3dExtrusion.h>
#include <wyapDocument.h>
#include <wyapDocManager.h>
#include <wyapCmdManager.h>
#include <wyapCmdManager.h>
#include "commands/GuiCommand.h"
#include "AboutDialog.h"
#include "exporter/Exporter.h"
#include "test/Test.h"
#include "commands/FileCommands.h"
#include "commands/CommandNames.h"
#include "ViewWidget.h"

#define TOOLBAR_ICON_SIZE 32
#define QSTR(wstr) QString::fromStdWString(wstr)

static ViewWidgetContainer* g_mainLayoutWindow = nullptr;

static QString shortenPathByLength(const QString& path, int maxChars)
{
    if (maxChars <= 10 || path.size() <= maxChars)
    {
        return path;
    }

    const int keepLeft = maxChars / 2 - 2;
    const int keepRight = maxChars - keepLeft - 3;
    return path.left(keepLeft) + "..." + path.right(keepRight);
}

MainWindow::MainWindow(QWidget* parent) :
    QMainWindow(parent),
    _tipsLabel(nullptr),
    _pDockPanelManager(nullptr)
{
    this->resize(1200, 800);
    QString title = tr("wy3dApp");
    this->setWindowTitle(title);
    Application::instance().setTitle(title);
    ViewWidgetContainer* layoutWindow = new ViewWidgetContainer(this);
    this->setCentralWidget(layoutWindow);
    g_mainLayoutWindow = layoutWindow;

    // 设置程序单例类的主窗口
    Application::instance().setMainWindow(this);

    // 初始化状态栏
    this->initStatusBar();
    // 初始化状态栏帮助类
    Application::instance().getStatusBar()->init(_tipsLabel);
    // 初始化停靠窗口
    this->initDockableWidgets();

    // 添加文档管理器反应器
    wyap::DocManager* pDocMgr = Application::instance().getDocManager();
    assert(pDocMgr);
    wy::ErrorStatus error = pDocMgr->addReactor(this);
    assert(wy::ErrorStatus::Ok == error);
}

MainWindow::~MainWindow()
{
    Application::instance().setDockPanelManager(nullptr);

    // 移除文档管理器反应器
    wyap::DocManager* pDocMgr = Application::instance().getDocManager();
    assert(pDocMgr);
    wy::ErrorStatus error = pDocMgr->removeReactor(this);
    assert(wy::ErrorStatus::Ok == error);
}

void MainWindow::initStatusBar()
{
    auto setWidgetFontSize = [](QWidget* pWidget, int pointSize)
    {
        QFont font = pWidget->font();
        font.setPointSize(pointSize);
        pWidget->setFont(font);
    };

    QStatusBar* pStarusBar = statusBar();
    pStarusBar->setStyleSheet("QStatusBar::item { border: none; }");
    pStarusBar->setMinimumHeight(30);

    // 提示标签
    {
        // 占位符:一个空格,方便布局
        pStarusBar->addPermanentWidget(new QLabel(" ", this));

        // 提示标签
        _tipsLabel = new QLabel(this);
        _tipsLabel->setStyleSheet("QLabel { height: 30px; }");
        setWidgetFontSize(_tipsLabel, 12);
        _tipsLabel->setText("");
        pStarusBar->addPermanentWidget(_tipsLabel, 1);
    }
}

void MainWindow::initDockableWidgets()
{
    _pDockPanelManager = new DockPanelManager(this);
    _pDockPanelManager->addBuiltinPanels();
    Application::instance().setDockPanelManager(_pDockPanelManager);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    assert(event);

    // Require the user to exit the task environment before closing the application.
    wyap::EnvManager* pEnvMgr = Application::instance().getEnvManager();
    wyap::Environment* pTopEnv = pEnvMgr->getTopEnvironment();
    if (pTopEnv && pTopEnv->getType() == wyap::Environment::Type::Task)
    {
        MessageBoxUtil::showWarning(tr("Please exit the task environment first."));
        event->ignore();
        return;
    }

    // Ask whether to return to the application and save the unsaved changes.
    const std::vector<wyap::Document*> modifiedDocs = this->getModifiedDocuments();
    if (!modifiedDocs.empty())
    {
        if (this->confirmReturnToSave(modifiedDocs))
        {
            event->ignore();
            return;
        }
    }

    // Abort current modal command.
    wy::ErrorStatus error = Application::instance().getCmdManager()->abortCurrentModalCommand(
        wyap::CmdExecution::AbortCause::ForceTerminate);
    if (wy::ErrorStatus::Ok != error &&
        wy::ErrorStatus::NoCurrentModalCommand != error)
    {
        assert(false);
        event->ignore();
        return;
    }

    // Close all documents.
    this->closeAllDocumentsForExit();

    // Clean up autosave files (normal exit)
    Application::instance().getAutoSave()->cleanupOnExit();

    // Close application.
    event->accept();
}

std::vector<wyap::Document*> MainWindow::getModifiedDocuments() const
{
    std::vector<wyap::Document*> modifiedDocs;
    wyap::DocManager* pDocMgr = Application::instance().getDocManager();
    const std::vector<wyap::Document*> documents = pDocMgr->getDocuments();
    for (wyap::Document* pDoc : documents)
    {
        if (!pDoc)
        {
            assert(false);
            continue;
        }
        if (pDoc->hasStatus(wyap::DocumentStatus::Modified))
        {
            modifiedDocs.emplace_back(pDoc);
        }
    }
    return modifiedDocs;
}

bool MainWindow::confirmReturnToSave(
    const std::vector<wyap::Document*>& modifiedDocs) const
{
    if (modifiedDocs.empty())
    {
        return false;
    }

    const int kMaxPathChars = 60;
    QStringList fileNames;
    int count = 0;
    for (wyap::Document* pDoc : modifiedDocs)
    {
        if (!pDoc)
        {
            assert(false);
            continue;
        }
        if (count < 10)
        {
            const QString filePath = QString::fromStdString(pDoc->getFileName());
            fileNames << shortenPathByLength(filePath, kMaxPathChars);
        }
        else if (count == 10)
        {
            fileNames << "......";
            break;
        }
        ++count;
    }

    QString text = tr("The following documents have unsaved changes.")
        + "\n"
        + tr("Do you want to return to the application so you can save these changes?")
        + "\n\n"
        + fileNames.join("\n");

    QMessageBox messageBox(
        QMessageBox::Warning,
        tr("Exit"),
        text,
        QMessageBox::NoButton,
        const_cast<MainWindow*>(this));
    QPushButton* pReturnButton = messageBox.addButton(
        tr("Yes"),
        QMessageBox::YesRole);
    QPushButton* pExitButton = messageBox.addButton(
        tr("No - Exit"),
        QMessageBox::NoRole);
    messageBox.setDefaultButton(qobject_cast<QPushButton*>(pReturnButton));
    messageBox.exec();
    return messageBox.clickedButton() == pReturnButton;
}

void MainWindow::closeAllDocumentsForExit()
{
    wyap::DocManager* pDocMgr = Application::instance().getDocManager();
    const std::vector<wyap::Document*> documents = pDocMgr->getDocuments();
    wyap::Document* pActiveDoc = pDocMgr->getActiveDocument();
    
    // Close all inactive documents.
    for (wyap::Document* pDoc : documents)
    {
        if (!pDoc)
        {
            assert(false);
            continue;
        }
        if (pDoc == pActiveDoc)
        {
            continue;
        }
        wy::ErrorStatus error = pDocMgr->closeDocument(pDoc);
        assert(wy::ErrorStatus::Ok == error);
    }

    // Close active document.
    if (pActiveDoc)
    {
        wy::ErrorStatus error = pDocMgr->closeActiveDocument(wyap::ExecutionMode::Sync);
        assert(wy::ErrorStatus::Ok == error);
    }
}

ViewWidgetContainer* MainWindow::getViewWidgetContainer() const
{
    return qobject_cast<ViewWidgetContainer*>(this->centralWidget());
}

ViewWidgetContainer* getViewWidgetContainer()
{
    return g_mainLayoutWindow;
}

void MainWindow::onDocumentDeactivated(wyap::Document* pDeactivatedDoc)
{
    assert(pDeactivatedDoc);
    this->setWindowTitle(tr("wy3dApp"));
}

void MainWindow::onDocumentActivated(wyap::Document* pActivatedDoc)
{
    assert(pActivatedDoc);
    const std::string& fileName = pActivatedDoc->getFileName();
    this->setWindowTitle(QString::fromStdString(fileName));
}
