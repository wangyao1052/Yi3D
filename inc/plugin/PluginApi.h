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

#ifndef YI3D_PLUGIN_API_H
#define YI3D_PLUGIN_API_H

#include <cstdint>
#include <cassert>

#include "wy3dAppVersion.h"

class Plugin;

// Baked into the plugin module at compile time; the module reports it through
// wy3dappGetPluginSdkVersion and the host requires its major.minor to equal
// its own before creating an instance (patch differences are tolerated).
struct PluginSdkVersion
{
    std::uint8_t major;
    std::uint8_t minor;
    std::uint8_t patch;
};

using wy3dappNewPluginFn = Plugin* (*)();
using wy3dappDeletePluginFn = void (*)(Plugin*);
using wy3dappGetPluginSdkVersionFn = PluginSdkVersion (*)();

#if defined(_WIN32)
#define WY3DAPP_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define WY3DAPP_PLUGIN_EXPORT extern "C" __attribute__((visibility("default")))
#endif

#define WY3DAPP_IMPLEMENT_PLUGIN_ENTRY(PLUGIN_CLASS) \
    WY3DAPP_PLUGIN_EXPORT Plugin* wy3dappNewPlugin() \
    { \
        return new PLUGIN_CLASS(); \
    } \
    WY3DAPP_PLUGIN_EXPORT void wy3dappDeletePlugin(Plugin* pPlugin) \
    { \
        PLUGIN_CLASS* pPluginImpl = dynamic_cast<PLUGIN_CLASS*>(pPlugin); \
        if (pPluginImpl) \
        { \
            delete pPluginImpl; \
        } \
        else \
        { \
            assert(false); \
        } \
    } \
    WY3DAPP_PLUGIN_EXPORT PluginSdkVersion wy3dappGetPluginSdkVersion() \
    { \
        return PluginSdkVersion{ \
            YI3D_APP_VERSION_MAJOR, \
            YI3D_APP_VERSION_MINOR, \
            YI3D_APP_VERSION_PATCH \
        }; \
    }
        

#endif // YI3D_PLUGIN_API_H
