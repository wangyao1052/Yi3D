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

#ifndef WY3DAPP_PLUGIN_MANAGER_H
#define WY3DAPP_PLUGIN_MANAGER_H

#include <string>
#include <vector>

#include <wyapEnvManager.h>

#include <plugin/PluginApi.h>

#include "utils/Log.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

struct PluginManifest;

struct LoadedPlugin
{
    Plugin* pPlugin = nullptr;
    void* pModule = nullptr;
    wy3dappDeletePluginFn deletePluginFn = nullptr;
    std::string id;
    std::string name;
    std::string u8LibraryFullPath;
};

class PluginManager : public wyap::EnvManagerReactor
{
public:
    static PluginManager& instance();

    void initialize();
    void terminate();

    void onEnvironmentEntered(wyap::Environment* pEnvironment) override;
    void onEnvironmentToBeExited(wyap::Environment* pEnvironment) override;
    void onEnvironmentToBeSuspended(wyap::Environment* pEnvironment) override;
    void onEnvironmentResumed(wyap::Environment* pEnvironment) override;

private:
    PluginManager() = default;
    ~PluginManager() = default;
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    struct LoadPluginsStatistics
    {
        size_t total = 0;
        size_t loaded = 0;
        size_t disabled = 0;
        size_t failed = 0;
    };
    LoadPluginsStatistics loadPlugins();

    bool loadPlugin(
        const PluginManifest& manifest,
        const std::string& pluginDirFullPathU8,
        LoadedPlugin& out,
        std::string& error);

private:
    std::vector<LoadedPlugin> _plugins;
    Log _log;
};

}

#endif // WY3DAPP_PLUGIN_MANAGER_H
