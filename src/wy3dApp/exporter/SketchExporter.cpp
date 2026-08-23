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

#include "exporter/SketchExporter.h"
#include "exporter/SketchDxfExporter.h"
#include <memory>
#include <Standard_Failure.hxx>
#include <wyVector2.h>
#include <wydbDatabase.h>

SketchExporterManager& SketchExporterManager::instance()
{
    static SketchExporterManager instance;
    return instance;
}

SketchExporterManager::SketchExporterManager()
{
    _filter2Exporter[tr("DXF format (*.dxf)")] = std::make_shared<SketchDxfExporter>();
}

SketchExporterManager::~SketchExporterManager()
{
}

bool SketchExporter::perform(const wy3d::Sketch* pSketch, const std::wstring& fileFullPath)
{
    try
    {
        return this->performImpl(pSketch, fileFullPath);
    }
    catch (const Standard_Failure&)
    {
        assert(false);
        return false;
    }
    catch (...)
    {
        assert(false);
        return false;
    }
}

