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

#ifndef YI3D_PLUGIN_H
#define YI3D_PLUGIN_H

#include <plugin/PluginApi.h>

namespace wyap { class Environment; }

class Plugin
{
protected:
    Plugin() = default;
    virtual ~Plugin() = default;
    Plugin(const Plugin&) = delete;
    Plugin& operator=(const Plugin&) = delete;
    Plugin(Plugin&&) = delete;
    Plugin& operator=(Plugin&&) = delete;

public:
    virtual bool initialize() = 0;
    virtual void terminate() = 0;

    virtual void onEnvironmentEntered(wyap::Environment* pEnvironment) = 0;
    virtual void onEnvironmentToBeExited(wyap::Environment* pEnvironment) = 0;
    virtual void onEnvironmentToBeSuspended(wyap::Environment* pEnvironment) = 0;
    virtual void onEnvironmentResumed(wyap::Environment* pEnvironment) = 0;
};

#endif // YI3D_PLUGIN_H
