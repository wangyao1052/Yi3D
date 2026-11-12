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

#ifndef WY3DAPP_TASK_ENVIRONMENT_UI_H
#define WY3DAPP_TASK_ENVIRONMENT_UI_H

#include <QPointer>

#include "environments/EnvironmentUIImpl.h"
#include "widgets/frame/ConfirmationBar.h"

namespace wy3dApp {

class CommandAction;

// Host-owned UI shared by task environments. Adds the Finish/Cancel command
// actions and the floating ConfirmationBar to EnvironmentUIImpl; gateway and
// modeling environments use EnvironmentUIImpl directly and never see them.
class TaskEnvironmentUI : public EnvironmentUIImpl
{
public:
    explicit TaskEnvironmentUI(const wyap::Environment* pEnvironment);
    ~TaskEnvironmentUI() override;

    // Host-owned confirmation actions shared by ConfirmationBar, menus and
    // toolbars. They are created for each environment session with fixed
    // icons and default texts.
    CommandAction* getConfirmAction();
    CommandAction* getCancelAction();

    // Create the host-owned confirmation actions for this session. Called by
    // TaskEnvironment::onEnter()/onResume() before the UI is built, so menus,
    // toolbars and the ConfirmationBar can use them without lazy checks.
    void createConfirmationActions();

    // Show or hide the ConfirmationBar over the active viewport.
    void setConfirmationBarVisible(bool visible);

    // EnvironmentUIImpl
    void destroy() override;
    void restoreUiState() override;

private:
    // Create the confirmation bar at the single post-layout point
    // (restoreUiState()). The bar is created hidden; the caller shows it.
    void createConfirmationBar();
    // Destroy the confirmation bar synchronously, before its actions.
    void destroyConfirmationBar();

private:
    QPointer<ConfirmationBar> _pConfirmationBar;
    // Task environments show the confirmation bar by default; call
    // setConfirmationBarVisible(false) to opt out.
    bool _confirmationVisible = true;
};

} // namespace wy3dApp

#endif // WY3DAPP_TASK_ENVIRONMENT_UI_H
