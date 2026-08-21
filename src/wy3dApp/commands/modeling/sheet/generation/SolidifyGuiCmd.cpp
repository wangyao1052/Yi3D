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

#include "SolidifyGuiCmd.h"

#include <cassert>
#include <QCoreApplication>

#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapSelManager.h>
#include <wy3dSelectionType.h>
#include <wy3dSheet.h>
#include <wy3dSolidify.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>

#include "application/Application.h"
#include "scene/Scene.h"
#include "scene/nodes/ElementNodeType.h"
#include "select/filters/CommonSelFilters.h"
#include "utils/MessageBoxUtil.h"

// ============================================================================
// MakeSolidify
// ============================================================================

void MakeSolidify::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pSolidify) idSet.insert(_pSolidify->getId());
}

bool MakeSolidify::init(const wydb::ElementId& sheetId, unsigned int& errorCode)
{
    errorCode = 0;
    if (!_pDb || !_pTopTrans || _pSolidify || _isFinished) return false;
    if (sheetId.isNull()) return false;

    const wydb::Element* pElem = _pDb->getElement(sheetId);
    if (!pElem) return false;
    const wy3d::Sheet* pConstSheet = wy3d::Sheet::cast(pElem);
    if (!pConstSheet) return false;
    if (!pConstSheet->getParent().isNull()) return false;

    wy3d::Solidify* pObj = nullptr;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction();
    if (!pTrans) return false;
    wy3d::Sheet* pSheet = wy3d::Sheet::cast(pTrans->getElementForWrite(sheetId));
    if (!pSheet)
    {
        _pDb->getTransactionManager()->abortTransaction();
        return false;
    }

    if (wy::ErrorStatus::Ok != wy3d::Solidify::create(pTrans, pSheet, pObj) || !pObj)
    {
        _pDb->getTransactionManager()->abortTransaction();
        return false;
    }
    _pDb->getTransactionManager()->endTransaction();
    _pSolidify = pObj;
    errorCode = wy3d::getErrorCodeFromChainUpdateFeedback(
        _pDb->getTransactionManager()->getChainUpdateFeedback(pObj->getId()).get());
    if (errorCode != 0) return false;
    return true;
}

// ============================================================================
// SolidifyGuiCmd
// ============================================================================

SolidifyGuiCmd::SolidifyGuiCmd() : OsgGuiCommand()
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

SolidifyGuiCmd::~SolidifyGuiCmd() {}

wyap::CmdExecution::StartResult SolidifyGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = __baseClass::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Sheet);
    _pointPickOption.selType = wy3d::SelectionType::Element;
    _pointPickOption.pSelPreFilter = std::make_shared<CommonPreSelFilterForPointPick>(
        wy3d::Sheet::classInfo());
    _pointPickOption.pSelFilter = std::make_shared<SingleClassSelFilter>(
        wy3d::Sheet::classInfo());

    const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
    wydb::ElementId sheetId(wydb::ElementId::kNull);
    if (this->isValidSheetSelectionSet(ss, sheetId) && !sheetId.isNull())
    {
        if (this->makeSolidify(sheetId))
        {
            return wyap::CmdExecution::StartResult::Succeeded; // not a good choice
        }
        else
        {
            return wyap::CmdExecution::StartResult::Failed;
        }
    }
    else
    {
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        Application::instance().getStatusBar()->setTips(QCoreApplication::translate(
            "SolidifyGuiCmd", "Select a closed sheet to solidify."));
        Application::instance().setCursor(CursorType::SelectElements);
    }
    return wyap::CmdExecution::StartResult::Succeeded;
}

void SolidifyGuiCmd::cleanup()
{
    _pSheetPreview = nullptr;
    _pMakeSolidify = nullptr;
}

void SolidifyGuiCmd::onMouseMove(const MouseEvent& event)
{
    this->mouseMovePointPickPreview(event.x, event.y, _pointPickOption, _pSheetPreview);
}

void SolidifyGuiCmd::onLeftMouseUp(const MouseEvent& event)
{
    if (!_pSheetPreview) return;

    wydb::ElementId sheetId = _pSheetPreview->getSelection().getElementId();
    if (sheetId.isNull())
    {
        assert(false);
        return;
    }
    this->perform(sheetId);
}

void SolidifyGuiCmd::onFeatureTreeItemClicked(const wydb::ElementId& id)
{
    _pSheetPreview = nullptr;
    if (id.isNull()) return;
    if (!this->isValidSheet(id)) return;
    this->perform(id);
}

bool SolidifyGuiCmd::isValidSheetSelectionSet(
    const wyap::SelectionSet& ss, wydb::ElementId& sheetId)
{
    sheetId = wydb::ElementId::kNull;
    if (ss.getCount() != 1) return false;
    const wyap::Selection& sel = ss.createIterator().current();
    if (sel.getSelectionType() != static_cast<unsigned int>(wy3d::SelectionType::Element)) return false;
    wydb::ElementId id = sel.getElementId();
    if (id.isNull() || !isValidSheet(id)) return false;
    sheetId = id;
    return true;
}

bool SolidifyGuiCmd::isValidSheet(const wydb::ElementId& sheetId)
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return false;
    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    if (!pSheet || !pSheet->getParent().isNull()) return false;
    return true;
}

bool SolidifyGuiCmd::makeSolidify(wydb::ElementId sheetId)
{
    _pMakeSolidify = std::make_shared<MakeSolidify>(this);
    unsigned int errorCode(0);
    if (!_pMakeSolidify->init(sheetId, errorCode))
    {
        _pMakeSolidify = nullptr;
        if (0 != errorCode) MessageBoxUtil::showError(errorCode);
        return false;
    }

    _pMakeSolidify->commit();
    _pMakeSolidify = nullptr;
    return true;
}

void SolidifyGuiCmd::perform(wydb::ElementId sheetId)
{
    if (this->makeSolidify(sheetId))
    {
        wy::ErrorStatus ret = this->requestEnd();
        assert(wy::ErrorStatus::Ok == ret);
    }
    else
    {
        wy::ErrorStatus ret = this->requestAbort(
            wyap::CmdExecution::AbortCause::ErrorTerminate);
        assert(wy::ErrorStatus::Ok == ret);
    }
}
