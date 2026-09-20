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

#include "SketchChamferGuiCmd.h"
#include "snap/SketchSnapSystem.h"

#include <QCoreApplication>
#include <wyVector2.h>
#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapSelManager.h>
#include <wy3dImpl.h>
#include <wy3dSketch.h>

#include "application/Application.h"
#include "commands/UndoRedoCommands.h"
#include "commands/sketch/SketchTrimExtendUtil.h"
#include "utils/MathUtils.h"
#include <utils/wy3dSketchChamferAlgo.h>
#include "select/filters/CommonSelFilters.h"
#include "scene/nodes/ElementNodeType.h"
#include "commands/dialogs/DoubleValueInputDialog.h"


double SketchChamferGuiCmd::_D1 = 10.0;  // 倒角距离1
double SketchChamferGuiCmd::_D2 = 10.0;  // 倒角距离2

SketchChamferGuiCmd::SketchChamferGuiCmd() : OsgGuiCommand(), _step(Step::Undefined), _pickPos1st(), _pickPos2nd()
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

SketchChamferGuiCmd::~SketchChamferGuiCmd()
{
}

wyap::CmdExecution::StartResult SketchChamferGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = GuiCommand::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);
 
    _sketchInfo = GuiCommandUtil::initSketchInfo();
    if (_sketchInfo.pSketchSnapSys) _sketchInfo.pSketchSnapSys->clearSnapResult();
 
    _sketchInfo = GuiCommandUtil::initSketchInfo();

    // 清空选择集
    Application::instance().getSelManager()->beginChange();
    Application::instance().getSelManager()->clearSelections();
    Application::instance().getSelManager()->endChange();

    // 基本信息
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return wyap::CmdExecution::StartResult::Failed;
    }
    if (_sketchInfo.sketchId.isNull())
    {
        assert(false);
        return wyap::CmdExecution::StartResult::Failed;
    }
    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(_sketchInfo.sketchId));
    if (!pSketch)
    {
        assert(false);
        return wyap::CmdExecution::StartResult::Failed;
    }

    // 弹出对话框:倒角距离
    DoubleValueInputDialog dialog(10.0, 10.0,
        QCoreApplication::translate("SketchChamfer", "Chamfer"),
        QCoreApplication::translate("SketchChamfer", "Distance1"),
        QCoreApplication::translate("SketchChamfer", "Distance2"));
    if (QDialog::Accepted != dialog.exec())
    {
        return wyap::CmdExecution::StartResult::Rejected;
    }
    _D1 = std::fabs(dialog.getValue());
    _D2 = std::fabs(dialog.getValue2nd());

    // 第一步
    _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::SketchEntity);
    _pointPickOption.selType = wy3d::SelectionType::Element;
    this->gotoStep(Step::First);

    return wyap::CmdExecution::StartResult::Succeeded;
}
void SketchChamferGuiCmd::onEnd()
{
    GuiCommand::onEnd();

}
void SketchChamferGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    GuiCommand::onAbort(cause);

}

void SketchChamferGuiCmd::reset()
{
    _pickPos1st.set(0.0, 0.0);
    _pCurveTransient1st = nullptr;

    _pickPos2nd.set(0.0, 0.0);
    _pCurveTransient2nd = nullptr;

    _pChamferTransient = nullptr;
    _pChamferData = nullptr;

    this->gotoStep(Step::First);
}

void SketchChamferGuiCmd::gotoStep(Step step)
{
    _step = step;

    switch (step)
    {
    case Step::First:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchChamfer", "Select the first sketch line."));
        Application::instance().setCursor(CursorType::SelectElements);

        // 前置选择过滤器
        _pointPickOption.pSelPreFilter = std::make_shared<CommonPreSelFilterForPointPick>(
            wy3d::SketchEntity::classInfo(), wydb::ElementId::kNull);
    }
    break;

    case Step::Second:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchChamfer", "Select the second sketch line."));
        Application::instance().setCursor(CursorType::SelectElements);

        // 前置选择过滤器
        wydb::ElementId excludeId = wydb::ElementId::kNull;
        if (_pCurveTransient1st) excludeId = _pCurveTransient1st->getId();
        _pointPickOption.pSelPreFilter = std::make_shared<CommonPreSelFilterForPointPick>(
            wy3d::SketchEntity::classInfo(), excludeId);
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

void SketchChamferGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (!this->getOsgView()) return;

    // 点选
    std::pair<wydb::ElementId, wy::Vector3> pickRet = this->pointPickElement(event.x, event.y, _pointPickOption);

    // 预览
    preview(pickRet.first, pickRet.second);

    // 第一步
    if (Step::First == _step)
    {
        if (!pickRet.first.isNull() && !_pCurveTransient1st)
            Application::instance().setCursor(CursorType::Forbid);
        else
            Application::instance().setCursor(CursorType::SelectElements);
    }
    // 第二步
    else if (Step::Second == _step)
    {
        assert(_pCurveTransient1st);
        if (!_pCurveTransient2nd && _pCurveTransient1st)
        {
            if (_pCurveTransient1st->getStartParam() != 0.0 || _pCurveTransient1st->getEndParam() != 1.0)
            {
                _pCurveTransient1st = std::make_shared<SketchCurveTransient>(_pCurveTransient1st->getId(), 0.0, 1.0);
            }
        }

        if (!pickRet.first.isNull() && !_pCurveTransient2nd)
            Application::instance().setCursor(CursorType::Forbid);
        else
            Application::instance().setCursor(CursorType::SelectElements);
    }

    return;
}

void SketchChamferGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    // 第一步
    if (Step::First == _step)
    {
        if (_pCurveTransient1st)
        {
            this->gotoStep(Step::Second);
        }
    }
    // 第二步
    else if (Step::Second == _step)
    {
        assert(_pCurveTransient1st);
        if (_pCurveTransient2nd)
        {
            bool chamferRet = chamfer(_pChamferData.get());
            assert(chamferRet);
            this->reset();
        }
        else if (_pCurveTransient1st)
        {
            // 如果单击的是选中的第一条曲线则重置
            std::pair<wydb::ElementId, wy::Vector3> pickRet = this->pointPickElement(event.x, event.y, _pointPickOption);
            if (pickRet.first == _pCurveTransient1st->getId())
            {
                this->reset();
            }
        }
    }
    else
    {
        assert(false);
    }

    return;
}

static const wy3d::SketchCurve* _getSketchCurve(const wydb::ElementId& id)
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return nullptr;
    const wy3d::SketchCurve* pSketchCurve = wy3d::SketchCurve::cast(pDb->getElement(id));
    return pSketchCurve;
}

void SketchChamferGuiCmd::preview(wydb::ElementId id, const wy::Vector3& pickPos)
{
    // 第一步
    if (Step::First == _step)
    {
        const wy3d::SketchCurve* pSketchCurve = _getSketchCurve(id);
        if (!pSketchCurve)
        {
            _pCurveTransient1st = nullptr;
            return;
        }

        // 目前只支持直线
        const wy3d::SketchLine* pLine = wy3d::SketchLine::cast(pSketchCurve);
        if (!pLine)
        {
            _pCurveTransient1st = nullptr;
            return;
        }

        _pickPos1st = _sketchInfo.sketchPlane.uv(pickPos);
        if (_pCurveTransient1st && _pCurveTransient1st->getId() == id)
        {
            // 维持不变
        }
        else
        {
            _pCurveTransient1st = std::make_shared<SketchCurveTransient>(pSketchCurve, 0.0, 1.0);
        }
    }
    // 第二步
    else
    {
        // 校验
        assert(_pCurveTransient1st);
        if (!_pCurveTransient1st)
        {
            _pCurveTransient2nd = nullptr;
            _pChamferTransient = nullptr;
            _pChamferData = nullptr;
            return;
        }

        // 不满足求圆角的图元
        const wy3d::SketchCurve* pSketchCurve = _getSketchCurve(id);
        if (!pSketchCurve)
        {
            _pCurveTransient2nd = nullptr;
            _pChamferTransient = nullptr;
            _pChamferData = nullptr;
            return;
        }

        // 目前只支持直线
        const wy3d::SketchLine* pLine = wy3d::SketchLine::cast(pSketchCurve);
        if (!pLine)
        {
            _pCurveTransient2nd = nullptr;
            _pChamferTransient = nullptr;
            _pChamferData = nullptr;
            return;
        }

        // 拾取点坐标
        _pickPos2nd = _sketchInfo.sketchPlane.uv(pickPos);

        // 计算圆角
        std::shared_ptr<ChamferData> pChamferData = chamferPreview(_pCurveTransient1st->getId(), _pickPos1st, id, _pickPos2nd);
        if (pChamferData)
        {
            if (!_pChamferData || (_pChamferData && *pChamferData != *_pChamferData))
            {
                _pCurveTransient1st = std::make_shared<SketchCurveTransient>(pChamferData->id1st, pChamferData->startParam1st, pChamferData->endParam1st);
                _pCurveTransient2nd = std::make_shared<SketchCurveTransient>(pChamferData->id2nd, pChamferData->startParam2nd, pChamferData->endParam2nd);
                _pChamferTransient = std::make_shared<SketchCurveTransient>(_sketchInfo.sketchPlane, pChamferData->chamferStartPnt, pChamferData->chamferEndPnt);
                _pChamferData = pChamferData;
            }
        }
        else
        {
            _pCurveTransient2nd = nullptr;
            _pChamferTransient = nullptr;
            _pChamferData = pChamferData;
        }

        return;
    }
}

std::shared_ptr<ChamferData> SketchChamferGuiCmd::chamferPreview(
    wydb::ElementId id1st, const wy::Vector2& pickPos1st,
    wydb::ElementId id2nd, const wy::Vector2& pickPos2nd)
{
    assert(id1st != id2nd);
    if (id1st == id2nd) return nullptr;

    wydb::Database* pDb = Application::instance().getActiveDatabase();
    assert(pDb);
    if (!pDb) return nullptr;

    const wy3d::SketchCurve* pCurve1st = wy3d::SketchCurve::cast(pDb->getElement(id1st));
    assert(pCurve1st);
    if (!pCurve1st) return nullptr;

    const wy3d::SketchCurve* pCurve2nd = wy3d::SketchCurve::cast(pDb->getElement(id2nd));
    assert(pCurve2nd);
    if (!pCurve2nd) return nullptr;

    if (const wy3d::SketchLine* pLine1st = wy3d::SketchLine::cast(pCurve1st))
    {
        double t1 = SketchTrimExtendUtil::getParamOfLine(pLine1st, pickPos1st);
        t1 = std::clamp(t1, 0.0, 1.0);

        if (const wy3d::SketchLine* pLine2nd = wy3d::SketchLine::cast(pCurve2nd))
        {
            double t2 = SketchTrimExtendUtil::getParamOfLine(pLine2nd, pickPos2nd);
            t2 = std::clamp(t2, 0.0, 1.0);
            return this->chamferPreviewLineLine(pLine1st, t1, pLine2nd, t2);
        }
    }

    return nullptr;
}

static std::shared_ptr<ChamferData> makeChamferData(std::shared_ptr<wy3d::SketchChamferData> pSketchChamferData)
{
    std::shared_ptr<ChamferData> pChamferData = std::make_shared<ChamferData>();
    // curve 1
    pChamferData->startParam1st = pSketchChamferData->startParam1st;
    pChamferData->endParam1st = pSketchChamferData->endParam1st;
    // curve 2
    pChamferData->startParam2nd = pSketchChamferData->startParam2nd;
    pChamferData->endParam2nd = pSketchChamferData->endParam2nd;
    // chamfer
    pChamferData->chamferStartPnt = pSketchChamferData->chamferStartPnt;
    pChamferData->chamferEndPnt = pSketchChamferData->chamferEndPnt;

    return pChamferData;
}

std::shared_ptr<ChamferData> SketchChamferGuiCmd::chamferPreviewLineLine(
    const wy3d::SketchLine* pLine1st, double refParam1st,
    const wy3d::SketchLine* pLine2nd, double refParam2nd)
{
    assert(pLine1st);
    assert(pLine2nd);

    wy::Vector2 startPnt1st = pLine1st->getStartPoint();
    wy::Vector2 endPnt1st = pLine1st->getEndPoint();
    wy::Vector2 startPnt2nd = pLine2nd->getStartPoint();
    wy::Vector2 endPnt2nd = pLine2nd->getEndPoint();
    wy::Vector2 pickPosOnLine1st = startPnt1st + (endPnt1st - startPnt1st) * refParam1st;
    wy::Vector2 pickPosOnLine2nd = startPnt2nd + (endPnt2nd - startPnt2nd) * refParam2nd;

    std::shared_ptr<wy3d::SketchChamferData> pSketchFilletData = wy3d::SketchChamferAlgo::chamferLineLine(_D1, _D2, wy3d::TOL,
        startPnt1st, endPnt1st, pickPosOnLine1st,
        startPnt2nd, endPnt2nd, pickPosOnLine2nd);
    if (!pSketchFilletData) return nullptr;

    std::shared_ptr<ChamferData> pChamferData = makeChamferData(pSketchFilletData);
    pChamferData->id1st = pLine1st->getId();
    pChamferData->id2nd = pLine2nd->getId();

    return pChamferData;
}

bool SketchChamferGuiCmd::chamfer(const ChamferData* pChamferData)
{
    assert(pChamferData);
    if (!pChamferData) return false;

    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return false;

    wydb::Transaction* pTransGroup = pDb->getTransactionManager()->startTransactionGroup();
    if (!pTransGroup) return false;
    {
        if (!chamferItem(pDb, pChamferData->id1st, pChamferData->startParam1st, pChamferData->endParam1st))
        {
            goto ABORT_TRANS;
        }
        if (!chamferItem(pDb, pChamferData->id2nd, pChamferData->startParam2nd, pChamferData->endParam2nd))
        {
            goto ABORT_TRANS;
        }
        if (!chamferLine(pDb, pChamferData->chamferStartPnt, pChamferData->chamferEndPnt))
        {
            goto ABORT_TRANS;
        }
    }
    pDb->getTransactionManager()->endTransaction();
    return true;

ABORT_TRANS:
    pDb->getTransactionManager()->abortTransaction();
    return false;
}

bool SketchChamferGuiCmd::chamferItem(wydb::Database* pDb, const wydb::ElementId& id, double startParam, double endParam)
{
    assert(pDb);

    const wy3d::SketchCurve* pConstCurve = wy3d::SketchCurve::cast(pDb->getElement(id));
    if (!pConstCurve) return false;

    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    wy3d::SketchCurve* pCurve = wy3d::SketchCurve::cast(pTrans->getElementForWrite(id));
    if (!pCurve) goto ABORT_TRANS;
    if (wy3d::SketchLine* pLine = wy3d::SketchLine::cast(pCurve))
    {
        wy::Vector2 lineVec = pLine->getEndPoint() - pLine->getStartPoint();
        wy::Vector2 newStartPnt = pLine->getStartPoint() + startParam * lineVec;
        wy::Vector2 newEndPnt = pLine->getStartPoint() + endParam * lineVec;
        pLine->setStartPoint(newStartPnt);
        pLine->setEndPoint(newEndPnt);
    }
    else if (wy3d::SketchCircle* pCircle = wy3d::SketchCircle::cast(pCurve))
    {
        assert(startParam == 0.0);
        assert(endParam == 1.0);
    }
    else if (wy3d::SketchArc* pArc = wy3d::SketchArc::cast(pCurve))
    {
        double startAngle = pArc->getStartAngle();
        double totalAngle = pArc->getTotalAngle();
        if (startParam != 0.0)
        {
            pArc->setStartAngle(startAngle + startParam * totalAngle);
        }
        if (endParam != 1.0)
        {
            pArc->setEndAngle(startAngle + endParam * totalAngle);
        }
    }
    else
    {
        assert(false);
        goto ABORT_TRANS;
    }

    pDb->getTransactionManager()->endTransaction();
    return true;

ABORT_TRANS:
    pDb->getTransactionManager()->abortTransaction();
    return false;
}

bool SketchChamferGuiCmd::chamferLine(wydb::Database* pDb, const wy::Vector2& startPnt, const wy::Vector2& endPnt)
{
    assert(pDb);

    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    wy3d::SketchLine* pLine = nullptr;

    wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(_sketchInfo.sketchId));
    if (!pSketch) goto ABORT_TRANS;
    if (wy::ErrorStatus::Ok != wy3d::SketchLine::create(pTrans, startPnt, endPnt, pLine) || !pLine)
    {
        goto ABORT_TRANS;
    }

    pSketch->addEntity(pLine);
    pDb->getTransactionManager()->endTransaction();
    return true;

ABORT_TRANS:
    pDb->getTransactionManager()->abortTransaction();
    return false;
}