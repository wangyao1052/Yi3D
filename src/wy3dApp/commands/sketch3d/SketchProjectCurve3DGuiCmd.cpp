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

#include "SketchProjectCurve3DGuiCmd.h"

#include <cassert>

#include <QCoreApplication>
#include <QMessageBox>
#include <QOpenGLWidget>

#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>

#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dDatumPlane.h>
#include <wy3dSelectionType.h>
#include <wy3dSheet.h>
#include <wy3dSketch.h>
#include <wy3dSketch3D.h>
#include <wy3dSketchEntity3D.h>
#include <wy3dSolid.h>

#include "application/Application.h"
#include "commands/dialogs/SketchProjectCurvePanel.h"
#include "commands/transient/Sketch3DDirectionTransient.h"
#include "commands/transient/Sketch3DEdgesTransient.h"
#include "scene/nodes/ElementNodeType.h"
#include "select/SelectionSetHighlightor.h"
#include "utils/TopoShapeUtil.h"
#include "widgets/frame/MainWindow.h"

namespace
{

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

// The face pass of the pick needs acceptElement, or a click on a body becomes a whole-element
// selection and every face pick is swallowed. That same coarse pass is what this filter undoes:
// anything that is not a face of a body or a datum plane is rejected, which lets the ray fall
// through to the face pass and find the face underneath. Both kinds of target are wanted here:
// a face, which is bounded and trims the projection to itself, and a datum plane, which stands for
// an unbounded plane. A datum plane is only pickable along its own border, which is what makes a
// click on one land here rather than on a body behind it.
class ProjectTargetSelFilter : public SelectFilterFunctor
{
public:
    virtual SelectFilterStatus operator()(
        const wydb::Database* pDb, const wyap::Selection& sel, SelectAction) const override
    {
        assert(pDb);
        if (!pDb) return SelectFilterStatus::Continue;

        const wydb::Element* pElement = pDb->getElement(sel.getElementId());
        if (!pElement) return SelectFilterStatus::Continue;

        switch (wy3d::UIntToSelectionType(sel.getSelectionType()))
        {
        case wy3d::SelectionType::Face:
            if (sel.getSubPath().empty()) return SelectFilterStatus::Continue;
            // Any face of a body will do, curved ones included: a cylinder is the interesting
            // case. Deliberately not isTopLevelBody, as in the intersection command - a face of a
            // body a boolean produced is just as good a target.
            return (wy3d::Solid::cast(pElement) || wy3d::Sheet::cast(pElement))
                ? SelectFilterStatus::Ok : SelectFilterStatus::Continue;

        case wy3d::SelectionType::Element:
            // A datum plane is picked whole; the projection runs onto its plane, which is
            // unbounded, so there is nothing to trim the result to.
            return wy3d::DatumPlane::cast(pElement) ? SelectFilterStatus::Ok : SelectFilterStatus::Continue;

        default:
            return SelectFilterStatus::Continue;
        }
    }
};

// Where the direction arrow is planted: the middle of the source curves, which lies in the sketch
// plane because they all do. A sketch with nothing measurable in it leaves the origin at zero,
// and an arrow drawn there is no worse than one drawn anywhere else.
wy::Vector3 sourceCenter(const std::vector<TopoDS_Edge>& edges)
{
    Bnd_Box box;
    for (const TopoDS_Edge& edge : edges)
    {
        if (!edge.IsNull()) BRepBndLib::Add(edge, box);
    }
    if (box.IsVoid()) return wy::Vector3::kZero;

    double x0(0.0), y0(0.0), z0(0.0), x1(0.0), y1(0.0), z1(0.0);
    box.Get(x0, y0, z0, x1, y1, z1);
    return wy::Vector3((x0 + x1) / 2.0, (y0 + y1) / 2.0, (z0 + z1) / 2.0);
}

} // namespace

SketchProjectCurve3DGuiCmd::SketchProjectCurve3DGuiCmd()
    : OsgGuiCommand()
    , _pPanel(nullptr)
    , _step(Step::SelectSketch)
    , _hasTarget(false)
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

SketchProjectCurve3DGuiCmd::~SketchProjectCurve3DGuiCmd()
{
}

wyap::CmdExecution::StartResult SketchProjectCurve3DGuiCmd::onStart()
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

    _step = Step::SelectSketch;
    _sourcePlane = wy3d::SketchPlane();
    _sourceEdges.clear();
    _arrowOrigin = wy::Vector3::kZero;
    _hasTarget = false;
    _target = ProjectionTarget();
    _previewEdges.clear();

    this->setPickForStep(_step);

    this->createPanel();

    this->setStepOneTip();

    Application::instance().setCursor(CursorType::SelectElements);

    return wyap::CmdExecution::StartResult::Succeeded;
}

void SketchProjectCurve3DGuiCmd::onEnd()
{
    _pPreview = nullptr;
    _pSourceHighlight = nullptr;
    _pTargetHighlight = nullptr;
    _pProjectionPreview = nullptr;
    _pDirectionArrow = nullptr;
    this->destroyPanel();
    GuiCommand::onEnd();
}

void SketchProjectCurve3DGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    _pPreview = nullptr;
    _pSourceHighlight = nullptr;
    _pTargetHighlight = nullptr;
    _pProjectionPreview = nullptr;
    _pDirectionArrow = nullptr;
    this->destroyPanel();
    GuiCommand::onAbort(cause);
}

void SketchProjectCurve3DGuiCmd::onMouseMove(const MouseEvent& event)
{
    this->mouseMovePointPickPreview(event.x, event.y, _pointPickOption, _pPreview);
}

void SketchProjectCurve3DGuiCmd::onLeftMouseUp(const MouseEvent& event)
{
    wyap::Selection sel = this->pointPick(event.x, event.y, _pointPickOption);
    if (sel.getElementId().isNull()) return;

    if (Step::SelectSketch == _step)
    {
        if (!this->takeSourceSketch(sel)) return;
        this->setStepTwoTip();
        return;
    }

    // Clicking the target that is already picked takes it back, which is how the user changes
    // their mind without having to find another one first.
    if (_pTargetHighlight && _pTargetHighlight->containsSelection(sel))
    {
        this->clearTarget();
        return;
    }

    // Picking another target is how a projection that landed nowhere gets retried, so a pick that
    // resolves to nothing leaves the sketch and the previous target where they were and the
    // command alive.
    if (!this->takeTarget(sel)) return;
    this->recomputePreview();
}

void SketchProjectCurve3DGuiCmd::setPickForStep(Step step)
{
    if (Step::SelectSketch == step)
    {
        // The sketch is picked whole: the sub path names the one curve that was clicked and is
        // ignored, because all of the sketch is the source. Sketch3D is deliberately out of the
        // mask as well - a 3D sketch has no plane, so there is no direction to project along.
        _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Sketch);
        _pointPickOption.selType = wy3d::SelectionType::SketchCurve;
        _pointPickOption.acceptElement = false;
        _pointPickOption.pSelFilter = nullptr;
        return;
    }

    // Faces and datum planes in one pick, as in the intersection command. acceptElement has to stay
    // on, see the filter above: that is what lets the whole-element pass see the datum plane at all.
    _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Solid) |
        static_cast<unsigned int>(ElementNodeType::Sheet) |
        static_cast<unsigned int>(ElementNodeType::DatumPlane);
    _pointPickOption.selType = wy3d::SelectionType::Face;
    _pointPickOption.acceptElement = true;
    _pointPickOption.pSelFilter = std::make_shared<ProjectTargetSelFilter>();
}

bool SketchProjectCurve3DGuiCmd::takeSourceSketch(const wyap::Selection& sel)
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return false;
    }

    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(sel.getElementId()));
    if (!pSketch) return false;

    std::vector<TopoDS_Edge> edges;
    if (wy3d::Sketch3DProjectionUtil::Result::Ok !=
        wy3d::Sketch3DProjectionUtil::collectSourceEdges(pSketch, edges))
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate(
            "SketchProjectCurve3DGuiCmd", "Projection failed: the sketch cannot be read."));
        return false;
    }

    // A sketch with nothing in it is not a caller mistake, so it gets its own sentence - and the
    // command stays on this step, where another sketch is a click away.
    if (edges.empty())
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate(
            "SketchProjectCurve3DGuiCmd", "The sketch has no curves to project."));
        return false;
    }

    _sourcePlane = pSketch->getPlane();
    _sourceEdges = std::move(edges);
    _arrowOrigin = sourceCenter(_sourceEdges);
    _step = Step::SelectTarget;
    this->setPickForStep(_step);

    // The direction is known from here on, which is the moment the arrow starts earning its
    // place: without it there is nothing on screen that says which side of the sketch the sweep
    // is going to run on.
    if (_pPanel) _pPanel->setDirectionAvailable(true);
    this->updateDirectionArrow();

    // Element level rather than curve level: the whole sketch is the source, and the node
    // highlights all of its curves for a whole-element selection. Highlighting just the clicked
    // curve would say the opposite.
    _pSourceHighlight = std::make_shared<SelectionSetHighlightor>(wyap::SelectionSet());
    if (_pSourceHighlight) _pSourceHighlight->addSelection(wyap::Selection(sel.getElementId()));

    this->updatePanelState();

    return true;
}

bool SketchProjectCurve3DGuiCmd::takeTarget(const wyap::Selection& sel)
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return false;
    }

    const wydb::Element* pElement = pDb->getElement(sel.getElementId());
    if (!pElement) return false;

    ProjectionTarget target;

    if (const wy3d::DatumPlane* pDatumPlane = wy3d::DatumPlane::cast(pElement))
    {
        // A datum plane has no face to take: the projection runs onto its plane, and nothing
        // trims the result. It is picked whole, so the sub path says nothing here.
        target.isPlane = true;
        target.plane = pDatumPlane->getPlane();
        if (!target.plane.isValid()) return false;
    }
    else
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

        // The face stays bounded on purpose: its own boundary trims the projection, which is what
        // makes a face a face rather than an unbounded surface.
        const std::pair<bool, TopoDS_Face> faceRet = TopoShapeUtil::getFace(shape, faceIndex);
        if (!faceRet.first) return false;

        target.face = faceRet.second;
    }

    _target = target;
    _hasTarget = true;

    // The picked target wears the ordinary selected green for as long as it is the target, the way
    // every other command in the application shows what it is about to act on.
    _pTargetHighlight = std::make_shared<SelectionSetHighlightor>(wyap::SelectionSet());
    if (_pTargetHighlight) _pTargetHighlight->addSelection(sel);

    return true;
}

void SketchProjectCurve3DGuiCmd::clearTarget()
{
    // Dropping the highlightor is what puts the target back the way it was.
    _pTargetHighlight = nullptr;
    _hasTarget = false;
    _target = ProjectionTarget();

    // The preview belongs to the target that is gone, and with it gone Enter has nothing to
    // confirm.
    _pProjectionPreview = nullptr;
    _previewEdges.clear();

    this->setStepTwoTip();
    this->updatePanelState();
}

void SketchProjectCurve3DGuiCmd::updateDirectionArrow()
{
    _pDirectionArrow = nullptr;
    if (Step::SelectTarget != _step) return;

    const bool bidirectional = (_pPanel && _pPanel->isBidirectional());
    const bool reverse = (_pPanel && _pPanel->isReverse());

    // The sketch plane's own normal is the only direction this feature has, and the flags are the
    // only say the user gets over it.
    wy::Vector3 direction = _sourcePlane.getNormal();
    // Reverse is ignored when both sides are taken, exactly as it is in the projection itself.
    if (reverse && !bidirectional) direction = -direction;

    _pDirectionArrow = std::make_shared<Sketch3DDirectionTransient>(_arrowOrigin, direction, bidirectional);
}

void SketchProjectCurve3DGuiCmd::recomputePreview()
{
    // Dropped before anything is computed: a recompute that came to nothing must not leave the
    // previous curve on the screen, where it would read as the answer to the flags as they are
    // now. This is also what stops Enter from committing a stale projection.
    _pProjectionPreview = nullptr;
    _previewEdges.clear();

    if (Step::SelectTarget != _step || !_hasTarget)
    {
        this->updatePanelState();
        return;
    }

    wy3d::Sketch3DProjectionUtil::Options options;
    options.reverse = (_pPanel && _pPanel->isReverse());
    options.bidirectional = (_pPanel && _pPanel->isBidirectional());

    // The target decides which overload runs, and nothing else does: a datum plane is handed over
    // as its plane, so the projection is not trimmed by anything.
    std::vector<TopoDS_Edge> edges;
    const wy3d::Sketch3DProjectionUtil::Result result = _target.isPlane
        ? wy3d::Sketch3DProjectionUtil::project(_sourcePlane, _sourceEdges, _target.plane, options, edges)
        : wy3d::Sketch3DProjectionUtil::project(_sourcePlane, _sourceEdges, _target.face, options, edges);
    if (wy3d::Sketch3DProjectionUtil::Result::Ok != result)
    {
        // No setStepTwoTip here: it would ask for a target again and bury the one line that says
        // what actually happened, which reportFailure has just written.
        this->reportFailure(result);
        this->updatePanelState();
        return;
    }

    _previewEdges = std::move(edges);
    _pProjectionPreview = std::make_shared<Sketch3DEdgesTransient>(_previewEdges);
    this->setStepTwoTip();
    this->updatePanelState();
}

void SketchProjectCurve3DGuiCmd::updatePanelState()
{
    if (!_pPanel) return;

    // The panel answers one question - is there a curve to confirm - and it is read straight off
    // the preview rather than tracked separately, so the two cannot come to disagree.
    if (!_previewEdges.empty())
    {
        _pPanel->setProjection(SketchProjectCurvePanel::Projection::Ready);
        return;
    }

    if (_hasTarget)
    {
        _pPanel->setProjection(SketchProjectCurvePanel::Projection::NoProjection);
        return;
    }

    // Nothing is picked yet, so the line names the one thing this step is waiting for.
    _pPanel->setProjection(Step::SelectSketch == _step
        ? SketchProjectCurvePanel::Projection::NoSketch
        : SketchProjectCurvePanel::Projection::NoTarget);
}

void SketchProjectCurve3DGuiCmd::onEnterKey()
{
    // Enter means "commit what is on the screen" and nothing else: without a preview there is
    // nothing to confirm, and the command stays where it is.
    if (Step::SelectTarget != _step || !_hasTarget || _previewEdges.empty()) return;

    if (!this->commit()) return;
    this->requestEnd();
}

void SketchProjectCurve3DGuiCmd::onSpaceKey()
{
    this->onEnterKey();
}

bool SketchProjectCurve3DGuiCmd::isContextMenuActionVisible_CompleteSelection() const
{
    return Step::SelectTarget == _step && _hasTarget && !_previewEdges.empty();
}

void SketchProjectCurve3DGuiCmd::onContextMenuAction_CompleteSelection()
{
    this->onEnterKey();
}

bool SketchProjectCurve3DGuiCmd::isContextMenuActionVisible_ClearSelection() const
{
    return Step::SelectTarget == _step && _hasTarget;
}

void SketchProjectCurve3DGuiCmd::onContextMenuAction_ClearSelection()
{
    // The discoverable half of "click the target again to take it back".
    if (!_hasTarget) return;
    this->clearTarget();
}

bool SketchProjectCurve3DGuiCmd::commit()
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return false;
    }

    // One action, one transaction, one undo step. Nothing is computed in here: the edges were
    // settled when the preview was built, so what was on the screen is what gets created.
    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans)
    {
        assert(false);
        return false;
    }

    wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(_sketch3DInfo.sketch3dId));
    if (!pSketch3D)
    {
        pDb->getTransactionManager()->abortTransaction();
        return false;
    }

    // All or nothing: half of a projection is worse than none of it, and a partial result would
    // also break the one action equals one undo step rule.
    for (const TopoDS_Edge& edge : _previewEdges)
    {
        wy3d::SketchEntity3D* pEntity = nullptr;
        const wy3d::Sketch3DEdgeUtil::Result convertResult =
            wy3d::Sketch3DEdgeUtil::convert(pTrans, edge, pEntity);
        if (wy3d::Sketch3DEdgeUtil::Result::Ok != convertResult || !pEntity ||
            wy::ErrorStatus::Ok != pSketch3D->addEntity(pEntity))
        {
            pDb->getTransactionManager()->abortTransaction();
            this->reportCreateFailure(convertResult);
            return false;
        }
    }

    if (wy::ErrorStatus::Ok != pDb->getTransactionManager()->endTransaction())
    {
        pDb->getTransactionManager()->abortTransaction();
        return false;
    }

    return true;
}

void SketchProjectCurve3DGuiCmd::setStepOneTip() const
{
    Application::instance().getStatusBar()->setTips(QCoreApplication::translate(
        "SketchProjectCurve3DGuiCmd", "Select the sketch to project."));
}

void SketchProjectCurve3DGuiCmd::setStepTwoTip() const
{
    // Enter is only offered once there is something to confirm, so the tip does not ask for a key
    // that would do nothing.
    QString tip = _previewEdges.empty()
        ? QCoreApplication::translate("SketchProjectCurve3DGuiCmd",
            "Select the face or datum plane to project onto.")
        : QCoreApplication::translate("SketchProjectCurve3DGuiCmd",
            "Projection curve computed. Press Enter or Spacebar to confirm.");

    // Tab is offered on the panel's terms, which is also when it does something: with Bidirectional
    // on the panel greys the box out and there is no direction left to flip.
    if (_pPanel && _pPanel->isDirectionAvailable() && !_pPanel->isBidirectional())
    {
        tip += QLatin1Char(' ');
        tip += QCoreApplication::translate("SketchProjectCurve3DGuiCmd", "Press Tab to flip the direction.");
    }

    Application::instance().getStatusBar()->setTips(tip);
}

// The failures here are recoverable and expected to happen over and over - turning on reverse
// when there is nothing on that side is a normal thing to try - so they go to the status bar
// rather than a modal box, which would interrupt the flow and could take the keyboard away from
// the viewport. The preview disappearing is the other half of the news, and it is already visible.
void SketchProjectCurve3DGuiCmd::reportFailure(wy3d::Sketch3DProjectionUtil::Result result) const
{
    QString message;
    switch (result)
    {
    case wy3d::Sketch3DProjectionUtil::Result::NoProjection:
        message = QCoreApplication::translate("SketchProjectCurve3DGuiCmd",
            "No projection curve. The sketch does not reach the picked face or plane; try Reverse or Bidirectional.");
        break;
    case wy3d::Sketch3DProjectionUtil::Result::AlgorithmFailed:
        message = QCoreApplication::translate("SketchProjectCurve3DGuiCmd",
            "Projection failed: the projection algorithm did not complete.");
        break;
    case wy3d::Sketch3DProjectionUtil::Result::UnsupportedCurve:
        message = QCoreApplication::translate("SketchProjectCurve3DGuiCmd",
            "Projection failed: the resulting curve is more complex than the sketch can hold.");
        break;
    case wy3d::Sketch3DProjectionUtil::Result::InvalidInput:
        message = QCoreApplication::translate("SketchProjectCurve3DGuiCmd",
            "Projection failed: the sketch and the picked face or datum plane cannot be used together.");
        break;
    default:
        message = QCoreApplication::translate("SketchProjectCurve3DGuiCmd",
            "Projection failed: the geometry is invalid.");
        break;
    }

    Application::instance().getStatusBar()->setTips(message);
}

void SketchProjectCurve3DGuiCmd::reportCreateFailure(wy3d::Sketch3DEdgeUtil::Result result) const
{
    QString message;
    switch (result)
    {
    case wy3d::Sketch3DEdgeUtil::Result::Degenerate:
        message = QCoreApplication::translate("SketchProjectCurve3DGuiCmd",
            "Projection failed: the resulting curve is degenerate. The sketch may be tangent to the face or datum plane.");
        break;
    case wy3d::Sketch3DEdgeUtil::Result::InfiniteCurve:
        message = QCoreApplication::translate("SketchProjectCurve3DGuiCmd",
            "Projection failed: the resulting curve is unbounded.");
        break;
    case wy3d::Sketch3DEdgeUtil::Result::UnsupportedType:
    case wy3d::Sketch3DEdgeUtil::Result::CreateFailed:
        message = QCoreApplication::translate("SketchProjectCurve3DGuiCmd",
            "Projection failed: the resulting curve cannot be created in the sketch.");
        break;
    default:
        message = QCoreApplication::translate("SketchProjectCurve3DGuiCmd",
            "Projection failed: the resulting curve geometry is invalid.");
        break;
    }

    QMessageBox::warning(nullptr,
        QCoreApplication::translate("SketchProjectCurve3DGuiCmd", "Project Curve"),
        message);
}

void SketchProjectCurve3DGuiCmd::createPanel()
{
    assert(!_pPanel);

    MainWindow* pMainWindow = Application::instance().getMainWindow();
    QOpenGLWidget* pViewport = pMainWindow ? pMainWindow->findChild<QOpenGLWidget*>() : nullptr;
    // No viewport means no 3D picking either, and the command is not worth refusing over a panel:
    // without one every projection is the plain forward sweep.
    if (!pViewport) return;

    _pPanel = new SketchProjectCurvePanel(pViewport);

    // The panel is its own context: the command is not a QObject. Deleting the panel in
    // destroyPanel disconnects this, so the lambda cannot outlive the command.
    QObject::connect(_pPanel, &SketchProjectCurvePanel::optionsChanged,
        _pPanel, [this]() { this->updateDirectionArrow(); this->recomputePreview(); });
    // The panel's own two buttons are the mouse route to what Enter and Esc already do.
    QObject::connect(_pPanel, &FloatingCmdPanel::accepted,
        _pPanel, [this]() { this->onEnterKey(); });
    QObject::connect(_pPanel, &FloatingCmdPanel::canceled,
        _pPanel, [this]() { this->onEscapeKey(); });

    _pPanel->show();
}

void SketchProjectCurve3DGuiCmd::destroyPanel()
{
    if (!_pPanel) return;

    _pPanel->hide();
    delete _pPanel;
    _pPanel = nullptr;
}
