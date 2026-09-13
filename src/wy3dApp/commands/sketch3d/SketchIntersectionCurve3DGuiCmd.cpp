///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 Wang Yao <wangyao1052@163.com>
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

#include "SketchIntersectionCurve3DGuiCmd.h"

#include <cassert>
#include <algorithm>

#include <QCoreApplication>
#include <QMessageBox>
#include <QOpenGLWidget>

#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>

#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dDatumPlane.h>
#include <wy3dSelectionType.h>
#include <wy3dSheet.h>
#include <wy3dSketch3D.h>
#include <wy3dSketchEntity3D.h>
#include <wy3dSolid.h>

#include "application/Application.h"
#include "commands/dialogs/SketchIntersectionCurvePanel.h"
#include "scene/nodes/ElementNodeType.h"
#include "select/SelectionSetHighlightor.h"
#include "utils/TopoShapeUtil.h"
#include "widgets/frame/MainWindow.h"

namespace
{

using Source = wy3d::Sketch3DIntersectionUtil::Source;

// std::stoul throws on a malformed sub path, a bad value should only make the pick fail
bool parseSubPathIndex(const std::string& subPath, unsigned int& index)
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

// A datum plane has no sub entities, so it is only ever offered as a whole element and the
// pick has to accept whole elements. That same coarse pass turns a click on a solid into a
// whole body selection and would swallow every face pick, so this filter exists to reject
// those: a rejected selection returns null, which lets the ray fall through to the face pass.
// It also carries the second step rule - once a datum plane is held, planes are out, since
// two planes meet in an unbounded line that cannot become a sketch entity.
class IntersectionSourceSelFilter : public SelectFilterFunctor
{
public:
    explicit IntersectionSourceSelFilter(bool allowPlane) : _allowPlane(allowPlane) {}

    virtual SelectFilterStatus operator()(
        const wydb::Database* pDb, const wyap::Selection& sel, SelectAction) const override
    {
        assert(pDb);
        if (!pDb) return SelectFilterStatus::Continue;

        const wydb::Element* pElement = pDb->getElement(sel.getElementId());
        if (!pElement) return SelectFilterStatus::Continue;

        switch (wy3d::UIntToSelectionType(sel.getSelectionType()))
        {
        case wy3d::SelectionType::SolidFace:
            if (sel.getSubPath().empty()) return SelectFilterStatus::Continue;
            // Any face will do, curved ones included: a cylinder is the point of this tool.
            return (wy3d::Solid::cast(pElement) || wy3d::Sheet::cast(pElement))
                ? SelectFilterStatus::Ok : SelectFilterStatus::Continue;

        case wy3d::SelectionType::Element:
            if (!_allowPlane) return SelectFilterStatus::Continue;
            return wy3d::DatumPlane::cast(pElement) ? SelectFilterStatus::Ok : SelectFilterStatus::Continue;

        default:
            return SelectFilterStatus::Continue;
        }
    }

private:
    bool _allowPlane;
};

} // namespace

SketchIntersectionCurve3DGuiCmd::SketchIntersectionCurve3DGuiCmd()
    : OsgGuiCommand(), _pPanel(nullptr), _hasFirst(false), _firstSelection(wydb::ElementId::kNull)
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

SketchIntersectionCurve3DGuiCmd::~SketchIntersectionCurve3DGuiCmd()
{
}

wyap::CmdExecution::StartResult SketchIntersectionCurve3DGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = GuiCommand::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    if (!GuiCommandUtil::initSketch3DInfo(_sketch3DInfo))
    {
        assert(false);
        return wyap::CmdExecution::StartResult::Failed;
    }
    if (_sketch3DInfo.sketch3dId.isNull())
    {
        assert(false);
        return wyap::CmdExecution::StartResult::Failed;
    }

    GuiCommandUtil::clearSelections();

    // Faces and datum planes in one pick. acceptElement has to stay on, see the filter above.
    // The sketch being edited is not reachable here: its entities are Sketch3DEntity nodes
    // and its own Sketch3D node has been swapped out by the environment.
    _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Solid) |
        static_cast<unsigned int>(ElementNodeType::Sheet) |
        static_cast<unsigned int>(ElementNodeType::DatumPlane);
    _pointPickOption.selType = wy3d::SelectionType::SolidFace;
    this->setAllowPlane(true);

    _hasFirst = false;
    _firstSelection = wyap::Selection(wydb::ElementId::kNull);
    _firstSource = Source();

    this->createPanel();

    this->setStepOneTip();

    Application::instance().setCursor(CursorType::SelectElements);

    return wyap::CmdExecution::StartResult::Succeeded;
}

void SketchIntersectionCurve3DGuiCmd::onEnd()
{
    _pPreview = nullptr;
    _pHeldHighlight = nullptr;
    this->destroyPanel();
    GuiCommand::onEnd();
}

void SketchIntersectionCurve3DGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    _pPreview = nullptr;
    _pHeldHighlight = nullptr;
    this->destroyPanel();
    GuiCommand::onAbort(cause);
}

void SketchIntersectionCurve3DGuiCmd::onMouseMove(const MouseEvent& event)
{
    this->mouseMovePointPickPreview(event.x, event.y, _pointPickOption, _pPreview);
}

void SketchIntersectionCurve3DGuiCmd::onLeftMouseUp(const MouseEvent& event)
{
    wyap::Selection sel = this->pointPick(event.x, event.y, _pointPickOption);
    if (sel.getElementId().isNull())
        return;

    // The filter has already vetted the pick, so a failure here means the element moved
    // under us between the pick and this call. Nothing to report, the pick just does nothing.
    Source source;
    if (!this->resolveSource(sel, source)) return;

    if (!_hasFirst)
    {
        this->holdFirst(sel, source);
        return;
    }

    if (sel == _firstSelection)
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchIntersectionCurve3DGuiCmd",
            "The two items must be different."));
        return;
    }

    // The pair is applied and the first item stays held: it is the source of every later pick,
    // so the command goes on intersecting each new face with it - the fan the tips describe.
    this->applyPair(sel, source);
}

bool SketchIntersectionCurve3DGuiCmd::resolveSource(const wyap::Selection& sel, Source& source) const
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return false;
    }

    const wydb::Element* pElement = pDb->getElement(sel.getElementId());
    if (!pElement) return false;

    if (wy3d::SelectionType::SolidFace == wy3d::UIntToSelectionType(sel.getSelectionType()))
    {
        TopoDS_Shape shape;
        if (const wy3d::Solid* pSolid = wy3d::Solid::cast(pElement))
        {
            shape = pSolid->getShape();
        }
        else if (const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pElement))
        {
            shape = pSheet->getShape();
        }
        else
        {
            return false;
        }
        if (shape.IsNull()) return false;

        unsigned int faceIndex(0);
        if (!parseSubPathIndex(sel.getSubPath(), faceIndex)) return false;

        // The face stays bounded on purpose: its own boundary trims the section.
        const std::pair<bool, TopoDS_Face> faceRet = TopoShapeUtil::getFace(shape, faceIndex);
        if (!faceRet.first) return false;

        source = Source{false, faceRet.second, wy3d::SketchPlane()};
        return true;
    }

    if (wy3d::SelectionType::Element == wy3d::UIntToSelectionType(sel.getSelectionType()))
    {
        const wy3d::DatumPlane* pDatumPlane = wy3d::DatumPlane::cast(pElement);
        if (!pDatumPlane) return false;

        // A datum plane stands for an unbounded plane, so the section is the whole curve.
        source = Source{true, TopoDS_Shape(), pDatumPlane->getPlane()};
        return true;
    }

    return false;
}

void SketchIntersectionCurve3DGuiCmd::holdFirst(const wyap::Selection& sel, const Source& source)
{
    // Held for the rest of the command and never handed back: this item is the source of every
    // later pick, so it is the one the fan hangs from. The ordinary highlight, as in the other
    // anchor-and-members commands: a second colour would stand out, and the tip already says
    // which item is the anchor.
    _hasFirst = true;
    _firstSelection = sel;
    _firstSource = source;

    _pHeldHighlight = std::make_shared<SelectionSetHighlightor>(wyap::SelectionSet());
    if (_pHeldHighlight) _pHeldHighlight->addSelection(sel);

    this->setAllowPlane(!source.isPlane);
    this->setStepTwoTip(source.isPlane);
}

void SketchIntersectionCurve3DGuiCmd::setAllowPlane(bool allow)
{
    _pointPickOption.pSelFilter = std::make_shared<IntersectionSourceSelFilter>(allow);
}

void SketchIntersectionCurve3DGuiCmd::applyPair(const wyap::Selection& second, const Source& source)
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return;
    }

    // Read off the panel at every pair, so switching applies to the curves created after it -
    // the ones already in the sketch are left alone. With no panel the fit is the default.
    const wy3d::Sketch3DIntersectionUtil::Mode mode = (!_pPanel || _pPanel->isFit())
        ? wy3d::Sketch3DIntersectionUtil::Mode::Fit
        : wy3d::Sketch3DIntersectionUtil::Mode::Interpolate;

    // Pure geometry, before any transaction is opened: a failure needs no rollback.
    std::vector<TopoDS_Edge> edges;
    const wy3d::Sketch3DIntersectionUtil::Result result =
        wy3d::Sketch3DIntersectionUtil::intersect(_firstSource, source, edges, mode);
    if (wy3d::Sketch3DIntersectionUtil::Result::Ok != result)
    {
        this->reportFailure(result);
        return;
    }

    // One pair is one transaction, so one undo step removes the whole intersection curve
    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans)
    {
        assert(false);
        return;
    }

    wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(_sketch3DInfo.sketch3dId));
    if (!pSketch3D)
    {
        pDb->getTransactionManager()->abortTransaction();
        return;
    }

    // All or nothing: half of a loop is worse than no curve at all, and a partial result
    // would also break the one pair equals one undo step rule.
    for (const TopoDS_Edge& edge : edges)
    {
        wy3d::SketchEntity3D* pEntity = nullptr;
        const wy3d::Sketch3DEdgeUtil::Result convertResult =
            wy3d::Sketch3DEdgeUtil::convert(pTrans, edge, pEntity);
        if (wy3d::Sketch3DEdgeUtil::Result::Ok != convertResult || !pEntity ||
            wy::ErrorStatus::Ok != pSketch3D->addEntity(pEntity))
        {
            pDb->getTransactionManager()->abortTransaction();
            this->reportCreateFailure(convertResult);
            return;
        }
    }

    if (wy::ErrorStatus::Ok != pDb->getTransactionManager()->endTransaction())
    {
        pDb->getTransactionManager()->abortTransaction();
        return;
    }
}

void SketchIntersectionCurve3DGuiCmd::setStepOneTip() const
{
    Application::instance().getStatusBar()->setTips(
        QCoreApplication::translate("SketchIntersectionCurve3DGuiCmd", "Select the first face or datum plane."));
}

void SketchIntersectionCurve3DGuiCmd::setStepTwoTip(bool firstIsPlane) const
{
    // A datum plane is only pickable along its border, and a second one cannot be paired with
    // it, so the tip asks for a face only rather than letting the pick be rejected silently.
    Application::instance().getStatusBar()->setTips(firstIsPlane
        ? QCoreApplication::translate("SketchIntersectionCurve3DGuiCmd",
            "Select the face to intersect with the datum plane.")
        : QCoreApplication::translate("SketchIntersectionCurve3DGuiCmd",
            "Select the face or datum plane to intersect with the first one."));
}

void SketchIntersectionCurve3DGuiCmd::reportFailure(wy3d::Sketch3DIntersectionUtil::Result result) const
{
    QString message;
    switch (result)
    {
    case wy3d::Sketch3DIntersectionUtil::Result::NoIntersection:
        message = QCoreApplication::translate("SketchIntersectionCurve3DGuiCmd",
            "Intersection failed: the two items do not intersect.");
        break;
    case wy3d::Sketch3DIntersectionUtil::Result::ParallelSources:
        message = QCoreApplication::translate("SketchIntersectionCurve3DGuiCmd",
            "Intersection failed: the two items lie on the same plane, so they meet in a surface rather than a curve.");
        break;
    case wy3d::Sketch3DIntersectionUtil::Result::InfiniteSection:
        message = QCoreApplication::translate("SketchIntersectionCurve3DGuiCmd",
            "Intersection failed: the two items meet in an unbounded line, which cannot become a sketch curve.");
        break;
    case wy3d::Sketch3DIntersectionUtil::Result::InvalidInput:
        message = QCoreApplication::translate("SketchIntersectionCurve3DGuiCmd",
            "Intersection failed: the two items cannot be intersected.");
        break;
    case wy3d::Sketch3DIntersectionUtil::Result::AlgorithmFailed:
        message = QCoreApplication::translate("SketchIntersectionCurve3DGuiCmd",
            "Intersection failed: the intersection algorithm did not complete.");
        break;
    case wy3d::Sketch3DIntersectionUtil::Result::UnsupportedCurve:
        message = QCoreApplication::translate("SketchIntersectionCurve3DGuiCmd",
            "Intersection failed: the resulting curve is more complex than the sketch can hold. Switch to interpolation and try again.");
        break;
    default:
        message = QCoreApplication::translate("SketchIntersectionCurve3DGuiCmd",
            "Intersection failed: the geometry is invalid.");
        break;
    }

    QMessageBox::warning(nullptr,
        QCoreApplication::translate("SketchIntersectionCurve3DGuiCmd", "Intersection Curve"),
        message);
}

void SketchIntersectionCurve3DGuiCmd::reportCreateFailure(wy3d::Sketch3DEdgeUtil::Result result) const
{
    QString message;
    switch (result)
    {
    case wy3d::Sketch3DEdgeUtil::Result::Degenerate:
        message = QCoreApplication::translate("SketchIntersectionCurve3DGuiCmd",
            "Intersection failed: the resulting curve is degenerate. The two items may be tangent.");
        break;
    case wy3d::Sketch3DEdgeUtil::Result::InfiniteCurve:
        message = QCoreApplication::translate("SketchIntersectionCurve3DGuiCmd",
            "Intersection failed: the resulting curve is unbounded.");
        break;
    case wy3d::Sketch3DEdgeUtil::Result::UnsupportedType:
    case wy3d::Sketch3DEdgeUtil::Result::CreateFailed:
        message = QCoreApplication::translate("SketchIntersectionCurve3DGuiCmd",
            "Intersection failed: the resulting curve cannot be created in the sketch.");
        break;
    default:
        message = QCoreApplication::translate("SketchIntersectionCurve3DGuiCmd",
            "Intersection failed: the resulting curve geometry is invalid.");
        break;
    }

    QMessageBox::warning(nullptr,
        QCoreApplication::translate("SketchIntersectionCurve3DGuiCmd", "Intersection Curve"),
        message);
}

void SketchIntersectionCurve3DGuiCmd::createPanel()
{
    assert(!_pPanel);

    MainWindow* pMainWindow = Application::instance().getMainWindow();
    QOpenGLWidget* pViewport = pMainWindow ? pMainWindow->findChild<QOpenGLWidget*>() : nullptr;
    // No viewport means no 3D picking either, and the command is not worth refusing over a
    // panel: without one every pair is created in fit mode.
    if (!pViewport) return;

    _pPanel = new SketchIntersectionCurvePanel(pViewport);
    _pPanel->show();
}

void SketchIntersectionCurve3DGuiCmd::destroyPanel()
{
    if (!_pPanel) return;

    _pPanel->hide();
    delete _pPanel;
    _pPanel = nullptr;
}
