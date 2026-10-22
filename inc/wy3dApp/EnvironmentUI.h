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

#ifndef WY3DAPP_ENVIRONMENT_UI_H
#define WY3DAPP_ENVIRONMENT_UI_H

#include <QObject>
#include <QString>

#include <string>
#include <vector>

#include <wy3dApp/wy3dAppDefs.h>

class QActionGroup;
class QIcon;
class QMenu;
class QToolBar;

namespace wy3dApp {

class CommandAction;
class ContextMenu;

// Interface of an environment's menus, tool bars and command actions.
// Implemented by the internal EnvironmentUIImpl.
class WY3DAPP_EXPORT EnvironmentUI : public QObject
{
    Q_OBJECT
public:
    ~EnvironmentUI() override;

    // Add menu.
    virtual QMenu* addMenu(const QString& title, const std::string& name) = 0;

    // Add tool bar.
    virtual QToolBar* addToolBar(const QString& title, const std::string& name) = 0;

    // New action group.
    virtual QActionGroup* newActionGroup() = 0;

    // New command action.
    virtual CommandAction* newCommandAction(
        const std::string& commandName,
        const QString& text,
        const QIcon& icon,
        QActionGroup* pActionGroup = nullptr) = 0;

    // Add environment-specific items to a command context menu.
    // Returns true if any actions were added.
    virtual bool addContextMenuActions(ContextMenu& menu) = 0;

    // Get all menus.
    virtual const std::vector<QMenu*>& getMenus() const = 0;

    // Get all tool bars.
    virtual const std::vector<QToolBar*>& getToolBars() const = 0;

protected:
    EnvironmentUI() = default;
};

} // namespace wy3dApp

#endif // WY3DAPP_ENVIRONMENT_UI_H
