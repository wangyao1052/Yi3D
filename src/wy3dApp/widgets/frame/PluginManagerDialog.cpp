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

#include "PluginManagerDialog.h"

#include <QAbstractItemView>
#include <QAction>
#include <QCoreApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <wyapCmdManager.h>

#include "application/ApplicationImpl.h"
#include "plugins/PluginManager.h"
#include "utils/MessageBoxUtil.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

namespace {

// UI thread only. Counts live PluginManagerDialog instances: the dialog counts
// as open from construction until destruction, regardless of which modal
// window (e.g. one opened by plugin code) is currently on top.
int s_activeInstanceCount = 0;

QString pluginStateText(PluginState state)
{
    switch (state)
    {
    case PluginState::Loaded:
        return QCoreApplication::translate("wy3dApp::PluginManagerDialog", "Loaded");
    case PluginState::Disabled:
        return QCoreApplication::translate("wy3dApp::PluginManagerDialog", "Disabled");
    case PluginState::Failed:
        return QCoreApplication::translate("wy3dApp::PluginManagerDialog", "Failed");
    case PluginState::Unloading:
        return QCoreApplication::translate("wy3dApp::PluginManagerDialog", "Unloading");
    case PluginState::Unloaded:
        return QCoreApplication::translate("wy3dApp::PluginManagerDialog", "Unloaded");
    case PluginState::NeedsRestart:
        return QCoreApplication::translate("wy3dApp::PluginManagerDialog", "Needs restart");
    }
    return QString();
}

} // namespace

PluginManagerDialog::PluginManagerDialog(QWidget* parent)
    : QDialog(parent)
    , _pTable(nullptr)
    , _pUnloadButton(nullptr)
    , _pLoadButton(nullptr)
    , _pRefreshButton(nullptr)
    , _pCloseButton(nullptr)
    , _pHintLabel(nullptr)
{
    ++s_activeInstanceCount;
    this->initUi();
}

PluginManagerDialog::~PluginManagerDialog()
{
    if (s_activeInstanceCount > 0)
    {
        --s_activeInstanceCount;
    }
}

bool PluginManagerDialog::isActive()
{
    return s_activeInstanceCount > 0;
}

void PluginManagerDialog::initUi()
{
    this->setWindowTitle(tr("Plugin Management"));
    this->setWindowIcon(QIcon(QStringLiteral(":/images/Plugin_Manage.svg")));
    this->resize(820, 440);

    _pTable = new QTableWidget(this);
    _pTable->setColumnCount(4);
    _pTable->setHorizontalHeaderLabels(
        { tr("Name"), tr("ID"), tr("Status"), tr("Details") });
    _pTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _pTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    _pTable->setSelectionMode(QAbstractItemView::SingleSelection);
    _pTable->setAlternatingRowColors(true);
    _pTable->setWordWrap(false);
    _pTable->verticalHeader()->setVisible(false);

    // Interactive sections: the user can drag the column dividers. The last
    // section still fills the remaining width.
    QHeaderView* pHorizontalHeader = _pTable->horizontalHeader();
    pHorizontalHeader->setSectionResizeMode(QHeaderView::Interactive);
    pHorizontalHeader->setStretchLastSection(true);
    pHorizontalHeader->setMinimumSectionSize(60);
    _pTable->setColumnWidth(0, 200);
    _pTable->setColumnWidth(1, 130);
    _pTable->setColumnWidth(2, 90);

    _pHintLabel = new QLabel(this);
    _pHintLabel->setWordWrap(true);
    _pHintLabel->setMinimumHeight(36);

    QLabel* pSessionHint = new QLabel(
        tr("Plugin operations apply to the current session only; plugin.ini is not modified."),
        this);
    pSessionHint->setWordWrap(true);

    _pUnloadButton = new QPushButton(tr("Unload"), this);
    _pLoadButton = new QPushButton(tr("Load"), this);
    _pRefreshButton = new QPushButton(tr("Refresh"), this);
    _pCloseButton = new QPushButton(tr("Close"), this);

    // Creo-style layout: the plugin actions form a vertical column to the
    // right of the table; the dialog's close button stays at the bottom right.
    const int actionButtonWidth = 96;
    _pLoadButton->setMinimumWidth(actionButtonWidth);
    _pUnloadButton->setMinimumWidth(actionButtonWidth);
    _pRefreshButton->setMinimumWidth(actionButtonWidth);
    _pCloseButton->setMinimumWidth(actionButtonWidth);

    QVBoxLayout* pActionLayout = new QVBoxLayout();
    pActionLayout->addWidget(_pLoadButton);
    pActionLayout->addWidget(_pUnloadButton);
    pActionLayout->addWidget(_pRefreshButton);
    pActionLayout->addStretch(1);

    QHBoxLayout* pContentLayout = new QHBoxLayout();
    pContentLayout->addWidget(_pTable, 1);
    pContentLayout->addLayout(pActionLayout);

    // Creo-style separator between the content and the close row.
    QFrame* pSeparator = new QFrame(this);
    pSeparator->setFrameShape(QFrame::HLine);
    pSeparator->setFrameShadow(QFrame::Sunken);

    QHBoxLayout* pCloseLayout = new QHBoxLayout();
    pCloseLayout->addStretch(1);
    pCloseLayout->addWidget(_pCloseButton);

    QVBoxLayout* pMainLayout = new QVBoxLayout(this);
    pMainLayout->addLayout(pContentLayout, 1);
    pMainLayout->addWidget(_pHintLabel);
    pMainLayout->addWidget(pSessionHint);
    pMainLayout->addWidget(pSeparator);
    pMainLayout->addLayout(pCloseLayout);

    this->connect(_pUnloadButton, &QPushButton::clicked, this, [this]() {
        this->unloadSelected();
    });
    this->connect(_pLoadButton, &QPushButton::clicked, this, [this]() {
        this->loadSelected();
    });
    this->connect(_pRefreshButton, &QPushButton::clicked, this, [this]() {
        this->refresh();
    });
    this->connect(_pCloseButton, &QPushButton::clicked, this, &QDialog::accept);
    this->connect(_pTable, &QTableWidget::itemSelectionChanged, this, [this]() {
        this->updateButtons();
    });
    _pTable->setContextMenuPolicy(Qt::CustomContextMenu);
    this->connect(
        _pTable, &QTableWidget::customContextMenuRequested,
        this, [this](const QPoint& pos) { this->showContextMenu(pos); });

    this->refresh();
}

void PluginManagerDialog::refresh()
{
    const QString previousKey = this->selectedKey();
    const std::vector<PluginInfo> plugins = PluginManager::instance().getPlugins();

    _pTable->blockSignals(true);
    _pTable->setRowCount(0);
    for (const PluginInfo& info : plugins)
    {
        const int row = _pTable->rowCount();
        _pTable->insertRow(row);

        const QString name = QString::fromStdString(
            info.name.empty() ? info.key : info.name);
        QTableWidgetItem* pNameItem = new QTableWidgetItem(name);
        pNameItem->setData(Qt::UserRole, QString::fromStdString(info.key));
        pNameItem->setToolTip(QString::fromStdString(info.key));
        _pTable->setItem(row, 0, pNameItem);
        _pTable->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(info.id)));
        _pTable->setItem(row, 2, new QTableWidgetItem(pluginStateText(info.state)));

        QString details;
        if (!info.error.empty())
        {
            details = QString::fromStdString(info.error);
        }
        else
        {
            details = QString::fromStdString(info.libraryPath);
        }
        QTableWidgetItem* pDetailsItem = new QTableWidgetItem(details);
        pDetailsItem->setToolTip(details);
        _pTable->setItem(row, 3, pDetailsItem);
    }
    _pTable->blockSignals(false);

    int rowToSelect = -1;
    if (!previousKey.isEmpty())
    {
        for (int row = 0; row < _pTable->rowCount(); ++row)
        {
            QTableWidgetItem* pItem = _pTable->item(row, 0);
            if (pItem && pItem->data(Qt::UserRole).toString() == previousKey)
            {
                rowToSelect = row;
                break;
            }
        }
    }
    if (rowToSelect < 0 && _pTable->rowCount() > 0) rowToSelect = 0;

    if (rowToSelect >= 0) _pTable->selectRow(rowToSelect);
    this->updateButtons();
}

void PluginManagerDialog::updateButtons()
{
    _pUnloadButton->setEnabled(false);
    _pLoadButton->setEnabled(false);
    _pHintLabel->clear();

    const QString key = this->selectedKey();
    if (key.isEmpty()) return;

    const std::string keyU8 = key.toStdString();
    const std::vector<PluginInfo> plugins = PluginManager::instance().getPlugins();
    const PluginInfo* pSelected = nullptr;
    for (const PluginInfo& info : plugins)
    {
        if (info.key == keyU8)
        {
            pSelected = &info;
            break;
        }
    }
    if (!pSelected) return;

    if (pSelected->state == PluginState::Loaded)
    {
        std::string reason;
        if (PluginManager::instance().canUnloadPlugin(keyU8, reason))
        {
            _pUnloadButton->setEnabled(true);
            _pHintLabel->setText(
                tr("Unload removes the plugin from this session. Plugin files are kept."));
        }
        else
        {
            _pHintLabel->setText(QString::fromStdString(reason));
        }
        return;
    }

    switch (pSelected->state)
    {
    case PluginState::Disabled:
    case PluginState::Failed:
    case PluginState::Unloaded:
        _pLoadButton->setEnabled(true);
        _pHintLabel->setText(
            tr("Load the plugin into this session. plugin.ini is not modified."));
        break;
    case PluginState::NeedsRestart:
        _pHintLabel->setText(
            tr("Restart the application before loading this plugin again."));
        break;
    default:
        _pHintLabel->setText(tr("The plugin is busy."));
        break;
    }
}

void PluginManagerDialog::unloadSelected()
{
    const QString key = this->selectedKey();
    if (key.isEmpty()) return;

    const QString name = this->displayNameForKey(key);
    QString message = tr(
        "Unload plugin \"%1\"?\n\n"
        "Its commands and UI will be removed from this session. Plugin files are kept.")
        .arg(name);

    wyap::CmdManager* pCmdManager = ApplicationImpl::instance().getCmdManager();
    if (pCmdManager && pCmdManager->getCurrentModalCommand())
    {
        message += tr("\n\nThe command currently running will be aborted.");
    }

    if (QMessageBox::question(
        this,
        tr("Unload Plugin"),
        message,
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No) != QMessageBox::Yes)
    {
        return;
    }

    std::string error;
    if (!PluginManager::instance().unloadPlugin(key.toStdString(), error))
    {
        if (!error.empty())
        {
            MessageBoxUtil::showError(QString::fromStdString(error));
        }
    }
    this->refresh();
}

void PluginManagerDialog::loadSelected()
{
    const QString key = this->selectedKey();
    if (key.isEmpty()) return;

    std::string error;
    if (!PluginManager::instance().loadPlugin(key.toStdString(), error))
    {
        if (!error.empty())
        {
            MessageBoxUtil::showError(QString::fromStdString(error));
        }
    }
    this->refresh();
}

void PluginManagerDialog::showContextMenu(const QPoint& pos)
{
    QTableWidgetItem* pItem = _pTable->itemAt(pos);
    if (!pItem)
    {
        return;
    }

    // Right-clicking a row selects it, so the menu reflects that plugin.
    _pTable->selectRow(pItem->row());

    QMenu menu(this);
    QAction* pLoadAction = menu.addAction(tr("Load"));
    pLoadAction->setEnabled(_pLoadButton->isEnabled());
    QAction* pUnloadAction = menu.addAction(tr("Unload"));
    pUnloadAction->setEnabled(_pUnloadButton->isEnabled());

    QAction* pChosenAction = menu.exec(_pTable->viewport()->mapToGlobal(pos));
    if (pChosenAction == pLoadAction)
    {
        this->loadSelected();
    }
    else if (pChosenAction == pUnloadAction)
    {
        this->unloadSelected();
    }
}

QString PluginManagerDialog::selectedKey() const
{
    const int row = _pTable ? _pTable->currentRow() : -1;
    if (row < 0) return QString();

    QTableWidgetItem* pItem = _pTable->item(row, 0);
    return pItem ? pItem->data(Qt::UserRole).toString() : QString();
}

QString PluginManagerDialog::displayNameForKey(const QString& key) const
{
    const std::vector<PluginInfo> plugins = PluginManager::instance().getPlugins();
    for (const PluginInfo& info : plugins)
    {
        if (info.key == key.toStdString())
        {
            if (!info.name.empty()) return QString::fromStdString(info.name);
            if (!info.id.empty()) return QString::fromStdString(info.id);
            return key;
        }
    }
    return key;
}

} // namespace wy3dApp
