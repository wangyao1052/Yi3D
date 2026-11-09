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

#include "plugins/PluginManifest.h"
#include "utils/Log.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

// Runtime state of a discovered plugin. Disabled/Failed/Unloaded records are
// kept so the management dialog can list and retry them.
enum class PluginState
{
    Loaded,
    Disabled,
    Failed,
    Unloading,
    Unloaded,
    NeedsRestart,
};

// Snapshot returned by PluginManager::getPlugins().
struct PluginInfo
{
    std::string key;          // Plugin directory name; stable runtime identifier.
    std::string id;           // plugin.ini id; empty for an invalid manifest.
    std::string name;         // Display name; falls back to id or key.
    std::string libraryPath;  // Full library path (also for disabled/failed records).
    PluginState state = PluginState::Failed;
    std::string error;        // Last failure / restart reason.
};

class PluginManager : public wyap::EnvManagerReactor
{
public:
    static PluginManager& instance();

    void initialize();
    void terminate();

    // Runtime management (UI thread only). Load/unload is invoked only from
    // the modal Plugin Manager dialog, which suppresses the active
    // environment's default command for the dialog lifetime.
    std::vector<PluginInfo> getPlugins() const;

    // Returns false and fills reason when the plugin cannot be unloaded now.
    bool canUnloadPlugin(const std::string& key, std::string& reason) const;

    // Unload one plugin from memory. Keeps the record so loadPlugin() can reload it.
    bool unloadPlugin(const std::string& key, std::string& error);

    // Load or reload one discovered plugin.
    bool loadPlugin(const std::string& key, std::string& error);

    // EnvManagerReactor
    void onEnvironmentEntered(wyap::Environment* pEnvironment) override;
    void onEnvironmentToBeExited(wyap::Environment* pEnvironment) override;
    void onEnvironmentToBeSuspended(wyap::Environment* pEnvironment) override;
    void onEnvironmentResumed(wyap::Environment* pEnvironment) override;

private:
    PluginManager() = default;
    ~PluginManager() = default;
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    struct PluginRecord
    {
        std::string key;              // Plugin directory name.
        std::string dirFullPathU8;
        std::string iniFullPathU8;
        PluginManifest manifest;
        PluginState state = PluginState::Failed;
        std::string error;
        std::string libraryFullPathU8;
        Plugin* pPlugin = nullptr;
        void* pModule = nullptr;
        wy3dappDeletePluginFn deletePluginFn = nullptr;
    };

    struct LoadPluginsStatistics
    {
        size_t total = 0;
        size_t loaded = 0;
        size_t disabled = 0;
        size_t failed = 0;
    };

    PluginRecord* findRecord(const std::string& key);
    const PluginRecord* findRecord(const std::string& key) const;

    LoadPluginsStatistics loadPlugins();
    bool loadRecord(PluginRecord& record, std::string& error);
    bool unloadRecord(PluginRecord& record, std::string& error);

    // Replays the enter callback for every environment currently on the
    // EnvManager stack, bottom to top, after a plugin was loaded or reloaded.
    void replayEnvironmentStack(PluginRecord& record);

    // Returns true if a live environment was created by a plugin (not a host
    // environment). Such an environment owns plugin code through its vtable and
    // must be exited before the plugin DLL can be unloaded.
    bool findBlockingEnvironment(std::string& environmentName) const;

    static bool isUiThread();

private:
    std::vector<PluginRecord> _records;
    // Guard against re-entering load/unload from plugin callbacks.
    bool _operationInProgress = false;
    Log _log;
};

} // namespace wy3dApp

#endif // WY3DAPP_PLUGIN_MANAGER_H
