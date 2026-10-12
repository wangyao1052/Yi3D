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

#include "plugins/PluginManager.h"

#include <filesystem>
#include <set>
#include <cassert>

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QString>

#include <plugin/Plugin.h>

#include <wy3dApp/Application.h>
#include "plugins/PluginManifest.h"
#include "Version.h"
#include <wy3dApp/GatewayEnvironment.h>

#if defined(_WIN32)
#define NOMINMAX
#include <Windows.h>
#else
#include <dlfcn.h>
#include <wy3dApp/wy3dAppDefs.h>
#endif

namespace wy3dApp {

namespace fs = std::filesystem;

PluginManager& PluginManager::instance()
{
    static PluginManager instance;
    return instance;
}

void PluginManager::initialize()
{
    const QString logPath = QDir(QCoreApplication::applicationDirPath()).filePath("plugins/plugin-manager.log");
    bool initLogRet = _log.initialize(logPath.toStdString());
    assert(initLogRet);

    LoadPluginsStatistics statistics = this->loadPlugins();

    wyap::EnvManager* pEnvManager = Application::instance().getEnvManager();
    assert(pEnvManager);
    wy::ErrorStatus error = pEnvManager->addReactor(this);
    assert(wy::ErrorStatus::Ok == error);

    _log.info(std::to_string(statistics.total) + " plugin(s): "
        + std::to_string(statistics.loaded) + " loaded, "
        + std::to_string(statistics.disabled) + " disabled, "
        + std::to_string(statistics.failed) + " failed.");
}

bool PluginManager::loadPlugin(
    const PluginManifest& manifest,
    const std::string& pluginDirFullPathU8,
    LoadedPlugin& out, std::string& error)
{
    fs::path pathPluginDir(pluginDirFullPathU8);
    fs::path pathLibraryName(manifest.library);
    fs::path pathLibrary = pathPluginDir / pathLibraryName;
    std::string libraryFullPathU8 = pathLibrary.u8string();

    Plugin* pPlugin = nullptr;
    wy3dappDeletePluginFn deletePluginFn = nullptr;
    void* pModule = nullptr;

#if defined(_WIN32)
    const QString wideLibraryPath = QString::fromStdString(libraryFullPathU8);
    HMODULE hModule = ::LoadLibraryExW(
        reinterpret_cast<LPCWSTR>(wideLibraryPath.utf16()),
        nullptr,
        LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!hModule)
    {
        error = "LoadLibrary failed with error " + std::to_string(::GetLastError()) + ".";
        return false;
    }

    FARPROC procVersion = ::GetProcAddress(hModule, "wy3dappGetPluginSdkVersion");
    if (!procVersion)
    {
        bool freeRet = ::FreeLibrary(hModule);
        assert(true == freeRet);
        error = "entry point wy3dappGetPluginSdkVersion not found.";
        return false;
    }
    const Version pluginSdkVersion =
        reinterpret_cast<wy3dappGetPluginSdkVersionFn>(procVersion)();
    if (kAppVersion.major != pluginSdkVersion.major ||
        kAppVersion.minor != pluginSdkVersion.minor)
    {
        bool freeRet = ::FreeLibrary(hModule);
        assert(true == freeRet);
        error = "plugin SDK version mismatch (plugin built for "
            + pluginSdkVersion.toString() + ", host is " + kAppVersion.toString() + ")";
        return false;
    }

    FARPROC procNew = ::GetProcAddress(hModule, "wy3dappNewPlugin");
    if (!procNew)
    {
        bool freeRet = ::FreeLibrary(hModule);
        assert(true == freeRet);
        error = "entry point wy3dappNewPlugin not found.";
        return false;
    }
    pPlugin = reinterpret_cast<wy3dappNewPluginFn>(procNew)();
    if (!pPlugin)
    {
        bool freeRet = ::FreeLibrary(hModule);
        assert(true == freeRet);
        error = "entry point wy3dappNewPlugin returned no instance.";
        return false;
    }

    FARPROC procDelete = ::GetProcAddress(hModule, "wy3dappDeletePlugin");
    assert(procDelete != nullptr);
    deletePluginFn = reinterpret_cast<wy3dappDeletePluginFn>(procDelete);
    pModule = hModule;
#else
    void* hModule = ::dlopen(libraryFullPathU8.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!hModule)
    {
        const char* pDlError = ::dlerror();
        error = "dlopen failed: " + std::string(pDlError ? pDlError : "unknown error");
        return false;
    }

    void* procVersion = ::dlsym(hModule, "wy3dappGetPluginSdkVersion");
    if (!procVersion)
    {
        bool freeRet = (0 == ::dlclose(hModule));
        assert(true == freeRet);
        error = "entry point wy3dappGetPluginSdkVersion not found.";
        return false;
    }
    const Version pluginSdkVersion =
        reinterpret_cast<wy3dappGetPluginSdkVersionFn>(procVersion)();
    if (kAppVersion.major != pluginSdkVersion.major ||
        kAppVersion.minor != pluginSdkVersion.minor)
    {
        bool freeRet = (0 == ::dlclose(hModule));
        assert(true == freeRet);
        error = "plugin SDK version mismatch (plugin built for "
            + pluginSdkVersion.toString() + ", host is " + kAppVersion.toString() + ")";
        return false;
    }

    void* procNew = ::dlsym(hModule, "wy3dappNewPlugin");
    if (!procNew)
    {
        bool freeRet = (0 == ::dlclose(hModule));
        assert(true == freeRet);
        error = "entry point wy3dappNewPlugin not found.";
        return false;
    }
    pPlugin = reinterpret_cast<wy3dappNewPluginFn>(procNew)();
    if (!pPlugin)
    {
        bool freeRet = (0 == ::dlclose(hModule));
        assert(true == freeRet);
        error = "entry point wy3dappNewPlugin returned no instance.";
        return false;
    }

    void* procDelete = ::dlsym(hModule, "wy3dappDeletePlugin");
    assert(procDelete != nullptr);
    deletePluginFn = reinterpret_cast<wy3dappDeletePluginFn>(procDelete);
    pModule = hModule;
#endif

    if (!pPlugin->initialize())
    {
        pPlugin->terminate();
        if (deletePluginFn) deletePluginFn(pPlugin);
        error = "plugin initialize failed.";
        return false;
    }

    out.pPlugin = pPlugin;
    out.pModule = pModule;
    out.deletePluginFn = deletePluginFn;
    out.id = manifest.id;
    out.name = manifest.name;
    out.u8LibraryFullPath = libraryFullPathU8;

    return true;
}

PluginManager::LoadPluginsStatistics PluginManager::loadPlugins()
{
    LoadPluginsStatistics statistics;

    const QString pluginsPath = QDir(QCoreApplication::applicationDirPath()).filePath("plugins");
    QDir pluginsDir(pluginsPath);
    if (!pluginsDir.exists())
    {
        assert(false);
        return statistics;
    }

    struct PluginInfo
    {
        PluginManifest manifest;
        QString dirFullPath;
        bool valid = true;
    };
    std::vector<PluginInfo> pluginInfos;
    pluginInfos.reserve(10);

    const QFileInfoList entries = pluginsDir.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot,
        QDir::Name);
    for (const QFileInfo& entry : entries)
    {
        const QString iniFullPath = QDir(entry.absoluteFilePath()).filePath("plugin.ini");
        if (!QFileInfo::exists(iniFullPath)) continue;

        PluginInfo info;
        info.dirFullPath = entry.absoluteFilePath();
        info.valid = true;

        std::string error;
        if (!PluginManifestParser::loadFromFile(iniFullPath.toStdString(), info.manifest, error))
        {
            _log.warn("invalid plugin manifest.(" + iniFullPath.toStdString() + ")(" + error + ")");
            info.valid = false;
        }

        pluginInfos.emplace_back(std::move(info));
    }

    statistics.total = pluginInfos.size();

    std::set<std::string> pluginIds;
    for (const PluginInfo& pluginInfo : pluginInfos)
    {
        if (!pluginInfo.valid)
        {
            ++statistics.failed;
            continue;
        }

        if (pluginIds.find(pluginInfo.manifest.id) != pluginIds.cend())
        {
            _log.warn("duplicate plugin id '" + pluginInfo.manifest.id + "', skipped.("
                + pluginInfo.dirFullPath.toStdString() + ")");
            ++statistics.failed;
            continue;
        }

        if (!pluginInfo.manifest.enabled)
        {
            _log.info("plugin '" + pluginInfo.manifest.id + "' disabled, skipped.");
            ++statistics.disabled;
            pluginIds.insert(pluginInfo.manifest.id);
            continue;
        }

        pluginIds.insert(pluginInfo.manifest.id);

        LoadedPlugin loaded;
        std::string error;
        if (!loadPlugin(pluginInfo.manifest, pluginInfo.dirFullPath.toStdString(), loaded, error))
        {
            _log.warn("failed to load '" + pluginInfo.manifest.id + "' ("
                + pluginInfo.manifest.library + "): " + error);
            ++statistics.failed;
            continue;
        }

        _plugins.push_back(loaded);
        _log.info("loaded plugin '" + pluginInfo.manifest.id + "'.");
        ++statistics.loaded;
    }
    
    return statistics;
}

void PluginManager::terminate()
{
    wyap::EnvManager* pEnvManager = Application::instance().getEnvManager();
    assert(pEnvManager);
    pEnvManager->removeReactor(this);

    wyap::Environment* pActiveEnvironment = pEnvManager->getActiveEnvironment();
    if (pActiveEnvironment)
    {
        GatewayEnvironment* pGatewayEnvironment = dynamic_cast<GatewayEnvironment*>(pActiveEnvironment);
        assert(pGatewayEnvironment);
        onEnvironmentToBeExited(pEnvManager->getActiveEnvironment());
    }
    else
    {
        assert(false);
    }

    for (LoadedPlugin& plugin : _plugins)
    {
        if (!plugin.pPlugin)
        {
            assert(false);
            continue;
        }
        plugin.pPlugin->terminate();
        if (plugin.deletePluginFn) plugin.deletePluginFn(plugin.pPlugin);
    }
    _plugins.clear();

    _log.close();
}

void PluginManager::onEnvironmentEntered(wyap::Environment* pEnvironment)
{
    assert(pEnvironment);
    for (LoadedPlugin& plugin : _plugins)
    {
        if (!plugin.pPlugin)
        {
            assert(false);
            continue;
        }
        plugin.pPlugin->onEnvironmentEntered(pEnvironment);
    }
}

void PluginManager::onEnvironmentResumed(wyap::Environment* pEnvironment)
{
    assert(pEnvironment);
    for (LoadedPlugin& plugin : _plugins)
    {
        if (!plugin.pPlugin)
        {
            assert(false);
            continue;
        }
        plugin.pPlugin->onEnvironmentResumed(pEnvironment);
    }
}

void PluginManager::onEnvironmentToBeExited(wyap::Environment* pEnvironment)
{
    assert(pEnvironment);
    for (size_t i = _plugins.size(); i > 0; --i)
    {
        LoadedPlugin& plugin = _plugins[i - 1];
        if (!plugin.pPlugin)
        {
            assert(false);
            continue;
        }
        plugin.pPlugin->onEnvironmentToBeExited(pEnvironment);
    }
}

void PluginManager::onEnvironmentToBeSuspended(wyap::Environment* pEnvironment)
{
    assert(pEnvironment);
    for (size_t i = _plugins.size(); i > 0; --i)
    {
        LoadedPlugin& plugin = _plugins[i - 1];
        if (!plugin.pPlugin)
        {
            assert(false);
            continue;
        }
        plugin.pPlugin->onEnvironmentToBeSuspended(pEnvironment);
    }
}

}
