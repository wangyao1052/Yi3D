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

#include "FilledSheetGuiCmd.h"

#include <algorithm>
#include <cassert>
#include <map>
#include <QCursor>
#include <QCoreApplication>

#include <TopoDS.hxx>
#include <TopoDS_Shape.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopExp.hxx>

#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapSelManager.h>
#include <wy3dSelectionType.h>
#include <wy3dSketch.h>
#include <wy3dSketch3D.h>
#include <wy3dFilledSheet.h>
#include <wy3dSolid.h>
#include <wy3dSheet.h>
#include <wy3dDefaultChainUpdateFeedback.h>

#include "application/Application.h"
#include "scene/Scene.h"
#include "scene/nodes/ElementNode.h"
#include "scene/nodes/ElementNodeType.h"
#include "utils/SketchUtil.h"
#include "utils/MessageBoxUtil.h"
#include "select/filters/CommonSelFilters.h"
#include "wy3d/topo/TopoShapeUtil.h"

// ============================================================================
// MakeFilledSheet
// ============================================================================

void MakeFilledSheet::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pFilledSheet) idSet.insert(_pFilledSheet->getId());
}

bool MakeFilledSheet::init(const wydb::ElementId& sketchId, unsigned int& errorCode)
{
    errorCode = 0;
    if (!_pDb || !_pTopTrans || _pFilledSheet || _isFinished)
        return false;
    if (sketchId.isNull())
        return false;

    const wydb::Element* pElem = _pDb->getElement(sketchId);
    if (!pElem) return false;
    const wy3d::Sketch* pConstSketch = wy3d::Sketch::cast(pElem);
    const wy3d::Sketch3D* pConstSketch3D = wy3d::Sketch3D::cast(pElem);
    if (!pConstSketch && !pConstSketch3D) return false;
    if (pConstSketch && !pConstSketch->getParent().isNull()) return false;
    if (pConstSketch3D && !pConstSketch3D->getParent().isNull()) return false;

    wy3d::FilledSheet* pSheet = nullptr;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction();
    if (!pTrans) return false;

    if (pConstSketch)
    {
        wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
        if (!pSketch)
        {
            assert(false);
            goto ABORT_TRANS;
        }
        if (wy::ErrorStatus::Ok != wy3d::FilledSheet::create(pTrans, pSketch, pSheet) || !pSheet)
        {
            assert(false);
            goto ABORT_TRANS;
        }
    }
    else
    {
        wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(sketchId));
        if (!pSketch3D)
        {
            assert(false);
            goto ABORT_TRANS;
        }
        if (wy::ErrorStatus::Ok != wy3d::FilledSheet::create(pTrans, pSketch3D, pSheet) || !pSheet)
        {
            assert(false);
            goto ABORT_TRANS;
        }
    }
    _pDb->getTransactionManager()->endTransaction();
    _pFilledSheet = pSheet;
    errorCode = wy3d::getErrorCodeFromChainUpdateFeedback(
        _pDb->getTransactionManager()->getChainUpdateFeedback(pSheet->getId()).get());
    if (errorCode != 0) return false;
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    _pFilledSheet = nullptr;
    return false;
}

// One constraint sketch in or out. The change happens in its own sub-transaction merged back into
// the command's transaction group, so the command stays one undo step and rebuilds once
bool MakeFilledSheet::changeConstraintSketch(const wydb::ElementId& constraintSketchId, bool add)
{
    if (!_pDb || !_pTopTrans || !_pFilledSheet || _isFinished) return false;

    wydb::TransactionManager* pTransMgr = _pDb->getTransactionManager();
    wydb::Transaction* pTrans = pTransMgr->startTransaction();
    if (!pTrans) return false;
    {
        if (wy::ErrorStatus::Ok != _pFilledSheet->upgradeForWrite())
        {
            assert(false);
            pTransMgr->abortTransaction();
            return false;
        }
        wydb::Element* pConstraintElem = pTrans->getElementForWrite(constraintSketchId);
        if (!pConstraintElem)
        {
            assert(false);
            pTransMgr->abortTransaction();
            return false;
        }

        // The constraint sketch is a whole sketch element (2D or 3D), dispatched to the core by its
        // type. Not acceptable (already taken by another feature, say): keep the set as it was
        wy::ErrorStatus error(wy::ErrorStatus::InvalidInput);
        if (wy3d::Sketch* pSketch = wy3d::Sketch::cast(pConstraintElem))
        {
            error = add ? _pFilledSheet->addConstraintSketch(pSketch)
                        : _pFilledSheet->removeConstraintSketch(pSketch);
        }
        else if (wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pConstraintElem))
        {
            error = add ? _pFilledSheet->addConstraintSketch(pSketch3D)
                        : _pFilledSheet->removeConstraintSketch(pSketch3D);
        }
        if (wy::ErrorStatus::Ok != error)
        {
            pTransMgr->abortTransaction();
            return false;
        }
    }
    if (wy::ErrorStatus::Ok != pTransMgr->endTransaction())
    {
        assert(false);
        return false;
    }
    pTransMgr->mergeTransaction();
    return true;
}

unsigned int MakeFilledSheet::getChainUpdateErrorCode() const
{
    if (!_pDb || !_pFilledSheet) return 0;
    return wy3d::getErrorCodeFromChainUpdateFeedback(
        _pDb->getTransactionManager()->getChainUpdateFeedback(_pFilledSheet->getId()).get());
}

bool MakeFilledSheet::addConstraintSketch(const wydb::ElementId& constraintSketchId, unsigned int& errorCode)
{
    errorCode = 0;
    if (!_pFilledSheet || constraintSketchId.isNull()) return false;

    const std::vector<wydb::ElementId>& constraintSketchIds = _pFilledSheet->getConstraintSketches();
    if (constraintSketchIds.cend() != std::find(constraintSketchIds.cbegin(), constraintSketchIds.cend(), constraintSketchId))
    {
        return true;
    }

    if (!this->changeConstraintSketch(constraintSketchId, true)) return false;

    // A constraint sketch is a soft constraint: it may leave the surface unbuildable. Drop it
    // again when it does, so the feature keeps the shape it had before the click
    errorCode = this->getChainUpdateErrorCode();
    if (0 != errorCode)
    {
        if (!this->changeConstraintSketch(constraintSketchId, false)) assert(false);
        return false;
    }
    return true;
}

bool MakeFilledSheet::removeConstraintSketch(const wydb::ElementId& constraintSketchId, unsigned int& errorCode)
{
    errorCode = 0;
    if (!_pFilledSheet || constraintSketchId.isNull()) return false;

    const std::vector<wydb::ElementId>& constraintSketchIds = _pFilledSheet->getConstraintSketches();
    if (constraintSketchIds.cend() == std::find(constraintSketchIds.cbegin(), constraintSketchIds.cend(), constraintSketchId))
    {
        return true;
    }

    if (!this->changeConstraintSketch(constraintSketchId, false)) return false;

    // Dropping one can unbuild the surface as well, so put it back when it does
    errorCode = this->getChainUpdateErrorCode();
    if (0 != errorCode)
    {
        if (!this->changeConstraintSketch(constraintSketchId, true)) assert(false);
        return false;
    }
    return true;
}

bool MakeFilledSheet::clearConstraintSketches(unsigned int& errorCode)
{
    errorCode = 0;
    if (!_pFilledSheet) return false;

    // Each removal changes the set being walked, so the walk works off a copy
    const std::vector<wydb::ElementId> constraintSketchIds = _pFilledSheet->getConstraintSketches();
    for (const wydb::ElementId& constraintSketchId : constraintSketchIds)
    {
        if (!this->removeConstraintSketch(constraintSketchId, errorCode)) return false;
    }
    return true;
}

// ============================================================================
// FilledSheetGuiCmd
// ============================================================================

FilledSheetGuiCmd::FilledSheetGuiCmd() : OsgGuiCommand(),
    _step(Step::Undefined), _sketchId(wydb::ElementId::kNull)
{
    _options.pointSelect = false;
    _options.boxSelect = false;

    _pSelSetHighlightor = std::make_shared<SelectionSetHighlightor>();
}

FilledSheetGuiCmd::~FilledSheetGuiCmd()
{
}

wyap::CmdExecution::StartResult FilledSheetGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = GuiCommand::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    _sketchPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Sketch) |
        static_cast<unsigned int>(ElementNodeType::Sketch3D);
    _sketchPickOption.selType = wy3d::SelectionType::Element;
    _sketchPickOption.pSelPreFilter = std::make_shared<CommonPreSelFilterForPointPick>(
        wy3d::Sketch::classInfo(), wy3d::Sketch3D::classInfo());
    _sketchPickOption.pSelFilter = std::make_shared<MultiClassSelFilter>(
        std::vector<wyrx::ClassInfo*>{ wy3d::Sketch::classInfo(), wy3d::Sketch3D::classInfo() });

    _edgePickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Solid) |
        static_cast<unsigned int>(ElementNodeType::Sheet);
    _edgePickOption.selType = wy3d::SelectionType::Edge;
    _edgePickOption.acceptElement = false;

    const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
    std::vector<wydb::ElementId> pickedSketchIds;
    if (this->collectPickedSketches(ss, pickedSketchIds))
    {
        // Pick-first with a preselected sketch: the first one that can serve as a boundary
        // becomes the boundary, every other one is kept for the constraint set
        wydb::ElementId sketchId(wydb::ElementId::kNull);
        std::vector<wydb::ElementId> constraintSketchIds;
        for (const wydb::ElementId& id : pickedSketchIds)
        {
            QString error;
            if (sketchId.isNull() && this->isValidBoundarySketch(id, error))
            {
                sketchId = id;
            }
            else
            {
                constraintSketchIds.emplace_back(id);
            }
        }

        if (!sketchId.isNull())
        {
            _sketchId = sketchId;
            _preselectedConstraintSketchIds = constraintSketchIds;
            Application::instance().getSelManager()->beginChange();
            Application::instance().getSelManager()->clearSelections();
            Application::instance().getSelManager()->endChange();
            this->finishStep(Step::SelectSketch);
            return wyap::CmdExecution::StartResult::Succeeded;
        }
    }

    {
        // pick-first: a preselected edge
        bool edgePicked = false;
        if (ss.getCount() == 1)
        {
            const wyap::Selection& sel = ss.createIterator().current();
            if (sel.getSelectionType() == static_cast<unsigned int>(wy3d::SelectionType::Edge) &&
                !sel.getElementId().isNull())
            {
                _pSelSetHighlightor->addSelection(sel);
                edgePicked = true;
            }
        }

        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        if (edgePicked)
        {
            this->gotoStep(Step::SelectEdges);
        }
        else
        {
            this->gotoStep(Step::SelectSketch);
        }
    }

    return wyap::CmdExecution::StartResult::Succeeded;
}

void FilledSheetGuiCmd::cleanup()
{
    this->releaseConstraintSketches();
    _step = Step::Undefined;
    _sketchId = wydb::ElementId::kNull;
    _pValidSketchPreview = nullptr;
    _pInvalidSketchTooltip = nullptr;
    _pEdgePreview = nullptr;
    _pConstraintSketchPreview = nullptr;
    if (_pSelSetHighlightor) _pSelSetHighlightor->clearSelections();
    _edgePickOption.pSelPreFilter = nullptr;
    _constraintSketchPickOption.pSelPreFilter = nullptr;
    _preselectedConstraintSketchIds.clear();
    _constraintSketchId2ValidInfo.clear();
    _pMakeFilledSheet = nullptr;
    _pMakeNonParametricSheet = nullptr;
}

// The edge path is created by Enter, Spacebar or the context menu rather than on the pick that
// closes the loop: what the picked edges amount to is only asked here, so the status bar stays
// free of a running verdict
bool FilledSheetGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SelectSketch:
    {
        _pMakeFilledSheet = std::make_shared<MakeFilledSheet>(this);
        unsigned int errorCode(0);
        if (!_pMakeFilledSheet->init(_sketchId, errorCode))
        {
            _pValidSketchPreview = nullptr;
            _pInvalidSketchTooltip = nullptr;
            _pMakeFilledSheet = nullptr;
            if (0 != errorCode) MessageBoxUtil::showError(errorCode);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }
        _pValidSketchPreview = nullptr;
        _pInvalidSketchTooltip = nullptr;

        // Either boundary can still take constraint sketches, so the command stays here
        this->gotoStep(Step::SelectConstraintCurves);
        return true;
    }
    break;

    case Step::SelectConstraintCurves:
    {
        if (!_pMakeFilledSheet)
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }
        _pConstraintSketchPreview = nullptr;
        _pMakeFilledSheet->commit();
        _pMakeFilledSheet = nullptr;
        this->requestEnd();
        return true;
    }
    break;

    case Step::SelectEdges:
    {
        std::vector<TopoDS_Edge> edges;
        if (!this->collectPickedEdges(edges))
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }

        // The one place the picked edges are judged: the pick path only collects them
        TopoDS_Shape sheetShape;
        wy3d::ErrorCode shapeError = wy3d::TopoShapeUtil::makeFilledSheetFromEdges(edges, sheetShape);
        if (wy3d::ErrorCode::NoError != shapeError)
        {
            MessageBoxUtil::showError(static_cast<unsigned int>(shapeError));
            return false;
        }

        _pMakeNonParametricSheet = std::make_shared<MakeNonParametricSheet>(this);
        unsigned int errorCode(0);
        if (!_pMakeNonParametricSheet->init(sheetShape, errorCode))
        {
            _pEdgePreview = nullptr;
            _pMakeNonParametricSheet = nullptr;
            if (0 != errorCode) MessageBoxUtil::showError(errorCode);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }
        _pEdgePreview = nullptr;
        _pMakeNonParametricSheet->commit();
        _pMakeNonParametricSheet = nullptr;
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

void FilledSheetGuiCmd::gotoStep(Step step)
{
    _step = step;
    // _pSelSetHighlightor is deliberately left alone: both ways into SelectEdges
    // (pick-first and the first edge click) carry the picked edges in it

    switch (step)
    {
    case Step::SelectSketch:
    {
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        _pValidSketchPreview = nullptr;
        _pInvalidSketchTooltip = nullptr;
        _pEdgePreview = nullptr;

        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("FilledSheetGuiCmd",
            "Select the boundary of the filled surface: a 2D or 3D sketch, or edges of a solid or sheet."));
        Application::instance().setCursor(CursorType::SelectElements);
    }
    break;

    case Step::SelectEdges:
    {
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        _pValidSketchPreview = nullptr;
        _pInvalidSketchTooltip = nullptr;
        _pEdgePreview = nullptr;

        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("FilledSheetGuiCmd",
            "Select edges to enclose one or more loops; press Enter or Spacebar to confirm; press Esc to clear the edges."));
        Application::instance().setCursor(CursorType::SelectElements);
    }
    break;

    case Step::SelectConstraintCurves:
    {
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        _pValidSketchPreview = nullptr;
        _pInvalidSketchTooltip = nullptr;
        _pEdgePreview = nullptr;
        _pConstraintSketchPreview = nullptr;

        // Constraint sketches can be 2D or 3D, the boundary sketch itself excluded
        _constraintSketchPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Sketch) |
            static_cast<unsigned int>(ElementNodeType::Sketch3D);
        _constraintSketchPickOption.selType = wy3d::SelectionType::Element;
        _constraintSketchPickOption.pSelPreFilter = std::make_shared<CommonPreSelFilterForPointPick>(
            wy3d::Sketch::classInfo(), wy3d::Sketch3D::classInfo(), _sketchId);
        _constraintSketchPickOption.pSelFilter = std::make_shared<MultiClassSelFilter>(
            std::vector<wyrx::ClassInfo*>{ wy3d::Sketch::classInfo(), wy3d::Sketch3D::classInfo() });

        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("FilledSheetGuiCmd",
            "Select constraint curves (optional); press Enter or Spacebar to complete."));
        Application::instance().setCursor(CursorType::SelectElements);

        // The preselected sketches are only recognizable here; they are added before the
        // highlight set is built
        const std::vector<wydb::ElementId> preselectedIds = _preselectedConstraintSketchIds;
        _preselectedConstraintSketchIds.clear();
        for (const wydb::ElementId& id : preselectedIds) this->toggleConstraintSketch(id);
        this->rebuildHighlights();
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

// The highlight set is the boundary sketch plus every constraint sketch
void FilledSheetGuiCmd::rebuildHighlights()
{
    wyap::SelectionSet ss;
    if (!_sketchId.isNull()) ss.add(wyap::Selection(_sketchId));
    for (const wydb::ElementId& constraintSketchId : this->getConstraintSketchIds())
    {
        ss.add(wyap::Selection(constraintSketchId));
    }
    this->activateConstraintSketches();
    // Release the old one first: its destructor un-highlights everything in its set, so
    // letting it die after the new object is built would wipe the colors just applied
    _pSelSetHighlightor = nullptr;
    _pSelSetHighlightor = std::make_shared<SelectionSetHighlightor>(ss);
}

// A sketch owned by a feature is Inactive, and the scene neither draws nor picks an inactive
// node: the command keeps its constraint sketches active as well as highlighted, so that the
// click taking a constraint out lands on something the pick can reach
void FilledSheetGuiCmd::activateConstraintSketches()
{
    Scene* pScene = Application::instance().getActiveScene();
    if (!pScene) return;

    for (const wydb::ElementId& constraintSketchId : this->getConstraintSketchIds())
    {
        ElementNode* pElemNode = pScene->getElementNode(constraintSketchId);
        if (!pElemNode) continue;
        if (!pElemNode->isActive()) _activatedConstraintSketchIds.emplace_back(constraintSketchId);
        pElemNode->setActive(true);
    }
}

// The active state belongs to the model, so it is given back rather than cleared: a sketch that
// the finished feature still owns goes back to inactive, one that came out of the set is a
// top-level sketch again. The ids are kept on the side because the feature is long gone by the
// time cleanup runs
void FilledSheetGuiCmd::releaseConstraintSketches()
{
    Scene* pScene = Application::instance().getActiveScene();
    const wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (pScene && pDb)
    {
        for (const wydb::ElementId& sketchId : _activatedConstraintSketchIds)
        {
            ElementNode* pElemNode = pScene->getElementNode(sketchId);
            const wydb::Element* pElem = pDb->getElement(sketchId);
            if (pElemNode && pElem) pElemNode->setActive(pElemNode->computeWhetherActive(pElem));
        }
    }
    _activatedConstraintSketchIds.clear();
}

void FilledSheetGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (_step == Step::SelectSketch)
    {
        std::pair<wydb::ElementId, wy::Vector3> pickRet = this->pointPickElement(event.x, event.y, _sketchPickOption);
        wydb::ElementId pickedSketchId = pickRet.first;
        if (!pickedSketchId.isNull())
        {
            _pEdgePreview = nullptr;
            preview(pickedSketchId);
            if (!_pValidSketchPreview)
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
        else
        {
            _pValidSketchPreview = nullptr;
            _pInvalidSketchTooltip = nullptr;

            this->mouseMovePointPickPreview(event.x, event.y, _edgePickOption, _pEdgePreview);
            Application::instance().setCursor(CursorType::SelectElements);
        }
    }
    else if (_step == Step::SelectEdges)
    {
        this->mouseMovePointPickPreview(event.x, event.y, _edgePickOption, _pEdgePreview);
        Application::instance().setCursor(CursorType::SelectElements);
    }
    else if (_step == Step::SelectConstraintCurves)
    {
        this->mouseMovePointPickPreview(event.x, event.y, _constraintSketchPickOption, _pConstraintSketchPreview);
        if (_pConstraintSketchPreview)
        {
            const wydb::ElementId constraintSketchId = _pConstraintSketchPreview->getSelection().getElementId();
            if (!this->isConstraintSketchValid(constraintSketchId))
            {
                if (!_pInvalidSketchTooltip || _pInvalidSketchTooltip->getSketchId() != constraintSketchId)
                {
                    _pInvalidSketchTooltip = std::make_shared<InvalidSketchToolTip>(constraintSketchId,
                        _constraintSketchId2ValidInfo[constraintSketchId].error);
                }
                Application::instance().setCursor(CursorType::Forbid);
            }
            else
            {
                _pInvalidSketchTooltip = nullptr;
                Application::instance().setCursor(CursorType::SelectElements);
            }
        }
        else
        {
            _pInvalidSketchTooltip = nullptr;
            Application::instance().setCursor(CursorType::SelectElements);
        }
    }

    return;
}

void FilledSheetGuiCmd::onLeftMouseUp(const MouseEvent& event)
{
    if (_step == Step::SelectSketch)
    {
        if (_pValidSketchPreview)
        {
            _sketchId = _pValidSketchPreview->getSketchId();
            this->finishStep(_step);
        }
        else if (_pEdgePreview)
        {
            const wyap::Selection& sel = _pEdgePreview->getSelection();
            _pSelSetHighlightor->addSelection(sel);
            _pEdgePreview = nullptr;
            this->gotoStep(Step::SelectEdges);
        }
    }
    else if (_step == Step::SelectEdges)
    {
        if (_pEdgePreview)
        {
            const wyap::Selection& sel = _pEdgePreview->getSelection();
            if (_pSelSetHighlightor->containsSelection(sel))
            {
                _pSelSetHighlightor->removeSelection(sel);
            }
            else
            {
                _pSelSetHighlightor->addSelection(sel);
            }
            _pEdgePreview = nullptr;
        }
    }
    else if (_step == Step::SelectConstraintCurves)
    {
        if (_pConstraintSketchPreview)
        {
            const wydb::ElementId constraintSketchId = _pConstraintSketchPreview->getSelection().getElementId();
            _pConstraintSketchPreview = nullptr;
            this->toggleConstraintSketch(constraintSketchId);
        }
    }

    return;
}

void FilledSheetGuiCmd::onFeatureTreeItemClicked(const wydb::ElementId& id)
{
    if (Step::SelectConstraintCurves == _step)
    {
        // Clicking a sketch in the feature tree adds or removes a constraint sketch; anything else
        // (datum planes, features, curves) is ignored without a word, as the loft profile pick does
        if (id.isNull()) return;
        const wydb::Database* pDb = Application::instance().getActiveDatabase();
        const wydb::Element* pElem = pDb ? pDb->getElement(id) : nullptr;
        if (!pElem) return;
        if (!wy3d::Sketch::cast(pElem) && !wy3d::Sketch3D::cast(pElem)) return;
        this->toggleConstraintSketch(id);
        return;
    }

    if (Step::SelectSketch != _step) return;
    if (id.isNull()) return;

    QString error;
    if (!isValidBoundarySketch(id, error))
    {
        MessageBoxUtil::showWarning(error);
        return;
    }

    _sketchId = id;
    Application::instance().getSelManager()->beginChange();
    Application::instance().getSelManager()->clearSelections();
    Application::instance().getSelManager()->endChange();
    this->finishStep(Step::SelectSketch);
}

void FilledSheetGuiCmd::onEscapeKey()
{
    if (Step::SelectEdges == _step)
    {
        _pSelSetHighlightor->clearSelections();
        _pEdgePreview = nullptr;
        _edgePickOption.pSelPreFilter = nullptr;
        this->gotoStep(Step::SelectSketch);
    }
    else
    {
        GuiCommand::onEscapeKey();
    }
}

bool FilledSheetGuiCmd::isContextMenuActionVisible_CompleteSelection() const
{
    // Constraint sketches are optional, so finishing is always allowed here; the edge path has nothing
    // to finish while nothing is picked
    if (Step::SelectConstraintCurves == _step) return true;
    // Hiding it while nothing is picked keeps the entry from doing nothing at all
    return Step::SelectEdges == _step && _pSelSetHighlightor
        && !_pSelSetHighlightor->getSelectionSet().isEmpty();
}

void FilledSheetGuiCmd::onContextMenuAction_CompleteSelection()
{
    this->onEnterKey();
}

void FilledSheetGuiCmd::onEnterKey()
{
    if (Step::SelectConstraintCurves == _step)
    {
        // The constraint sketches are optional, so finishing is always allowed here
        this->finishStep(_step);
        return;
    }

    if (Step::SelectEdges != _step) return;
    // Nothing picked yet: keep the tip rather than report invalid data
    if (!_pSelSetHighlightor || _pSelSetHighlightor->getSelectionSet().isEmpty()) return;

    // A selection that cannot be read is the precondition finishStep asserts on, so such a pick
    // is answered with silence rather than by finishing
    std::vector<TopoDS_Edge> edges;
    if (!this->collectPickedEdges(edges)) return;

    // finishStep ends or aborts the command: no member access after this call
    this->finishStep(_step);
}

void FilledSheetGuiCmd::onSpaceKey()
{
    this->onEnterKey();
}

bool FilledSheetGuiCmd::isContextMenuActionVisible_ClearSelection() const
{
    if (Step::SelectConstraintCurves == _step) return this->hasConstraintSketch();
    return Step::SelectEdges == _step;
}

void FilledSheetGuiCmd::onContextMenuAction_ClearSelection()
{
    if (Step::SelectConstraintCurves == _step)
    {
        // There is no earlier step to fall back to: drop every constraint sketch and stay here
        if (!_pMakeFilledSheet) return;
        unsigned int errorCode(0);
        if (!_pMakeFilledSheet->clearConstraintSketches(errorCode))
        {
            if (0 != errorCode) MessageBoxUtil::showError(errorCode);
            return;
        }
        this->rebuildHighlights();
        return;
    }

    if (Step::SelectEdges == _step)
    {
        if (_pSelSetHighlightor)
        {
            _pSelSetHighlightor->clearSelections();
            _edgePickOption.pSelPreFilter = nullptr;
            Application::instance().getStatusBar()->setTips(QCoreApplication::translate("FilledSheetGuiCmd",
                "Select edges to enclose one or more loops; press Enter or Spacebar to confirm; press Esc to clear the edges."));
        }
    }
}

// Sketch elements out of the preselection: only element-level Sketch/Sketch3D selections count
bool FilledSheetGuiCmd::collectPickedSketches(const wyap::SelectionSet& ss, std::vector<wydb::ElementId>& ids) const
{
    ids.clear();

    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return false;

    for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
    {
        const wyap::Selection& sel = iter.current();
        if (sel.getSelectionType() != static_cast<unsigned int>(wy3d::SelectionType::Element)) return false;
        const wydb::ElementId id = sel.getElementId();
        if (id.isNull()) return false;

        const wydb::Element* pElem = pDb->getElement(id);
        if (!wy3d::Sketch::cast(pElem) && !wy3d::Sketch3D::cast(pElem)) return false;
        ids.emplace_back(id);
    }

    return !ids.empty();
}

bool FilledSheetGuiCmd::isValidBoundarySketch(const wydb::ElementId& sketchId, QString& error)
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return false;
    const wydb::Element* pElem = pDb->getElement(sketchId);
    if (!pElem) { error = QCoreApplication::translate("FilledSheetGuiCmd", "Sketch not found"); return false; }

    if (const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pElem))
    {
        if (!pSketch->getParent().isNull()) { error = QCoreApplication::translate("FilledSheetGuiCmd", "Sketch is already in use"); return false; }
        return SketchUtil::isValidProfileForFilledSheet(*pSketch, error);
    }
    if (const wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pElem))
    {
        if (!pSketch3D->getParent().isNull()) { error = QCoreApplication::translate("FilledSheetGuiCmd", "Sketch is already in use"); return false; }
        return SketchUtil::isValidProfile3DForFilledSheet(*pSketch3D, error);
    }
    error = QCoreApplication::translate("FilledSheetGuiCmd", "Sketch not found");
    return false;
}

bool FilledSheetGuiCmd::isValidConstraintSketch(const wydb::ElementId& constraintSketchId, QString& error)
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return false;
    const wydb::Element* pElem = pDb->getElement(constraintSketchId);
    if (!pElem) { error = QCoreApplication::translate("FilledSheetGuiCmd", "Sketch not found"); return false; }
    // A sketch already in the set is only being cleared, and its parent is this feature
    if (this->isConstraintSketch(constraintSketchId)) return true;

    if (const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pElem))
    {
        if (!pSketch->getParent().isNull()) { error = QCoreApplication::translate("FilledSheetGuiCmd", "Sketch is already in use"); return false; }
        return SketchUtil::isValidConstraintCurve(*pSketch, error);
    }
    if (const wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pElem))
    {
        if (!pSketch3D->getParent().isNull()) { error = QCoreApplication::translate("FilledSheetGuiCmd", "Sketch is already in use"); return false; }
        return SketchUtil::isValidConstraintCurve3D(*pSketch3D, error);
    }
    error = QCoreApplication::translate("FilledSheetGuiCmd", "Sketch not found");
    return false;
}

bool FilledSheetGuiCmd::isConstraintSketchValid(const wydb::ElementId& constraintSketchId)
{
    if (constraintSketchId.isNull()) return false;
    if (_constraintSketchId2ValidInfo.find(constraintSketchId) == _constraintSketchId2ValidInfo.cend())
    {
        QString error;
        _constraintSketchId2ValidInfo[constraintSketchId].valid = this->isValidConstraintSketch(constraintSketchId, error);
        _constraintSketchId2ValidInfo[constraintSketchId].error = error;
    }
    return _constraintSketchId2ValidInfo[constraintSketchId].valid;
}

std::vector<wydb::ElementId> FilledSheetGuiCmd::getConstraintSketchIds() const
{
    if (!_pMakeFilledSheet) return std::vector<wydb::ElementId>();
    const wy3d::FilledSheet* pFilledSheet = _pMakeFilledSheet->getFilledSheet();
    return pFilledSheet ? pFilledSheet->getConstraintSketches() : std::vector<wydb::ElementId>();
}

bool FilledSheetGuiCmd::hasConstraintSketch() const
{
    return !this->getConstraintSketchIds().empty();
}

bool FilledSheetGuiCmd::isConstraintSketch(const wydb::ElementId& constraintSketchId) const
{
    const std::vector<wydb::ElementId> constraintSketchIds = this->getConstraintSketchIds();
    return constraintSketchIds.cend()
        != std::find(constraintSketchIds.cbegin(), constraintSketchIds.cend(), constraintSketchId);
}

// A click in the view adds the element to the constraint set, or takes it out again when it is
// already in there
void FilledSheetGuiCmd::toggleConstraintSketch(const wydb::ElementId& constraintSketchId)
{
    if (constraintSketchId.isNull() || !_pMakeFilledSheet) return;

    const bool isCurrent = this->isConstraintSketch(constraintSketchId);
    if (!isCurrent)
    {
        QString error;
        if (!this->isValidConstraintSketch(constraintSketchId, error))
        {
            if (!error.isEmpty()) MessageBoxUtil::showWarning(error);
            return;
        }
    }

    // Only the change that goes through tells: a rejected one leaves the surface as it was, and
    // one that cannot be built is rolled back by then
    unsigned int errorCode(0);
    const bool changed = isCurrent
        ? _pMakeFilledSheet->removeConstraintSketch(constraintSketchId, errorCode)
        : _pMakeFilledSheet->addConstraintSketch(constraintSketchId, errorCode);
    if (!changed)
    {
        if (0 != errorCode) MessageBoxUtil::showError(errorCode);
        return;
    }
    this->rebuildHighlights();
}

void FilledSheetGuiCmd::preview(wydb::ElementId sketchId)
{
    if (sketchId.isNull()) return;
    if (_sketchId2ValidInfo.find(sketchId) == _sketchId2ValidInfo.cend())
    {
        QString error;
        _sketchId2ValidInfo[sketchId].valid = isValidBoundarySketch(sketchId, error);
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

// std::stoul throws on a malformed sub-path, a bad value only makes collection fail
static bool _parseSubPathIndex(const std::string& subPath, unsigned int& index)
{
    if (subPath.empty()) return false;
    unsigned long long value(0);
    for (char c : subPath)
    {
        if (c < '0' || c > '9') return false;
        value = value * 10 + static_cast<unsigned long long>(c - '0');
        if (value > 0xFFFFFFFFull) return false;
    }
    index = static_cast<unsigned int>(value);
    return true;
}

bool FilledSheetGuiCmd::collectPickedEdges(std::vector<TopoDS_Edge>& edges) const
{
    edges.clear();
    if (!_pSelSetHighlightor) return false;
    const wyap::SelectionSet& ss = _pSelSetHighlightor->getSelectionSet();
    if (ss.isEmpty()) return false;

    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return false;

    std::map<wydb::ElementId, std::vector<std::string>> id2SubPaths;
    for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
    {
        const wyap::Selection& sel = iter.current();
        if (sel.getElementId().isNull()) return false;
        if (sel.getSelectionType() != static_cast<unsigned int>(wy3d::SelectionType::Edge)) return false;
        const std::string& subPath = sel.getSubPath();
        if (subPath.empty()) return false;
        id2SubPaths[sel.getElementId()].emplace_back(subPath);
    }

    for (const auto& kv : id2SubPaths)
    {
        TopoDS_Shape shape;
        const wydb::Element* pElem = pDb->getElement(kv.first);
        if (const wy3d::Solid* pSolid = wy3d::Solid::cast(pElem))
        {
            shape = pSolid->getShape();
        }
        else if (const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pElem))
        {
            shape = pSheet->getShape();
        }
        else
        {
            return false;
        }
        if (shape.IsNull()) return false;

        TopTools_IndexedMapOfShape edgeMap;
        TopExp::MapShapes(shape, TopAbs_ShapeEnum::TopAbs_EDGE, edgeMap);
        for (const std::string& subPath : kv.second)
        {
            unsigned int edgeIndex(0);
            if (!_parseSubPathIndex(subPath, edgeIndex)) return false;
            if (edgeIndex >= static_cast<unsigned int>(edgeMap.Extent())) return false;
            edges.emplace_back(TopoDS::Edge(edgeMap(edgeIndex + 1)));
        }
    }

    return !edges.empty();
}
