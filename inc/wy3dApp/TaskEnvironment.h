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

#ifndef WY3DAPP_TASK_ENVIRONMENT_H
#define WY3DAPP_TASK_ENVIRONMENT_H

#include <wyapEnvironment.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class ContextMenu;
class EnvironmentUI;

class WY3DAPP_EXPORT TaskEnvironment : public wyap::TaskEnvironment
{
    WYRX_DECLARE_NO_CREATE_MEMBERS(TaskEnvironment, wy3dApp::TaskEnvironment, wyap::TaskEnvironment)

public:
    // Get the environment UI owned by this environment.
    virtual EnvironmentUI* getUI() const = 0;

    virtual void updateCommandActionStates() = 0;

    // Override to add environment-specific items to a command context menu.
    // Returns true if any actions were added.
    virtual bool addContextMenuActions(ContextMenu& menu)
    {
        (void)menu;
        return false;
    }

protected:
    TaskEnvironment() = default;
    ~TaskEnvironment() = default;
};

} // namespace wy3dApp

#endif // WY3DAPP_TASK_ENVIRONMENT_H
