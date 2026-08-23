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

#include "ExtrudedSheetGuiCmd.h"

#include <cassert>
#include <cmath>
#include <QCursor>
#include <QCoreApplication>

#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapSelManager.h>
#include <wy3dSelectionType.h>
#include <wy3dSketch.h>
#include <wy3dSketchProfileForSheet.h>
#include <wy3dExtrudedSheet.h>
#include <wy3dImpl.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>

#include "application/Application.h"
#include "scene/Scene.h"
#include "scene/nodes/ElementNodeType.h"
#include "utils/SketchUtil.h"
#include "translation/ErrorCodeTranslation.h"
#include "utils/MessageBoxUtil.h"
#include "select/filters/CommonSelFilters.h"
#include "commands/sketch/dialogs/GuiCmdHoverInputPopup.h"
#include "widgets/frame/MainWindow.h"
#include "commands/transient/ValidSketchTransient.h"

static constexpr double kHoverPopupDelaySeconds = 0.8;

static bool parseDoubleText(const QString& text, double& value)
{
    bool ok(false);
    value = text.trimmed().toDouble(&ok);
    return ok;
}

// ============================================================================
// MakeExtrudedSheet
// ============================================================================

void MakeExtrudedSheet::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pExtrudedSheet) idSet.insert(_pExtrudedSheet->getId());
}

bool MakeExtrudedSheet::init(const wydb::ElementId& sketchId, unsigned int& errorCode)
{
    errorCode = 0;
    if (!_pDb || !_pTopTrans || _pExtrudedSheet || _isFinished)
        return false;
    if (sketchId.isNull())
        return false;

    const wydb::Element* pElem = _pDb->getElement(sketchId);
    if (!pElem) return false;
    const wy3d::Sketch* pConstSketch = wy3d::Sketch::cast(pElem);
    if (!pConstSketch) return false;
    if (!pConstSketch->getParent().isNull()) return false;

    wy3d::ExtrudedSheet* pSheet = nullptr;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction();
    if (!pTrans) return false;
    wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
    if (!pSketch)
    {
        assert(false);
        goto ABORT_TRANS;
    }

    if (wy::ErrorStatus::Ok != wy3d::ExtrudedSheet::create(pTrans, pSketch, _direction, wy3d::kMinValue, pSheet) || !pSheet)
    {
        assert(false);
        goto ABORT_TRANS;
    }
    _pDb->getTransactionManager()->endTransaction();
    _pExtrudedSheet = pSheet;
    _workPlnNormal = pSketch->getPlane().getNormal();
    errorCode = wy3d::getErrorCodeFromChainUpdateFeedback(
        _pDb->getTransactionManager()->getChainUpdateFeedback(pSheet->getId()).get());
    if (errorCode != 0) return false;
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    _pExtrudedSheet = nullptr;
    _workPlnNormal.set(0.0, 0.0, 1.0);
    return false;
}

bool MakeExtrudedSheet::update(double depth)
{
    if (!_pDb || !_pTopTrans || !_pExtrudedSheet || _isFinished) return false;
    if (std::fabs(depth) < wy3d::kMinValue || std::fabs(depth) > wy3d::kMaxValue) return false;

    wydb::TransactionManager* pTransMgr = _pDb->getTransactionManager();
    wydb::Transaction* pTrans = pTransMgr->startTransaction();
    if (!pTrans) return false;
    {
        _pExtrudedSheet->upgradeForWrite();
        _pExtrudedSheet->setDepth(depth);
    }
    if (wy::ErrorStatus::Ok == pTransMgr->endTransaction())
    {
        pTransMgr->mergeTransaction();
    }
    else
    {
        assert(false);
    }
    return true;
}

bool MakeExtrudedSheet::setDirection(wy3d::ExtrusionDirection direction)
{
    if (!_pDb || !_pTopTrans || !_pExtrudedSheet || _isFinished) return false;

    _direction = direction;

    wydb::TransactionManager* pTransMgr = _pDb->getTransactionManager();
    wydb::Transaction* pTrans = pTransMgr->startTransaction();
    if (!pTrans) return false;
    {
        _pExtrudedSheet->upgradeForWrite();
        _pExtrudedSheet->setDirection(direction);
    }
    if (wy::ErrorStatus::Ok == pTransMgr->endTransaction())
    {
        pTransMgr->mergeTransaction();
    }
    else
    {
        assert(false);
    }
    return true;
}

// ============================================================================
// ExtrudedSheetGuiCmd
// ============================================================================

ExtrudedSheetGuiCmd::ExtrudedSheetGuiCmd() : OsgGuiCommand(),
    _step(Step::Undefined), _sketchId(wydb::ElementId::kNull), _pickPos(), _depth(0.0),
    _direction(wy3d::ExtrusionDirection::OneSide),
    _pDepthPopup(nullptr),
    _hoverPopupState()
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

ExtrudedSheetGuiCmd::~ExtrudedSheetGuiCmd()
{
}

wyap::CmdExecution::StartResult ExtrudedSheetGuiCmd::onStart()
{
    if (!SketchUtil::hasUnusedSketch(Application::instance().getActiveDatabase()))
    {
        MessageBoxUtil::showInformation_NoAvailableSketches();
        return wyap::CmdExecution::StartResult::Rejected;
    }

    wyap::CmdExecution::StartResult ret = GuiCommand::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Sketch);
    _pointPickOption.selType = wy3d::SelectionType::Element;
    _pointPickOption.pSelFilter = std::make_shared<SingleClassSelFilter>(wy3d::Sketch::classInfo());

    const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
    wydb::ElementId sketchId(wydb::ElementId::kNull);
    if (this->isValidSketchSelectionSet(ss, sketchId) && !sketchId.isNull())
    {
        _sketchId = sketchId;
        _pickPos = SketchUtil::getSketchOrigin(Application::instance().getActiveDatabase(), _sketchId);
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();
        this->finishStep(Step::SelectSketch);
    }
    else
    {
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();
        this->gotoStep(Step::SelectSketch);
    }

    return wyap::CmdExecution::StartResult::Succeeded;
}

void ExtrudedSheetGuiCmd::onEnd()
{
    this->hidePopup();
    GuiCommand::onEnd();
    if (_pMakeExtrudedSheet) _pMakeExtrudedSheet = nullptr;
}

void ExtrudedSheetGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    this->hidePopup();
    GuiCommand::onAbort(cause);
    if (_pMakeExtrudedSheet) _pMakeExtrudedSheet = nullptr;
}

void ExtrudedSheetGuiCmd::reset()
{
    this->cleanup();
}

void ExtrudedSheetGuiCmd::cleanup()
{
    this->hidePopup();
    _step = Step::Undefined;
    _sketchId = wydb::ElementId::kNull;
    _pickPos.set(0.0, 0.0, 0.0);
    _depth = 0.0;
    _direction = wy3d::ExtrusionDirection::OneSide;
    _pValidSketchPreview = nullptr;
    _pInvalidSketchTooltip = nullptr;
    _hoverPopupState.resetValue();
    _pMakeExtrudedSheet = nullptr;
}

bool ExtrudedSheetGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SelectSketch:
    {
        _pMakeExtrudedSheet = std::make_shared<MakeExtrudedSheet>(this);
        unsigned int errorCode(0);
        if (!_pMakeExtrudedSheet->init(_sketchId, errorCode))
        {
            _pValidSketchPreview = nullptr;
            _pInvalidSketchTooltip = nullptr;
            _pMakeExtrudedSheet = nullptr;
            if (0 != errorCode) MessageBoxUtil::showError(errorCode);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }
        _pValidSketchPreview = nullptr;
        _pInvalidSketchTooltip = nullptr;
        this->gotoStep(Step::SpecifyDepth);
        return true;
    }
    break;

    case Step::SpecifyDepth:
    {
        if (_pMakeExtrudedSheet)
        {
            if (!_pMakeExtrudedSheet->update(_depth)) return false;
            _pMakeExtrudedSheet->commit();
            _pMakeExtrudedSheet = nullptr;
        }
        this->requestEnd();
        return true;
    }
    break;

    default:
    {
        assert(false);
    }
    break;
    }

    return false;
}

void ExtrudedSheetGuiCmd::gotoStep(Step step)
{
    _step = step;
    this->hidePopup();
    _hoverPopupState.resetValue();

    switch (step)
    {
    case Step::SelectSketch:
    {
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("ExtrudedSheetGuiCmd",
            "Select the sketch to extrude."));
        Application::instance().setCursor(CursorType::SelectElements);
    }
    break;

    case Step::SpecifyDepth:
    {
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("ExtrudedSheetGuiCmd",
            "Specify the extrusion depth; you can directly input the value. Press Tab to switch the direction (One Side / Symmetric)."));
        Application::instance().setCursor(CursorType::Locate);
    }
    break;

    default:
    {
        Application::instance().getStatusBar()->setTips("");
        Application::instance().setCursor(CursorType::Select);
        assert(false);
    }
    break;
    }
}

void ExtrudedSheetGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (event.x != _hoverPopupState.lastMouseX ||
        event.y != _hoverPopupState.lastMouseY)
    {
        this->hidePopup();
        _hoverPopupState.lastMouseX = event.x;
        _hoverPopupState.lastMouseY = event.y;
        _hoverPopupState.lastMouseMoveTime = event.time;
    }

    if (_step == Step::SelectSketch)
    {
        std::pair<wydb::ElementId, wy::Vector3> pickRet = this->pointPickElement(event.x, event.y, _pointPickOption);
        wydb::ElementId pickedSketchId = pickRet.first;
        _pickPos = pickRet.second;

        if (!pickedSketchId.isNull())
            preview(pickedSketchId);
        else
            _pValidSketchPreview = nullptr;

        if (!pickedSketchId.isNull() && !_pValidSketchPreview)
        {
            if (!_pInvalidSketchTooltip || _pInvalidSketchTooltip->getSketchId() != pickedSketchId)
            {
                _pInvalidSketchTooltip = std::make_shared<InvalidSketchToolTip>(pickedSketchId,
                    _sketchId2ValidInfo[pickedSketchId].error);
            }
            Application::instance().setCursor(CursorType::Forbid);
        }
        else
        {
            _pInvalidSketchTooltip = nullptr;
            Application::instance().setCursor(CursorType::SelectElements);
        }
    }
    else if (_step == Step::SpecifyDepth)
    {
        double height(0.0);
        if (this->computeHeight(event.x, event.y, _pickPos, height, _pMakeExtrudedSheet.get()))
        {
            if (wy3d::ExtrusionDirection::Symmetric == _direction)
            {
                // 对称拉伸下鼠标所在侧不决定方向;鼠标定位的是体的远端,
                // 所以总深度 = 2 * 鼠标到草图面的距离
                _hoverPopupState.depthSign = 1;
                _hoverPopupState.depth = 2.0 * std::fabs(height);
                if (_pMakeExtrudedSheet) _pMakeExtrudedSheet->update(_hoverPopupState.depth);
            }
            else
            {
                _hoverPopupState.depthSign = height < 0.0 ? -1 : 1;
                _hoverPopupState.depth = std::fabs(height);
                if (_pMakeExtrudedSheet) _pMakeExtrudedSheet->update(height);
            }
        }
    }

    return;
}

void ExtrudedSheetGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    this->hidePopup();
    _hoverPopupState.lastMouseX = event.x;
    _hoverPopupState.lastMouseY = event.y;
    _hoverPopupState.lastMouseMoveTime = event.time;

    if (_step == Step::SelectSketch)
    {
    }
    else if (_step == Step::SpecifyDepth)
    {
        double height(0.0);
        if (this->computeHeight(event.x, event.y, _pickPos, height, _pMakeExtrudedSheet.get()))
        {
            _depth = (wy3d::ExtrusionDirection::Symmetric == _direction) ? 2.0 * std::fabs(height) : height;
            if (this->finishStep(_step))
            {
                this->simulateMouseMoveFromPopup();
            }
        }
        else
        {
            assert(false);
        }
    }

    return;
}

void ExtrudedSheetGuiCmd::onLeftMouseUp(const MouseEvent& event)
{
    if (_step == Step::SelectSketch)
    {
        if (_pValidSketchPreview)
        {
            _sketchId = _pValidSketchPreview->getSketchId();
            this->finishStep(_step);
        }
    }

    return;
}

void ExtrudedSheetGuiCmd::onFeatureTreeItemClicked(const wydb::ElementId& id)
{
    if (Step::SelectSketch != _step) return;
    if (id.isNull()) return;

    QString error;
    if (!isValidSketch(id, error)) return;

    _sketchId = id;
    _pickPos = SketchUtil::getSketchOrigin(Application::instance().getActiveDatabase(), _sketchId);
    Application::instance().getSelManager()->beginChange();
    Application::instance().getSelManager()->clearSelections();
    Application::instance().getSelManager()->endChange();
    this->finishStep(Step::SelectSketch);
}

void ExtrudedSheetGuiCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

bool ExtrudedSheetGuiCmd::isValidSketchSelectionSet(const wyap::SelectionSet& ss, wydb::ElementId& sketchId)
{
    sketchId = wydb::ElementId::kNull;

    if (ss.getCount() != 1) return false;
    const wyap::Selection& sel = ss.createIterator().current();
    if (sel.getSelectionType() != static_cast<unsigned int>(wy3d::SelectionType::Element)) return false;
    wydb::ElementId id = sel.getElementId();
    if (id.isNull()) return false;

    QString error;
    if (!isValidSketch(id, error)) return false;
    sketchId = id;
    return true;
}

bool ExtrudedSheetGuiCmd::isValidSketch(const wydb::ElementId& sketchId, QString& error)
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return false;
    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(sketchId));
    if (!pSketch) { error = "Sketch not found"; return false; }
    if (!pSketch->getParent().isNull()) { error = "Sketch is already in use"; return false; }

    wy3d::SketchProfileForSheet profileForSheet(pSketch);
    if (profileForSheet.check()) return true;

    std::shared_ptr<wy3d::SketchError> pError = profileForSheet.getError();
    if (pError)
        error = ErrorCodeTranslation::instance().getErrorCodeDescription(pError->type);
    else
        error = "Invalid sketch profile";
    return false;
}

void ExtrudedSheetGuiCmd::preview(wydb::ElementId sketchId)
{
    if (sketchId.isNull()) return;
    if (_sketchId2ValidInfo.find(sketchId) == _sketchId2ValidInfo.cend())
    {
        QString error;
        _sketchId2ValidInfo[sketchId].valid = isValidSketch(sketchId, error);
        _sketchId2ValidInfo[sketchId].error = error;
    }

    if (_sketchId2ValidInfo[sketchId].valid)
    {
        if (!_pValidSketchPreview || _pValidSketchPreview->getSketchId() != sketchId)
        {
            _pValidSketchPreview = std::make_shared<ValidSketchTransient>(sketchId);
        }
    }
    else
    {
        _pValidSketchPreview = nullptr;
    }
}

void ExtrudedSheetGuiCmd::initializePopups()
{
    MainWindow* pMainWindow = Application::instance().getMainWindow();
    if (!_pDepthPopup)
    {
        _pDepthPopup = std::make_unique<GuiCmdHoverInputPopup2_2ndTabLabel>(
            QCoreApplication::translate("ExtrudedSheetGuiCmd", "Depth"),
            QCoreApplication::translate("ExtrudedSheetGuiCmd", "Direction"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pDepthPopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pDepthPopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pDepthPopup->setDirectionToggleHandler([this]()
        {
            // Tab toggles the direction; the preview updates immediately
            _direction = (wy3d::ExtrusionDirection::Symmetric == _direction)
                ? wy3d::ExtrusionDirection::OneSide
                : wy3d::ExtrusionDirection::Symmetric;
            if (wy3d::ExtrusionDirection::Symmetric == _direction)
            {
                _hoverPopupState.depthSign = 1;
            }
            if (_pMakeExtrudedSheet)
            {
                _pMakeExtrudedSheet->setDirection(_direction);
                _pMakeExtrudedSheet->update(_hoverPopupState.depth);
            }
            this->updateDirectionLabel();
        });
        this->updateDirectionLabel();
        _pDepthPopup->hide();
    }
}

void ExtrudedSheetGuiCmd::showPopup()
{
    if (_step != Step::SpecifyDepth) return;
    if (!_pDepthPopup) this->initializePopups();
    if (!_pDepthPopup) return;
    _pDepthPopup->setValue(_hoverPopupState.depth);
    this->updateDirectionLabel();
    _pDepthPopup->showAtGlobal(QCursor::pos());
}

void ExtrudedSheetGuiCmd::updateDirectionLabel()
{
    if (!_pDepthPopup)
    {
        return;
    }
    const QString directionText = (wy3d::ExtrusionDirection::Symmetric == _direction)
        ? QCoreApplication::translate("ExtrudedSheetGuiCmd", "Symmetric")
        : QCoreApplication::translate("ExtrudedSheetGuiCmd", "One Side");
    _pDepthPopup->setDirectionLabel(directionText);
}

void ExtrudedSheetGuiCmd::hidePopup()
{
    if (_pDepthPopup && _pDepthPopup->isVisible())
    {
        _pDepthPopup->hide();
    }
}

void ExtrudedSheetGuiCmd::tryShowPopupOnHover(double time)
{
    if (_step != Step::SpecifyDepth) return;
    if (_hoverPopupState.lastMouseMoveTime < 0.0) return;
    if (_pDepthPopup && _pDepthPopup->isVisible()) return;
    if (time - _hoverPopupState.lastMouseMoveTime >= kHoverPopupDelaySeconds)
    {
        this->showPopup();
    }
}

void ExtrudedSheetGuiCmd::onPopupEnterKey()
{
    if (_step != Step::SpecifyDepth || !_pDepthPopup) return;

    double depth(0.0);
    if (!parseDoubleText(_pDepthPopup->getRowText(), depth)) return;
    _depth = (wy3d::ExtrusionDirection::Symmetric == _direction)
        ? std::fabs(depth)
        : (_hoverPopupState.depthSign < 0 ? -depth : depth);

    if (this->finishStep(_step))
    {
        this->simulateMouseMoveFromPopup();
    }
}

void ExtrudedSheetGuiCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void ExtrudedSheetGuiCmd::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX)
        return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}
