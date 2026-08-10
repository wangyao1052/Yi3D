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

#include "RevolvedSheetGuiCmd.h"

#include <QCoreApplication>
#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapDocManager.h>
#include <wyapDocument.h>
#include <wy3dSketch.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dImpl.h>
#include <wy3dSketchProfileForSheet.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>

#include "application/Application.h"
#include "scene/Scene.h"
#include "scene/nodes/ElementNode.h"
#include "utils/SketchUtil.h"
#include "utils/MessageBoxUtil.h"
#include "utils/GuiCommandUtil.h"
#include "select/filters/CommonSelFilters.h"
#include "translation/ErrorCodeTranslation.h"

RevolvedSheetGuiCmd::RevolvedSheetGuiCmd() : OsgGuiCommand(),
    _step(Step::Undefined), _sketchId(wydb::ElementId::kNull), _axisCurveId(wydb::ElementId::kNull)
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

RevolvedSheetGuiCmd::~RevolvedSheetGuiCmd()
{
}

wyap::CmdExecution::StartResult RevolvedSheetGuiCmd::onStart()
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
        this->clearSelections();
        this->finishStep(Step::SelectSketch);
    }
    else
    {
        this->clearSelections();
        this->gotoStep(Step::SelectSketch);
    }

    return wyap::CmdExecution::StartResult::Succeeded;
}

void RevolvedSheetGuiCmd::onEnd()
{
    GuiCommand::onEnd();
}

void RevolvedSheetGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    GuiCommand::onAbort(cause);
}

void RevolvedSheetGuiCmd::cleanup()
{
    _pMakeRevolvedSheet = nullptr;

    _step = Step::Undefined;
    _sketchId = wydb::ElementId::kNull;
    _axisCurveId = wydb::ElementId::kNull;
    _pValidSketch = nullptr;
    _pInvalidSketchTooltip = nullptr;
    _pAxisCurvePreview = nullptr;
}

void RevolvedSheetGuiCmd::reset()
{
    this->cleanup();
}

bool RevolvedSheetGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SelectSketch:
    {
        assert(!_sketchId.isNull());
        _pInvalidSketchTooltip = nullptr;
        _pMakeRevolvedSheet = std::make_shared<MakeRevolvedSheet>(this);
        this->gotoStep(Step::SelectAxisCurve);
        return true;
    }
    break;

    case Step::SelectAxisCurve:
    {
        assert(!_sketchId.isNull());
        assert(!_axisCurveId.isNull());
        _pAxisCurvePreview = nullptr;
        if (_pMakeRevolvedSheet)
        {
            unsigned int errorCode(0);
            if (!_pMakeRevolvedSheet->create(_sketchId, _axisCurveId, errorCode))
            {
                _pMakeRevolvedSheet = nullptr;
                if (0 != errorCode)
                {
                    MessageBoxUtil::showError(errorCode);
                }
                this->requestAbort(AbortCause::ErrorTerminate);
                return false;
            }
            _pMakeRevolvedSheet->commit();
            _pMakeRevolvedSheet = nullptr;
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

void RevolvedSheetGuiCmd::gotoStep(Step step)
{
    _step = step;

    switch (step)
    {
    case Step::SelectSketch:
    {
        this->clearSelections();

        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("RevolvedSheetGuiCmd",
            "Select the sketch to revolve."));
        Application::instance().setCursor(CursorType::SelectElements);
    }
    break;

    case Step::SelectAxisCurve:
    {
        this->clearSelections();

        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("RevolvedSheetGuiCmd",
            "Select an axis line in the sketch."));
        Application::instance().setCursor(CursorType::SelectElements);

        _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Sketch);
        _pointPickOption.selType = wy3d::SelectionType::SketchCurve;
        _pointPickOption.pSelFilter = nullptr;
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

void RevolvedSheetGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (_step == Step::SelectSketch)
    {
        std::pair<wydb::ElementId, wy::Vector3> pickRet = this->pointPickElement(event.x, event.y, _pointPickOption);
        wydb::ElementId pickedSketchId = pickRet.first;

        if (!pickedSketchId.isNull())
            preview(pickedSketchId);
        else
            _pValidSketch = nullptr;

        if (!pickedSketchId.isNull() && !_pValidSketch)
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
    else if (_step == Step::SelectAxisCurve)
    {
        this->mouseMovePointPickPreview(event.x, event.y, _pointPickOption, _pAxisCurvePreview);
    }
}

void RevolvedSheetGuiCmd::onLeftMouseUp(const MouseEvent& event)
{
    if (_step == Step::SelectSketch)
    {
        if (_pValidSketch)
        {
            _sketchId = _pValidSketch->getSketchId();
            this->finishStep(_step);
        }
    }
    else if (_step == Step::SelectAxisCurve)
    {
        if (_pAxisCurvePreview)
        {
            const wyap::Selection& sel = _pAxisCurvePreview->getSelection();
            _axisCurveId = wydb::ElementId(static_cast<std::uint64_t>(std::stoul(sel.getSubPath())));
            this->finishStep(_step);
        }
    }
}

void RevolvedSheetGuiCmd::onFeatureTreeItemClicked(const wydb::ElementId& id)
{
    if (Step::SelectSketch != _step) return;
    if (id.isNull()) return;

    QString error;
    if (this->isValidSketch(id, error))
    {
        _sketchId = id;
        this->clearSelections();
        this->finishStep(Step::SelectSketch);
    }
}

bool RevolvedSheetGuiCmd::isValidSketchSelectionSet(const wyap::SelectionSet& ss, wydb::ElementId& sketchId)
{
    sketchId = wydb::ElementId::kNull;
    if (ss.getCount() != 1) return false;

    const wyap::Selection& sel = ss.createIterator().current();
    if (sel.getSelectionType() != static_cast<unsigned int>(wy3d::SelectionType::Element)) return false;

    wydb::ElementId id = sel.getElementId();
    QString error;
    if (this->isValidSketch(id, error))
    {
        sketchId = id;
        return true;
    }
    return false;
}

bool RevolvedSheetGuiCmd::isValidSketch(const wydb::ElementId& sketchId, QString& error)
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

void RevolvedSheetGuiCmd::preview(wydb::ElementId sketchId)
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
        if (!_pValidSketch || _pValidSketch->getSketchId() != sketchId)
        {
            _pValidSketch = std::make_shared<ValidSketchTransient>(sketchId);
        }
    }
    else
    {
        _pValidSketch = nullptr;
    }
}

void RevolvedSheetGuiCmd::clearSelections()
{
    Application::instance().getSelManager()->beginChange();
    Application::instance().getSelManager()->clearSelections();
    Application::instance().getSelManager()->endChange();
}

// ============================================================================
// MakeRevolvedSheet
// ============================================================================

void MakeRevolvedSheet::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pRevolvedSheet) idSet.insert(_pRevolvedSheet->getId());
}

bool MakeRevolvedSheet::create(const wydb::ElementId& sketchId, const wydb::ElementId& axisCurveId, unsigned int& errorCode)
{
    errorCode = 0;
    if (!_pDb || !_pTopTrans || _pRevolvedSheet || _isFinished)
        return false;
    if (sketchId.isNull() || axisCurveId.isNull())
        return false;

    const wy3d::Sketch* pConstSketch = wy3d::Sketch::cast(_pDb->getElement(sketchId));
    if (!pConstSketch) return false;
    if (!pConstSketch->getParent().isNull()) return false;

    const wy3d::SketchCurve* pAxis = wy3d::SketchCurve::cast(_pDb->getElement(axisCurveId));
    if (!pAxis)
    {
        errorCode = static_cast<unsigned int>(wy3d::ErrorCode::REVOLUTION_NoRevolutionAxisLine);
        return false;
    }

    wy3d::RevolvedSheet* pSheet = nullptr;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction();
    if (!pTrans) return false;
    wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
    if (!pSketch) { assert(false); goto ABORT_TRANS; }

    if (wy::ErrorStatus::Ok != wy3d::RevolvedSheet::create(pTrans, pSketch, pAxis, 0.0, wy3d::TWO_PI, pSheet) || !pSheet)
    {
        assert(false);
        goto ABORT_TRANS;
    }
    _pDb->getTransactionManager()->endTransaction();
    _pRevolvedSheet = pSheet;
    errorCode = wy3d::getErrorCodeFromChainUpdateFeedback(
        _pDb->getTransactionManager()->getChainUpdateFeedback(pSheet->getId()).get());
    if (errorCode != 0) return false;
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    _pRevolvedSheet = nullptr;
    return false;
}
