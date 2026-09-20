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

#include "Sketch3DTrimGuiCmd.h"

#include <cassert>

#include <QCoreApplication>

#include <Geom_BSplineCurve.hxx>

#include <utils/wy3dSketch3DSplineUtil.h>
#include <wy3dImpl.h>
#include <wy3dMath.h>
#include <wy3dSelectionType.h>
#include <wy3dSketchEntity3D.h>

#include "application/Application.h"
#include "scene/nodes/ElementNodeType.h"
#include "select/filters/CommonSelFilters.h"

Sketch3DTrimGuiCmd::Sketch3DTrimGuiCmd() : OsgGuiCommand()
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

Sketch3DTrimGuiCmd::~Sketch3DTrimGuiCmd()
{
}

void Sketch3DTrimGuiCmd::onDatabaseChanged(
    const wydb::Database* pDb,
    const wydb::Transaction* pTransaction,
    const wydb::DatabaseChangeInfo& changeInfo)
{
    assert(pDb);

    // The pieces a trim created. Each inherits the original's knots, so the crossings it still
    // carries keep being answered for once the original has been cut down.
    for (const wydb::ElementId& id : changeInfo.addedIds)
    {
        if (!wy3d::SketchCurve3D::cast(pDb->getElement(id))) continue;
        auto iter = _id2Parent.find(id);
        if (iter == _id2Parent.cend())
        {
            // Already there before the command started - an undo bringing an element back.
            wy3d::Sketch3DTrimNodeSPtr pNode = _pTrimGraph->getNode(id);
            if (pNode) pNode->refresh(pDb);
        }
        else
        {
            wy3d::Sketch3DTrimNodeSPtr pParentNode = _pTrimGraph->getNode(iter->second);
            if (!pParentNode) continue;
            wy3d::Sketch3DTrimNodeSPtr pNode = _pTrimGraph->getNode(id);
            if (pNode) // an undo erased the piece but its node outlived it
            {
                pNode->refresh(pDb);
            }
            else
            {
                wy3d::Sketch3DTrimNodeSPtr pNewNode = pParentNode->clone(id);
                pNewNode->refresh(pDb);
                pParentNode->appendChild(pNewNode);
                const bool added = _pTrimGraph->addNode(pNewNode);
                assert(added);
            }
        }
    }

    for (const wydb::ElementId& id : changeInfo.modifiedIds)
    {
        if (!wy3d::SketchCurve3D::cast(pDb->getElement(id))) continue;
        wy3d::Sketch3DTrimNodeSPtr pNode = _pTrimGraph->getNode(id);
        assert(pNode);
        if (pNode) pNode->refresh(pDb);
    }
}

wyap::CmdExecution::StartResult Sketch3DTrimGuiCmd::onStart()
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

    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return wyap::CmdExecution::StartResult::Failed;
    }
    const wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pDb->getElement(_sketch3DInfo.sketch3dId));
    if (!pSketch3D)
    {
        assert(false);
        return wyap::CmdExecution::StartResult::Failed;
    }

    _pTrimGraph = std::make_unique<wy3d::Sketch3DTrimGraph>(pSketch3D, 1e-6);
    if (!_pTrimGraph->isValid())
    {
        assert(false);
        return wyap::CmdExecution::StartResult::Failed;
    }

    _id2Parent.clear();
    _pickedId = wydb::ElementId::kNull;
    _pickedSegment = wy3d::Sketch3DTrimSegment();
    pDb->addReactor(this);

    Application::instance().setCursor(CursorType::SelectElements);
    Application::instance().getStatusBar()->setTips(QCoreApplication::translate("Sketch3DTrimGuiCmd",
        "Select sketch entities to trim."));

    _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Sketch3DEntity);
    _pointPickOption.selType = wy3d::SelectionType::Element;
    _pointPickOption.pSelPreFilter = std::make_shared<CommonPreSelFilterForPointPick>(
        wy3d::SketchEntity3D::classInfo());

    return wyap::CmdExecution::StartResult::Succeeded;
}

void Sketch3DTrimGuiCmd::onEnd()
{
    _pTransient = nullptr;
    if (wydb::Database* pDb = Application::instance().getActiveDatabase())
    {
        pDb->removeReactor(this);
    }
    GuiCommand::onEnd();
}

void Sketch3DTrimGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    _pTransient = nullptr;
    if (wydb::Database* pDb = Application::instance().getActiveDatabase())
    {
        pDb->removeReactor(this);
    }
    GuiCommand::onAbort(cause);
}

void Sketch3DTrimGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (!this->getOsgView()) return;

    const std::pair<wydb::ElementId, wy::Vector3> pickRet =
        this->pointPickElement(event.x, event.y, _pointPickOption);
    this->pickTrimSegment(pickRet.first, pickRet.second);

    Application::instance().setCursor(_pTransient ? CursorType::Delete : CursorType::SelectElements);
}

void Sketch3DTrimGuiCmd::pickTrimSegment(const wydb::ElementId& id, const wy::Vector3& pickPos3d)
{
    if (id.isNull())
    {
        _pTransient = nullptr;
        _pickedId = wydb::ElementId::kNull;
        return;
    }

    assert(_pTrimGraph);
    const wy3d::Sketch3DTrimSegment segment = _pTrimGraph->pick(id, pickPos3d);
    if (!segment.isValid())
    {
        _pTransient = nullptr;
        _pickedId = wydb::ElementId::kNull;
        return;
    }

    // Keep the same preview while the piece under the cursor does not change, so moving the mouse
    // inside one piece does not rebuild the geometry on every event.
    if (_pTransient && _pickedId == id
        && _pickedSegment.startKnot.getParam() == segment.startKnot.getParam()
        && _pickedSegment.endKnot.getParam() == segment.endKnot.getParam())
    {
        return;
    }

    const wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        _pTransient = nullptr;
        _pickedId = wydb::ElementId::kNull;
        return;
    }
    const wy3d::SketchCurve3D* pCurve = wy3d::SketchCurve3D::cast(pDb->getElement(id));
    if (!pCurve)
    {
        _pTransient = nullptr;
        _pickedId = wydb::ElementId::kNull;
        return;
    }

    _pickedId = id;
    _pickedSegment = segment;
    _pTransient = std::make_shared<Sketch3DTrimExtendTransient>(pCurve,
        segment.startKnot.getParam(), segment.endKnot.getParam());
}

void Sketch3DTrimGuiCmd::onLeftMouseUp(const MouseEvent& event)
{
    if (_pTransient)
    {
        assert(_pickedSegment.isValid());
        if (_pickedSegment.isValid())
        {
            const bool trimmed = this->trim(_pickedId, _pickedSegment.startKnot, _pickedSegment.endKnot);
            assert(trimmed);
        }
    }

    _pTransient = nullptr;
    _pickedId = wydb::ElementId::kNull;
    _pickedSegment = wy3d::Sketch3DTrimSegment();
    Application::instance().setCursor(CursorType::SelectElements);
}

bool Sketch3DTrimGuiCmd::trim(const wydb::ElementId curveId,
    const wy3d::Sketch3DTrimKnot& startKnot, const wy3d::Sketch3DTrimKnot& endKnot)
{
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

    bool ret = false;
    wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pTrans->getElementForWrite(pConstSketch3D->getId()));
    if (!pSketch3D) goto ABORT_TRANS;
    wy3d::SketchCurve3D* pCurve = wy3d::SketchCurve3D::cast(pTrans->getElementForWrite(pConstCurve->getId()));
    if (!pCurve) goto ABORT_TRANS;

    if (wy3d::SketchLine3D* pLine = wy3d::SketchLine3D::cast(pCurve))
    {
        ret = trimLine(pTrans, pSketch3D, pLine, startKnot, endKnot);
    }
    else if (wy3d::SketchCircle3D* pCircle = wy3d::SketchCircle3D::cast(pCurve))
    {
        ret = trimCircle(pTrans, pSketch3D, pCircle, startKnot, endKnot);
    }
    else if (wy3d::SketchArc3D* pArc = wy3d::SketchArc3D::cast(pCurve))
    {
        ret = trimArc(pTrans, pSketch3D, pArc, startKnot, endKnot);
    }
    else if (wy3d::SketchEllipse3D* pEllipse = wy3d::SketchEllipse3D::cast(pCurve))
    {
        ret = trimEllipse(pTrans, pSketch3D, pEllipse, startKnot, endKnot);
    }
    else if (wy3d::SketchEllipseArc3D* pEllipseArc = wy3d::SketchEllipseArc3D::cast(pCurve))
    {
        ret = trimEllipseArc(pTrans, pSketch3D, pEllipseArc, startKnot, endKnot);
    }
    else if (wy3d::SketchSpline3D* pSpline = wy3d::SketchSpline3D::cast(pCurve))
    {
        ret = trimSpline(pTrans, pSketch3D, pSpline, startKnot, endKnot);
    }
    else
    {
        assert(false);
        goto ABORT_TRANS;
    }

    if (!ret) goto ABORT_TRANS;
    if (wy::ErrorStatus::Ok != pDb->getTransactionManager()->endTransaction()) goto ABORT_TRANS;
    return true;

ABORT_TRANS:
    assert(false);
    pDb->getTransactionManager()->abortTransaction();
    return false;
}

bool Sketch3DTrimGuiCmd::trimLine(wydb::Transaction* pTrans, wy3d::Sketch3D* pSketch3D,
    wy3d::SketchLine3D* pLine,
    const wy3d::Sketch3DTrimKnot& startKnot, const wy3d::Sketch3DTrimKnot& endKnot)
{
    assert(pTrans);
    assert(pSketch3D);
    assert(pLine);
    assert(startKnot.getParam() >= 0.0 && startKnot.getParam() <= 1.0);
    assert(endKnot.getParam() >= 0.0 && endKnot.getParam() <= 1.0);

    if (startKnot.getParam() == 0.0 && endKnot.getParam() == 1.0)
    {
        return wy::ErrorStatus::Ok == pLine->erase(true);
    }

    const wy::Vector3 startPnt = pLine->getStartPoint();
    const wy::Vector3 lineVec = pLine->getEndPoint() - startPnt;

    if (startKnot.getParam() == 0.0)
    {
        return wy::ErrorStatus::Ok == pLine->setStartPoint(startPnt + lineVec * endKnot.getParam());
    }
    else if (endKnot.getParam() == 1.0)
    {
        return wy::ErrorStatus::Ok == pLine->setEndPoint(startPnt + lineVec * startKnot.getParam());
    }
    else
    {
        // The piece in the middle goes; the original keeps the head and a new line takes the tail.
        if (wy::ErrorStatus::Ok != pLine->setEndPoint(startPnt + lineVec * startKnot.getParam()))
        {
            return false;
        }

        wy3d::SketchLine3D* pNewLine = nullptr;
        if (wy::ErrorStatus::Ok != wy3d::SketchLine3D::create(pTrans,
            startPnt + lineVec * endKnot.getParam(), startPnt + lineVec, pNewLine) || !pNewLine)
        {
            return false;
        }
        if (wy::ErrorStatus::Ok != pSketch3D->addEntity(pNewLine)) return false;
        _id2Parent[pNewLine->getId()] = pLine->getId();
        return true;
    }
}

bool Sketch3DTrimGuiCmd::trimCircle(wydb::Transaction* pTrans, wy3d::Sketch3D* pSketch3D,
    wy3d::SketchCircle3D* pCircle,
    const wy3d::Sketch3DTrimKnot& startKnot, const wy3d::Sketch3DTrimKnot& endKnot)
{
    assert(pTrans);
    assert(pSketch3D);
    assert(pCircle);
    assert(startKnot.getParam() >= 0.0 && startKnot.getParam() <= 1.0);
    assert(endKnot.getParam() >= 0.0 && endKnot.getParam() <= 1.0);

    if (startKnot.getParam() == 0.0 && endKnot.getParam() == 1.0)
    {
        return wy::ErrorStatus::Ok == pCircle->erase(true);
    }

    // Cutting a piece out of a circle leaves an arc that spans the rest of it. A circle's parameter
    // is its polar angle, so the two knot parameters are already angles.
    const double startAngle = endKnot.getParam() * wy3d::TWO_PI;
    double endAngle = startKnot.getParam() * wy3d::TWO_PI;
    if (endAngle < startAngle) endAngle += wy3d::TWO_PI;

    if ((endAngle - startAngle) <= wy3d::EPS)
    {
        assert(false);
        return wy::ErrorStatus::Ok == pCircle->erase(true);
    }

    wy3d::SketchArc3D* pNewArc = nullptr;
    if (wy::ErrorStatus::Ok != wy3d::SketchArc3D::create(pTrans, pCircle->getCenter(), pCircle->getNormal(),
        pCircle->getXDir(), pCircle->getRadius(), startAngle, endAngle, pNewArc) || !pNewArc)
    {
        return false;
    }
    if (wy::ErrorStatus::Ok != pSketch3D->addEntity(pNewArc)) return false;
    _id2Parent[pNewArc->getId()] = pCircle->getId();
    pCircle->erase(true);
    return true;
}

bool Sketch3DTrimGuiCmd::trimArc(wydb::Transaction* pTrans, wy3d::Sketch3D* pSketch3D,
    wy3d::SketchArc3D* pArc,
    const wy3d::Sketch3DTrimKnot& startKnot, const wy3d::Sketch3DTrimKnot& endKnot)
{
    assert(pTrans);
    assert(pSketch3D);
    assert(pArc);
    assert(startKnot.getParam() >= 0.0 && startKnot.getParam() <= 1.0);
    assert(endKnot.getParam() >= 0.0 && endKnot.getParam() <= 1.0);

    if (startKnot.getParam() == 0.0 && endKnot.getParam() == 1.0)
    {
        return wy::ErrorStatus::Ok == pArc->erase(true);
    }

    const double startAngle = pArc->getStartAngle();
    const double endAngle = pArc->getEndAngle();
    const double totalAngle = pArc->getTotalAngle();

    if (startKnot.getParam() == 0.0)
    {
        const double newStartAngle = wy3d::normalizeRadian(startAngle + endKnot.getParam() * totalAngle);
        return wy::ErrorStatus::Ok == pArc->setStartAngle(newStartAngle);
    }
    else if (endKnot.getParam() == 1.0)
    {
        const double newEndAngle = wy3d::normalizeRadian(startAngle + startKnot.getParam() * totalAngle);
        return wy::ErrorStatus::Ok == pArc->setEndAngle(newEndAngle);
    }
    else
    {
        // The arc keeps the head; the tail becomes a new arc running to the original's end angle,
        // which is left as it is so getTotalAngle recovers the sweep exactly.
        const double newEndAngle = wy3d::normalizeRadian(startAngle + startKnot.getParam() * totalAngle);
        if (wy::ErrorStatus::Ok != pArc->setEndAngle(newEndAngle)) return false;

        const double newStartAngle = wy3d::normalizeRadian(startAngle + endKnot.getParam() * totalAngle);
        wy3d::SketchArc3D* pNewArc = nullptr;
        if (wy::ErrorStatus::Ok != wy3d::SketchArc3D::create(pTrans, pArc->getCenter(), pArc->getNormal(),
            pArc->getXDir(), pArc->getRadius(), newStartAngle, endAngle, pNewArc) || !pNewArc)
        {
            return false;
        }
        if (wy::ErrorStatus::Ok != pSketch3D->addEntity(pNewArc)) return false;
        _id2Parent[pNewArc->getId()] = pArc->getId();
        return true;
    }
}

bool Sketch3DTrimGuiCmd::trimEllipse(wydb::Transaction* pTrans, wy3d::Sketch3D* pSketch3D,
    wy3d::SketchEllipse3D* pEllipse,
    const wy3d::Sketch3DTrimKnot& startKnot, const wy3d::Sketch3DTrimKnot& endKnot)
{
    assert(pTrans);
    assert(pSketch3D);
    assert(pEllipse);
    assert(startKnot.getParam() >= 0.0 && startKnot.getParam() <= 1.0);
    assert(endKnot.getParam() >= 0.0 && endKnot.getParam() <= 1.0);

    if (startKnot.getParam() == 0.0 && endKnot.getParam() == 1.0)
    {
        return wy::ErrorStatus::Ok == pEllipse->erase(true);
    }

    // Same as the circle: the parameter is the polar angle, which is also what an ellipse arc
    // stores, so no conversion is needed on the way out.
    const double startAngle = endKnot.getParam() * wy3d::TWO_PI;
    double endAngle = startKnot.getParam() * wy3d::TWO_PI;
    if (endAngle < startAngle) endAngle += wy3d::TWO_PI;

    if ((endAngle - startAngle) <= wy3d::EPS)
    {
        assert(false);
        return wy::ErrorStatus::Ok == pEllipse->erase(true);
    }

    wy3d::SketchEllipseArc3D* pNewEllipseArc = nullptr;
    if (wy::ErrorStatus::Ok != wy3d::SketchEllipseArc3D::create(pTrans, pEllipse->getCenter(),
        pEllipse->getNormal(), pEllipse->getXDir(), pEllipse->getMajorRadius(), pEllipse->getRadiusRatio(),
        startAngle, endAngle, pNewEllipseArc) || !pNewEllipseArc)
    {
        return false;
    }
    if (wy::ErrorStatus::Ok != pSketch3D->addEntity(pNewEllipseArc)) return false;
    _id2Parent[pNewEllipseArc->getId()] = pEllipse->getId();
    pEllipse->erase(true);
    return true;
}

bool Sketch3DTrimGuiCmd::trimEllipseArc(wydb::Transaction* pTrans, wy3d::Sketch3D* pSketch3D,
    wy3d::SketchEllipseArc3D* pEllipseArc,
    const wy3d::Sketch3DTrimKnot& startKnot, const wy3d::Sketch3DTrimKnot& endKnot)
{
    assert(pTrans);
    assert(pSketch3D);
    assert(pEllipseArc);
    assert(startKnot.getParam() >= 0.0 && startKnot.getParam() <= 1.0);
    assert(endKnot.getParam() >= 0.0 && endKnot.getParam() <= 1.0);

    if (startKnot.getParam() == 0.0 && endKnot.getParam() == 1.0)
    {
        return wy::ErrorStatus::Ok == pEllipseArc->erase(true);
    }

    const double startAngle = wy3d::normalizeRadian(pEllipseArc->getStartAngle());
    const double endAngle = wy3d::normalizeRadian(pEllipseArc->getEndAngle());
    const double totalAngle = pEllipseArc->getTotalAngle();

    if (startKnot.getParam() == 0.0)
    {
        const double newStartAngle = wy3d::normalizeRadian(startAngle + endKnot.getParam() * totalAngle);
        return wy::ErrorStatus::Ok == pEllipseArc->setStartAngle(newStartAngle);
    }
    else if (endKnot.getParam() == 1.0)
    {
        const double newEndAngle = wy3d::normalizeRadian(startAngle + startKnot.getParam() * totalAngle);
        return wy::ErrorStatus::Ok == pEllipseArc->setEndAngle(newEndAngle);
    }
    else
    {
        const double newEndAngle = startAngle + startKnot.getParam() * totalAngle;
        if (wy::ErrorStatus::Ok != pEllipseArc->setEndAngle(newEndAngle)) return false;

        // The tail's end angle is carried around the seam when the cut point sits past it.
        const double newStartAngle = wy3d::normalizeRadian(startAngle + endKnot.getParam() * totalAngle);
        const double newTailEndAngle = (endAngle < newStartAngle) ? (endAngle + wy3d::TWO_PI) : endAngle;

        wy3d::SketchEllipseArc3D* pNewEllipseArc = nullptr;
        if (wy::ErrorStatus::Ok != wy3d::SketchEllipseArc3D::create(pTrans, pEllipseArc->getCenter(),
            pEllipseArc->getNormal(), pEllipseArc->getXDir(), pEllipseArc->getMajorRadius(),
            pEllipseArc->getRadiusRatio(), newStartAngle, newTailEndAngle, pNewEllipseArc) || !pNewEllipseArc)
        {
            return false;
        }
        if (wy::ErrorStatus::Ok != pSketch3D->addEntity(pNewEllipseArc)) return false;
        _id2Parent[pNewEllipseArc->getId()] = pEllipseArc->getId();
        return true;
    }
}

bool Sketch3DTrimGuiCmd::trimSpline(wydb::Transaction* pTrans, wy3d::Sketch3D* pSketch3D,
    wy3d::SketchSpline3D* pSpline,
    const wy3d::Sketch3DTrimKnot& startKnot, const wy3d::Sketch3DTrimKnot& endKnot)
{
    assert(pTrans);
    assert(pSketch3D);
    assert(pSpline);
    assert(startKnot.getParam() >= 0.0 && startKnot.getParam() <= 1.0);
    assert(endKnot.getParam() >= 0.0 && endKnot.getParam() <= 1.0);

    if (startKnot.getParam() == 0.0 && endKnot.getParam() == 1.0)
    {
        return wy::ErrorStatus::Ok == pSpline->erase(true);
    }

    // Taken once: setPoints and friends rebuild the curve, so a second getOccSpline after the head
    // has been written back would read the trimmed spline instead of the original.
    const Handle(Geom_BSplineCurve) pBSpline = pSpline->getOccSpline();
    if (pBSpline.IsNull())
    {
        assert(false);
        return false;
    }

    // Rewrites the spline in place with the piece between two parameters.
    const auto applySegment = [&](wy3d::SketchSpline3D* pTarget, double startParam, double endParam) -> bool {
        unsigned int degree = 0;
        std::vector<wy::Vector3> controlPoints;
        std::vector<double> knots;
        std::vector<unsigned int> multiplicities;
        if (!wy3d::Sketch3DSplineUtil::segment(pBSpline, startParam, endParam, degree, controlPoints,
            knots, multiplicities))
        {
            return false;
        }
        if (wy::ErrorStatus::Ok != pTarget->setMode(wy3d::SplineMode::ControlPoints)) return false;
        if (wy::ErrorStatus::Ok != pTarget->setDegree(degree)) return false;
        if (wy::ErrorStatus::Ok != pTarget->setPoints(controlPoints)) return false;
        if (wy::ErrorStatus::Ok != pTarget->setKnots(knots)) return false;
        if (wy::ErrorStatus::Ok != pTarget->setMultiplicities(multiplicities)) return false;
        return true;
    };

    if (startKnot.getParam() == 0.0)
    {
        return applySegment(pSpline, endKnot.getParam(), 1.0);
    }
    else if (endKnot.getParam() == 1.0)
    {
        return applySegment(pSpline, 0.0, startKnot.getParam());
    }
    else
    {
        // The original keeps the head; the tail is built straight from the data rather than by
        // rewriting a second entity, because create already registers it with the transaction.
        unsigned int degree = 0;
        std::vector<wy::Vector3> controlPoints;
        std::vector<double> knots;
        std::vector<unsigned int> multiplicities;
        if (!wy3d::Sketch3DSplineUtil::segment(pBSpline, endKnot.getParam(), 1.0, degree, controlPoints,
            knots, multiplicities))
        {
            return false;
        }
        if (!applySegment(pSpline, 0.0, startKnot.getParam())) return false;

        wy3d::SketchSpline3D* pNewSpline = nullptr;
        if (wy::ErrorStatus::Ok != wy3d::SketchSpline3D::create(pTrans, degree, controlPoints, knots,
            multiplicities, pNewSpline) || !pNewSpline)
        {
            return false;
        }
        if (wy::ErrorStatus::Ok != pSketch3D->addEntity(pNewSpline)) return false;
        _id2Parent[pNewSpline->getId()] = pSpline->getId();
        return true;
    }
}
