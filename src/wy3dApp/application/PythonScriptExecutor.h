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

#ifndef WY3DAPP_PYTHON_SCRIPT_EXECUTOR_H
#define WY3DAPP_PYTHON_SCRIPT_EXECUTOR_H

#include <map>
#include <string>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class PythonScriptExecutor
{
public:
    PythonScriptExecutor();
    ~PythonScriptExecutor();

    enum class Error
    {
        NoError = 0,
        PythonLibraryNotFound = 1,
        LoadPythonLibraryFailed = 2,
        InvalidPythonLibrary = 3,
        PythonScriptFileNotFound = 4,
        OpenPythonScriptFileFailed = 5,
        RunScriptError = 6,
        UnknownError = 99,
    };
    Error Run(const std::string& scriptFileFullPath,
        const std::map<std::string, std::string>& params = {},
        bool abortActiveTransactionOnExit = true);

private:
    void abortActiveTransaction();
};

} // namespace wy3dApp

#endif // WY3DAPP_PYTHON_SCRIPT_EXECUTOR_H
