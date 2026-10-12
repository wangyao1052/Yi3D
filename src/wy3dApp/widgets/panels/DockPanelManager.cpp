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

#include "DockPanelManager.h"
#include "DockPanelIds.h"

#include "featureTree/FeatureTreeWidget.h"
#include "output/OutputWidget.h"
#include "property/PropertyEditorWidget.h"

#include <QDockWidget>
#include <QMainWindow>
#include <QWidget>

#include <cassert>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

DockPanelManager::DockPanelManager(QMainWindow* pMainWindow) :
    QObject(pMainWindow),
    _pMainWindow(pMainWindow),
    _panelMap()
{
    assert(_pMainWindow);
}

void DockPanelManager::addBuiltinPanels()
{
    _builtinPanelIds = {
        DockPanelIds::FeatureTree,
        DockPanelIds::Property,
        DockPanelIds::Output
    };

    this->addPanel({
        DockPanelIds::FeatureTree,
        tr("FeatureTree"),
        Qt::LeftDockWidgetArea,
        Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea,
        true,
        [](QWidget* parent) -> QWidget*
        {
            return new FeatureTreeWidget(parent);
        }
    });

    this->addPanel({
        DockPanelIds::Property,
        tr("Property"),
        Qt::RightDockWidgetArea,
        Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea,
        true,
        [](QWidget* parent) -> QWidget*
        {
            return new PropertyEditorWidget(parent);
        }
    });

    this->addPanel({
        DockPanelIds::Output,
        tr("Output"),
        Qt::BottomDockWidgetArea,
        Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea,
        false,
        [](QWidget* parent) -> QWidget*
        {
            return new OutputWidget(parent);
        }
    });
}

bool DockPanelManager::addPanel(const DockPanelDefinition& definition)
{
    if (!_pMainWindow ||
        definition.id.empty() ||
        definition.title.isEmpty() ||
        !definition.widgetCreator)
    {
        assert(false);
        return false;
    }

    if (_panelMap.find(definition.id) != _panelMap.end())
    {
        assert(false);
        return false;
    }

    DockPanelEntry entry;
    entry.definition = definition;
    this->createDockPanel(entry);
    _panelMap.emplace(definition.id, entry);
    return true;
}

bool DockPanelManager::removePanel(const std::string& id)
{
    if (id.empty())
    {
        assert(false);
        return false;
    }

    if (this->isBuiltinPanelId(id))
    {
        assert(false);
        return false;
    }

    auto iter = _panelMap.find(id);
    if (iter == _panelMap.end())
    {
        return false;
    }

    DockPanelEntry& entry = iter->second;
    if (entry.dockWidget)
    {
        _pMainWindow->removeDockWidget(entry.dockWidget);
        entry.dockWidget->hide();
        entry.dockWidget->deleteLater();
        entry.dockWidget = nullptr;
    }
    entry.widget = nullptr;

    _panelMap.erase(iter);
    return true;
}

bool DockPanelManager::hasPanel(const std::string& id) const
{
    return _panelMap.find(id) != _panelMap.cend();
}

bool DockPanelManager::isPanelVisible(const std::string& id) const
{
    const DockPanelEntry* pEntry = this->findPanel(id);
    return pEntry && pEntry->dockWidget && pEntry->dockWidget->isVisible();
}

DockPanelManager::DockPanelEntry* DockPanelManager::findPanel(const std::string& id)
{
    auto iter = _panelMap.find(id);
    if (iter == _panelMap.end())
    {
        return nullptr;
    }
    return &iter->second;
}

const DockPanelManager::DockPanelEntry* DockPanelManager::findPanel(const std::string& id) const
{
    auto iter = _panelMap.find(id);
    if (iter == _panelMap.cend())
    {
        return nullptr;
    }
    return &iter->second;
}

QWidget* DockPanelManager::findWidget(const std::string& id) const
{
    const DockPanelEntry* pEntry = this->findPanel(id);
    return pEntry ? pEntry->widget : nullptr;
}

QDockWidget* DockPanelManager::findDockWidget(const std::string& id) const
{
    const DockPanelEntry* pEntry = this->findPanel(id);
    return pEntry ? pEntry->dockWidget : nullptr;
}

void DockPanelManager::showPanel(const std::string& id)
{
    if (QDockWidget* pDock = this->findDockWidget(id))
    {
        pDock->show();
    }
}

void DockPanelManager::hidePanel(const std::string& id)
{
    if (QDockWidget* pDock = this->findDockWidget(id))
    {
        pDock->hide();
    }
}

void DockPanelManager::togglePanel(const std::string& id)
{
    if (QDockWidget* pDock = this->findDockWidget(id))
    {
        pDock->setVisible(!pDock->isVisible());
    }
}

void DockPanelManager::createDockPanel(DockPanelEntry& entry)
{
    assert(_pMainWindow);
    assert(entry.definition.widgetCreator);

    QDockWidget* pDockWidget = new QDockWidget(_pMainWindow);
    pDockWidget->setWindowTitle(entry.definition.title);
    pDockWidget->setAllowedAreas(entry.definition.allowedAreas);
    _pMainWindow->addDockWidget(entry.definition.defaultArea, pDockWidget);

    QWidget* pWidget = entry.definition.widgetCreator(pDockWidget);
    assert(pWidget);
    pDockWidget->setWidget(pWidget);

    if (entry.definition.visibleByDefault)
    {
        pDockWidget->show();
    }
    else
    {
        pDockWidget->hide();
    }

    entry.dockWidget = pDockWidget;
    entry.widget = pWidget;
}

}
