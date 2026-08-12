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

#include "OffsetSheetGuiCmd.h"

#include <cassert>
#include <cmath>
#include <QCoreApplication>
#include <QOpenGLWidget>

#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapSelManager.h>
#include <wy3dSelectionType.h>
#include <wy3dSheet.h>
#include <wy3dOffsetSheet.h>
#include <wy3dImpl.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>

#include "application/Application.h"
#include "scene/Scene.h"
#include "scene/nodes/ElementNodeType.h"
#include "select/filters/CommonSelFilters.h"
#include "commands/dialogs/OffsetSheetCmdPanel.h"
#include "utils/MessageBoxUtil.h"
#include "widgets/frame/MainWindow.h"

static constexpr double kDefaultOffset = 2.0;

// ============================================================================
// MakeOffsetSheet
// ============================================================================

void MakeOffsetSheet::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pOffsetSheet) idSet.insert(_pOffsetSheet->getId());
}

bool MakeOffsetSheet::init(const wydb::ElementId& sheetId, unsigned int& errorCode)
{
    errorCode = 0;
    if (!_pDb || !_pTopTrans || _pOffsetSheet || _isFinished) return false;
    if (sheetId.isNull()) return false;

    const wydb::Element* pElem = _pDb->getElement(sheetId);
    if (!pElem) return false;
    const wy3d::Sheet* pConstSheet = wy3d::Sheet::cast(pElem);
    if (!pConstSheet) return false;
    if (!pConstSheet->getParent().isNull()) return false;

    wy3d::OffsetSheet* pObj = nullptr;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction();
    if (!pTrans) return false;
    wy3d::Sheet* pSheet = wy3d::Sheet::cast(pTrans->getElementForWrite(sheetId));
    if (!pSheet) { _pDb->getTransactionManager()->abortTransaction(); return false; }

    if (wy::ErrorStatus::Ok != wy3d::OffsetSheet::create(
            pTrans, pSheet, kDefaultOffset, pObj) || !pObj)
    {
        _pDb->getTransactionManager()->abortTransaction();
        return false;
    }
    _pDb->getTransactionManager()->endTransaction();
    _pOffsetSheet = pObj;
    errorCode = wy3d::getErrorCodeFromChainUpdateFeedback(
        _pDb->getTransactionManager()->getChainUpdateFeedback(pObj->getId()).get());
    if (errorCode != 0) return false;
    return true;
}

bool MakeOffsetSheet::update(double offset)
{
    if (!_pDb || !_pTopTrans || !_pOffsetSheet || _isFinished) return false;
    if (std::fabs(offset) < wy3d::kMinValue ||
        std::fabs(offset) > wy3d::kMaxValue) return false;

    wydb::TransactionManager* pTransMgr = _pDb->getTransactionManager();
    wydb::Transaction* pTrans = pTransMgr->startTransaction();
    if (!pTrans) return false;
    {
        _pOffsetSheet->upgradeForWrite();
        _pOffsetSheet->setOffset(offset);
    }
    if (wy::ErrorStatus::Ok == pTransMgr->endTransaction())
        pTransMgr->mergeTransaction();
    else
        assert(false);
    return true;
}

// ============================================================================
// OffsetSheetGuiCmd
// ============================================================================

OffsetSheetGuiCmd::OffsetSheetGuiCmd() : OsgGuiCommand(),
    _sheetId(wydb::ElementId::kNull), _pCmdPanel(nullptr), _offset(kDefaultOffset)
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

OffsetSheetGuiCmd::~OffsetSheetGuiCmd() {}

wyap::CmdExecution::StartResult OffsetSheetGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = GuiCommand::onStart();
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
        _sheetId = sheetId;
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();
        this->onSheetSelected();
    }
    else
    {
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();
        Application::instance().getStatusBar()->setTips(
            QCoreApplication::translate("OffsetSheetGuiCmd",
                "Select a sheet to offset."));
        Application::instance().setCursor(CursorType::SelectElements);
    }
    return wyap::CmdExecution::StartResult::Succeeded;
}

void OffsetSheetGuiCmd::cleanup()
{
    _sheetId = wydb::ElementId::kNull;
    _offset = kDefaultOffset;
    _pMakeOffsetSheet = nullptr;
    _pSheetPreview = nullptr;
    this->destroyCmdPanel();
}

void OffsetSheetGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (!_sheetId.isNull()) return;

    this->mouseMovePointPickPreview(
        event.x, event.y, _pointPickOption, _pSheetPreview);
    if (_pSheetPreview)
        Application::instance().setCursor(CursorType::SelectElements);
}

void OffsetSheetGuiCmd::onLeftMouseUp(const MouseEvent& event)
{
    if (!_sheetId.isNull()) return;

    if (_pSheetPreview)
    {
        _sheetId = _pSheetPreview->getSelection().getElementId();
        this->onSheetSelected();
    }
}

void OffsetSheetGuiCmd::onFeatureTreeItemClicked(const wydb::ElementId& id)
{
    if (!_sheetId.isNull()) return;
    if (id.isNull() || !isValidSheet(id)) return;
    _sheetId = id;
    Application::instance().getSelManager()->beginChange();
    Application::instance().getSelManager()->clearSelections();
    Application::instance().getSelManager()->endChange();
    this->onSheetSelected();
}

bool OffsetSheetGuiCmd::isValidSheetSelectionSet(
    const wyap::SelectionSet& ss, wydb::ElementId& sheetId)
{
    sheetId = wydb::ElementId::kNull;
    if (ss.getCount() != 1) return false;
    const wyap::Selection& sel = ss.createIterator().current();
    if (sel.getSelectionType() != static_cast<unsigned int>(
            wy3d::SelectionType::Element)) return false;
    wydb::ElementId id = sel.getElementId();
    if (id.isNull() || !isValidSheet(id)) return false;
    sheetId = id;
    return true;
}

bool OffsetSheetGuiCmd::isValidSheet(const wydb::ElementId& sheetId)
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return false;
    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    if (!pSheet || !pSheet->getParent().isNull()) return false;
    return true;
}

void OffsetSheetGuiCmd::onSheetSelected()
{
    // add Sheet to selection manager for highlighting
    {
        wyap::Selection sel(
            static_cast<unsigned int>(wy3d::SelectionType::Element), _sheetId);
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->addSelection(sel);
        Application::instance().getSelManager()->endChange();
    }

    // create element with default value
    _pMakeOffsetSheet = std::make_shared<MakeOffsetSheet>(this);
    unsigned int errorCode(0);
    if (!_pMakeOffsetSheet->init(_sheetId, errorCode))
    {
        _pMakeOffsetSheet = nullptr;
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();
        if (0 != errorCode) MessageBoxUtil::showError(errorCode);
        this->requestAbort(AbortCause::ErrorTerminate);
        return;
    }

    // show dialog
    if (!this->createCmdPanel())
    {
        _pMakeOffsetSheet = nullptr;
        this->requestAbort(AbortCause::ErrorTerminate);
        return;
    }

    Application::instance().getStatusBar()->setTips(
        QCoreApplication::translate("OffsetSheetGuiCmd",
            "Specify offset distance, then OK to apply."));
    Application::instance().setCursor(CursorType::Select);
}

bool OffsetSheetGuiCmd::createCmdPanel()
{
    QOpenGLWidget* pParentWidget = nullptr;
    MainWindow* pMainWindow = Application::instance().getMainWindow();
    if (pMainWindow)
        pParentWidget = pMainWindow->findChild<QOpenGLWidget*>();
    if (!pParentWidget) return false;

    _pCmdPanel = new OffsetSheetCmdPanel(pParentWidget);
    _pCmdPanel->setOffsetValue(_offset);

    QObject::connect(_pCmdPanel, &OffsetSheetCmdPanel::offsetChanged,
        _pCmdPanel, [this](double v) { this->onDialogOffsetChanged(v); });
    QObject::connect(_pCmdPanel, &OffsetSheetCmdPanel::accepted,
        _pCmdPanel, [this]() { this->onDialogAccepted(); });
    QObject::connect(_pCmdPanel, &OffsetSheetCmdPanel::canceled,
        _pCmdPanel, [this]() { this->onDialogCanceled(); });
    _pCmdPanel->show();
    return true;
}

void OffsetSheetGuiCmd::destroyCmdPanel()
{
    if (_pCmdPanel)
    {
        _pCmdPanel->hide();
        delete _pCmdPanel;
        _pCmdPanel = nullptr;
    }
}

void OffsetSheetGuiCmd::onDialogOffsetChanged(double value)
{
    _offset = value;
    if (_pMakeOffsetSheet)
        _pMakeOffsetSheet->update(_offset);
}

void OffsetSheetGuiCmd::onDialogAccepted()
{
    if (_pMakeOffsetSheet)
    {
        _pMakeOffsetSheet->commit();
        _pMakeOffsetSheet = nullptr;
    }
    this->requestEnd();
}

void OffsetSheetGuiCmd::onDialogCanceled()
{
    Application::instance().getSelManager()->beginChange();
    Application::instance().getSelManager()->clearSelections();
    Application::instance().getSelManager()->endChange();
    _pMakeOffsetSheet = nullptr;
    this->requestAbort(AbortCause::UserCancel);
}
