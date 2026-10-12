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

#include "SketchDrawArcTangentGuiCmd.h"

#include <QCoreApplication>
#include <QCursor>
#include <QString>
#include <cmath>

#include <wyVector2.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapSelManager.h>
#include <wy3dImpl.h>
#include <wy3dSketch.h>
#include <wy3dSketchArc.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dSketchCurve.h>

#include <wy3dApp/Application.h>
#include "commands/sketch/dialogs/GuiCmdHoverInputPopup.h"
#include "widgets/frame/MainWindow.h"
#include "widgets/frame/ViewWidget.h"
#include "widgets/frame/ViewWidgetContainer.h"
#include "scene/Colors.h"
#include "snap/SketchSnapSystem.h"
#include "wy3dAppDefs.h"

namespace wy3dApp {

static bool parseDoubleText(const QString& text, double& value)
{
    bool ok(false);
    value = text.trimmed().toDouble(&ok);
    return ok;
}


// Solves the arc through startPnt and endPnt whose traversal leaves startPnt along the unit
// tangent: the center lies on the normal line C = startPnt + k * perpCCW(tangent), where
// k = |SE|^2 / (2 * perpCCW(tangent)·SE), so the circle is unique. A SketchArc is always CCW
// from the start angle to the end angle: when k < 0 the arc leaving the start point along the
// tangent runs from the end point to the start point, so the stored angles are swapped.
static bool solveTangentArc(const wy::Vector2& startPnt, const wy::Vector2& tangent,
    const wy::Vector2& endPnt, wy::Vector2& center, double& radius,
    double& startAngle, double& endAngle)
{
    wy::Vector2 vec = endPnt - startPnt;
    double chord = vec.length();
    if (chord < wy3d::kMinValue)
    {
        return false;
    }

    wy::Vector2 normal(-tangent.y(), tangent.x());
    double offset = normal.dot(vec);
    if (std::fabs(offset) < wy3d::TOL * chord) // the end point lies on the tangent line
    {
        return false;
    }

    double k = chord * chord / (2.0 * offset);
    radius = std::fabs(k);
    if (radius < wy3d::kMinValue || radius > wy3d::kMaxValue)
    {
        return false;
    }

    center = startPnt + k * normal;
    double angleAtStartPnt = std::atan2(startPnt.y() - center.y(), startPnt.x() - center.x());
    double angleAtEndPnt = std::atan2(endPnt.y() - center.y(), endPnt.x() - center.x());
    if (k > 0.0)
    {
        startAngle = angleAtStartPnt;
        endAngle = angleAtEndPnt;
    }
    else
    {
        startAngle = angleAtEndPnt;
        endAngle = angleAtStartPnt;
    }
    return true;
}

// Gets the endpoint of the snapped sketch curve and the outward tangent at that endpoint.
// Only EndPoint snaps that resolve to a curve of the current sketch and coincide with one of
// its end points are accepted; circle/ellipse quadrant snaps and 3D snaps are rejected.
static bool getEndpointTangentFromSnap(const SketchSnapResult* pSnapResult, const wydb::Database* pDb,
    const wydb::ElementId& sketchId, wy::Vector2& endpoint, wy::Vector2& outwardDir)
{
    if (!pSnapResult || !pDb)
    {
        return false;
    }

    for (const SketchSnapResultItem& snapRetItem : pSnapResult->getItems())
    {
        const SketchSnapObject* pSnapObject = snapRetItem.pSnapObject.get();
        if (!pSnapObject || SketchSnapType::EndPoint != pSnapObject->getType())
        {
            continue;
        }

        const wy3d::SketchCurve* pSketchCurve = wy3d::SketchCurve::cast(pDb->getElement(pSnapObject->getId()));
        if (!pSketchCurve || pSketchCurve->isClosed() || pSketchCurve->getParent() != sketchId)
        {
            continue;
        }

        const wy::Vector2 snapPos = static_cast<const PointSnapObject*>(pSnapObject)->getValue();
        const wy::Vector2 curveStartPnt = pSketchCurve->getStartPoint();
        const wy::Vector2 curveEndPnt = pSketchCurve->getEndPoint();

        // 1.0 for the curve end point, 0.0 for the curve start point
        double param(0.0);
        if ((snapPos - curveEndPnt).length() <= wy3d::kMinValue)
        {
            param = 1.0;
        }
        else if ((snapPos - curveStartPnt).length() <= wy3d::kMinValue)
        {
            param = 0.0;
        }
        else
        {
            continue;
        }

        // SketchCenterLine does not override getDirectionAt, use its travel direction instead
        if (const wy3d::SketchCenterLine* pCenterLine = wy3d::SketchCenterLine::cast(pSketchCurve))
        {
            outwardDir = pCenterLine->getDirection();
        }
        else
        {
            outwardDir = pSketchCurve->getDirectionAt(param);
        }
        if (outwardDir.normalize() < wy3d::TOL)
        {
            continue;
        }

        // Outward means away from the curve body: at the start point it is opposite to the travel direction
        if (param < 0.5)
        {
            outwardDir = -outwardDir;
        }

        endpoint = (param < 0.5) ? curveStartPnt : curveEndPnt;
        return true;
    }

    return false;
}

SketchDrawArcTangentGuiCmd::SketchDrawArcTangentGuiCmd()
    : OsgGuiCommand(),
    _step(Step::Undefined), _mode(ArcTangentMode::PassPoint),
    _startPnt(), _startTangent(1.0, 0.0), _endPnt(), _pSnapContext(nullptr),
    _center(), _radius(0.0),
    _pModePanel(nullptr), _pXYPopup(nullptr), _pRadiusPopup(nullptr), _pSweepPopup(nullptr),
    _hoverPopupState()
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

SketchDrawArcTangentGuiCmd::~SketchDrawArcTangentGuiCmd()
{
}

wyap::CmdExecution::StartResult SketchDrawArcTangentGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = GuiCommand::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    // A command instance may be reused: the mode always starts from the default
    _mode = ArcTangentMode::PassPoint;

    _sketchInfo = GuiCommandUtil::initSketchInfo();
    if (_sketchInfo.pSketchSnapSys) _sketchInfo.pSketchSnapSys->clearSnapResult();

    Application::instance().getSelManager()->beginChange();
    Application::instance().getSelManager()->clearSelections();
    Application::instance().getSelManager()->endChange();

    this->gotoStep(Step::SpecifyStartPoint);

    return wyap::CmdExecution::StartResult::Succeeded;
}

void SketchDrawArcTangentGuiCmd::onEnd()
{
    this->destroyModePanel();

    GuiCommand::onEnd();
}

void SketchDrawArcTangentGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    this->destroyModePanel();

    GuiCommand::onAbort(cause);
}

void SketchDrawArcTangentGuiCmd::showModePanel()
{
    if (!_pModePanel)
    {
        // The view widget of the active document. findChild<QOpenGLWidget*>() would answer with
        // the first document's viewport instead whenever more than one document is open.
        QWidget* pParentWidget = nullptr;
        MainWindow* pMainWindow = Application::instance().getMainWindow();
        wyap::Document* pActiveDoc = Application::instance().getActiveDocument();
        if (pMainWindow && pActiveDoc)
        {
            if (ViewWidgetContainer* pViewWidgetContainer = pMainWindow->getViewWidgetContainer())
            {
                pParentWidget = pViewWidgetContainer->getViewWidget(pActiveDoc);
            }
        }
        if (!pParentWidget)
        {
            assert(false);
            return;
        }

        // Created on first use and only hidden afterwards, so a position the user dragged it to
        // survives the steps where the panel is not needed.
        _pModePanel = new SketchArcTangentModePanel(pParentWidget);

        // The sketch overlay bar (cancel / finish sketch) owns the top-left corner the panel would
        // anchor to by default: sit right below it instead.
        if (QWidget* pOverlayBar = pParentWidget->findChild<QWidget*>(QStringLiteral("viewportOverlayBar")))
        {
            _pModePanel->setAnchorOffset(QPoint(8, pOverlayBar->y() + pOverlayBar->height() + 8));
        }

        QObject::connect(_pModePanel, &SketchArcTangentModePanel::modeChanged,
            _pModePanel, [this](ArcTangentMode mode) { this->onModeChanged(mode); });
    }

    // The mode lives for the whole command session: sync the buttons on every show
    _pModePanel->setMode(_mode);
    _pModePanel->show();
}

void SketchDrawArcTangentGuiCmd::hideModePanel()
{
    if (_pModePanel)
    {
        _pModePanel->hide();
    }
}

void SketchDrawArcTangentGuiCmd::destroyModePanel()
{
    if (_pModePanel)
    {
        _pModePanel->hide();
        delete _pModePanel;
        _pModePanel = nullptr;
    }
}

void SketchDrawArcTangentGuiCmd::onModeChanged(ArcTangentMode mode)
{
    if (_mode == mode)
    {
        return;
    }

    _mode = mode;
    if (_step == Step::SpecifyStartPoint || _step == Step::Undefined)
    {
        return; // nothing in flight, the new mode starts with the next arc
    }

    // Keep the anchored start point, drop the in-flight end definition and enter the new mode
    _endPnt.set(0.0, 0.0);
    if (_pArcTransient) _pArcTransient->hide();
    this->gotoStep(this->entryStepForMode());
    this->simulateMouseMoveFromPopup();
}

void SketchDrawArcTangentGuiCmd::cleanup()
{
    this->hidePopup();

    _step = Step::Undefined;
    _startPnt.set(0.0, 0.0);
    _startTangent.set(1.0, 0.0);
    _endPnt.set(0.0, 0.0);
    _pSnapContext = nullptr;
    _center.set(0.0, 0.0);
    _radius = 0.0;
    _pArcTransient = nullptr;
    _pCenterPointTransient = nullptr;

    Application::instance().getSnapSystem()->clearSnapResult();
    if (_sketchInfo.pSketchSnapSys) _sketchInfo.pSketchSnapSys->clearSnapResult();
}

void SketchDrawArcTangentGuiCmd::reset()
{
    this->cleanup();
    this->gotoStep(Step::SpecifyStartPoint);
}

void SketchDrawArcTangentGuiCmd::onEscapeKey()
{
    this->hidePopup();

    if (_step == Step::SpecifyStartPoint || _step == Step::Undefined)
    {
        this->requestAbort(AbortCause::UserCancel);
    }
    else if (_step == Step::SpecifySweepAngle)
    {
        // Back to the radius step, keeping the anchored start point
        this->gotoStep(Step::SpecifyRadius);
        this->simulateMouseMoveFromPopup();
    }
    else
    {
        this->reset();
        this->simulateMouseMoveFromPopup();
    }
}

bool SketchDrawArcTangentGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SpecifyStartPoint:
    {
        this->gotoStep(this->entryStepForMode());
        return true;
    }
    break;

    case Step::SpecifyRadius:
    {
        this->gotoStep(Step::SpecifySweepAngle);
        return true;
    }
    break;

    case Step::SpecifyEndPoint:
    case Step::SpecifySweepAngle:
    {
        if (!this->createArc())
        {
            return false;
        }

        _pArcTransient = nullptr;
        _pCenterPointTransient = nullptr;
        _startPnt.set(0.0, 0.0);
        _startTangent.set(1.0, 0.0);
        _endPnt.set(0.0, 0.0);
        this->gotoStep(Step::SpecifyStartPoint);
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

SketchDrawArcTangentGuiCmd::Step SketchDrawArcTangentGuiCmd::entryStepForMode() const
{
    switch (_mode)
    {
    case ArcTangentMode::PassPoint:
        return Step::SpecifyEndPoint;
    case ArcTangentMode::Radius:
        return Step::SpecifyRadius;
    }

    assert(false);
    return Step::SpecifyEndPoint;
}

void SketchDrawArcTangentGuiCmd::gotoStep(Step step)
{
    _step = step;
    this->hidePopup();
    _hoverPopupState.resetValue();

    Application::instance().getSnapSystem()->clearSnapResult();
    if (_sketchInfo.pSketchSnapSys) _sketchInfo.pSketchSnapSys->clearSnapResult();

    switch (step)
    {
    case Step::SpecifyStartPoint:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawArcTangent",
            "Specify the start point on an endpoint of an existing curve; the arc is tangent to the curve."));
        Application::instance().setCursor(CursorType::Locate);

        if (_sketchInfo.pSketchSnapSys)
        {
            _sketchInfo.pSketchSnapSys->partiallyUpdate(Application::instance().getActiveDatabase());
        }
        _pSnapContext = std::make_shared<SketchCurveEndPointContext>(wydb::ElementId::kNull);
    }
    break;

    case Step::SpecifyEndPoint:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawArcTangent",
            "Specify the end point of the arc; you can directly input the coordinate values."));
        Application::instance().setCursor(CursorType::Locate);

        _pSnapContext = std::make_shared<SketchLocateContext>(wydb::ElementId::kNull);

        _pArcTransient = std::make_shared<SketchArcTransient>(
            _sketchInfo.sketchPlane, Colors::kSketchEntity, 2.0f);
    }
    break;

    case Step::SpecifyRadius:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawArcTangent",
            "Specify the radius; you can directly input the value."));
        Application::instance().setCursor(CursorType::Locate);

        _pSnapContext = std::make_shared<SketchLocateContext>(wydb::ElementId::kNull);

        _pArcTransient = std::make_shared<SketchArcTransient>(
            _sketchInfo.sketchPlane, Colors::kSketchEntity, 2.0f);
    }
    break;

    case Step::SpecifySweepAngle:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawArcTangent",
            "Specify the total angle; you can directly input the value."));
        Application::instance().setCursor(CursorType::Locate);

        _pSnapContext = std::make_shared<SketchDrawArcContext>(wydb::ElementId::kNull, _center, _radius, _startPnt);

        _pArcTransient = std::make_shared<SketchArcTransient>(
            _sketchInfo.sketchPlane, Colors::kSketchEntity, 2.0f);
    }
    break;

    default:
    {
        Application::instance().getStatusBar()->setTips("");
        assert(false);
    }
    break;
    }

    // The panel belongs to the second step: it appears once the start point is anchored and goes
    // away again while the command waits for the next one.
    if (step == Step::SpecifyStartPoint)
    {
        this->hideModePanel();
    }
    else
    {
        this->showModePanel();
    }
}

void SketchDrawArcTangentGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (event.x != _hoverPopupState.lastMouseX ||
        event.y != _hoverPopupState.lastMouseY) // moved
    {
        this->hidePopup();
        _hoverPopupState.lastMouseX = event.x;
        _hoverPopupState.lastMouseY = event.y;
        _hoverPopupState.lastMouseMoveTime = event.time;
    }

    switch (_step)
    {
    case Step::SpecifyStartPoint:
    {
        this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane,
            this->getSnapExcludeIds(), _pSnapContext, _sketchInfo.pSketchSnapSys, false); // snap3d = false
    }
    break;

    case Step::SpecifyEndPoint:
    {
        _endPnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, this->getSnapExcludeIds(), _pSnapContext, _sketchInfo.pSketchSnapSys);
        _hoverPopupState.point = _endPnt;
        this->updatePreview();
    }
    break;

    case Step::SpecifyRadius:
    {
        _endPnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, this->getSnapExcludeIds(), _pSnapContext, _sketchInfo.pSketchSnapSys);
        _hoverPopupState.point = _endPnt;

        // The live radius the popup offers: the radius of the arc through the cursor
        wy::Vector2 center;
        double radius(0.0), startAngle(0.0), endAngle(0.0);
        _hoverPopupState.radius = solveTangentArc(_startPnt, _startTangent, _endPnt,
            center, radius, startAngle, endAngle) ? radius : 0.0;
        this->updatePreview();
    }
    break;

    case Step::SpecifySweepAngle:
    {
        const wy::Vector2 cursorPnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, this->getSnapExcludeIds(), _pSnapContext, _sketchInfo.pSketchSnapSys);
        if ((cursorPnt - _center).length() < wy3d::kMinValue)
        {
            break; // too close to the center to read an angle from
        }

        // The angle the cursor sits at, counted in the frozen bend direction
        const double startAngle = std::atan2(_startPnt.y() - _center.y(), _startPnt.x() - _center.x());
        const double cursorAngle = std::atan2(cursorPnt.y() - _center.y(), cursorPnt.x() - _center.x());
        const double delta = cursorAngle - startAngle;
        wy::Vector2 normal(-_startTangent.y(), _startTangent.x());
        const double sweepAngle = (normal.dot(_center - _startPnt) > 0.0)
            ? std::fmod(delta + wy3d::TWO_PI, wy3d::TWO_PI)
            : std::fmod(-delta + wy3d::TWO_PI, wy3d::TWO_PI);

        this->setSweepPreview(sweepAngle);
    }
    break;

    default:
    break;
    }
}

void SketchDrawArcTangentGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    this->hidePopup();
    _hoverPopupState.lastMouseX = event.x;
    _hoverPopupState.lastMouseY = event.y;
    _hoverPopupState.lastMouseMoveTime = event.time;

    switch (_step)
    {
    case Step::SpecifyStartPoint:
    {
        this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane,
            this->getSnapExcludeIds(), _pSnapContext, _sketchInfo.pSketchSnapSys, false); // snap3d = false

        wy::Vector2 startPnt, tangent;
        if (!this->getEndpointSnap(startPnt, tangent))
        {
            return;
        }

        _startPnt = startPnt;
        _startTangent = tangent;
        if (this->finishStep(Step::SpecifyStartPoint))
        {
            this->onMouseMove(event);
        }
    }
    break;

    case Step::SpecifyEndPoint:
    {
        _endPnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, this->getSnapExcludeIds(), _pSnapContext, _sketchInfo.pSketchSnapSys);
        if (this->finishStep(Step::SpecifyEndPoint))
        {
            this->simulateMouseMoveFromPopup();
        }
    }
    break;

    case Step::SpecifyRadius:
    {
        // Refresh the live values from the click position, then take the cursor-side radius
        this->onMouseMove(event);
        if (_hoverPopupState.radius < wy3d::kMinValue)
        {
            break; // the cursor has no arc to read a radius from
        }

        this->commitRadius(_hoverPopupState.radius);
        if (this->finishStep(Step::SpecifyRadius))
        {
            this->simulateMouseMoveFromPopup();
        }
    }
    break;

    case Step::SpecifySweepAngle:
    {
        // Refresh the live values from the click position, then take the live angle
        this->onMouseMove(event);
        if (_hoverPopupState.sweepAngleDeg <= 0.0)
        {
            break;
        }

        if (this->finishStep(Step::SpecifySweepAngle))
        {
            this->simulateMouseMoveFromPopup();
        }
    }
    break;

    default:
    {
        assert(false);
    }
    break;
    }
}

void SketchDrawArcTangentGuiCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

void SketchDrawArcTangentGuiCmd::initializePopups()
{
    MainWindow* pMainWindow = Application::instance().getMainWindow();
    if (!_pXYPopup)
    {
        _pXYPopup = std::make_unique<GuiCmdHoverInputPopup2>(
            QStringLiteral("X"),
            QStringLiteral("Y"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pXYPopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pXYPopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pXYPopup->hide();
    }

    if (!_pRadiusPopup)
    {
        _pRadiusPopup = std::make_unique<GuiCmdHoverInputPopup1>(
            QCoreApplication::translate("SketchDrawArcTangent", "Radius"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pRadiusPopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pRadiusPopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pRadiusPopup->hide();
    }

    if (!_pSweepPopup)
    {
        _pSweepPopup = std::make_unique<GuiCmdHoverInputPopup1>(
            QCoreApplication::translate("SketchDrawArcTangent", "SweepAngle"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pSweepPopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pSweepPopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pSweepPopup->hide();
    }
}

void SketchDrawArcTangentGuiCmd::showPopup()
{
    if (!_pXYPopup || !_pRadiusPopup || !_pSweepPopup)
    {
        this->initializePopups();
    }

    GuiCmdHoverInputPopupBase* pActivePopup = this->getActivePopup();
    if (!pActivePopup)
    {
        return;
    }

    switch (_step)
    {
    case Step::SpecifyEndPoint:
    {
        static_cast<GuiCmdHoverInputPopup2*>(pActivePopup)->setValues(
            _hoverPopupState.point.x(),
            _hoverPopupState.point.y());
    }
    break;

    case Step::SpecifyRadius:
    {
        static_cast<GuiCmdHoverInputPopup1*>(pActivePopup)->setValue(_hoverPopupState.radius);
    }
    break;

    case Step::SpecifySweepAngle:
    {
        static_cast<GuiCmdHoverInputPopup1*>(pActivePopup)->setValue(_hoverPopupState.sweepAngleDeg);
    }
    break;

    default:
        return;
    }

    pActivePopup->showAtGlobal(QCursor::pos());
}

void SketchDrawArcTangentGuiCmd::hidePopup()
{
    if (_pXYPopup && _pXYPopup->isVisible())
    {
        _pXYPopup->hide();
    }
    if (_pRadiusPopup && _pRadiusPopup->isVisible())
    {
        _pRadiusPopup->hide();
    }
    if (_pSweepPopup && _pSweepPopup->isVisible())
    {
        _pSweepPopup->hide();
    }
}

GuiCmdHoverInputPopupBase* SketchDrawArcTangentGuiCmd::getActivePopup() const
{
    switch (_step)
    {
    case Step::SpecifyEndPoint:
        return _pXYPopup.get();
    case Step::SpecifyRadius:
        return _pRadiusPopup.get();
    case Step::SpecifySweepAngle:
        return _pSweepPopup.get();
    default:
        break;
    }
    return nullptr;
}

void SketchDrawArcTangentGuiCmd::tryShowPopupOnHover(double time)
{
    if (_step != Step::SpecifyEndPoint &&
        _step != Step::SpecifyRadius &&
        _step != Step::SpecifySweepAngle)
    {
        return;
    }
    if (_hoverPopupState.lastMouseMoveTime < 0.0)
    {
        return;
    }

    GuiCmdHoverInputPopupBase* pActivePopup = this->getActivePopup();
    if (pActivePopup && pActivePopup->isVisible())
    {
        return;
    }
    if (time - _hoverPopupState.lastMouseMoveTime >= 0.45)
    {
        this->showPopup();
    }
}

void SketchDrawArcTangentGuiCmd::onPopupEnterKey()
{
    GuiCmdHoverInputPopupBase* pActivePopup = this->getActivePopup();
    if (!pActivePopup)
    {
        return;
    }

    if (_step == Step::SpecifyEndPoint)
    {
        // An empty field falls back to the current hovered value
        double x(_hoverPopupState.point.x());
        double y(_hoverPopupState.point.y());
        const QString xText = static_cast<GuiCmdHoverInputPopup2*>(pActivePopup)->getRow1Text().trimmed();
        if (!xText.isEmpty() && !parseDoubleText(xText, x))
        {
            return;
        }
        const QString yText = static_cast<GuiCmdHoverInputPopup2*>(pActivePopup)->getRow2Text().trimmed();
        if (!yText.isEmpty() && !parseDoubleText(yText, y))
        {
            return;
        }

        _endPnt.set(x, y);
        if (this->finishStep(Step::SpecifyEndPoint))
        {
            this->simulateMouseMoveFromPopup();
        }
        return;
    }

    if (_step == Step::SpecifyRadius)
    {
        double radius(0.0);
        const QString text = static_cast<GuiCmdHoverInputPopup1*>(pActivePopup)->getRowText().trimmed();
        if (text.isEmpty() || !parseDoubleText(text, radius))
        {
            return;
        }

        radius = std::fabs(radius);
        if (radius < wy3d::kMinValue || radius > wy3d::kMaxValue)
        {
            return;
        }

        this->commitRadius(radius);
        if (this->finishStep(Step::SpecifyRadius))
        {
            this->simulateMouseMoveFromPopup();
        }
        return;
    }

    if (_step == Step::SpecifySweepAngle)
    {
        double sweepAngleDeg(0.0);
        const QString text = static_cast<GuiCmdHoverInputPopup1*>(pActivePopup)->getRowText().trimmed();
        if (text.isEmpty() || !parseDoubleText(text, sweepAngleDeg))
        {
            return;
        }
        if (sweepAngleDeg <= 0.0 || sweepAngleDeg >= 360.0)
        {
            return;
        }

        this->setSweepPreview(wy3d::degreesToRadians(sweepAngleDeg));
        if (this->finishStep(Step::SpecifySweepAngle))
        {
            this->simulateMouseMoveFromPopup();
        }
        return;
    }
}

void SketchDrawArcTangentGuiCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void SketchDrawArcTangentGuiCmd::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX)
        return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}

std::set<wydb::ElementId> SketchDrawArcTangentGuiCmd::getSnapExcludeIds() const
{
    std::set<wydb::ElementId> snapExcludeIds;
    snapExcludeIds.insert(_sketchInfo.sketchId);
    return snapExcludeIds;
}

bool SketchDrawArcTangentGuiCmd::getEndpointSnap(wy::Vector2& startPnt, wy::Vector2& tangent) const
{
    if (!_sketchInfo.pSketchSnapSys)
    {
        return false;
    }

    SketchSnapResultSPtr pSnapResult = _sketchInfo.pSketchSnapSys->getSnapResult();
    const wydb::Database* pDb = Application::instance().getActiveDatabase();
    return getEndpointTangentFromSnap(pSnapResult.get(), pDb, _sketchInfo.sketchId, startPnt, tangent);
}

void SketchDrawArcTangentGuiCmd::commitRadius(double radius)
{
    // The bend side is the side the cursor last hovered on; a cursor exactly on the tangent line
    // keeps the counterclockwise arc.
    wy::Vector2 normal(-_startTangent.y(), _startTangent.x());
    const double side = (normal.dot(_hoverPopupState.point - _startPnt) >= 0.0) ? 1.0 : -1.0;
    _radius = radius;
    _center = _startPnt + (side * radius) * normal;
}

void SketchDrawArcTangentGuiCmd::setSweepPreview(double sweepAngle)
{
    // The bend direction was frozen with the radius: the center sits on the side the sweep turns
    // towards, so a positive sweep always follows it
    const double startAngle = std::atan2(_startPnt.y() - _center.y(), _startPnt.x() - _center.x());
    wy::Vector2 normal(-_startTangent.y(), _startTangent.x());
    const double endAngle = (normal.dot(_center - _startPnt) > 0.0)
        ? startAngle + sweepAngle
        : startAngle - sweepAngle;

    _endPnt = _center + _radius * wy::Vector2(std::cos(endAngle), std::sin(endAngle));
    _hoverPopupState.sweepAngleDeg = wy3d::radiansToDegrees(sweepAngle);
    this->updatePreview();
}

void SketchDrawArcTangentGuiCmd::updatePreview()
{
    if (!_pArcTransient)
    {
        return;
    }

    wy::Vector2 center;
    double radius(0.0), startAngle(0.0), endAngle(0.0);
    if (!solveTangentArc(_startPnt, _startTangent, _endPnt, center, radius, startAngle, endAngle))
    {
        _pArcTransient->hide();
        if (_pCenterPointTransient) _pCenterPointTransient->hide();
        return;
    }

    _pArcTransient->update(center, radius, startAngle, endAngle);
    _pArcTransient->show();

    if (!_pCenterPointTransient)
    {
        _pCenterPointTransient = std::make_shared<CenterPointTransient>();
    }
    _pCenterPointTransient->update(_sketchInfo.sketchPlane, center);
    _pCenterPointTransient->show();
}

bool SketchDrawArcTangentGuiCmd::createArc()
{
    wy::Vector2 center;
    double radius(0.0), startAngle(0.0), endAngle(0.0);
    if (!solveTangentArc(_startPnt, _startTangent, _endPnt, center, radius, startAngle, endAngle))
    {
        return false;
    }

    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return false;
    }

    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    wy3d::Sketch* pSketch = nullptr;
    wy3d::SketchArc* pSketchArc = nullptr;
    wydb::Element* pSketchElem = pTrans->getElementForWrite(_sketchInfo.sketchId);
    if (!pSketchElem) goto ABORT_TRANS;
    pSketch = wy3d::Sketch::cast(pSketchElem);
    if (!pSketch) goto ABORT_TRANS;
    if (wy::ErrorStatus::Ok != wy3d::SketchArc::create(pTrans, center, radius, startAngle, endAngle, pSketchArc) || !pSketchArc)
    {
        goto ABORT_TRANS;
    }
    if (wy::ErrorStatus::Ok != pSketch->addEntity(pSketchArc))
    {
        goto ABORT_TRANS;
    }
    pDb->getTransactionManager()->endTransaction();
    return true;

ABORT_TRANS:
    assert(false);
    pDb->getTransactionManager()->abortTransaction();
    return false;
}

}
