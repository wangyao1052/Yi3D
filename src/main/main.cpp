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

#include <QDir>

#include <wy3dApp/wy3dAppDefs.h>
#include "crashDump/CrashDump.h"

namespace wy3dApp {
// Exported by wy3dApp.dll; kept out of the public headers because only the
// shell needs this entry point.
WY3DAPP_EXPORT int runApplication(int argc, char** argv);
}

int main(int argc, char *argv[])
{
    // core dump
    initializeCrashDump(QDir::currentPath());

    return wy3dApp::runApplication(argc, argv);
}
