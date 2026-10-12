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

#include "utils/Log.h"

#include <cassert>
#include <filesystem>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>

#include <wy3dApp/Application.h>
#include "widgets/panels/DockPanelIds.h"
#include "widgets/panels/DockPanelManager.h"
#include "widgets/panels/output/OutputWidget.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

namespace {

const char* levelTag(Log::Level level)
{
    switch (level)
    {
    case Log::Level::Warning: return "WARN ";
    case Log::Level::Error: return "ERROR";
    default: return "INFO ";
    }
}

std::string currentTimestamp()
{
    return QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss").toStdString();
}

} // namespace

Log::Log()
{
}

Log::~Log()
{
    this->close();
}

bool Log::initialize(const std::string& u8FilePath, bool isOutputWidgetMirrorEnabled)
{
    QDir().mkpath(QFileInfo(QString::fromStdString(u8FilePath)).absolutePath());

    _file.open(std::filesystem::u8path(u8FilePath), std::ios::out | std::ios::app);
    if (_file.is_open())
    {
        _file << "--- session " << currentTimestamp()
            << " pid=" << QCoreApplication::applicationPid() << " ---\n";
        _file.flush();
    }
    else
    {
        assert(false);
        return false;
    }

    if (isOutputWidgetMirrorEnabled)
    {
        DockPanelManager* pDockPanelManager = Application::instance().getDockPanelManager();
        assert(pDockPanelManager);
        _pOutputWidget = pDockPanelManager->findWidgetAs<OutputWidget>(DockPanelIds::Output);
        assert(_pOutputWidget);
    }

    return true;
}

void Log::close()
{
    if (_file.is_open()) _file.close();
    _pOutputWidget = nullptr;
}

void Log::write(Level level, const std::string& u8Message)
{
    const std::string line = currentTimestamp() + " " + levelTag(level) + " " + u8Message;

    if (_file.is_open())
    {
        _file << line << "\n";
        _file.flush();
    }

    if (_pOutputWidget)
    {
        switch (level)
        {
        case Level::Warning: _pOutputWidget->warn(line); break;
        case Level::Error: _pOutputWidget->error(line); break;
        default: _pOutputWidget->info(line); break;
        }
    }
}

}
