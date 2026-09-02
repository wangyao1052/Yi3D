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

#include "SketchDxfImporter.h"

#include <cassert>
#include <map>
#include <string>

#include <QCoreApplication>
#include <QFileInfo>

#include "application/PythonScriptExecutor.h"

bool SketchDxfImporter::performImpl(wydb::Database* pDb, const std::wstring& fileFullPath)
{
    if (!pDb)
    {
        assert(false);
        return false;
    }

    // The conversion script is deployed next to the executable. It owns the
    // transaction (creates the XY sketch, fills it and commits); the executor
    // aborts any leftover transaction when the script fails
    QString scriptPath = QCoreApplication::applicationDirPath() + QStringLiteral("/scripts/modules/dxf/import_sketch.py");
    if (!QFileInfo::exists(scriptPath))
    {
        assert(false);
        return false;
    }

    std::map<std::string, std::string> params;
    params["dxf_path"] = QString::fromStdWString(fileFullPath).toUtf8().constData();

    PythonScriptExecutor executor;
    PythonScriptExecutor::Error error = executor.Run(scriptPath.toUtf8().constData(), params);
    if (PythonScriptExecutor::Error::NoError != error)
        return false;

    return true;
}
