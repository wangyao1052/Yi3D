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

#include <QObject>
#include <QString>
#include <Qt>
#include <functional>
#include <map>
#include <set>
#include <string>
#include "wy3dAppDefs.h"

class QDockWidget;
class QMainWindow;
class QWidget;

namespace wy3dApp {

class FeatureTreeWidget;
class PropertyEditorWidget;
class OutputWidget;

struct DockPanelDefinition
{
    using WidgetCreator = std::function<QWidget*(QWidget* parent)>;

    std::string id;
    QString title;
    Qt::DockWidgetArea defaultArea;
    Qt::DockWidgetAreas allowedAreas;
    bool visibleByDefault;
    WidgetCreator widgetCreator;
};

class DockPanelManager : public QObject
{
    Q_OBJECT

public:
    explicit DockPanelManager(QMainWindow* pMainWindow);

    void addBuiltinPanels();
    bool addPanel(const DockPanelDefinition& definition);
    bool removePanel(const std::string& id);
    bool hasPanel(const std::string& id) const;
    QWidget* findWidget(const std::string& id) const;
    template <typename T>
    T* findWidgetAs(const std::string& id) const
    {
        return qobject_cast<T*>(this->findWidget(id));
    }
    QDockWidget* findDockWidget(const std::string& id) const;

    bool isPanelVisible(const std::string& id) const;
    void showPanel(const std::string& id);
    void hidePanel(const std::string& id);
    void togglePanel(const std::string& id);

private:
    struct DockPanelEntry
    {
        DockPanelDefinition definition;
        QDockWidget* dockWidget = nullptr;
        QWidget* widget = nullptr;
    };

private:
    DockPanelEntry* findPanel(const std::string& id);
    const DockPanelEntry* findPanel(const std::string& id) const;
    bool isBuiltinPanelId(const std::string& id) const { return _builtinPanelIds.count(id) > 0; }
    void createDockPanel(DockPanelEntry& entry);

private:
    QMainWindow* _pMainWindow;
    std::map<std::string, DockPanelEntry> _panelMap;
    std::set<std::string> _builtinPanelIds;
};

}
