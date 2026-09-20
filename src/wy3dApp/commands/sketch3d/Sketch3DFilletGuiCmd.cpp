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

#include "Sketch3DFilletGuiCmd.h"

#include <cassert>
#include <cmath>

#include <QCoreApplication>

#include <wy3dImpl.h>
#include <wy3dSelectionType.h>
#include <wy3dSketchEntity3D.h>

#include "application/Application.h"
#include "commands/dialogs/DoubleValueInputDialog.h"
#include "scene/nodes/ElementNodeType.h"
#include "select/filters/CommonSelFilters.h"

namespace
{
    // The two conics neither this command nor the 2D algorithm can round off. Refused as they are
    // hovered rather than after the pair is complete, so a curve that can never take part does not
    // become the first of the two.
    bool isSupportedCurve(const wy3d::SketchCurve3D* pCurve)
    {
        return pCurve
            && !pCurve->isKindOf(wy3d::SketchEllipse3D::classInfo())
            && !pCurve->isKindOf(wy3d::SketchEllipseArc3D::classInfo());
    }

    const wy3d::SketchCurve3D* getCurve(const wydb::ElementId& id)
    {
        wydb::Database* pDb = Application::instance().getActiveDatabase();
        if (!pDb) return nullptr;
        return wy3d::SketchCurve3D::cast(pDb->getElement(id));
    }
}

double Sketch3DFilletGuiCmd::_R = 10.0;

Sketch3DFilletGuiCmd::Sketch3DFilletGuiCmd()
    : OsgGuiCommand(), _step(Step::First), _id1st(wydb::ElementId::kNull), _pickPos1st(),
    _id2nd(wydb::ElementId::kNull), _pickPos2nd()
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

Sketch3DFilletGuiCmd::~Sketch3DFilletGuiCmd()
{
}

wyap::CmdExecution::StartResult Sketch3DFilletGuiCmd::onStart()
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
    if (!wy3d::Sketch3D::cast(pDb->getElement(_sketch3DInfo.sketch3dId)))
    {
        assert(false);
        return wyap::CmdExecution::StartResult::Failed;
    }

    DoubleValueInputDialog dialog(_R,
        QCoreApplication::translate("Sketch3DFillet", "Fillet"),
        QCoreApplication::translate("Sketch3DFillet", "Radius"));
    if (QDialog::Accepted != dialog.exec())
    {
        return wyap::CmdExecution::StartResult::Rejected;
    }
    _R = std::fabs(dialog.getValue());

    _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Sketch3DEntity);
    _pointPickOption.selType = wy3d::SelectionType::Element;

    this->gotoStep(Step::First);

    return wyap::CmdExecution::StartResult::Succeeded;
}

void Sketch3DFilletGuiCmd::onEnd()
{
    GuiCommand::onEnd();
}

void Sketch3DFilletGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    GuiCommand::onAbort(cause);
}

void Sketch3DFilletGuiCmd::reset()
{
    _id1st = wydb::ElementId::kNull;
    _pickPos1st = wy::Vector3::kZero;
    _pCurveTransient1st = nullptr;

    this->clearSecondPreview();

    this->gotoStep(Step::First);
}

void Sketch3DFilletGuiCmd::clearSecondPreview()
{
    _id2nd = wydb::ElementId::kNull;
    _pickPos2nd = wy::Vector3::kZero;
    _pCurveTransient2nd = nullptr;
    _pFilletTransient = nullptr;
    _pFilletData = nullptr;
}

void Sketch3DFilletGuiCmd::gotoStep(Step step)
{
    _step = step;

    switch (step)
    {
    case Step::First:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("Sketch3DFillet",
            "Select the first sketch entity."));
        Application::instance().setCursor(CursorType::SelectElements);

        _pointPickOption.pSelPreFilter = std::make_shared<CommonPreSelFilterForPointPick>(
            wy3d::SketchEntity3D::classInfo(), wydb::ElementId::kNull);
    }
    break;

    case Step::Second:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("Sketch3DFillet",
            "Select the second sketch entity."));
        Application::instance().setCursor(CursorType::SelectElements);

        _pointPickOption.pSelPreFilter = std::make_shared<CommonPreSelFilterForPointPick>(
            wy3d::SketchEntity3D::classInfo(), _id1st);
    }
    break;

    default:
    {
        Application::instance().getStatusBar()->setTips("");
        assert(false);
    }
    break;
    }
}

void Sketch3DFilletGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (!this->getOsgView()) return;

    const std::pair<wydb::ElementId, wy::Vector3> pickRet =
        this->pointPickElement(event.x, event.y, _pointPickOption);
    this->preview(pickRet.first, pickRet.second);

    // An entity under the cursor the command cannot use reads the same in either step: something is
    // there and no preview came of it.
    const bool usable = (Step::First == _step)
        ? (_pCurveTransient1st != nullptr)
        : (_pCurveTransient2nd != nullptr);
    if (!pickRet.first.isNull() && !usable)
    {
        Application::instance().setCursor(CursorType::Forbid);
    }
    else
    {
        Application::instance().setCursor(CursorType::SelectElements);
    }
}

void Sketch3DFilletGuiCmd::preview(const wydb::ElementId& id, const wy::Vector3& pickPos)
{
    if (Step::First == _step)
    {
        const wy3d::SketchCurve3D* pCurve = getCurve(id);
        if (!isSupportedCurve(pCurve))
        {
            _pCurveTransient1st = nullptr;
            return;
        }

        _id1st = id;
        _pickPos1st = pickPos;

        // Left alone while the cursor stays on the same curve, so that moving along one entity does
        // not rebuild the same geometry on every event.
        if (!_pCurveTransient1st || _pCurveTransient1st->getId() != id)
        {
            _pCurveTransient1st = std::make_shared<Sketch3DTrimExtendTransient>(pCurve, 0.0, 1.0);
        }
        return;
    }

    assert(_pCurveTransient1st);
    if (!_pCurveTransient1st)
    {
        this->clearSecondPreview();
        return;
    }

    const wy3d::SketchCurve3D* pCurve2nd = getCurve(id);
    if (!isSupportedCurve(pCurve2nd) || id == _id1st)
    {
        this->clearSecondPreview();

        // The first curve goes back to being highlighted whole: the preview of a fillet that is no
        // longer there must not leave it looking trimmed.
        if (_pCurveTransient1st->getStartParam() != 0.0 || _pCurveTransient1st->getEndParam() != 1.0)
        {
            const wy3d::SketchCurve3D* pCurve1st = getCurve(_id1st);
            if (pCurve1st)
            {
                _pCurveTransient1st = std::make_shared<Sketch3DTrimExtendTransient>(pCurve1st, 0.0, 1.0);
            }
        }
        return;
    }

    const wy3d::SketchCurve3D* pCurve1st = getCurve(_id1st);
    if (!pCurve1st)
    {
        this->clearSecondPreview();
        return;
    }

    wy3d::Sketch3DFilletData fillet;
    if (wy3d::Sketch3DFilletAlgo::Result::Ok != wy3d::Sketch3DFilletAlgo::fillet(_R, wy3d::TOL,
        pCurve1st, _pickPos1st, pCurve2nd, pickPos, fillet))
    {
        this->clearSecondPreview();
        return;
    }

    _id2nd = id;
    _pickPos2nd = pickPos;

    std::shared_ptr<Fillet3DData> pFilletData = std::make_shared<Fillet3DData>();
    pFilletData->id1st = _id1st;
    pFilletData->id2nd = _id2nd;
    pFilletData->fillet = fillet;

    if (_pFilletData && *pFilletData == *_pFilletData)
    {
        return;
    }

    _pCurveTransient1st = std::make_shared<Sketch3DTrimExtendTransient>(pCurve1st,
        fillet.startParam1st, fillet.endParam1st);
    _pCurveTransient2nd = std::make_shared<Sketch3DTrimExtendTransient>(pCurve2nd,
        fillet.startParam2nd, fillet.endParam2nd);
    _pFilletTransient = std::make_shared<Sketch3DCurveTransient>(fillet.filletCenter,
        fillet.frame.normal, fillet.frame.xDir, fillet.filletRadius,
        fillet.filletStartAngle, fillet.filletEndAngle);
    _pFilletData = pFilletData;
}

void Sketch3DFilletGuiCmd::onLeftMouseUp(const MouseEvent& event)
{
    if (Step::First == _step)
    {
        if (_pCurveTransient1st)
        {
            this->gotoStep(Step::Second);
        }
        return;
    }

    if (_pFilletTransient)
    {
        // An undo while the command was running can take one of the two away between the last
        // mouse move and this click. Committing then would be committing to a pair that is no
        // longer there, so the guard is here rather than inside fillet().
        if (!getCurve(_id1st) || !getCurve(_id2nd))
        {
            this->reset();
            return;
        }

        const bool filleted = this->fillet(_pFilletData.get());
        assert(filleted);
        this->reset();
        return;
    }

    // Clicking the first curve again drops it, so that a pair can be started over without leaving
    // the command.
    const std::pair<wydb::ElementId, wy::Vector3> pickRet =
        this->pointPickElement(event.x, event.y, _pointPickOption);
    if (pickRet.first == _id1st)
    {
        this->reset();
    }
}

bool Sketch3DFilletGuiCmd::fillet(const Fillet3DData* pFilletData)
{
    assert(pFilletData);

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
    const wy3d::SketchCurve3D* pConstCurve1st = wy3d::SketchCurve3D::cast(pDb->getElement(pFilletData->id1st));
    const wy3d::SketchCurve3D* pConstCurve2nd = wy3d::SketchCurve3D::cast(pDb->getElement(pFilletData->id2nd));
    if (!pConstCurve1st || !pConstCurve2nd)
    {
        assert(false);
        return false;
    }

    // One local transaction, no group: the sketch environment already opened a top-level group on
    // entry, and it is leaving the environment that is meant to mark the sketch's shape dirty.
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

    {
        wy3d::SketchCurve3D* pCurve1st =
            wy3d::SketchCurve3D::cast(pTrans->getElementForWrite(pConstCurve1st->getId()));
        if (!pCurve1st) goto ABORT_TRANS;
        if (!Sketch3DCurveRangeUtil::apply(pCurve1st, pFilletData->fillet.startParam1st,
            pFilletData->fillet.endParam1st))
        {
            goto ABORT_TRANS;
        }
    }

    {
        wy3d::SketchCurve3D* pCurve2nd =
            wy3d::SketchCurve3D::cast(pTrans->getElementForWrite(pConstCurve2nd->getId()));
        if (!pCurve2nd) goto ABORT_TRANS;
        if (!Sketch3DCurveRangeUtil::apply(pCurve2nd, pFilletData->fillet.startParam2nd,
            pFilletData->fillet.endParam2nd))
        {
            goto ABORT_TRANS;
        }
    }

    {
        // The centre is already in space and the two angles are already measured from the frame's
        // xDir, which is the very angle SketchArc3D::create wants: nothing is converted here.
        const wy::Vector3& center = pFilletData->fillet.filletCenter;
        const wy::Vector3& normal = pFilletData->fillet.frame.normal;
        const wy::Vector3& xDir = pFilletData->fillet.frame.xDir;

        wy3d::SketchArc3D* pArc = nullptr;
        if (wy::ErrorStatus::Ok != wy3d::SketchArc3D::create(pTrans, center, normal, xDir,
            pFilletData->fillet.filletRadius, pFilletData->fillet.filletStartAngle,
            pFilletData->fillet.filletEndAngle, pArc) || !pArc)
        {
            goto ABORT_TRANS;
        }
        if (wy::ErrorStatus::Ok != pSketch3D->addEntity(pArc)) goto ABORT_TRANS;
    }

    if (wy::ErrorStatus::Ok != pDb->getTransactionManager()->endTransaction()) goto ABORT_TRANS;
    return true;

ABORT_TRANS:
    assert(false);
    pDb->getTransactionManager()->abortTransaction();
    return false;
}
