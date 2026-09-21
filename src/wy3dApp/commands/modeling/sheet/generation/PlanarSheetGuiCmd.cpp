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

#include "PlanarSheetGuiCmd.h"

#include <cassert>
#include <map>
#include <QCoreApplication>

#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopExp.hxx>
#include <BRep_Builder.hxx>
#include <BRepLib_FindSurface.hxx>
#include <Standard_Failure.hxx>

#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapSelManager.h>
#include <wy3dSelectionType.h>
#include <wy3dSketch.h>
#include <wy3dSketchProfile.h>
#include <wy3dPlanarSheet.h>
#include <wy3dSolid.h>
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
#include "commands/transient/ValidSketchTransient.h"
#include "wy3d/topo/TopoShapeUtil.h"

namespace
{

// BRepLib_FindSurface arguments: Tol = -1 asks for the max of the shape's own edge tolerances,
// and OnlyPlane keeps the fitted surface to a plane. Held to the same values as the core
// builder so this early verdict cannot disagree with the one reached on commit
const double kUseShapeTolerance = -1.0;
const Standard_Boolean kOnlyPlane = Standard_True;

// std::stoul throws on a malformed sub-path, a bad value only makes collection fail
bool _parseSubPathIndex(const std::string& subPath, unsigned int& index)
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

bool _edgesSharePlane(const std::vector<TopoDS_Edge>& edges)
{   
    try
    {
        if (edges.size() < 2) return true;
        BRep_Builder builder;
        TopoDS_Compound compound;
        builder.MakeCompound(compound);
        for (const TopoDS_Edge& edge : edges)
        {
            builder.Add(compound, edge);
        }

        return BRepLib_FindSurface(compound, kUseShapeTolerance, kOnlyPlane).Found();
    }
    catch (const Standard_Failure&)
    {
        return false;
    }
}

} // namespace

// ============================================================================
// MakePlanarSheet
// ============================================================================

void MakePlanarSheet::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pPlanarSheet) idSet.insert(_pPlanarSheet->getId());
}

bool MakePlanarSheet::init(const wydb::ElementId& sketchId, unsigned int& errorCode)
{
    errorCode = 0;
    if (!_pDb || !_pTopTrans || _pPlanarSheet || _isFinished)
        return false;
    if (sketchId.isNull())
        return false;

    const wydb::Element* pElem = _pDb->getElement(sketchId);
    if (!pElem) return false;
    const wy3d::Sketch* pConstSketch = wy3d::Sketch::cast(pElem);
    if (!pConstSketch) return false;
    if (!pConstSketch->getParent().isNull()) return false;

    wy3d::PlanarSheet* pSheet = nullptr;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction();
    if (!pTrans) return false;
    wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
    if (!pSketch)
    {
        assert(false);
        goto ABORT_TRANS;
    }

    if (wy::ErrorStatus::Ok != wy3d::PlanarSheet::create(pTrans, pSketch, pSheet) || !pSheet)
    {
        assert(false);
        goto ABORT_TRANS;
    }
    _pDb->getTransactionManager()->endTransaction();
    _pPlanarSheet = pSheet;
    _workPlnNormal = pSketch->getPlane().getNormal();
    errorCode = wy3d::getErrorCodeFromChainUpdateFeedback(
        _pDb->getTransactionManager()->getChainUpdateFeedback(pSheet->getId()).get());
    if (errorCode != 0) return false;
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    _pPlanarSheet = nullptr;
    _workPlnNormal.set(0.0, 0.0, 1.0);
    return false;
}

// ============================================================================
// PlanarSheetGuiCmd
// ============================================================================

PlanarSheetGuiCmd::PlanarSheetGuiCmd() : OsgGuiCommand(),
    _step(Step::Undefined), _sketchId(wydb::ElementId::kNull)
{
    _options.pointSelect = false;
    _options.boxSelect = false;

    _pSelSetHighlightor = std::make_shared<SelectionSetHighlightor>();
}

PlanarSheetGuiCmd::~PlanarSheetGuiCmd()
{
}

wyap::CmdExecution::StartResult PlanarSheetGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = GuiCommand::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    _sketchPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Sketch);
    _sketchPickOption.selType = wy3d::SelectionType::Element;
    _sketchPickOption.pSelFilter = std::make_shared<SingleClassSelFilter>(wy3d::Sketch::classInfo());

    _edgePickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Solid) |
        static_cast<unsigned int>(ElementNodeType::Sheet);
    _edgePickOption.selType = wy3d::SelectionType::SolidEdge;
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
            if (sel.getSelectionType() == static_cast<unsigned int>(wy3d::SelectionType::SolidEdge) &&
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
            this->updateEdgeSelectionFeedback();
        }
        else
        {
            this->gotoStep(Step::SelectSketch);
        }
    }

    return wyap::CmdExecution::StartResult::Succeeded;
}

void PlanarSheetGuiCmd::reset()
{
    this->cleanup();
}

void PlanarSheetGuiCmd::cleanup()
{
    _step = Step::Undefined;
    _sketchId = wydb::ElementId::kNull;
    _pValidSketchPreview = nullptr;
    _pInvalidSketchTooltip = nullptr;
    _pEdgePreview = nullptr;
    if (_pSelSetHighlightor) _pSelSetHighlightor->clearSelections();
    _edgePickOption.pSelPreFilter = nullptr;
    _pMakePlanarSheet = nullptr;
    _pMakeNonParametricSheet = nullptr;
}

bool PlanarSheetGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SelectSketch:
    {
        _pMakePlanarSheet = std::make_shared<MakePlanarSheet>(this);
        unsigned int errorCode(0);
        if (!_pMakePlanarSheet->init(_sketchId, errorCode))
        {
            _pValidSketchPreview = nullptr;
            _pInvalidSketchTooltip = nullptr;
            _pMakePlanarSheet = nullptr;
            if (0 != errorCode) MessageBoxUtil::showError(errorCode);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }
        _pValidSketchPreview = nullptr;
        _pInvalidSketchTooltip = nullptr;
        _pMakePlanarSheet->commit();
        _pMakePlanarSheet = nullptr;
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

        TopoDS_Shape sheetShape;
        wy3d::ErrorCode shapeError = wy3d::TopoShapeUtil::makePlanarSheetFromEdges(edges, sheetShape);
        if (wy3d::ErrorCode::NoError != shapeError)
        {
            MessageBoxUtil::showError(static_cast<unsigned int>(shapeError));
            // No abort on this path, so the command is still alive: refresh the reason on the
            // status bar here rather than after finishStep returns
            this->updateEdgeSelectionFeedback();
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

void PlanarSheetGuiCmd::gotoStep(Step step)
{
    _step = step;

    switch (step)
    {
    case Step::SelectSketch:
    {
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        _pEdgePreview = nullptr;

        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("PlanarSheetGuiCmd",
            "Select a sketch to create a planar sheet, or select edges to enclose a planar face."));
        Application::instance().setCursor(CursorType::SelectElements);
    }
    break;

    case Step::SelectEdges:
    {
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        _pValidSketchPreview = nullptr;
        _pEdgePreview = nullptr;

        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("PlanarSheetGuiCmd",
            "Select edges to enclose one or more planar loops; press Enter or Spacebar to confirm; press Esc to clear the edges."));
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

void PlanarSheetGuiCmd::onMouseMove(const MouseEvent& event)
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

void PlanarSheetGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    return;
}

void PlanarSheetGuiCmd::onLeftMouseUp(const MouseEvent& event)
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
            this->updateEdgeSelectionFeedback();
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
            this->updateEdgeSelectionFeedback();
        }
    }

    return;
}

void PlanarSheetGuiCmd::onFeatureTreeItemClicked(const wydb::ElementId& id)
{
    if (Step::SelectSketch != _step) return;
    if (id.isNull()) return;

    QString error;
    if (!isValidSketch(id, error)) return;

    _sketchId = id;
    Application::instance().getSelManager()->beginChange();
    Application::instance().getSelManager()->clearSelections();
    Application::instance().getSelManager()->endChange();
    this->finishStep(Step::SelectSketch);
}

void PlanarSheetGuiCmd::onEscapeKey()
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

bool PlanarSheetGuiCmd::isContextMenuActionVisible_CompleteSelection() const
{
    // Hiding it while nothing is picked keeps the entry from doing nothing at all
    return Step::SelectEdges == _step && _pSelSetHighlightor
        && !_pSelSetHighlightor->getSelectionSet().isEmpty();
}

void PlanarSheetGuiCmd::onContextMenuAction_CompleteSelection()
{
    this->onEnterKey();
}

void PlanarSheetGuiCmd::onEnterKey()
{
    if (Step::SelectEdges != _step) return;
    // Nothing picked yet: keep the tip rather than report invalid data
    if (!_pSelSetHighlightor || _pSelSetHighlightor->getSelectionSet().isEmpty()) return;

    // The same precondition finishStep asserts on: a selection that cannot be read is
    // reported by the feedback instead of finishing, which would abort the command
    std::vector<TopoDS_Edge> edges;
    if (!this->collectPickedEdges(edges))
    {
        this->updateEdgeSelectionFeedback();
        return;
    }

    // finishStep ends or aborts the command: no member access after this call
    this->finishStep(_step);
}

void PlanarSheetGuiCmd::onSpaceKey()
{
    this->onEnterKey();
}

bool PlanarSheetGuiCmd::isContextMenuActionVisible_ClearSelection() const
{
    return Step::SelectEdges == _step;
}

void PlanarSheetGuiCmd::onContextMenuAction_ClearSelection()
{
    if (Step::SelectEdges == _step)
    {
        if (_pSelSetHighlightor)
        {
            _pSelSetHighlightor->clearSelections();
            _edgePickOption.pSelPreFilter = nullptr;
            Application::instance().getStatusBar()->setTips(QCoreApplication::translate("PlanarSheetGuiCmd",
                "Select edges to enclose one or more planar loops; press Enter or Spacebar to confirm; press Esc to clear the edges."));
        }
    }
}

bool PlanarSheetGuiCmd::isValidSketchSelectionSet(const wyap::SelectionSet& ss, wydb::ElementId& sketchId)
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

bool PlanarSheetGuiCmd::isValidSketch(const wydb::ElementId& sketchId, QString& error)
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return false;
    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(sketchId));
    if (!pSketch) { error = "Sketch not found"; return false; }
    if (!pSketch->getParent().isNull()) { error = "Sketch is already in use"; return false; }

    wy3d::SketchProfile sketchProfile(pSketch);
    if (sketchProfile.check()) return true;

    std::shared_ptr<wy3d::SketchError> pError = sketchProfile.getError();
    if (pError)
        error = ErrorCodeTranslation::instance().getErrorCodeDescription(pError->type);
    else
        error = "Invalid sketch profile";
    return false;
}

void PlanarSheetGuiCmd::preview(wydb::ElementId sketchId)
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

bool PlanarSheetGuiCmd::collectPickedEdges(std::vector<TopoDS_Edge>& edges) const
{
    edges.clear();
    const wyap::SelectionSet& ss = _pSelSetHighlightor->getSelectionSet();
    if (ss.isEmpty()) return false;

    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return false;

    std::map<wydb::ElementId, std::vector<std::string>> id2SubPaths;
    for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
    {
        const wyap::Selection& sel = iter.current();
        if (sel.getElementId().isNull()) return false;
        if (sel.getSelectionType() != static_cast<unsigned int>(wy3d::SelectionType::SolidEdge)) return false;
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
            unsigned int edgeIndex = 0;
            if (!_parseSubPathIndex(subPath, edgeIndex)) return false;
            if (edgeIndex >= static_cast<unsigned int>(edgeMap.Extent())) return false;
            edges.emplace_back(TopoDS::Edge(edgeMap(edgeIndex + 1)));
        }
    }

    return !edges.empty();
}

void PlanarSheetGuiCmd::updateEdgeSelectionFeedback()
{
    std::vector<TopoDS_Edge> edges;
    if (!this->collectPickedEdges(edges))
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("PlanarSheetGuiCmd",
            "Select edges to enclose a planar face."));
        return;
    }

    // Only a verdict: the sheet is created by Enter, Spacebar or the context menu, so that
    // more loops (a hole, another opening) can be added after the first one closes
    TopoDS_Shape sheetShape;
    const wy3d::ErrorCode errorCode = wy3d::TopoShapeUtil::makePlanarSheetFromEdges(edges, sheetShape);
    if (wy3d::ErrorCode::NoError == errorCode)
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("PlanarSheetGuiCmd",
            "Closed loops found; press Enter or Spacebar to confirm, or keep selecting edges to add more loops."));
        return;
    }
    if (wy3d::ErrorCode::PLANARSHEET_EdgesNotCoplanar == errorCode ||
        wy3d::ErrorCode::PLANARSHEET_LoopsNotNested == errorCode)
    {
        // The reason stays in the status bar: a tooltip is gone after three seconds, which is
        // easy to miss while the next edge is being picked
        Application::instance().getStatusBar()->setTips(
            ErrorCodeTranslation::instance().getErrorCodeDescription(errorCode));
        return;
    }

    // The loops may only not be closed yet, but edges that already span two planes can never
    // close into a planar sheet: say so instead of inviting more edges
    if (!_edgesSharePlane(edges))
    {
        Application::instance().getStatusBar()->setTips(
            ErrorCodeTranslation::instance().getErrorCodeDescription(
                wy3d::ErrorCode::PLANARSHEET_EdgesNotCoplanar));
        return;
    }

    Application::instance().getStatusBar()->setTips(QCoreApplication::translate("PlanarSheetGuiCmd",
        "Keep selecting edges to close the loop."));
}
