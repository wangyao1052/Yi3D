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

#include "Sketch3DChamferGuiCmd.h"

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
    // The same orange the 3D sketch previews use for a curve, so the chamfer segment reads as part
    // of the same preview as the two ranges it joins.
    const osg::Vec4 kPreviewColor(1.0f, 0.392f, 0.039f, 1.0f);

    const wy3d::SketchLine3D* getLine(const wydb::ElementId& id)
    {
        wydb::Database* pDb = Application::instance().getActiveDatabase();
        if (!pDb) return nullptr;
        return wy3d::SketchLine3D::cast(pDb->getElement(id));
    }
}

double Sketch3DChamferGuiCmd::_D1 = 10.0;
double Sketch3DChamferGuiCmd::_D2 = 10.0;

Sketch3DChamferGuiCmd::Sketch3DChamferGuiCmd()
    : OsgGuiCommand(), _step(Step::First), _id1st(wydb::ElementId::kNull), _pickPos1st(),
    _id2nd(wydb::ElementId::kNull), _pickPos2nd()
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

Sketch3DChamferGuiCmd::~Sketch3DChamferGuiCmd()
{
}

wyap::CmdExecution::StartResult Sketch3DChamferGuiCmd::onStart()
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

    DoubleValueInputDialog dialog(_D1, _D2,
        QCoreApplication::translate("Sketch3DChamfer", "Chamfer"),
        QCoreApplication::translate("Sketch3DChamfer", "Distance1"),
        QCoreApplication::translate("Sketch3DChamfer", "Distance2"));
    if (QDialog::Accepted != dialog.exec())
    {
        return wyap::CmdExecution::StartResult::Rejected;
    }
    _D1 = std::fabs(dialog.getValue());
    _D2 = std::fabs(dialog.getValue2nd());

    _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Sketch3DEntity);
    _pointPickOption.selType = wy3d::SelectionType::Element;

    this->gotoStep(Step::First);

    return wyap::CmdExecution::StartResult::Succeeded;
}

void Sketch3DChamferGuiCmd::onEnd()
{
    GuiCommand::onEnd();
}

void Sketch3DChamferGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    GuiCommand::onAbort(cause);
}

void Sketch3DChamferGuiCmd::reset()
{
    _id1st = wydb::ElementId::kNull;
    _pickPos1st = wy::Vector3::kZero;
    _pCurveTransient1st = nullptr;

    this->clearSecondPreview();

    this->gotoStep(Step::First);
}

void Sketch3DChamferGuiCmd::clearSecondPreview()
{
    _id2nd = wydb::ElementId::kNull;
    _pickPos2nd = wy::Vector3::kZero;
    _pCurveTransient2nd = nullptr;
    _pChamferTransient = nullptr;
    _pChamferData = nullptr;
}

void Sketch3DChamferGuiCmd::gotoStep(Step step)
{
    _step = step;

    switch (step)
    {
    case Step::First:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("Sketch3DChamfer",
            "Select the first straight sketch entity."));
        Application::instance().setCursor(CursorType::SelectElements);

        _pointPickOption.pSelPreFilter = std::make_shared<CommonPreSelFilterForPointPick>(
            wy3d::SketchEntity3D::classInfo(), wydb::ElementId::kNull);
    }
    break;

    case Step::Second:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("Sketch3DChamfer",
            "Select the second straight sketch entity."));
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

void Sketch3DChamferGuiCmd::onMouseMove(const MouseEvent& event)
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

void Sketch3DChamferGuiCmd::preview(const wydb::ElementId& id, const wy::Vector3& pickPos)
{
    if (Step::First == _step)
    {
        const wy3d::SketchLine3D* pLine = getLine(id);
        if (!pLine)
        {
            _pCurveTransient1st = nullptr;
            return;
        }

        _id1st = id;
        _pickPos1st = pickPos;

        // Left alone while the cursor stays on the same line, so that moving along one entity does
        // not rebuild the same geometry on every event.
        if (!_pCurveTransient1st || _pCurveTransient1st->getId() != id)
        {
            _pCurveTransient1st = std::make_shared<Sketch3DTrimExtendTransient>(pLine, 0.0, 1.0);
        }
        return;
    }

    assert(_pCurveTransient1st);
    if (!_pCurveTransient1st)
    {
        this->clearSecondPreview();
        return;
    }

    const wy3d::SketchLine3D* pLine2nd = getLine(id);
    if (!pLine2nd || id == _id1st)
    {
        this->clearSecondPreview();

        // The first line goes back to being highlighted whole: the preview of a chamfer that is no
        // longer there must not leave it looking trimmed.
        if (_pCurveTransient1st->getStartParam() != 0.0 || _pCurveTransient1st->getEndParam() != 1.0)
        {
            const wy3d::SketchLine3D* pLine1st = getLine(_id1st);
            if (pLine1st)
            {
                _pCurveTransient1st = std::make_shared<Sketch3DTrimExtendTransient>(pLine1st, 0.0, 1.0);
            }
        }
        return;
    }

    const wy3d::SketchLine3D* pLine1st = getLine(_id1st);
    if (!pLine1st)
    {
        this->clearSecondPreview();
        return;
    }

    wy3d::Sketch3DChamferData chamfer;
    if (wy3d::Sketch3DChamferAlgo::Result::Ok != wy3d::Sketch3DChamferAlgo::chamferLineLine(_D1, _D2,
        wy3d::TOL, pLine1st, _pickPos1st, pLine2nd, pickPos, chamfer))
    {
        this->clearSecondPreview();
        return;
    }

    _id2nd = id;
    _pickPos2nd = pickPos;

    std::shared_ptr<Chamfer3DData> pChamferData = std::make_shared<Chamfer3DData>();
    pChamferData->id1st = _id1st;
    pChamferData->id2nd = _id2nd;
    pChamferData->chamfer = chamfer;

    if (_pChamferData && *pChamferData == *_pChamferData)
    {
        return;
    }

    _pCurveTransient1st = std::make_shared<Sketch3DTrimExtendTransient>(pLine1st,
        chamfer.startParam1st, chamfer.endParam1st);
    _pCurveTransient2nd = std::make_shared<Sketch3DTrimExtendTransient>(pLine2nd,
        chamfer.startParam2nd, chamfer.endParam2nd);

    // A straight segment has no entity to sample, so the transient is updated rather than rebuilt.
    if (!_pChamferTransient)
    {
        _pChamferTransient = std::make_shared<LineTransient>();
        _pChamferTransient->setColor(kPreviewColor);
    }
    _pChamferTransient->update(chamfer.chamferStartPnt, chamfer.chamferEndPnt);
    _pChamferData = pChamferData;
}

void Sketch3DChamferGuiCmd::onLeftMouseUp(const MouseEvent& event)
{
    if (Step::First == _step)
    {
        if (_pCurveTransient1st)
        {
            this->gotoStep(Step::Second);
        }
        return;
    }

    if (_pChamferTransient)
    {
        // An undo while the command was running can take one of the two away between the last
        // mouse move and this click. Committing then would be committing to a pair that is no
        // longer there, so the guard is here rather than inside chamfer().
        if (!getLine(_id1st) || !getLine(_id2nd))
        {
            this->reset();
            return;
        }

        const bool chamfered = this->chamfer(_pChamferData.get());
        assert(chamfered);
        this->reset();
        return;
    }

    // Clicking the first line again drops it, so that a pair can be started over without leaving
    // the command.
    const std::pair<wydb::ElementId, wy::Vector3> pickRet =
        this->pointPickElement(event.x, event.y, _pointPickOption);
    if (pickRet.first == _id1st)
    {
        this->reset();
    }
}

bool Sketch3DChamferGuiCmd::chamfer(const Chamfer3DData* pChamferData)
{
    assert(pChamferData);

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
    const wy3d::SketchCurve3D* pConstCurve1st = wy3d::SketchCurve3D::cast(pDb->getElement(pChamferData->id1st));
    const wy3d::SketchCurve3D* pConstCurve2nd = wy3d::SketchCurve3D::cast(pDb->getElement(pChamferData->id2nd));
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
        if (!Sketch3DCurveRangeUtil::apply(pCurve1st, pChamferData->chamfer.startParam1st,
            pChamferData->chamfer.endParam1st))
        {
            goto ABORT_TRANS;
        }
    }

    {
        wy3d::SketchCurve3D* pCurve2nd =
            wy3d::SketchCurve3D::cast(pTrans->getElementForWrite(pConstCurve2nd->getId()));
        if (!pCurve2nd) goto ABORT_TRANS;
        if (!Sketch3DCurveRangeUtil::apply(pCurve2nd, pChamferData->chamfer.startParam2nd,
            pChamferData->chamfer.endParam2nd))
        {
            goto ABORT_TRANS;
        }
    }

    {
        wy3d::SketchLine3D* pLine = nullptr;
        if (wy::ErrorStatus::Ok != wy3d::SketchLine3D::create(pTrans, pChamferData->chamfer.chamferStartPnt,
            pChamferData->chamfer.chamferEndPnt, pLine) || !pLine)
        {
            goto ABORT_TRANS;
        }
        if (wy::ErrorStatus::Ok != pSketch3D->addEntity(pLine)) goto ABORT_TRANS;
    }

    if (wy::ErrorStatus::Ok != pDb->getTransactionManager()->endTransaction()) goto ABORT_TRANS;
    return true;

ABORT_TRANS:
    assert(false);
    pDb->getTransactionManager()->abortTransaction();
    return false;
}
