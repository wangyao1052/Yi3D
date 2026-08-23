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

#include "exporter/SketchImporter.h"
#include "exporter/SketchDxfImporter.h"
#include <memory>
#include <Standard_Failure.hxx>
#include <wyVector2.h>
#include <wydbDatabase.h>

SketchImporterManager& SketchImporterManager::instance()
{
    static SketchImporterManager instance;
    return instance;
}

SketchImporterManager::SketchImporterManager()
{
    _filter2Importer[tr("DXF format (*.dxf)")] = std::make_shared<SketchDxfImporter>();
}

SketchImporterManager::~SketchImporterManager()
{
}

bool SketchImporter::perform(wydb::Database* pDb, const std::wstring& fileFullPath)
{
    try
    {
        return this->performImpl(pDb, fileFullPath);
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

