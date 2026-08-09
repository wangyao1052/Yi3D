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

#include "Importer.h"
#include <cassert>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dImportedSolid.h>
#include <wy3dImportedSheet.h>
#include "application/Application.h"
#include "scene/Scene.h"

ImporterManager& ImporterManager::instance()
{
    static ImporterManager instance;
    return instance;
}

ImporterManager::ImporterManager()
{
    _filter2Importer[tr("BREP format (*.brep)")] = std::make_shared<BrepImporter>();
    _filter2Importer[tr("STEP format (*.step *.stp)")] = std::make_shared<StepImporter>();
    //_filter2Importer[tr("IGES format (*.iges *.igs)")] = std::make_shared<IgesImporter>();
}

ImporterManager::~ImporterManager()
{
}

bool Importer::perform(wydb::Database* pDb, const std::wstring& fileFullPath)
{
    if (!pDb) return false;
    wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
    if (!pTrans) return false;
    
    wy3d::ImportedSolid* pImportedSolid(nullptr);
    if (wy::ErrorStatus::Ok != wy3d::ImportedSolid::create(pTrans, fileFullPath, pImportedSolid) || !pImportedSolid)
    {
        assert(false);
        goto ABORT_TRANS;
    }

    wy3d::ImportedSheet* pImportedSheet(nullptr);
    if (wy::ErrorStatus::Ok != wy3d::ImportedSheet::create(pTrans, fileFullPath, pImportedSheet) || !pImportedSheet)
    {
        assert(false);
        goto ABORT_TRANS;
    }

    pDb->getTransactionManager()->endTransaction();
    return true;

ABORT_TRANS:
    assert(false);
    pDb->getTransactionManager()->abortTransaction();
    if (pImportedSolid) { wydb::deleteElement(pImportedSolid); pImportedSolid = nullptr; }
    if (pImportedSheet) { wydb::deleteElement(pImportedSheet); pImportedSheet = nullptr; }
    return false;
}