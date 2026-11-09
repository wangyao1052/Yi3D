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

#include <wyapCmdManager.h>
#include <wyapEnvironment.h>
#include <wy3dApp/wy3dAppDefs.h>

class QActionGroup;
class QIcon;
class QMenu;
class QToolBar;

namespace wy3dApp {

class CommandAction;

enum class CommandActionRefreshCause : unsigned short
{
    Unknown     = 0, // The cause is unknown or unspecified.
    Transaction = 1, // A transaction started, ended, aborted, was undone, or was redone.
    Document    = 2, // A document was activated.
    Environment = 3, // An environment was entered or resumed.
};

class WY3DAPP_EXPORT EnvironmentUI : public QObject
{
    Q_OBJECT
public:
    // Get all menus. The pointers are borrowed and owned by this UI.
    virtual const std::vector<QMenu*>& getMenus() const = 0;

    // Creates a menu owned by this EnvironmentUI and adds it to the menu bar.
    // The returned pointer is borrowed; callers must not delete it directly.
    // Remove it with deleteMenu(). Returns nullptr on failure.
    virtual QMenu* newMenu(const QString& title, const std::string& name) = 0;
    // Removes the menu and schedules its deletion (deleteLater).
    // The pointer is invalid after this call.
    virtual void deleteMenu(QMenu* pMenu) = 0;

    // Get all tool bars. The pointers are borrowed and owned by this UI.
    virtual const std::vector<QToolBar*>& getToolBars() const = 0;

    // Creates a tool bar owned by this EnvironmentUI and adds it to the main
    // window. The returned pointer is borrowed; callers must not delete it
    // directly. Remove it with deleteToolBar(). Returns nullptr on failure.
    virtual QToolBar* newToolBar(const QString& title, const std::string& name) = 0;
    // Removes the tool bar and schedules its deletion (deleteLater).
    // The pointer is invalid after this call.
    virtual void deleteToolBar(QToolBar* pToolBar) = 0;

    // Creates an action group owned by this EnvironmentUI.
    // The returned pointer is borrowed; callers must not delete it directly.
    // Remove it with deleteActionGroup().
    virtual QActionGroup* newActionGroup() = 0;
    // Deletes the action group (deleteLater).
    virtual void deleteActionGroup(QActionGroup* pActionGroup) = 0;

    // Creates a command action owned by this EnvironmentUI.
    // The returned pointer is borrowed; callers must not delete it directly.
    // Remove it with deleteCommandAction().
    virtual CommandAction* newCommandAction(
        const std::string& commandName,
        const QString& text,
        const QIcon& icon,
        QActionGroup* pActionGroup = nullptr) = 0;
    // Deletes the command action (deleteLater).
    virtual void deleteCommandAction(CommandAction* pCommandAction) = 0;

    // Find a command action by command name.
    // Returns a borrowed pointer, or nullptr if no action has been created for
    // that name.
    virtual CommandAction* findCommandAction(const std::string& commandName) const = 0;

    // The command posted when this environment is entered/resumed and
    // restarted after a modal command ends. Defaults to "Select".
    // An empty name disables the automatic restart.
    virtual std::string getDefaultCommandName() const = 0;
    // Sets the default command. The command must be registered by this
    // environment while it is active.
    virtual void setDefaultCommandName(const std::string& commandName) = 0;

protected:
    EnvironmentUI() = default;
    virtual ~EnvironmentUI() = default;
    EnvironmentUI(const EnvironmentUI&) = delete;
    EnvironmentUI(EnvironmentUI&&) = delete;
    EnvironmentUI& operator=(const EnvironmentUI&) = delete;
    EnvironmentUI& operator=(EnvironmentUI&&) = delete;
};

} // namespace wy3dApp

#endif // WY3DAPP_ENVIRONMENT_UI_H
