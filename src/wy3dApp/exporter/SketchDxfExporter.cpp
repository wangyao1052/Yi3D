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

#include "SketchDxfExporter.h"

#include <cassert>
#include <map>
#include <string>

#include <QCoreApplication>
#include <QFileInfo>

#include "application/PythonScriptExecutor.h"

bool SketchDxfExporter::performImpl(const wy3d::Sketch* pSketch, const std::wstring& fileFullPath)
{
    if (!pSketch)
    {
        assert(false);
        return false;
    }

    // The conversion script is deployed next to the executable
    QString scriptPath = QCoreApplication::applicationDirPath() + QStringLiteral("/scripts/dxf/export_sketch.py");
    if (!QFileInfo::exists(scriptPath))
    {
        assert(false);
        return false;
    }

    // Read-only export; pass false so a pending transaction (e.g. the sketch
    // environment's edit transaction) is never aborted by the executor
    std::map<std::string, std::string> params;
    params["dxf_path"] = QString::fromStdWString(fileFullPath).toUtf8().constData();
    params["sketch_id"] = std::to_string(pSketch->getId().value());

    PythonScriptExecutor executor;
    PythonScriptExecutor::Error error = executor.Run(scriptPath.toUtf8().constData(), params, false);
    if (PythonScriptExecutor::Error::NoError != error)
        return false;

    return true;
}
