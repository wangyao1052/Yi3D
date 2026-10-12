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

#ifndef WY3DAPP_LOG_H
#define WY3DAPP_LOG_H

#include <fstream>
#include <string>
#include "wy3dAppDefs.h"

namespace wy3dApp {

class OutputWidget;

class Log
{
public:
    Log();
    ~Log();
    Log(const Log&) = delete;
    Log& operator=(const Log&) = delete;
    Log(Log&&) = delete;
    Log& operator=(Log&&) = delete;

    enum class Level { Info = 0, Warning = 1, Error = 2 };

    bool initialize(const std::string& u8FilePath, bool isOutputWidgetMirrorEnabled = true);
    void close();

    void write(Level level, const std::string& u8Message);

    void info(const std::string& u8Message) { write(Level::Info, u8Message); }
    void warn(const std::string& u8Message) { write(Level::Warning, u8Message); }
    void error(const std::string& u8Message) { write(Level::Error, u8Message); }

private:
    std::ofstream _file;
    OutputWidget* _pOutputWidget = nullptr;
};

}

#endif // WY3DAPP_LOG_H
