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

#include "ThickenGuiCmd.h"

#include <cassert>
#include <cmath>
#include <QCoreApplication>
#include <QOpenGLWidget>

#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapSelManager.h>
#include <wy3dSelectionType.h>
#include <wy3dSheet.h>
#include <wy3dThicken.h>
#include <wy3dImpl.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>

#include "application/Application.h"
#include "scene/Scene.h"
#include "scene/nodes/ElementNodeType.h"
#include "select/filters/CommonSelFilters.h"
#include "commands/dialogs/ThickenCmdPanel.h"
#include "utils/MessageBoxUtil.h"
#include "widgets/frame/MainWindow.h"

static constexpr double kDefaultThickness = 2.0;

// ============================================================================
// MakeThicken
// ============================================================================

void MakeThicken::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pThicken) idSet.insert(_pThicken->getId());
}

bool MakeThicken::init(const wydb::ElementId& sheetId, unsigned int& errorCode)
{
    errorCode = 0;
    if (!_pDb || !_pTopTrans || _pThicken || _isFinished) return false;
    if (sheetId.isNull()) return false;

    const wydb::Element* pElem = _pDb->getElement(sheetId);
    if (!pElem) return false;
    const wy3d::Sheet* pConstSheet = wy3d::Sheet::cast(pElem);
    if (!pConstSheet) return false;
    if (!pConstSheet->getParent().isNull()) return false;

    wy3d::Thicken* pThicken = nullptr;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction();
    if (!pTrans) return false;
    wy3d::Sheet* pSheet = wy3d::Sheet::cast(pTrans->getElementForWrite(sheetId));
    if (!pSheet) { _pDb->getTransactionManager()->abortTransaction(); return false; }

    if (wy::ErrorStatus::Ok != wy3d::Thicken::create(pTrans, pSheet,
        kDefaultThickness, wy3d::ThickenDirection::OneSide, pThicken) || !pThicken)
    {
        _pDb->getTransactionManager()->abortTransaction();
        return false;
    }
    _pDb->getTransactionManager()->endTransaction();
    _pThicken = pThicken;
    errorCode = wy3d::getErrorCodeFromChainUpdateFeedback(
        _pDb->getTransactionManager()->getChainUpdateFeedback(pThicken->getId()).get());
    if (errorCode != 0) return false;
    return true;
}

bool MakeThicken::update(double thickness, int direction)
{
    if (!_pDb || !_pTopTrans || !_pThicken || _isFinished) return false;
    if (std::fabs(thickness) < wy3d::kMinValue ||
        std::fabs(thickness) > wy3d::kMaxValue) return false;

    wydb::TransactionManager* pTransMgr = _pDb->getTransactionManager();
    wydb::Transaction* pTrans = pTransMgr->startTransaction();
    if (!pTrans) return false;
    {
        _pThicken->upgradeForWrite();
        _pThicken->setThickness(thickness);
        _pThicken->setDirection(static_cast<wy3d::ThickenDirection>(direction));
    }
    if (wy::ErrorStatus::Ok == pTransMgr->endTransaction())
        pTransMgr->mergeTransaction();
    else
        assert(false);
    return true;
}

// ============================================================================
// ThickenGuiCmd
// ============================================================================

ThickenGuiCmd::ThickenGuiCmd() : OsgGuiCommand(),
    _sheetId(wydb::ElementId::kNull), _pCmdPanel(nullptr),
    _thickness(kDefaultThickness), _direction(0)
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

ThickenGuiCmd::~ThickenGuiCmd() {}

wyap::CmdExecution::StartResult ThickenGuiCmd::onStart()
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
            QCoreApplication::translate("ThickenGuiCmd",
                "Select a sheet to thicken."));
        Application::instance().setCursor(CursorType::SelectElements);
    }
    return wyap::CmdExecution::StartResult::Succeeded;
}

void ThickenGuiCmd::cleanup()
{
    _sheetId = wydb::ElementId::kNull;
    _thickness = kDefaultThickness;
    _direction = 0;
    _pMakeThicken = nullptr;
    _pSheetPreview = nullptr;
    this->destroyCmdPanel();
}

void ThickenGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (!_sheetId.isNull()) return; // already selected, dialog shown

    this->mouseMovePointPickPreview(
        event.x, event.y, _pointPickOption, _pSheetPreview);
    if (_pSheetPreview)
        Application::instance().setCursor(CursorType::SelectElements);
}

void ThickenGuiCmd::onLeftMouseUp(const MouseEvent& event)
{
    if (!_sheetId.isNull()) return;

    if (_pSheetPreview)
    {
        _sheetId = _pSheetPreview->getSelection().getElementId();
        this->onSheetSelected();
    }
}

void ThickenGuiCmd::onFeatureTreeItemClicked(const wydb::ElementId& id)
{
    if (!_sheetId.isNull()) return;
    if (id.isNull() || !isValidSheet(id)) return;
    _sheetId = id;
    Application::instance().getSelManager()->beginChange();
    Application::instance().getSelManager()->clearSelections();
    Application::instance().getSelManager()->endChange();
    this->onSheetSelected();
}

bool ThickenGuiCmd::isValidSheetSelectionSet(
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

bool ThickenGuiCmd::isValidSheet(const wydb::ElementId& sheetId)
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return false;
    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pDb->getElement(sheetId));
    if (!pSheet || !pSheet->getParent().isNull()) return false;
    return true;
}

void ThickenGuiCmd::onSheetSelected()
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

    // create element with default values
    _pMakeThicken = std::make_shared<MakeThicken>(this);
    unsigned int errorCode(0);
    if (!_pMakeThicken->init(_sheetId, errorCode))
    {
        _pMakeThicken = nullptr;
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
        _pMakeThicken = nullptr;
        this->requestAbort(AbortCause::ErrorTerminate);
        return;
    }

    Application::instance().getStatusBar()->setTips(
        QCoreApplication::translate("ThickenGuiCmd",
            "Specify thickness and direction, then OK to apply."));
    Application::instance().setCursor(CursorType::Select);
}

bool ThickenGuiCmd::createCmdPanel()
{
    QOpenGLWidget* pParentWidget = nullptr;
    MainWindow* pMainWindow = Application::instance().getMainWindow();
    if (pMainWindow)
        pParentWidget = pMainWindow->findChild<QOpenGLWidget*>();
    if (!pParentWidget) return false;

    _pCmdPanel = new ThickenCmdPanel(pParentWidget);
    _pCmdPanel->setThicknessValue(_thickness);

    QObject::connect(_pCmdPanel, &ThickenCmdPanel::thicknessChanged,
        _pCmdPanel, [this](double v) { this->onDialogThicknessChanged(v); });
    QObject::connect(_pCmdPanel, &ThickenCmdPanel::directionChanged,
        _pCmdPanel, [this](int d) { this->onDialogDirectionChanged(d); });
    QObject::connect(_pCmdPanel, &ThickenCmdPanel::accepted,
        _pCmdPanel, [this]() { this->onDialogAccepted(); });
    QObject::connect(_pCmdPanel, &ThickenCmdPanel::canceled,
        _pCmdPanel, [this]() { this->onDialogCanceled(); });
    _pCmdPanel->show();
    return true;
}

void ThickenGuiCmd::destroyCmdPanel()
{
    if (_pCmdPanel)
    {
        _pCmdPanel->hide();
        delete _pCmdPanel;
        _pCmdPanel = nullptr;
    }
}

void ThickenGuiCmd::onDialogThicknessChanged(double value)
{
    _thickness = value;
    if (_pMakeThicken)
        _pMakeThicken->update(_thickness, _direction);
}

void ThickenGuiCmd::onDialogDirectionChanged(int direction)
{
    _direction = direction;
    if (_pMakeThicken)
        _pMakeThicken->update(_thickness, _direction);
}

void ThickenGuiCmd::onDialogAccepted()
{
    if (_pMakeThicken)
    {
        _pMakeThicken->commit();
        _pMakeThicken = nullptr;
    }
    this->requestEnd();
}

void ThickenGuiCmd::onDialogCanceled()
{
    Application::instance().getSelManager()->beginChange();
    Application::instance().getSelManager()->clearSelections();
    Application::instance().getSelManager()->endChange();
    _pMakeThicken = nullptr;
    this->requestAbort(AbortCause::UserCancel);
}
