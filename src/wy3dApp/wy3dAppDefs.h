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

#ifndef WY3DAPP_DEFS_H
#define WY3DAPP_DEFS_H

// Export/import macro for the app framework shared library (wy3dApp).
#if defined(_WIN32)
    #if defined(WY3DAPP_LIBRARY)
        #define WY3DAPP_EXPORT __declspec(dllexport)
    #else
        #define WY3DAPP_EXPORT __declspec(dllimport)
    #endif
#else
    #define WY3DAPP_EXPORT __attribute__((visibility("default")))
#endif

#define NS_WY3DAPP_BEG namespace wy3dApp {
#define NS_WY3DAPP_END }

#endif // WY3DAPP_DEFS_H