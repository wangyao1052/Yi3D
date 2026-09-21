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
    wydb::ElementId sketchId(wydb::ElementId::kNull);
    if (this->isValidSketchSelectionSet(ss, sketchId) && !sketchId.isNull())
    {
        // pick-first: 预选了草图
        _sketchId = sketchId;
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();
        this->finishStep(Step::SelectSketch);
    }
    else
    {
        // pick-first: 预选了边
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
    _step = Step::Undefined;
    _sketchId = wydb::ElementId::kNull;
    _pValidSketchPreview = nullptr;
    _pInvalidSketchTooltip = nullptr;
    _pEdgePreview = nullptr;
    if (_pSelSetHighlightor) _pSelSetHighlightor->clearSelections();
    _edgePickOption.pSelPreFilter = nullptr;
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
            "Select a 2D or 3D sketch, or edges of a solid or sheet, to create a filled surface."));
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

    default:
    {
        Application::instance().getStatusBar()->setTips("");
        Application::instance().setCursor(CursorType::Select);
        assert(false);
    }
    break;
    }
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

    return;
}

void FilledSheetGuiCmd::onFeatureTreeItemClicked(const wydb::ElementId& id)
{
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
    return Step::SelectEdges == _step;
}

void FilledSheetGuiCmd::onContextMenuAction_ClearSelection()
{
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

bool FilledSheetGuiCmd::isValidSketchSelectionSet(const wyap::SelectionSet& ss, wydb::ElementId& sketchId)
{
    sketchId = wydb::ElementId::kNull;

    if (ss.getCount() != 1) return false;
    const wyap::Selection& sel = ss.createIterator().current();
    if (sel.getSelectionType() != static_cast<unsigned int>(wy3d::SelectionType::Element)) return false;
    wydb::ElementId id = sel.getElementId();
    if (id.isNull()) return false;

    QString error;
    if (!isValidBoundarySketch(id, error)) return false;
    sketchId = id;
    return true;
}

bool FilledSheetGuiCmd::isValidBoundarySketch(const wydb::ElementId& sketchId, QString& error)
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return false;
    const wydb::Element* pElem = pDb->getElement(sketchId);
    if (!pElem) { error = "Sketch not found"; return false; }

    if (const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pElem))
    {
        if (!pSketch->getParent().isNull()) { error = "Sketch is already in use"; return false; }
        return SketchUtil::isValidProfileForPlanarSheet(*pSketch, error);
    }
    if (const wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pElem))
    {
        if (!pSketch3D->getParent().isNull()) { error = "Sketch is already in use"; return false; }
        return SketchUtil::isValidProfile3DForFilledSheet(*pSketch3D, error);
    }
    error = "Sketch not found";
    return false;
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
