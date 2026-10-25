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

#include "ViewWidgetContainer.h"
#include "ViewWidget.h"
#include "OsgViewWidget.h"
#include <wy3dApp/Application.h>
#include "application/ApplicationImpl.h"
#include "commands/FileCommands.h"

#include <QHBoxLayout>
#include <QTabWidget>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QTabBar>
#include <QWidget>
#include <QFileInfo>
#include <QSignalBlocker>
#include <QMenu>
#include <QAction>
#include <QCoreApplication>

#include <algorithm>
#include <cassert>

#include <wyapDocument.h>
#include <wyapDocManager.h>
#include <wyapEnums.h>
#include <wyapEnvManager.h>
#include <wyapEnvironment.h>
#include <wyapSelManager.h>
#include "commands/CommandNames.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

ViewWidgetContainer::ViewWidgetContainer(QWidget* parent)
    : QWidget(parent), _pTabWidget(nullptr), _middlePressTabIndex(-1)
{
    QHBoxLayout* mainLayout = new QHBoxLayout();
    this->setLayout(mainLayout);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 文档Tab页容器
    // 启用文档风格并去掉tab基线(仅影响视觉效果)
    _pTabWidget = new QTabWidget(this);
    _pTabWidget->setMovable(true);
    _pTabWidget->setTabsClosable(true);
    _pTabWidget->setDocumentMode(true);
    if (_pTabWidget->tabBar())
    {
        _pTabWidget->tabBar()->setDrawBase(false);
        _pTabWidget->tabBar()->installEventFilter(this);
    }
    mainLayout->addWidget(_pTabWidget);

    // 信号槽
    QObject::connect(_pTabWidget, &QTabWidget::currentChanged,
        this, &ViewWidgetContainer::onCurrentTabChanged);
    QObject::connect(_pTabWidget, &QTabWidget::tabCloseRequested,
        this, &ViewWidgetContainer::onTabCloseRequested);

    // 添加文档管理器反应器
    wyap::DocManager* pDocMgr = ApplicationImpl::instance().getDocManager();
    assert(pDocMgr);
    wy::ErrorStatus error = pDocMgr->addReactor(this);
    assert(wy::ErrorStatus::Ok == error);
}

ViewWidgetContainer::~ViewWidgetContainer()
{
    // 移除文档管理器反应器
    wyap::DocManager* pDocMgr = ApplicationImpl::instance().getDocManager();
    assert(pDocMgr);
    wy::ErrorStatus error = pDocMgr->removeReactor(this);
    assert(wy::ErrorStatus::Ok == error);
}

int ViewWidgetContainer::addPage(wyap::Document* pDoc, ViewWidget* pViewWidget)
{
    if (!pDoc || !pViewWidget)
    {
        assert(false);
        return -1;
    }
    //ViewWidget* pPageWidget = this->findPage(pDoc);
    //if (pPageWidget)
    //{
    //    int index = _pTabWidget->indexOf(pPageWidget);
    //    assert(index >= 0);
    //    return index;
    //}
    assert(_pTabWidget);
    const int index = _pTabWidget->addTab(pViewWidget, this->buildTabTitle(pDoc));
    {
        QSignalBlocker blocker(_pTabWidget);
        _pTabWidget->setCurrentIndex(index);
    }
    assert(index >= 0);
    _page2Doc[pViewWidget] = pDoc;
    _doc2Page[pDoc] = pViewWidget;
    return index;
}

bool ViewWidgetContainer::closePage(wyap::Document* pDoc)
{
    if (!pDoc)
    {
        assert(false);
        return false;
    }

    ViewWidget* pViewWidget = this->findPage(pDoc);
    if (!pViewWidget)
    {
        return false;
    }
    assert(_pTabWidget);
    int index = _pTabWidget->indexOf(pViewWidget);
    if (index < 0)
    {
        assert(false);
        return false;
    }

    _doc2Page.erase(pDoc);
    _page2Doc.erase(pViewWidget);

    _pTabWidget->removeTab(index);
    delete pViewWidget;
    return true;
}

ViewWidget* ViewWidgetContainer::findPage(wyap::Document* pDoc) const
{
    if (!pDoc)
    {
        assert(false);
        return nullptr;
    }

    auto iter = _doc2Page.find(pDoc);
    if (_doc2Page.cend() == iter)
    {
        return nullptr;
    }
    return iter->second;
}

wyap::Document* ViewWidgetContainer::findDocument(ViewWidget* pViewWidget) const
{
    if (!pViewWidget)
    {
        assert(false);
        return nullptr;
    }

    auto iter = _page2Doc.find(pViewWidget);
    if (_page2Doc.cend() == iter)
    {
        return nullptr;
    }
    return iter->second;
}

ViewWidget* ViewWidgetContainer::getViewWidget(wyap::Document* pDoc) const
{
    assert(_pTabWidget);
    if (!pDoc)
    {
        assert(false);
        return nullptr;
    }
    return this->findPage(pDoc);
}

ViewWidget* ViewWidgetContainer::getViewWidgetAt(int index) const
{
    assert(_pTabWidget);
    QWidget* pPageWidget = _pTabWidget->widget(index);
    if (!pPageWidget)
    {
        return nullptr;
    }
    return dynamic_cast<ViewWidget*>(pPageWidget);
}

wyap::Document* ViewWidgetContainer::getDocumentAt(int index) const
{
    ViewWidget* pViewWidget = this->getViewWidgetAt(index);
    if (!pViewWidget)
    {
        return nullptr;
    }
    return this->findDocument(pViewWidget);
}

void ViewWidgetContainer::onCurrentTabChanged(int index)
{
    wyap::Document* pDoc = this->getDocumentAt(index);
    wyap::DocManager* pDocMgr = ApplicationImpl::instance().getDocManager();
    assert(pDocMgr);

    auto restoreActiveDocumentTab = [this, pDocMgr]()
    {
        wyap::Document* pActiveDoc = pDocMgr->getActiveDocument();
        if (!pActiveDoc)
        {
            return;
        }

        ViewWidget* pActiveViewWidget = this->findPage(pActiveDoc);
        if (!pActiveViewWidget)
        {
            return;
        }

        const int activeIndex = _pTabWidget->indexOf(pActiveViewWidget);
        if (activeIndex >= 0 && _pTabWidget->currentIndex() != activeIndex)
        {
            QSignalBlocker blocker(_pTabWidget);
            _pTabWidget->setCurrentIndex(activeIndex);
        }
    };

    // Only in the document environment is tab switching allowed to change the active document.
    wyap::EnvManager* pEnvMgr = ApplicationImpl::instance().getEnvManager();
    assert(pEnvMgr);
    wyap::Environment* pCurEnv = pEnvMgr->getActiveEnvironment();
    if (!pCurEnv || pCurEnv->getType() != wyap::Environment::Type::Document)
    {
        restoreActiveDocumentTab();
        return;
    }

    if (!pDoc)
    {
        restoreActiveDocumentTab();
        return;
    }
    if (pDocMgr->getActiveDocument() != pDoc)
    {
        wy::ErrorStatus error = ApplicationImpl::instance().getCmdManager()->abortCurrentModalCommand(
            wyap::CmdExecution::AbortCause::ForceTerminate);
        assert(wy::ErrorStatus::Ok == error
            || wy::ErrorStatus::NoCurrentModalCommand == error);
        // added by wangyao 2026.06.10 {
        ApplicationImpl::instance().getSelManager()->beginChange();
        ApplicationImpl::instance().getSelManager()->clearSelections();
        ApplicationImpl::instance().getSelManager()->endChange();
        // }
        error = pDocMgr->activateDocument(pDoc, wyap::ExecutionMode::Sync);
        if (wy::ErrorStatus::Ok != error)
        {
            assert(false);
            restoreActiveDocumentTab();
            return;
        }
    }
}

void ViewWidgetContainer::applyViewSettings()
{
    const int count = _pTabWidget->count();
    for (int i = 0; i < count; ++i)
    {
        ViewWidget* pViewWidget = this->getViewWidgetAt(i);
        OsgViewWidget* pOsgViewWidget = dynamic_cast<OsgViewWidget*>(pViewWidget);
        if (pOsgViewWidget)
        {
            pOsgViewWidget->applyViewSettings();
        }
        else
        {
            assert(false);
        }
    }
}

bool ViewWidgetContainer::eventFilter(QObject* watched, QEvent* event)
{
    assert(_pTabWidget);
    if (watched && _pTabWidget->tabBar() == watched && event)
    {
        QEvent::Type eventType = event->type();
        QTabBar* pTabBar = _pTabWidget->tabBar();
        if (QEvent::MouseButtonPress == eventType)
        {
            QMouseEvent* pMouseEvent = dynamic_cast<QMouseEvent*>(event);
            if (pMouseEvent && Qt::MiddleButton == pMouseEvent->button())
            {
                _middlePressTabIndex = pTabBar->tabAt(pMouseEvent->pos());
                return true;
            }
        }
        else if (QEvent::MouseButtonRelease == eventType)
        {
            QMouseEvent* pMouseEvent = dynamic_cast<QMouseEvent*>(event);
            if (pMouseEvent && Qt::MiddleButton == pMouseEvent->button())
            {
                const int pressIndex = _middlePressTabIndex;
                _middlePressTabIndex = -1;
                const int releaseIndex = pTabBar->tabAt(pMouseEvent->pos());
                if (pressIndex >= 0 && pressIndex == releaseIndex)
                {
                    this->onTabCloseRequested(pressIndex);
                }
                return true;
            }
        }
        else if (QEvent::ContextMenu == eventType)
        {
            QContextMenuEvent* pContextMenuEvent = dynamic_cast<QContextMenuEvent*>(event);
            if (!pContextMenuEvent)
            {
                assert(false);
                return QWidget::eventFilter(watched, event);
            }
            const int tabIndex = pTabBar->tabAt(pContextMenuEvent->pos());
            if (tabIndex < 0)
            {
                return QWidget::eventFilter(watched, event);
            }
            this->showTabContextMenu(tabIndex, pContextMenuEvent->globalPos());
            return true;
        }

        return QWidget::eventFilter(watched, event);
    }
    else
    {
        assert(false);
        return QWidget::eventFilter(watched, event);
    }
}

void ViewWidgetContainer::showTabContextMenu(int tabIndex, const QPoint& globalPos)
{
    assert(_pTabWidget);
    wyap::DocManager* pDocMgr = ApplicationImpl::instance().getDocManager();
    if (!pDocMgr)
    {
        assert(false);
        return;
    }

    // Capture the document the user right-clicked before the menu's nested
    // event loop runs: the tab index may shift while the menu is open (e.g. an
    // IPC script closes a document), so the index must not be used afterwards.
    wyap::Document* pContextDoc = this->getDocumentAt(tabIndex);
    if (!pContextDoc)
    {
        assert(false);
        return;
    }
    const wyap::DocumentId contextDocId = pContextDoc->getId();

    QMenu menu;
    QAction* pActionClose = menu.addAction(
        QCoreApplication::translate("MainWindow", "Close"));
    menu.addSeparator();
    QAction* pActionCloseOthers = menu.addAction(
        QCoreApplication::translate("MainWindow", "Close Others"));
    QAction* pActionCloseAll = menu.addAction(
        QCoreApplication::translate("MainWindow", "Close All"));
    assert(pActionClose);
    assert(pActionCloseOthers);
    assert(pActionCloseAll);
    if (pActionCloseOthers)
    {
        pActionCloseOthers->setEnabled(_pTabWidget->count() > 1);
    }

    QAction* pSelectedAction = menu.exec(globalPos);
    if (!pSelectedAction)
    {
        return;
    }

    // Relocate the right-clicked document by ID: a document closed and reopened
    // while the menu was open gets a new ID, so this cannot match another
    // document. If it is gone, abandon the action.
    const std::vector<wyap::Document*> documents = pDocMgr->getDocuments();
    auto iter = std::find_if(documents.cbegin(), documents.cend(),
        [contextDocId](wyap::Document* pDoc)
        {
            return pDoc && pDoc->getId() == contextDocId;
        });
    if (documents.cend() == iter)
    {
        return;
    }
    pContextDoc = *iter;

    if (pSelectedAction == pActionClose)
    {
        this->closeDocumentWithPrompt(pContextDoc);
    }
    else if (pSelectedAction == pActionCloseOthers)
    {
        std::vector<wyap::Document*> docsToClose = this->getDocumentsInTabOrder();
        auto anchorIter = std::find(docsToClose.begin(), docsToClose.end(), pContextDoc);
        if (docsToClose.end() != anchorIter)
        {
            docsToClose.erase(anchorIter);
        }
        this->closeDocuments(docsToClose);
    }
    else if (pSelectedAction == pActionCloseAll)
    {
        this->closeDocuments(this->getDocumentsInTabOrder());
    }
    else
    {
        assert(false);
    }
}

std::vector<wyap::Document*> ViewWidgetContainer::getDocumentsInTabOrder() const
{
    assert(_pTabWidget);
    std::vector<wyap::Document*> documents;
    if (!_pTabWidget)
    {
        return documents;
    }

    // Snapshot before any close: closing removes tabs and shifts indices.
    const int count = _pTabWidget->count();
    documents.reserve(static_cast<size_t>(count));
    for (int index = 0; index < count; ++index)
    {
        wyap::Document* pDoc = this->getDocumentAt(index);
        if (!pDoc)
        {
            assert(false);
            continue;
        }
        documents.emplace_back(pDoc);
    }
    return documents;
}

void ViewWidgetContainer::onTabCloseRequested(int index)
{
    wyap::Document* pDoc = this->getDocumentAt(index);
    if (!pDoc)
    {
        return;
    }
    this->closeDocumentWithPrompt(pDoc);
}

bool ViewWidgetContainer::closeDocumentWithPrompt(wyap::Document* pDoc)
{
    if (!pDoc)
    {
        assert(false);
        return false;
    }
    wyap::DocManager* pDocMgr = ApplicationImpl::instance().getDocManager();
    if (!pDocMgr)
    {
        assert(false);
        return false;
    }

    // A modal command may own an open transaction; abort it before prompting
    // the active document (same ordering as the original single-tab close).
    if (pDocMgr->getActiveDocument() == pDoc && !this->abortCurrentModalCommand())
    {
        return false;
    }

    // Ask whether to save the document if it has unsaved changes. Cancel also
    // covers a failed save, so the document is never closed after losing data.
    if (FileCmdsUtil::UiAskRet::Cancel == FileCmdsUtil::uiAskWhetherSaveDocument(pDoc))
    {
        return false;
    }

    // Closing the active document requires closeActiveDocument().
    if (pDocMgr->getActiveDocument() == pDoc)
    {
        if (!this->abortCurrentModalCommand())
        {
            return false;
        }
        // Known to fail when a task/sketch environment is on top; the original
        // code intentionally did not assert here.
        const wy::ErrorStatus error = pDocMgr->closeActiveDocument(wyap::ExecutionMode::Sync);
        return (wy::ErrorStatus::Ok == error);
    }

    const wy::ErrorStatus error = pDocMgr->closeDocument(pDoc);
    assert(wy::ErrorStatus::Ok == error);
    return (wy::ErrorStatus::Ok == error);
}

bool ViewWidgetContainer::closeDocuments(const std::vector<wyap::Document*>& documents)
{
    wyap::DocManager* pDocMgr = ApplicationImpl::instance().getDocManager();
    if (!pDocMgr)
    {
        assert(false);
        return false;
    }

    // Close inactive documents first: closeDocument() rejects the active one,
    // and closing the active document hands activation to another tab (or
    // exits the document environment when none is left).
    wyap::Document* pActiveDoc = pDocMgr->getActiveDocument();
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
        if (!this->closeDocumentWithPrompt(pDoc))
        {
            return false; // Canceled or failed: keep the remaining documents open.
        }
    }

    const bool closeActiveDoc = pActiveDoc
        && (std::find(documents.cbegin(), documents.cend(), pActiveDoc) != documents.cend());
    if (closeActiveDoc && !this->closeDocumentWithPrompt(pActiveDoc))
    {
        return false;
    }
    return true;
}

bool ViewWidgetContainer::abortCurrentModalCommand()
{
    wyap::CmdManager* pCmdMgr = ApplicationImpl::instance().getCmdManager();
    assert(pCmdMgr);
    const wy::ErrorStatus error = pCmdMgr->abortCurrentModalCommand(
        wyap::CmdExecution::AbortCause::ForceTerminate);
    assert(wy::ErrorStatus::Ok == error
        || wy::ErrorStatus::NoCurrentModalCommand == error);
    assert(nullptr == pCmdMgr->getCurrentModalCommand());
    return (wy::ErrorStatus::Ok == error
        || wy::ErrorStatus::NoCurrentModalCommand == error);
}

void ViewWidgetContainer::onDocumentCreated(wyap::Document* pNewDoc)
{
    // wyap::Application率先响应了onDocumentCreated,通过newView接口创建了视图.
    assert(pNewDoc);
    ViewWidget* pViewWidget = this->findPage(pNewDoc);
    assert(pViewWidget);
}

void ViewWidgetContainer::onDocumentToBeDestroyed(wyap::Document* pDocToDestroy)
{
    assert(pDocToDestroy);
    assert(_pTabWidget);
    bool closeRet = this->closePage(pDocToDestroy);
    assert(closeRet);
}

void ViewWidgetContainer::onDocumentDestroyed(const std::string& fileName)
{
}

void ViewWidgetContainer::onDocumentToBeDeactivated(wyap::Document* pDocToDeactivate)
{
}

void ViewWidgetContainer::onDocumentToBeActivated(wyap::Document* pDocToActivate)
{
}

void ViewWidgetContainer::onDocumentActivated(wyap::Document* pActivatedDoc)
{
    assert(pActivatedDoc);

    ViewWidget* pViewWidget = this->findPage(pActivatedDoc);
    if (!pViewWidget)
    {
        assert(false);
        return;
    }
    assert(_pTabWidget);
    int index = _pTabWidget->indexOf(pViewWidget);
    if (index < 0)
    {
        assert(false);
        return;
    }

    const QString tabTitle = this->buildTabTitle(pActivatedDoc);
    if (_pTabWidget->tabText(index) != tabTitle)
    {
        _pTabWidget->setTabText(index, tabTitle);
    }
    if (_pTabWidget->currentIndex() != index)
    {
        _pTabWidget->setCurrentIndex(index);
    }
}

void ViewWidgetContainer::onDocumentStatusChanged(wyap::Document* pDoc, unsigned int oldStatus)
{
    assert(pDoc);
    ViewWidget* pViewWidget = this->findPage(pDoc);
    if (!pViewWidget)
    {
        assert(false);
        return;
    }

    assert(_pTabWidget);
    int index = _pTabWidget->indexOf(pViewWidget);
    if (index < 0)
    {
        assert(false);
        return;
    }

    QString tabTitle = this->buildTabTitle(pDoc);
    _pTabWidget->setTabText(index, tabTitle);
}

void ViewWidgetContainer::onDocumentTitleUpdated(wyap::Document* pDoc)
{
    assert(pDoc);
    ViewWidget* pViewWidget = this->findPage(pDoc);
    if (!pViewWidget)
    {
        assert(false);
        return;
    }

    assert(_pTabWidget);
    int index = _pTabWidget->indexOf(pViewWidget);
    if (index < 0)
    {
        assert(false);
        return;
    }

    QString tabTitle = this->buildTabTitle(pDoc);
    _pTabWidget->setTabText(index, tabTitle);
}

QString ViewWidgetContainer::buildTabTitle(wyap::Document* pDoc) const
{
    if (!pDoc)
    {
        assert(false);
        return QString("");
    }
    const std::string& fileName = pDoc->getFileName();
    QString title = QFileInfo(QString::fromStdString(fileName)).completeBaseName();
    // Prefix "*" marks unsaved changes (AutoCAD style).
    if (pDoc->hasStatus(wyap::DocumentStatus::Modified))
    {
        title.prepend("*");
    }
    return title;
}

} // namespace wy3dApp
