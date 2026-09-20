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

#include "Sketch3DExtendGuiCmd.h"

#include <cassert>
#include <cfloat>
#include <cmath>
#include <vector>

#include <QCoreApplication>

#include <Geom_BSplineCurve.hxx>

#include <utils/wy3dSketch3DCurveParam.h>
#include <utils/wy3dSketch3DSplineUtil.h>
#include <wy3dImpl.h>
#include <wy3dMath.h>
#include <wy3dSelectionType.h>
#include <wy3dSketchEntity3D.h>

#include "application/Application.h"
#include "scene/nodes/ElementNodeType.h"
#include "select/filters/CommonSelFilters.h"

Sketch3DExtendGuiCmd::Sketch3DExtendGuiCmd() : OsgGuiCommand()
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

Sketch3DExtendGuiCmd::~Sketch3DExtendGuiCmd()
{
}

void Sketch3DExtendGuiCmd::onDatabaseChanged(
    const wydb::Database* pDb,
    const wydb::Transaction* pTransaction,
    const wydb::DatabaseChangeInfo& changeInfo)
{
    assert(pDb);

    // Extending adds and erases nothing, so either means an undo or a redo went through: the knots
    // were recorded against curves that no longer exist and the whole graph has to be built again.
    if (!changeInfo.addedIds.empty() || !changeInfo.erasedIds.empty())
    {
        this->initExtendGraph();
        _pTransient = nullptr;
        _pickedId = wydb::ElementId::kNull;
        _pickedSegment = wy3d::Sketch3DExtendSegment();
        return;
    }

    for (const wydb::ElementId& id : changeInfo.modifiedIds)
    {
        if (!wy3d::SketchCurve3D::cast(pDb->getElement(id))) continue;
        wy3d::Sketch3DExtendNodeSPtr pNode = _pExtendGraph->getNode(id);
        assert(pNode);
        if (pNode) pNode->refresh(pDb);
    }
}

wyap::CmdExecution::StartResult Sketch3DExtendGuiCmd::onStart()
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

    if (!this->initExtendGraph())
    {
        assert(false);
        return wyap::CmdExecution::StartResult::Failed;
    }

    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return wyap::CmdExecution::StartResult::Failed;
    }
    pDb->addReactor(this);

    _pTransient = nullptr;
    _pickedId = wydb::ElementId::kNull;
    _pickedSegment = wy3d::Sketch3DExtendSegment();
    Application::instance().setCursor(CursorType::SelectElements);
    Application::instance().getStatusBar()->setTips(QCoreApplication::translate("Sketch3DExtendGuiCmd",
        "Select sketch entities to extend."));

    _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Sketch3DEntity);
    _pointPickOption.selType = wy3d::SelectionType::Element;
    _pointPickOption.pSelPreFilter = std::make_shared<CommonPreSelFilterForPointPick>(
        wy3d::SketchEntity3D::classInfo());

    return wyap::CmdExecution::StartResult::Succeeded;
}

void Sketch3DExtendGuiCmd::onEnd()
{
    _pTransient = nullptr;
    if (wydb::Database* pDb = Application::instance().getActiveDatabase())
    {
        pDb->removeReactor(this);
    }
    GuiCommand::onEnd();
}

void Sketch3DExtendGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    _pTransient = nullptr;
    if (wydb::Database* pDb = Application::instance().getActiveDatabase())
    {
        pDb->removeReactor(this);
    }
    GuiCommand::onAbort(cause);
}

bool Sketch3DExtendGuiCmd::initExtendGraph()
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return false;
    }
    const wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pDb->getElement(_sketch3DInfo.sketch3dId));
    if (!pSketch3D)
    {
        assert(false);
        return false;
    }
    _pExtendGraph = std::make_unique<wy3d::Sketch3DExtendGraph>(pSketch3D, 1e-6);
    if (!_pExtendGraph->isValid())
    {
        assert(false);
        _pExtendGraph = nullptr;
        return false;
    }
    return true;
}

void Sketch3DExtendGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (!this->getOsgView()) return;

    const std::pair<wydb::ElementId, wy::Vector3> pickRet =
        this->pointPickElement(event.x, event.y, _pointPickOption);
    this->pickExtendSegment(pickRet.first, pickRet.second);

    Application::instance().setCursor(
        (!pickRet.first.isNull() && !_pTransient) ? CursorType::Forbid : CursorType::SelectElements);
}

void Sketch3DExtendGuiCmd::pickExtendSegment(const wydb::ElementId& id, const wy::Vector3& pickPos3d)
{
    if (id.isNull())
    {
        _pTransient = nullptr;
        _pickedId = wydb::ElementId::kNull;
        return;
    }

    assert(_pExtendGraph);
    const wy3d::Sketch3DExtendSegment segment = _pExtendGraph->pick(id, pickPos3d);
    if (!segment.isValid())
    {
        _pTransient = nullptr;
        _pickedId = wydb::ElementId::kNull;
        return;
    }

    // Keep the same preview while the stretch under the cursor does not change.
    if (_pTransient && _pickedId == id
        && _pickedSegment.startKnot.getParam() == segment.startKnot.getParam()
        && _pickedSegment.endKnot.getParam() == segment.endKnot.getParam())
    {
        return;
    }

    const wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        _pTransient = nullptr;
        _pickedId = wydb::ElementId::kNull;
        return;
    }
    const wy3d::SketchCurve3D* pCurve = wy3d::SketchCurve3D::cast(pDb->getElement(id));
    if (!pCurve)
    {
        assert(false);
        _pTransient = nullptr;
        _pickedId = wydb::ElementId::kNull;
        return;
    }

    _pickedId = id;
    _pickedSegment = segment;
    _pTransient = std::make_shared<Sketch3DTrimExtendTransient>(pCurve,
        segment.startKnot.getParam(), segment.endKnot.getParam());
}

void Sketch3DExtendGuiCmd::onLeftMouseUp(const MouseEvent& event)
{
    if (_pTransient)
    {
        assert(_pickedSegment.isValid());
        if (_pickedSegment.isValid())
        {
            if (_pickedSegment.startKnot.getParam() == 0.0 && _pickedSegment.endKnot.getParam() == 1.0)
            {
                // Already the whole curve. A line whose start can grow but whose end cannot reads
                // this way once the cursor sits on the end, and there is nothing to do.
            }
            else
            {
                const bool extended = this->extend(_pickedId,
                    _pickedSegment.startKnot, _pickedSegment.endKnot);
                assert(extended);
            }
        }
    }

    _pTransient = nullptr;
    _pickedId = wydb::ElementId::kNull;
    _pickedSegment = wy3d::Sketch3DExtendSegment();
    Application::instance().setCursor(CursorType::SelectElements);
}

bool Sketch3DExtendGuiCmd::extend(const wydb::ElementId curveId,
    const wy3d::Sketch3DExtendKnot& startKnot, const wy3d::Sketch3DExtendKnot& endKnot)
{
    const double startParam = startKnot.getParam();
    if (std::isnan(startParam) || std::isinf(startParam) || startParam == DBL_MAX || startParam == -DBL_MAX)
    {
        assert(false);
        return false;
    }
    const double endParam = endKnot.getParam();
    if (std::isnan(endParam) || std::isinf(endParam) || endParam == DBL_MAX || endParam == -DBL_MAX)
    {
        assert(false);
        return false;
    }

    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return false;
    }
    const wy3d::Sketch3D* pConstSketch3D = wy3d::Sketch3D::cast(pDb->getElement(_sketch3DInfo.sketch3dId));
    if (!pConstSketch3D)
    {
        assert(false);
        return false;
    }
    const wy3d::SketchCurve3D* pConstCurve = wy3d::SketchCurve3D::cast(pDb->getElement(curveId));
    if (!pConstCurve)
    {
        assert(false);
        return false;
    }

    // Chain updates are kept local so that it is leaving the sketch environment, not this edit,
    // that marks the sketch's shape dirty.
    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans)
    {
        assert(false);
        return false;
    }

    wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(pConstSketch3D->getId()));
    if (!pSketch3D) goto ABORT_TRANS;
    wy3d::SketchCurve3D* pCurve = wy3d::SketchCurve3D::cast(pTrans->getElementForWrite(pConstCurve->getId()));
    if (!pCurve) goto ABORT_TRANS;

    if (wy3d::SketchLine3D* pLine = wy3d::SketchLine3D::cast(pCurve))
    {
        assert(endParam > startParam && (endParam - startParam) > 1.0);
        const wy::Vector3 startPnt = pLine->getStartPoint();
        const wy::Vector3 lineVec = pLine->getEndPoint() - startPnt;
        if (startParam < -wy3d::EPS)
        {
            if (wy::ErrorStatus::Ok != pLine->setStartPoint(startPnt + lineVec * startParam)) goto ABORT_TRANS;
        }
        else if (endParam > 1.0 + wy3d::EPS)
        {
            if (wy::ErrorStatus::Ok != pLine->setEndPoint(startPnt + lineVec * endParam)) goto ABORT_TRANS;
        }
        else
        {
            assert(false);
            goto ABORT_TRANS;
        }
    }
    else if (wy3d::SketchArc3D* pArc = wy3d::SketchArc3D::cast(pCurve))
    {
        // An arc's knots are not signed the way a line's are: a target behind the start arrives as a
        // parameter above 1, so which end grows is told by which parameter stayed put, and the
        // angle it lands at is what places the new end.
        double fromParam = 0.0;
        double toParam = 1.0;
        if (endParam == 1.0)
        {
            assert(startParam > 1.0);
            fromParam = startParam;
        }
        else if (startParam == 0.0)
        {
            assert(endParam > 1.0);
            toParam = endParam;
        }
        else
        {
            assert(false);
            goto ABORT_TRANS;
        }

        double newStartAngle = 0.0;
        double newEndAngle = 0.0;
        wy3d::Sketch3DCurveParam::subArcAngles(wy3d::normalizeRadian(pArc->getStartAngle()),
            pArc->getTotalAngle(), fromParam, toParam, newStartAngle, newEndAngle);
        if (wy::ErrorStatus::Ok != pArc->setStartAngle(newStartAngle)) goto ABORT_TRANS;
        if (wy::ErrorStatus::Ok != pArc->setEndAngle(newEndAngle)) goto ABORT_TRANS;
    }
    else if (wy3d::SketchEllipseArc3D* pEllipseArc = wy3d::SketchEllipseArc3D::cast(pCurve))
    {
        double fromParam = 0.0;
        double toParam = 1.0;
        if (endParam == 1.0)
        {
            assert(startParam > 1.0);
            fromParam = startParam;
        }
        else if (startParam == 0.0)
        {
            assert(endParam > 1.0);
            toParam = endParam;
        }
        else
        {
            assert(false);
            goto ABORT_TRANS;
        }

        double newStartAngle = 0.0;
        double newEndAngle = 0.0;
        wy3d::Sketch3DCurveParam::subArcAngles(wy3d::normalizeRadian(pEllipseArc->getStartAngle()),
            pEllipseArc->getTotalAngle(), fromParam, toParam, newStartAngle, newEndAngle);
        if (wy::ErrorStatus::Ok != pEllipseArc->setStartAngle(newStartAngle)) goto ABORT_TRANS;
        if (wy::ErrorStatus::Ok != pEllipseArc->setEndAngle(newEndAngle)) goto ABORT_TRANS;
    }
    else if (wy3d::SketchSpline3D* pSpline = wy3d::SketchSpline3D::cast(pCurve))
    {
        const Handle(Geom_BSplineCurve) pBSpline = pSpline->getOccSpline();
        if (pBSpline.IsNull())
        {
            assert(false);
            goto ABORT_TRANS;
        }

        Handle(Geom_BSplineCurve) pNewBSpline(nullptr);
        if (startParam < 0.0)
        {
            pNewBSpline = wy3d::Sketch3DSplineUtil::addLineSegmentToBSpline(pBSpline,
                startKnot.getPosition(), true);
        }
        else if (endParam > 1.0)
        {
            pNewBSpline = wy3d::Sketch3DSplineUtil::addLineSegmentToBSpline(pBSpline,
                endKnot.getPosition(), false);
        }
        if (pNewBSpline.IsNull())
        {
            assert(false);
            goto ABORT_TRANS;
        }

        unsigned int degree = 0;
        std::vector<wy::Vector3> controlPoints;
        std::vector<double> knots;
        std::vector<unsigned int> multiplicities;
        if (!wy3d::Sketch3DSplineUtil::getBSplineData(pNewBSpline, degree, controlPoints, knots,
            multiplicities))
        {
            assert(false);
            goto ABORT_TRANS;
        }
        if (wy::ErrorStatus::Ok != pSpline->setMode(wy3d::SplineMode::ControlPoints)) goto ABORT_TRANS;
        if (wy::ErrorStatus::Ok != pSpline->setDegree(degree)) goto ABORT_TRANS;
        if (wy::ErrorStatus::Ok != pSpline->setPoints(controlPoints)) goto ABORT_TRANS;
        if (wy::ErrorStatus::Ok != pSpline->setKnots(knots)) goto ABORT_TRANS;
        if (wy::ErrorStatus::Ok != pSpline->setMultiplicities(multiplicities)) goto ABORT_TRANS;
    }
    else
    {
        assert(false);
        goto ABORT_TRANS;
    }

    if (wy::ErrorStatus::Ok != pDb->getTransactionManager()->endTransaction()) goto ABORT_TRANS;
    return true;

ABORT_TRANS:
    assert(false);
    pDb->getTransactionManager()->abortTransaction();
    return false;
}
