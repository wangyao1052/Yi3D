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

#include <cassert>
#include <filesystem>
#include <set>

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QEventLoop>
#include <QFileInfo>
#include <QString>
#include <QThread>

#include <plugin/Plugin.h>

#include <wy3dApp/Application.h>
#include "application/ApplicationImpl.h"
#include "Version.h"
#include <wyapCmdManager.h>

#if defined(_WIN32)
#define NOMINMAX
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

namespace wy3dApp {

namespace fs = std::filesystem;

namespace {

class OperationGuard
{
public:
    explicit OperationGuard(bool& flag)
        : _flag(flag)
    {
        _flag = true;
    }

    ~OperationGuard()
    {
        _flag = false;
    }

    OperationGuard(const OperationGuard&) = delete;
    OperationGuard& operator=(const OperationGuard&) = delete;

private:
    bool& _flag;
};

void flushDeferredDeletes()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

} // namespace

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

    wyap::EnvManager* pEnvManager = ApplicationImpl::instance().getEnvManager();
    assert(pEnvManager);
    wy::ErrorStatus error = pEnvManager->addReactor(this);
    assert(wy::ErrorStatus::Ok == error);

    _log.info(std::to_string(statistics.total) + " plugin(s): "
        + std::to_string(statistics.loaded) + " loaded, "
        + std::to_string(statistics.disabled) + " disabled, "
        + std::to_string(statistics.failed) + " failed.");
}

bool PluginManager::isUiThread()
{
    QCoreApplication* pApplication = QCoreApplication::instance();
    return pApplication && QThread::currentThread() == pApplication->thread();
}

PluginManager::PluginRecord* PluginManager::findRecord(const std::string& key)
{
    for (PluginRecord& record : _records)
    {
        if (record.key == key) return &record;
    }
    return nullptr;
}

const PluginManager::PluginRecord* PluginManager::findRecord(const std::string& key) const
{
    for (const PluginRecord& record : _records)
    {
        if (record.key == key) return &record;
    }
    return nullptr;
}

bool PluginManager::loadRecord(PluginRecord& record, std::string& error)
{
    error.clear();

    const fs::path pathPluginDir = fs::u8path(record.dirFullPathU8);
    const fs::path pathLibrary = pathPluginDir / fs::u8path(record.manifest.library);
    record.libraryFullPathU8 = pathLibrary.u8string();

    Plugin* pPlugin = nullptr;
    void* pModule = nullptr;
    wy3dappDeletePluginFn deletePluginFn = nullptr;

#if defined(_WIN32)
    const QString wideLibraryPath = QString::fromStdString(record.libraryFullPathU8);
    HMODULE hModule = ::LoadLibraryExW(
        reinterpret_cast<LPCWSTR>(wideLibraryPath.utf16()),
        nullptr,
        LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!hModule)
    {
        error = QCoreApplication::translate(
            "wy3dApp::PluginManager", "LoadLibrary failed with error %1.")
            .arg(::GetLastError()).toStdString();
        return false;
    }
    pModule = hModule;

    FARPROC procVersion = ::GetProcAddress(hModule, "wy3dappGetPluginSdkVersion");
    if (!procVersion)
    {
        ::FreeLibrary(hModule);
        error = QCoreApplication::translate(
            "wy3dApp::PluginManager",
            "entry point wy3dappGetPluginSdkVersion not found.").toStdString();
        return false;
    }
    const Version pluginSdkVersion =
        reinterpret_cast<wy3dappGetPluginSdkVersionFn>(procVersion)();
    if (kAppVersion.major != pluginSdkVersion.major ||
        kAppVersion.minor != pluginSdkVersion.minor)
    {
        ::FreeLibrary(hModule);
        error = QCoreApplication::translate(
            "wy3dApp::PluginManager",
            "plugin SDK version mismatch (plugin built for %1, host is %2)")
            .arg(QString::fromStdString(pluginSdkVersion.toString()),
                 QString::fromStdString(kAppVersion.toString()))
            .toStdString();
        return false;
    }

    FARPROC procNew = ::GetProcAddress(hModule, "wy3dappNewPlugin");
    FARPROC procDelete = ::GetProcAddress(hModule, "wy3dappDeletePlugin");
    if (!procNew || !procDelete)
    {
        ::FreeLibrary(hModule);
        error = QCoreApplication::translate(
            "wy3dApp::PluginManager",
            "plugin entry points wy3dappNewPlugin/wy3dappDeletePlugin not found.")
            .toStdString();
        return false;
    }
    deletePluginFn = reinterpret_cast<wy3dappDeletePluginFn>(procDelete);
    pPlugin = reinterpret_cast<wy3dappNewPluginFn>(procNew)();
    if (!pPlugin)
    {
        ::FreeLibrary(hModule);
        error = QCoreApplication::translate(
            "wy3dApp::PluginManager",
            "entry point wy3dappNewPlugin returned no instance.").toStdString();
        return false;
    }
#else
    void* hModule = ::dlopen(record.libraryFullPathU8.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!hModule)
    {
        const char* pDlError = ::dlerror();
        error = QCoreApplication::translate(
            "wy3dApp::PluginManager", "dlopen failed: %1")
            .arg(QString::fromStdString(pDlError ? pDlError : "unknown error"))
            .toStdString();
        return false;
    }
    pModule = hModule;

    void* procVersion = ::dlsym(hModule, "wy3dappGetPluginSdkVersion");
    if (!procVersion)
    {
        ::dlclose(hModule);
        error = QCoreApplication::translate(
            "wy3dApp::PluginManager",
            "entry point wy3dappGetPluginSdkVersion not found.").toStdString();
        return false;
    }
    const Version pluginSdkVersion =
        reinterpret_cast<wy3dappGetPluginSdkVersionFn>(procVersion)();
    if (kAppVersion.major != pluginSdkVersion.major ||
        kAppVersion.minor != pluginSdkVersion.minor)
    {
        ::dlclose(hModule);
        error = QCoreApplication::translate(
            "wy3dApp::PluginManager",
            "plugin SDK version mismatch (plugin built for %1, host is %2)")
            .arg(QString::fromStdString(pluginSdkVersion.toString()),
                 QString::fromStdString(kAppVersion.toString()))
            .toStdString();
        return false;
    }

    void* procNew = ::dlsym(hModule, "wy3dappNewPlugin");
    void* procDelete = ::dlsym(hModule, "wy3dappDeletePlugin");
    if (!procNew || !procDelete)
    {
        ::dlclose(hModule);
        error = QCoreApplication::translate(
            "wy3dApp::PluginManager",
            "plugin entry points wy3dappNewPlugin/wy3dappDeletePlugin not found.")
            .toStdString();
        return false;
    }
    deletePluginFn = reinterpret_cast<wy3dappDeletePluginFn>(procDelete);
    pPlugin = reinterpret_cast<wy3dappNewPluginFn>(procNew)();
    if (!pPlugin)
    {
        ::dlclose(hModule);
        error = QCoreApplication::translate(
            "wy3dApp::PluginManager",
            "entry point wy3dappNewPlugin returned no instance.").toStdString();
        return false;
    }
#endif

    {
        if (!pPlugin->initialize())
        {
            pPlugin->terminate();
            deletePluginFn(pPlugin);
#if defined(_WIN32)
            ::FreeLibrary(static_cast<HMODULE>(pModule));
#else
            ::dlclose(pModule);
#endif
            error = QCoreApplication::translate(
                "wy3dApp::PluginManager", "plugin initialize failed.").toStdString();
            return false;
        }
    }

    record.pPlugin = pPlugin;
    record.pModule = pModule;
    record.deletePluginFn = deletePluginFn;
    record.state = PluginState::Loaded;
    record.error.clear();
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

    std::set<std::string> manifestIds;
    const QFileInfoList entries = pluginsDir.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot,
        QDir::Name);
    for (const QFileInfo& entry : entries)
    {
        const QString iniFullPath = QDir(entry.absoluteFilePath()).filePath("plugin.ini");
        if (!QFileInfo::exists(iniFullPath)) continue;

        PluginRecord record;
        record.key = entry.fileName().toStdString();
        record.dirFullPathU8 = entry.absoluteFilePath().toStdString();
        record.iniFullPathU8 = iniFullPath.toStdString();

        std::string error;
        if (!PluginManifestParser::loadFromFile(record.iniFullPathU8, record.manifest, error))
        {
            record.state = PluginState::Failed;
            record.error = error;
            _log.warn("invalid plugin manifest.(" + iniFullPath.toStdString() + ")(" + error + ")");
            _records.push_back(std::move(record));
            ++statistics.failed;
            continue;
        }

        if (!manifestIds.insert(record.manifest.id).second)
        {
            record.state = PluginState::Failed;
            record.error = QCoreApplication::translate(
                "wy3dApp::PluginManager", "duplicate plugin id '%1'.")
                .arg(QString::fromStdString(record.manifest.id)).toStdString();
            _log.warn("duplicate plugin id '" + record.manifest.id + "', skipped.("
                + record.dirFullPathU8 + ")");
            _records.push_back(std::move(record));
            ++statistics.failed;
            continue;
        }

        const fs::path pathLibrary =
            fs::u8path(record.dirFullPathU8) / fs::u8path(record.manifest.library);
        record.libraryFullPathU8 = pathLibrary.u8string();

        if (!record.manifest.enabled)
        {
            record.state = PluginState::Disabled;
            _log.info("plugin '" + record.manifest.id + "' disabled, skipped.");
            _records.push_back(std::move(record));
            ++statistics.disabled;
            continue;
        }

        if (!this->loadRecord(record, error))
        {
            record.state = PluginState::Failed;
            record.error = error;
            _log.warn("failed to load '" + record.manifest.id + "' ("
                + record.manifest.library + "): " + error);
            _records.push_back(std::move(record));
            ++statistics.failed;
            continue;
        }

        _log.info("loaded plugin '" + record.manifest.id + "'.");
        _records.push_back(std::move(record));
        ++statistics.loaded;
    }

    statistics.total = _records.size();
    return statistics;
}

std::vector<PluginInfo> PluginManager::getPlugins() const
{
    std::vector<PluginInfo> result;
    result.reserve(_records.size());
    for (const PluginRecord& record : _records)
    {
        PluginInfo info;
        info.key = record.key;
        info.id = record.manifest.id;
        info.name = record.manifest.name;
        if (info.name.empty()) info.name = info.id.empty() ? record.key : info.id;
        info.libraryPath = record.libraryFullPathU8;
        info.state = record.state;
        info.error = record.error;
        result.push_back(std::move(info));
    }
    return result;
}

bool PluginManager::findBlockingEnvironment(std::string& environmentName) const
{
    environmentName.clear();
    wyap::EnvManager* pEnvManager = ApplicationImpl::instance().getEnvManager();
    if (!pEnvManager)
    {
        return false;
    }

    const std::vector<wyap::Environment*> stack = pEnvManager->getEnvironmentStack();
    for (wyap::Environment* pEnvironment : stack)
    {
        if (!pEnvironment) continue;
        const wyap::Environment::Status status = pEnvironment->getStatus();
        if (status != wyap::Environment::Status::Active &&
            status != wyap::Environment::Status::Suspended)
        {
            continue;
        }
        if (!ApplicationImpl::instance().isHostEnvironment(pEnvironment))
        {
            environmentName = pEnvironment->getName();
            return true;
        }
    }
    return false;
}

bool PluginManager::canUnloadPlugin(const std::string& key, std::string& reason) const
{
    reason.clear();
    if (!isUiThread())
    {
        reason = QCoreApplication::translate(
            "wy3dApp::PluginManager",
            "Plugin operations must run on the UI thread.").toStdString();
        return false;
    }
    if (_operationInProgress)
    {
        reason = QCoreApplication::translate(
            "wy3dApp::PluginManager",
            "Another plugin operation is in progress.").toStdString();
        return false;
    }

    const PluginRecord* pRecord = this->findRecord(key);
    if (!pRecord)
    {
        reason = QCoreApplication::translate(
            "wy3dApp::PluginManager", "Plugin not found.").toStdString();
        return false;
    }
    if (pRecord->state != PluginState::Loaded || !pRecord->pPlugin)
    {
        reason = QCoreApplication::translate(
            "wy3dApp::PluginManager", "Plugin is not loaded.").toStdString();
        return false;
    }
    if (!pRecord->pPlugin->supportsUnload())
    {
        reason = QCoreApplication::translate(
            "wy3dApp::PluginManager",
            "This plugin does not support unloading. "
            "Restart the application to update or remove it.").toStdString();
        return false;
    }
    wyap::EnvManager* pEnvManager = ApplicationImpl::instance().getEnvManager();
    if (!pEnvManager)
    {
        reason = QCoreApplication::translate(
            "wy3dApp::PluginManager", "Environment manager is unavailable.")
            .toStdString();
        return false;
    }

    const std::vector<wyap::Environment*> stack = pEnvManager->getEnvironmentStack();
    if (stack.empty())
    {
        reason = QCoreApplication::translate(
            "wy3dApp::PluginManager", "No active environment.").toStdString();
        return false;
    }
    if (!pEnvManager->getActiveEnvironment())
    {
        reason = QCoreApplication::translate(
            "wy3dApp::PluginManager",
            "An environment transition is in progress. Try again later.")
            .toStdString();
        return false;
    }
    for (wyap::Environment* pEnvironment : stack)
    {
        if (!pEnvironment) continue;
        const wyap::Environment::Status status = pEnvironment->getStatus();
        if (status != wyap::Environment::Status::Active &&
            status != wyap::Environment::Status::Suspended)
        {
            reason = QCoreApplication::translate(
            "wy3dApp::PluginManager",
            "An environment transition is in progress. Try again later.")
            .toStdString();
            return false;
        }
    }
    for (wyap::Environment* pEnvironment : stack)
    {
        if (!pEnvironment) continue;
        if (pEnvironment->getType() == wyap::Environment::Type::Task)
        {
            reason = QCoreApplication::translate(
                "wy3dApp::PluginManager",
                "A task environment is active: \"%1\". "
                "Exit it before unloading plugins.")
                .arg(QString::fromStdString(pEnvironment->getName())).toStdString();
            return false;
        }
    }

    std::string blockingEnvironment;
    if (this->findBlockingEnvironment(blockingEnvironment))
    {
        reason = QCoreApplication::translate(
            "wy3dApp::PluginManager",
            "A plugin-created environment is active: \"%1\". "
            "Exit it before unloading plugins.")
            .arg(QString::fromStdString(blockingEnvironment)).toStdString();
        return false;
    }
    return true;
}

bool PluginManager::unloadPlugin(const std::string& key, std::string& error)
{
    error.clear();

    std::string reason;
    if (!this->canUnloadPlugin(key, reason))
    {
        error = reason;
        return false;
    }

    PluginRecord* pRecord = this->findRecord(key);
    if (!pRecord || !pRecord->pPlugin)
    {
        error = QCoreApplication::translate(
            "wy3dApp::PluginManager", "Plugin is not loaded.").toStdString();
        return false;
    }

    OperationGuard operationGuard(_operationInProgress);

    wyap::CmdManager* pCmdManager = ApplicationImpl::instance().getCmdManager();
    if (!pCmdManager)
    {
        error = QCoreApplication::translate(
            "wy3dApp::PluginManager",
            "Command manager is unavailable.").toStdString();
        return false;
    }

    if (pCmdManager->getCurrentModalCommand())
    {
        const wy::ErrorStatus abortStatus = pCmdManager->abortCurrentModalCommand(
            wyap::CmdExecution::AbortCause::ForceTerminate);
        if (abortStatus != wy::ErrorStatus::Ok &&
            abortStatus != wy::ErrorStatus::NoCurrentModalCommand)
        {
            error = QCoreApplication::translate(
                "wy3dApp::PluginManager",
                "Failed to abort the current modal command (status %1).")
                .arg(static_cast<int>(abortStatus)).toStdString();
            return false;
        }
    }
    // The suppression scope restarts Select when the last suppression is
    // popped; while the plugin manager dialog is open it keeps the outer
    // suppression and Select stays stopped until the dialog closes.
    return this->unloadRecord(*pRecord, error);
}

bool PluginManager::unloadRecord(PluginRecord& record, std::string& error)
{
    error.clear();
    record.state = PluginState::Unloading;

    const std::string pluginId = record.manifest.id;

    // canUnloadPlugin() rejects unload while any Task environment is active, so
    // only Gateway/Document (Modeling) environments can be live here. The
    // plugin removes every item it added to those environments from terminate().
#ifndef NDEBUG
    if (wyap::EnvManager* pEnvManager = ApplicationImpl::instance().getEnvManager())
    {
        for (wyap::Environment* pEnvironment : pEnvManager->getEnvironmentStack())
        {
            assert(!pEnvironment ||
                   pEnvironment->getType() != wyap::Environment::Type::Task);
        }
    }
#endif

    {
        record.pPlugin->terminate();
    }

    if (record.deletePluginFn && record.pPlugin)
    {
        record.deletePluginFn(record.pPlugin);
    }
    else
    {
        assert(false);
    }
    record.pPlugin = nullptr;
    record.deletePluginFn = nullptr;

    flushDeferredDeletes();

    if (!record.pModule)
    {
        error = QCoreApplication::translate(
            "wy3dApp::PluginManager",
            "Plugin module handle is unavailable; restart the application.")
            .toStdString();
        record.state = PluginState::NeedsRestart;
        record.error = error;
        _log.warn("plugin '" + pluginId + "' unloaded from UI: " + error);
        return false;
    }

#if defined(_WIN32)
    HMODULE hModule = static_cast<HMODULE>(record.pModule);
    FARPROC probe = ::GetProcAddress(hModule, "wy3dappNewPlugin");
    const BOOL freeResult = ::FreeLibrary(hModule);
    record.pModule = nullptr;

    if (!freeResult)
    {
        error = QCoreApplication::translate(
            "wy3dApp::PluginManager",
            "FreeLibrary failed with error %1; restart the application.")
            .arg(::GetLastError()).toStdString();
        record.state = PluginState::NeedsRestart;
        record.error = error;
        _log.warn("plugin '" + pluginId + "' unloaded from UI: " + error);
        return false;
    }

    if (probe)
    {
        HMODULE hStillLoaded = nullptr;
        const BOOL found = ::GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(probe),
            &hStillLoaded);
        if (found && hStillLoaded)
        {
            error = QCoreApplication::translate(
                "wy3dApp::PluginManager",
                "Plugin module is still loaded by another reference; restart the application.")
                .toStdString();
            record.state = PluginState::NeedsRestart;
            record.error = error;
            _log.warn("plugin '" + pluginId + "' unloaded from UI: " + error);
            return false;
        }
    }
#else
    void* probe = ::dlsym(record.pModule, "wy3dappNewPlugin");
    const int closeResult = ::dlclose(record.pModule);
    record.pModule = nullptr;

    if (0 != closeResult)
    {
        const char* pDlError = ::dlerror();
        error = QCoreApplication::translate(
            "wy3dApp::PluginManager",
            "dlclose failed: %1; restart the application.")
            .arg(QString::fromStdString(pDlError ? pDlError : "unknown error"))
            .toStdString();
        record.state = PluginState::NeedsRestart;
        record.error = error;
        _log.warn("plugin '" + pluginId + "' unloaded from UI: " + error);
        return false;
    }

#if defined(RTLD_NOLOAD)
    if (probe)
    {
        void* pStillLoaded = ::dlopen(
            record.libraryFullPathU8.c_str(), RTLD_LAZY | RTLD_NOLOAD);
        if (pStillLoaded)
        {
            ::dlclose(pStillLoaded);
            error = QCoreApplication::translate(
                "wy3dApp::PluginManager",
                "Plugin module is still loaded by another reference; restart the application.")
                .toStdString();
            record.state = PluginState::NeedsRestart;
            record.error = error;
            _log.warn("plugin '" + pluginId + "' unloaded from UI: " + error);
            return false;
        }
    }
#endif
#endif

    record.state = PluginState::Unloaded;
    record.error.clear();
    _log.info("unloaded plugin '" + pluginId + "'.");
    return true;
}

bool PluginManager::loadPlugin(const std::string& key, std::string& error)
{
    error.clear();

    if (!isUiThread())
    {
        error = QCoreApplication::translate(
            "wy3dApp::PluginManager",
            "Plugin operations must run on the UI thread.").toStdString();
        return false;
    }
    if (_operationInProgress)
    {
        error = QCoreApplication::translate(
            "wy3dApp::PluginManager",
            "Another plugin operation is in progress.").toStdString();
        return false;
    }

    PluginRecord* pRecord = this->findRecord(key);
    if (!pRecord)
    {
        error = QCoreApplication::translate(
            "wy3dApp::PluginManager", "Plugin not found.").toStdString();
        return false;
    }
    if (pRecord->state == PluginState::Loaded)
    {
        error = QCoreApplication::translate(
            "wy3dApp::PluginManager", "Plugin is already loaded.").toStdString();
        return false;
    }
    if (pRecord->state == PluginState::Unloading)
    {
        error = QCoreApplication::translate(
            "wy3dApp::PluginManager", "Plugin is being unloaded.").toStdString();
        return false;
    }
    if (pRecord->state == PluginState::NeedsRestart)
    {
        error = QCoreApplication::translate(
            "wy3dApp::PluginManager",
            "The plugin module is still loaded; "
            "restart the application before loading it again.").toStdString();
        return false;
    }

    OperationGuard operationGuard(_operationInProgress);

    PluginManifest manifest;
    std::string parseError;
    if (!PluginManifestParser::loadFromFile(pRecord->iniFullPathU8, manifest, parseError))
    {
        pRecord->manifest = manifest;
        pRecord->state = PluginState::Failed;
        pRecord->error = parseError;
        error = parseError;
        _log.warn("failed to load plugin manifest for '" + pRecord->key + "': " + parseError);
        return false;
    }

    for (const PluginRecord& record : _records)
    {
        if (&record == pRecord) continue;
        if (!record.manifest.id.empty() && record.manifest.id == manifest.id)
        {
            error = QCoreApplication::translate(
                "wy3dApp::PluginManager", "duplicate plugin id '%1'.")
                .arg(QString::fromStdString(manifest.id)).toStdString();
            pRecord->state = PluginState::Failed;
            pRecord->error = error;
            _log.warn("failed to load plugin '" + manifest.id + "': " + error);
            return false;
        }
    }

    pRecord->manifest = manifest;
    const fs::path pathLibrary =
        fs::u8path(pRecord->dirFullPathU8) / fs::u8path(pRecord->manifest.library);
    pRecord->libraryFullPathU8 = pathLibrary.u8string();

    {
        std::string loadError;
        if (!this->loadRecord(*pRecord, loadError))
        {
            pRecord->state = PluginState::Failed;
            pRecord->error = loadError;
            error = loadError;
            _log.warn("failed to load plugin '" + manifest.id + "': " + loadError);
            return false;
        }

        _log.info("loaded plugin '" + manifest.id + "'.");
        this->replayEnvironmentStack(*pRecord);
    }
    return true;
}

void PluginManager::replayEnvironmentStack(PluginRecord& record)
{
    if (!record.pPlugin) return;

    wyap::EnvManager* pEnvManager = ApplicationImpl::instance().getEnvManager();
    if (!pEnvManager) return;

    // Replay bottom-to-top, matching the order in which the environments were
    // originally entered. Suspended environments below the active one also
    // need their UI/registrations from the newly loaded plugin.
    const std::vector<wyap::Environment*> stack =
        pEnvManager->getEnvironmentStack();
    for (wyap::Environment* pEnvironment : stack)
    {
        if (!pEnvironment) continue;
        record.pPlugin->onEnvironmentEntered(pEnvironment);
    }
}

void PluginManager::terminate()
{
    wyap::EnvManager* pEnvManager = ApplicationImpl::instance().getEnvManager();
    assert(pEnvManager);
    pEnvManager->removeReactor(this);

    // Fallback for shutdown paths that did not exit the environments first
    // (e.g. qApp->quit() without MainWindow::closeEvent): notify plugins for
    // every environment still on the stack before deleting them. In the normal
    // shutdown path the stack is already empty and the EnvManager reactor has
    // delivered these callbacks.
    const std::vector<wyap::Environment*> stack = pEnvManager->getEnvironmentStack();
    for (auto iter = stack.rbegin(); iter != stack.rend(); ++iter)
    {
        wyap::Environment* pEnvironment = *iter;
        if (!pEnvironment) continue;
        for (PluginRecord& record : _records)
        {
            if (record.state != PluginState::Loaded || !record.pPlugin) continue;
            record.pPlugin->onEnvironmentToBeExited(pEnvironment);
        }
    }

    for (PluginRecord& record : _records)
    {
        if (!record.pPlugin)
        {
            continue;
        }
        {
            record.pPlugin->terminate();
        }
        if (record.deletePluginFn)
        {
            record.deletePluginFn(record.pPlugin);
        }
        else
        {
            assert(false);
        }
        record.pPlugin = nullptr;
        record.deletePluginFn = nullptr;
    }
    _records.clear();

    _log.close();
}

void PluginManager::onEnvironmentEntered(wyap::Environment* pEnvironment)
{
    assert(pEnvironment);
    if (!pEnvironment) return;

    for (PluginRecord& record : _records)
    {
        if (record.state != PluginState::Loaded || !record.pPlugin) continue;
        record.pPlugin->onEnvironmentEntered(pEnvironment);
    }
}

void PluginManager::onEnvironmentToBeExited(wyap::Environment* pEnvironment)
{
    assert(pEnvironment);
    if (!pEnvironment) return;

    for (size_t i = _records.size(); i > 0; --i)
    {
        PluginRecord& record = _records[i - 1];
        if (record.state != PluginState::Loaded || !record.pPlugin) continue;
        record.pPlugin->onEnvironmentToBeExited(pEnvironment);
    }
}

void PluginManager::onEnvironmentToBeSuspended(wyap::Environment* pEnvironment)
{
    assert(pEnvironment);
    if (!pEnvironment) return;

    for (size_t i = _records.size(); i > 0; --i)
    {
        PluginRecord& record = _records[i - 1];
        if (record.state != PluginState::Loaded || !record.pPlugin) continue;
        record.pPlugin->onEnvironmentToBeSuspended(pEnvironment);
    }
}

void PluginManager::onEnvironmentResumed(wyap::Environment* pEnvironment)
{
    assert(pEnvironment);
    if (!pEnvironment) return;

    for (PluginRecord& record : _records)
    {
        if (record.state != PluginState::Loaded || !record.pPlugin) continue;
        record.pPlugin->onEnvironmentResumed(pEnvironment);
    }
}

} // namespace wy3dApp
