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

#include "SketchTrimGuiCmd.h"
#include "snap/SketchSnapSystem.h"

#include <cassert>
#include <QCoreApplication>

#include <wyVector2.h>
#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapSelManager.h>
#include <wy3dImpl.h>
#include <wy3dSketch.h>
#include <wy3dSketchCurve.h>

#include <wy3dSketchLine.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>
#include <wy3dSketchEllipse.h>
#include <wy3dSketchEllipseArc.h>
#include <wy3dSketchSpline.h>
#include <wy3dSketchSpline.h>

#include <wy3dApp/Application.h>
#include "application/ApplicationImpl.h"
#include "utils/MathUtils.h"
#include "utils/SplineUtil.h"
#include "scene/SketchEntityLinearization.h"
#include "commands/UndoRedoCommands.h"
#include "scene/RenderConst.h"
#include <wy3dApp/CommonSelFilters.h>
#include "scene/nodes/ElementNodeType.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

SketchTrimGuiCmd::SketchTrimGuiCmd() : OsgGuiCommand()
{
}

SketchTrimGuiCmd::~SketchTrimGuiCmd()
{
}

void SketchTrimGuiCmd::onDatabaseChanged(
    const wydb::Database* pDb,
    const wydb::Transaction* pTransaction,
    const wydb::DatabaseChangeInfo& changeInfo)
{
    assert(pDb);

    // 新增的元素
    for (const wydb::ElementId& id : changeInfo.addedIds)
    {
        const wy3d::SketchCurve* pCurve = wy3d::SketchCurve::cast(pDb->getElement(id));
        if (!pCurve) continue;
        auto iter = _id2Parent.find(id);
        if (iter == _id2Parent.cend()) // 进入修剪命令时就存在的元素,删除之后再undo
        {
            SketchTrimNodeSPtr pNode = _pTrimGraph->getNode(id);
            assert(pNode);
            if (pNode) pNode->refresh(pDb); // TODO 感觉这里是不需要调用的
        }
        else
        {
            wydb::ElementId idParent = iter->second;
            SketchTrimNodeSPtr pParentNode = _pTrimGraph->getNode(idParent);
            assert(pParentNode);
            if (!pParentNode) continue;
            SketchTrimNodeSPtr pNode = _pTrimGraph->getNode(id);
            if (pNode) // undo
            {
                pNode->refresh(pDb); // TODO 感觉这里是不需要调用的
            }
            else // trim
            {
                SketchTrimNodeSPtr pNewNode = pParentNode->clone(id);
                pNewNode->refresh(pDb);
                pParentNode->appendChild(pNewNode);
                bool addRet = _pTrimGraph->addNode(pNewNode);
                assert(addRet);
            }
        }
    }

    // 修改的元素
    for (const wydb::ElementId& id : changeInfo.modifiedIds)
    {
        const wy3d::SketchCurve* pCurve = wy3d::SketchCurve::cast(pDb->getElement(id));
        if (!pCurve) continue;
        SketchTrimNodeSPtr pNode = _pTrimGraph->getNode(id);
        assert(pNode);
        if (pNode) pNode->refresh(pDb);
    }
}

wyap::CmdExecution::StartResult SketchTrimGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = GuiCommand::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);
 
    _sketchInfo = GuiCommandUtil::initSketchInfo();
    if (_sketchInfo.pSketchSnapSys) _sketchInfo.pSketchSnapSys->clearSnapResult();
 
    _sketchInfo = GuiCommandUtil::initSketchInfo();

    // 清空选择集
    ApplicationImpl::instance().getSelManager()->beginChange();
    ApplicationImpl::instance().getSelManager()->clearSelections();
    ApplicationImpl::instance().getSelManager()->endChange();

    // 初始化修剪图信息
    wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
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
    _pTrimGraph = std::make_unique<SketchTrimGraph>(pSketch, 1e-6);
    if (!_pTrimGraph->isValid())
    {
        assert(false);
        return wyap::CmdExecution::StartResult::Failed;
    }

    // add database reactor
    pDb->addReactor(this);

    // 鼠标样式
    ApplicationImpl::instance().setCursor(CursorType::SelectElements);
    // 提示信息
    ApplicationImpl::instance().getStatusBar()->setTips(QCoreApplication::translate("Trim", "Select sketch entities to trim."));

    // 点选选项
    _pointPickOption.allowedElemTypes = ElementNodeType::SketchEntity;
    _pointPickOption.selType = wy3d::SelectionType::Element;
    _pointPickOption.preFilter = CommonSelPreFilter::create<wy3d::SketchEntity>();

    return wyap::CmdExecution::StartResult::Succeeded;
}
void SketchTrimGuiCmd::onEnd()
{
    GuiCommand::onEnd();

    // remove database reactor
    wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    assert(pDb);
    if (pDb)
    {
        pDb->removeReactor(this);
    }

}
void SketchTrimGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    GuiCommand::onAbort(cause);

    // remove database reactor
    wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    assert(pDb);
    if (pDb)
    {
        pDb->removeReactor(this);
    }

}

void SketchTrimGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (!this->getOsgView()) return;

    // 点选
    const wyap::Selection sel = this->pointPick(event.x, event.y, _pointPickOption);

    // 拾取修剪段
    pickTrimSegment(sel.getElementId(), sel.getPickPosition());

    // 设置鼠标样式
    if (_pTransient)
        ApplicationImpl::instance().setCursor(CursorType::Delete);
    else
        ApplicationImpl::instance().setCursor(CursorType::SelectElements);

    return;
}

void SketchTrimGuiCmd::pickTrimSegment(const wydb::ElementId& id, const wy::Vector3& pickPos3d)
{
    if (id.isNull())
    {
        _pTransient = nullptr;
    }
    else
    {
        wy::Vector2 pickPosUV = _sketchInfo.sketchPlane.uv(pickPos3d);
        assert(_pTrimGraph);
        SketchTrimSegment segment = _pTrimGraph->pick(id, pickPosUV);
        if (!segment.isValid()) // 排除无效段
        {
            assert(false);
            _pTransient = nullptr;
            return;
        }

        wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
        if (!pDb)
        {
            assert(false);
            _pTransient = nullptr;
            return;
        }
        const wy3d::SketchCurve* pSketchCurve = wy3d::SketchCurve::cast(pDb->getElement(id));
        if (!pSketchCurve)
        {
            assert(false);
            _pTransient = nullptr;
            return;
        }

        if (!_pTransient)
        {
            _pTransient = std::make_shared<TrimSegmentTransient>(this, _sketchInfo.sketchPlane, pSketchCurve, segment);
        }
        else if (_pTransient->getId() != id
            || _pTransient->getSegment().startKnot.getParam() != segment.startKnot.getParam()
            || _pTransient->getSegment().endKnot.getParam() != segment.endKnot.getParam()) // 比较是否是相同的修剪段
        {
            _pTransient = std::make_shared<TrimSegmentTransient>(this, _sketchInfo.sketchPlane, pSketchCurve, segment);
        }
    }
}

void SketchTrimGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    if (_pTransient)
    {
        wydb::ElementId id = _pTransient->getId();
        assert(_pTransient->getSegment().isValid());
        if (_pTransient->getSegment().isValid())
        {
            const SketchTrimKnot& startKnot = _pTransient->getSegment().startKnot;
            const SketchTrimKnot& endKnot = _pTransient->getSegment().endKnot;
            bool trimRet = trim(id, startKnot, endKnot);
            assert(trimRet);
        }
    }
    _pTransient = nullptr;
    ApplicationImpl::instance().setCursor(CursorType::SelectElements);

    return;
}

bool SketchTrimGuiCmd::trim(const wydb::ElementId curveId, const SketchTrimKnot& startKnot, const SketchTrimKnot& endKnot)
{
    wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return false;
    }
    const wy3d::Sketch* pConstSketch = wy3d::Sketch::cast(pDb->getElement(_sketchInfo.sketchId));
    if (!pConstSketch)
    {
        assert(false);
        return false;
    }
    const wy3d::SketchCurve* pConstCurve = wy3d::SketchCurve::cast(pDb->getElement(curveId));
    if (!pConstCurve)
    {
        assert(false);
        return false;
    }

    // important
    // 通过调用基类的startTransaction方法保证开启的事务是不传播级联更新的,在退出草图环境时会markdirty草图的Shape.
    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction("", option);
    // }
    if (!pTrans)
    {
        assert(false);
        return false;
    }
    wy::ErrorStatus error;
    bool ret = false;
    wy3d::Sketch* pSketch = nullptr;
    wy3d::SketchCurve* pSketchCurve = nullptr;
    pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(pConstSketch->getId()));
    if (!pSketch) goto ABORT_TRANS;
    pSketchCurve = wy3d::SketchCurve::cast(pTrans->getElementForWrite(pConstCurve->getId()));
    if (!pSketchCurve) goto ABORT_TRANS;
    if (wy3d::SketchLine* pLine = wy3d::SketchLine::cast(pSketchCurve))
    {
        ret = trimLine(pDb, pTrans, pSketch, pLine, startKnot, endKnot);
    }
    else if (wy3d::SketchCenterLine* pCenterLine = wy3d::SketchCenterLine::cast(pSketchCurve))
    {
        ret = trimCenterLine(pDb, pTrans, pSketch, pCenterLine, startKnot, endKnot);
    }
    else if (wy3d::SketchCircle* pCircle = wy3d::SketchCircle::cast(pSketchCurve))
    {
        ret = trimCircle(pDb, pTrans, pSketch, pCircle, startKnot, endKnot);
    }
    else if (wy3d::SketchArc* pArc = wy3d::SketchArc::cast(pSketchCurve))
    {
        ret = trimArc(pDb, pTrans, pSketch, pArc, startKnot, endKnot);
    }
    else if (wy3d::SketchEllipse* pEllipse = wy3d::SketchEllipse::cast(pSketchCurve))
    {
        ret = trimEllipse(pDb, pTrans, pSketch, pEllipse, startKnot, endKnot);
    }
    else if (wy3d::SketchEllipseArc* pEllipseArc = wy3d::SketchEllipseArc::cast(pSketchCurve))
    {
        ret = trimEllipseArc(pDb, pTrans, pSketch, pEllipseArc, startKnot, endKnot);
    }
    else if (wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pSketchCurve))
    {
        ret = trimSpline(pDb, pTrans, pSketch, pSpline, startKnot, endKnot);
    }
    else
    {
        assert(false);
        goto ABORT_TRANS;
    }
    if (!ret) goto ABORT_TRANS;
    error = pDb->getTransactionManager()->endTransaction();
    assert(wy::ErrorStatus::Ok == error);

    return true;

ABORT_TRANS:
    assert(false);
    pDb->getTransactionManager()->abortTransaction();
    return false;
}

bool SketchTrimGuiCmd::trimLine(
    wydb::Database* pDb, wydb::Transaction* pTrans,
    wy3d::Sketch* pSketch, wy3d::SketchLine* pLine,
    const SketchTrimKnot& startKnot, const SketchTrimKnot& endKnot)
{
    assert(pDb);
    assert(pTrans);
    assert(pSketch);
    assert(pLine);
    assert(startKnot.getParam() >= 0.0 && startKnot.getParam() <= 1.0);
    assert(endKnot.getParam() >= 0.0 && endKnot.getParam() <= 1.0);

    if (startKnot.getParam() == 0.0 && endKnot.getParam() == 1.0)
    {
        return wy::ErrorStatus::Ok == pLine->erase(true);
    }
    else if (startKnot.getParam() == 0.0)
    {
        wy::Vector2 startPnt = pLine->getStartPoint();
        wy::Vector2 endPnt = pLine->getEndPoint();
        wy::Vector2 newStartPnt = startPnt + endKnot.getParam() * (endPnt - startPnt);
        return wy::ErrorStatus::Ok == pLine->setStartPoint(newStartPnt);
    }
    else if (endKnot.getParam() == 1.0)
    {
        wy::Vector2 startPnt = pLine->getStartPoint();
        wy::Vector2 endPnt = pLine->getEndPoint();
        wy::Vector2 newEndPnt = startPnt + startKnot.getParam() * (endPnt - startPnt);
        return wy::ErrorStatus::Ok == pLine->setEndPoint(newEndPnt);
    }
    else
    {
        wy::ErrorStatus error(wy::ErrorStatus::Ok);
        wy::Vector2 startPnt = pLine->getStartPoint();
        wy::Vector2 endPnt = pLine->getEndPoint();
        // 修改直线段终点
        if (wy::ErrorStatus::Ok != pLine->setEndPoint(startPnt + startKnot.getParam() * (endPnt - startPnt)))
        {
            return false;
        }
        // 新的直线段
        wy3d::SketchLine* pNewSketchLine(nullptr);
        if (wy::ErrorStatus::Ok != wy3d::SketchLine::create(pTrans, startPnt + endKnot.getParam() * (endPnt - startPnt), endPnt, pNewSketchLine)
            || !pNewSketchLine)
        {
            return false;
        }
        error = pSketch->addEntity(pNewSketchLine);
        assert(wy::ErrorStatus::Ok == error);
        // 父子关系
        _id2Parent[pNewSketchLine->getId()] = pLine->getId();
        return true;
    }
}

bool SketchTrimGuiCmd::trimCenterLine(
    wydb::Database* pDb, wydb::Transaction* pTrans,
    wy3d::Sketch* pSketch, wy3d::SketchCenterLine* pLine,
    const SketchTrimKnot& startKnot, const SketchTrimKnot& endKnot)
{
    assert(pDb);
    assert(pTrans);
    assert(pSketch);
    assert(pLine);
    assert(startKnot.getParam() >= 0.0 && startKnot.getParam() <= 1.0);
    assert(endKnot.getParam() >= 0.0 && endKnot.getParam() <= 1.0);

    if (startKnot.getParam() == 0.0 && endKnot.getParam() == 1.0)
    {
        return wy::ErrorStatus::Ok == pLine->erase(true);
    }
    else if (startKnot.getParam() == 0.0)
    {
        wy::Vector2 startPnt = pLine->getStartPoint();
        wy::Vector2 endPnt = pLine->getEndPoint();
        wy::Vector2 newStartPnt = startPnt + endKnot.getParam() * (endPnt - startPnt);
        return wy::ErrorStatus::Ok == pLine->setStartPoint(newStartPnt);
    }
    else if (endKnot.getParam() == 1.0)
    {
        wy::Vector2 startPnt = pLine->getStartPoint();
        wy::Vector2 endPnt = pLine->getEndPoint();
        wy::Vector2 newEndPnt = startPnt + startKnot.getParam() * (endPnt - startPnt);
        return wy::ErrorStatus::Ok == pLine->setEndPoint(newEndPnt);
    }
    else
    {
        wy::ErrorStatus error(wy::ErrorStatus::Ok);
        wy::Vector2 startPnt = pLine->getStartPoint();
        wy::Vector2 endPnt = pLine->getEndPoint();
        // 修改直线段终点
        if (wy::ErrorStatus::Ok != pLine->setEndPoint(startPnt + startKnot.getParam() * (endPnt - startPnt)))
        {
            return false;
        }
        // 新的直线段
        wy3d::SketchCenterLine* pNewSketchLine(nullptr);
        if (wy::ErrorStatus::Ok != wy3d::SketchCenterLine::create(pTrans, startPnt + endKnot.getParam() * (endPnt - startPnt), endPnt, pNewSketchLine)
            || !pNewSketchLine)
        {
            return false;
        }
        error = pSketch->addEntity(pNewSketchLine);
        assert(wy::ErrorStatus::Ok == error);
        // 父子关系
        _id2Parent[pNewSketchLine->getId()] = pLine->getId();
        return true;
    }
}

bool SketchTrimGuiCmd::trimCircle(
    wydb::Database* pDb, wydb::Transaction* pTrans,
    wy3d::Sketch* pSketch, wy3d::SketchCircle* pCircle,
    const SketchTrimKnot& startKnot, const SketchTrimKnot& endKnot)
{
    assert(pDb);
    assert(pTrans);
    assert(pSketch);
    assert(pCircle);
    assert(startKnot.getParam() >= 0.0 && startKnot.getParam() <= 1.0);
    assert(endKnot.getParam() >= 0.0 && endKnot.getParam() <= 1.0);

    if (startKnot.getParam() == 0.0 && endKnot.getParam() == 1.0)
    {
        return wy::ErrorStatus::Ok == pCircle->erase(true);
    }
    else if (startKnot.getParam() >= 0.0 && startKnot.getParam() < 1.0
        && endKnot.getParam() >= 0.0 && endKnot.getParam() < 1.0)
    {
        wy::Vector2 center = pCircle->getCenter();
        double radius = pCircle->getRadius();
        double startAngle = endKnot.getParam() * wy3d::TWO_PI;
        double endAngle = startKnot.getParam() * wy3d::TWO_PI;
        if (endAngle < startAngle) endAngle += wy3d::TWO_PI;
        double totalAngle = endAngle - startAngle;
        if (totalAngle > wy3d::EPS)
        {
            // 创建新的圆弧
            wy3d::SketchArc* pNewArc(nullptr);
            if (wy::ErrorStatus::Ok != wy3d::SketchArc::create(pTrans, center, radius, startAngle, endAngle, pNewArc)
                || !pNewArc)
            {
                return false;
            }
            pSketch->addEntity(pNewArc);
            // 父子关系
            _id2Parent[pNewArc->getId()] = pCircle->getId();
            // 删除圆
            pCircle->erase(true);
            return true;
        }
        else
        {
            assert(false);
            return wy::ErrorStatus::Ok == pCircle->erase(true);
        }
    }
    else
    {
        assert(false);
        return wy::ErrorStatus::Ok == pCircle->erase(true);
    }
}

bool SketchTrimGuiCmd::trimArc(
    wydb::Database* pDb, wydb::Transaction* pTrans,
    wy3d::Sketch* pSketch, wy3d::SketchArc* pArc,
    const SketchTrimKnot& startKnot, const SketchTrimKnot& endKnot)
{
    assert(pDb);
    assert(pTrans);
    assert(pSketch);
    assert(pArc);
    assert(startKnot.getParam() >= 0.0 && startKnot.getParam() <= 1.0);
    assert(endKnot.getParam() >= 0.0 && endKnot.getParam() <= 1.0);

    if (startKnot.getParam() == 0.0 && endKnot.getParam() == 1.0)
    {
        return wy::ErrorStatus::Ok == pArc->erase(true);
    }
    else if (startKnot.getParam() == 0.0)
    {
        double newStartAngle = pArc->getStartAngle() + endKnot.getParam() * pArc->getTotalAngle();
        newStartAngle = wy3d::normalizeRadian(newStartAngle);
        return wy::ErrorStatus::Ok == pArc->setStartAngle(newStartAngle);
    }
    else if (endKnot.getParam() == 1.0)
    {
        double newEndAngle = pArc->getStartAngle() + startKnot.getParam() * pArc->getTotalAngle();
        newEndAngle = wy3d::normalizeRadian(newEndAngle);
        return wy::ErrorStatus::Ok == pArc->setEndAngle(newEndAngle);
    }
    else
    {
        double startAngle = pArc->getStartAngle();
        double endAngle = pArc->getEndAngle();
        double totalAngle = pArc->getTotalAngle();
        // 修改圆弧
        double newEndAngle = startAngle + startKnot.getParam() * totalAngle;
        newEndAngle = wy3d::normalizeRadian(newEndAngle);
        if (wy::ErrorStatus::Ok != pArc->setEndAngle(newEndAngle))
        {
            return false;
        }
        // 新增圆弧
        wy3d::SketchArc* pNewArc(nullptr);
        double newStartAngle = startAngle + endKnot.getParam() * totalAngle;
        newStartAngle = wy3d::normalizeRadian(newStartAngle);
        if (wy::ErrorStatus::Ok != wy3d::SketchArc::create(pTrans, pArc->getCenter(), pArc->getRadius(), newStartAngle, endAngle, pNewArc)
            || !pNewArc)
        {
            return false;
        }
        pSketch->addEntity(pNewArc);
        // 父子关系
        _id2Parent[pNewArc->getId()] = pArc->getId();
        return true;
    }
}

bool SketchTrimGuiCmd::trimEllipse(
    wydb::Database* pDb, wydb::Transaction* pTrans,
    wy3d::Sketch* pSketch, wy3d::SketchEllipse* pEllipse,
    const SketchTrimKnot& startKnot, const SketchTrimKnot& endKnot)
{
    assert(pDb);
    assert(pTrans);
    assert(pSketch);
    assert(pEllipse);
    assert(startKnot.getParam() >= 0.0 && startKnot.getParam() <= 1.0);
    assert(endKnot.getParam() >= 0.0 && endKnot.getParam() <= 1.0);

    if (startKnot.getParam() == 0.0 && endKnot.getParam() == 1.0)
    {
        return wy::ErrorStatus::Ok == pEllipse->erase(true);
    }
    else if (startKnot.getParam() >= 0.0 && startKnot.getParam() < 1.0
        && endKnot.getParam() >= 0.0 && endKnot.getParam() < 1.0)
    {
        wy::Vector2 center = pEllipse->getCenter();
        double startAngle = endKnot.getParam() * wy3d::TWO_PI;
        double endAngle = startKnot.getParam() * wy3d::TWO_PI;
        if (endAngle < startAngle) endAngle += wy3d::TWO_PI;
        double totalAngle = endAngle - startAngle;
        if (totalAngle > wy3d::EPS)
        {
            // 创建新的椭圆弧
            wy3d::SketchEllipseArc* pNewEllipseArc(nullptr);
            if (wy::ErrorStatus::Ok != wy3d::SketchEllipseArc::create(pTrans, center, pEllipse->getMajorAxis(), pEllipse->getRadiusRatio(), startAngle, endAngle, pNewEllipseArc)
                || !pNewEllipseArc)
            {
                return false;
            }
            pSketch->addEntity(pNewEllipseArc);
            // 父子关系
            _id2Parent[pNewEllipseArc->getId()] = pEllipse->getId();
            // 删除椭圆
            pEllipse->erase(true);
            return true;
        }
        else
        {
            assert(false);
            return wy::ErrorStatus::Ok == pEllipse->erase(true);
        }
    }
    else
    {
        assert(false);
        return wy::ErrorStatus::Ok == pEllipse->erase(true);
    }
}

bool SketchTrimGuiCmd::trimEllipseArc(
    wydb::Database* pDb, wydb::Transaction* pTrans,
    wy3d::Sketch* pSketch, wy3d::SketchEllipseArc* pEllipseArc,
    const SketchTrimKnot& startKnot, const SketchTrimKnot& endKnot)
{
    assert(pDb);
    assert(pTrans);
    assert(pSketch);
    assert(pEllipseArc);
    assert(startKnot.getParam() >= 0.0 && startKnot.getParam() <= 1.0);
    assert(endKnot.getParam() >= 0.0 && endKnot.getParam() <= 1.0);

    if (startKnot.getParam() == 0.0 && endKnot.getParam() == 1.0)
    {
        return wy::ErrorStatus::Ok == pEllipseArc->erase(true);
    }
    else if (startKnot.getParam() == 0.0)
    {
        double newStartAngle = pEllipseArc->getStartAngle() + endKnot.getParam() * pEllipseArc->getTotalAngle();
        newStartAngle = wy3d::normalizeRadian(newStartAngle);
        return wy::ErrorStatus::Ok == pEllipseArc->setStartAngle(newStartAngle);
    }
    else if (endKnot.getParam() == 1.0)
    {
        double newEndAngle = pEllipseArc->getStartAngle() + startKnot.getParam() * pEllipseArc->getTotalAngle();
        newEndAngle = wy3d::normalizeRadian(newEndAngle);
        return wy::ErrorStatus::Ok == pEllipseArc->setEndAngle(newEndAngle);
    }
    else
    {
        double startAngle = wy3d::normalizeRadian(pEllipseArc->getStartAngle());
        double endAngle = wy3d::normalizeRadian(pEllipseArc->getEndAngle());
        double totalAngle = pEllipseArc->getTotalAngle();
        // 修改椭圆弧
        double newEndAngle = startAngle + startKnot.getParam() * totalAngle;
        if (wy::ErrorStatus::Ok != pEllipseArc->setEndAngle(newEndAngle))
        {
            return false;
        }
        // 新增椭圆弧
        wy3d::SketchEllipseArc* pNewEllipseArc(nullptr);
        double newStartAngle = startAngle + endKnot.getParam() * totalAngle;
        newStartAngle = wy3d::normalizeRadian(newStartAngle);
        if (wy::ErrorStatus::Ok != wy3d::SketchEllipseArc::create(pTrans, pEllipseArc->getCenter(), pEllipseArc->getMajorAxis(), pEllipseArc->getRadiusRatio(), newStartAngle,
            endAngle < newStartAngle ? endAngle + wy3d::TWO_PI : endAngle, pNewEllipseArc) || !pNewEllipseArc)
        {
            return false;
        }
        pTrans->addNewlyCreatedElement(pNewEllipseArc);
        pSketch->addEntity(pNewEllipseArc);
        // 父子关系
        _id2Parent[pNewEllipseArc->getId()] = pEllipseArc->getId();
        return true;
    }
}

bool SketchTrimGuiCmd::trimSpline(
    wydb::Database* pDb, wydb::Transaction* pTrans,
    wy3d::Sketch* pSketch, wy3d::SketchSpline* pSpline,
    const SketchTrimKnot& startKnot, const SketchTrimKnot& endKnot)
{
    assert(pDb);
    assert(pTrans);
    assert(pSketch);
    assert(pSpline);
    assert(startKnot.getParam() >= 0.0 && startKnot.getParam() <= 1.0);
    assert(endKnot.getParam() >= 0.0 && endKnot.getParam() <= 1.0);    if (startKnot.getParam() == 0.0 && endKnot.getParam() == 1.0)
    {
        return wy::ErrorStatus::Ok == pSpline->erase(true);
    }
    else if (startKnot.getParam() == 0.0 || endKnot.getParam() == 1.0)
    {
        double startParam(0.0), endParam(1.0);
        if (startKnot.getParam() == 0.0)
        {
            startParam = endKnot.getParam();
            endParam = 1.0;
        }
        else
        {
            startParam = 0.0;
            endParam = startKnot.getParam();
        }

        unsigned int degree(0);
        std::vector<wy::Vector2> controlPoints;
        std::vector<double> knots;
        std::vector<unsigned int> multiplicities;
        if (SplineUtil::segment(pSpline, startParam, endParam,
            degree, controlPoints, knots, multiplicities))
        {
            if (wy::ErrorStatus::Ok != pSpline->setMode(wy3d::SplineMode::ControlPoints)) return false;
            if (wy::ErrorStatus::Ok != pSpline->setDegree(degree)) return false;
            if (wy::ErrorStatus::Ok != pSpline->setPoints(controlPoints)) return false;
            if (wy::ErrorStatus::Ok != pSpline->setKnots(knots)) return false;
            if (wy::ErrorStatus::Ok != pSpline->setMultiplicities(multiplicities)) return false;
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        // 修改原始的样条曲线
        {
            double startParam = 0.0;
            double endParam = startKnot.getParam();
            unsigned int degree(0);
            std::vector<wy::Vector2> controlPoints;
            std::vector<double> knots;
            std::vector<unsigned int> multiplicities;
            if (SplineUtil::segment(pSpline, startParam, endParam,
                degree, controlPoints, knots, multiplicities))
            {
                if (wy::ErrorStatus::Ok != pSpline->setMode(wy3d::SplineMode::ControlPoints)) return false;
                if (wy::ErrorStatus::Ok != pSpline->setDegree(degree)) return false;
                if (wy::ErrorStatus::Ok != pSpline->setPoints(controlPoints)) return false;
                if (wy::ErrorStatus::Ok != pSpline->setKnots(knots)) return false;
                if (wy::ErrorStatus::Ok != pSpline->setMultiplicities(multiplicities)) return false;
            }
            else
            {
                return false;
            }
        }

        // 新增样条曲线
        {
            double startParam = endKnot.getParam();
            double endParam = 1.0;
            unsigned int degree(0);
            std::vector<wy::Vector2> controlPoints;
            std::vector<double> knots;
            std::vector<unsigned int> multiplicities;
            if (SplineUtil::segment(pSpline, startParam, endParam,
                degree, controlPoints, knots, multiplicities))
            {
                wy3d::SketchSpline* pNewSpline(nullptr);
                if (wy::ErrorStatus::Ok != wy3d::SketchSpline::create(
                    pTrans, degree, controlPoints, knots, multiplicities, pNewSpline) || !pNewSpline)
                {
                    return false;
                }
                wy::ErrorStatus error = pSketch->addEntity(pNewSpline);
                assert(wy::ErrorStatus::Ok == error);

                // 父子关系
                _id2Parent[pNewSpline->getId()] = pSpline->getId();
            }
            else
            {
                return false;
            }
        }

        return true;
    }
    return false;
}

TrimSegmentTransient::TrimSegmentTransient(GuiCommand* pGuiCmd, const wy3d::Plane& sketchPlane, const wy3d::SketchCurve* pSketchCurve,
    const SketchTrimSegment& trimSegment) : _trimSegment(trimSegment)
{
    assert(pGuiCmd);
    assert(pSketchCurve);
    assert(_trimSegment.isValid());
    _id = pSketchCurve->getId();

    this->init(sketchPlane, pSketchCurve);
}

TrimSegmentTransient::~TrimSegmentTransient()
{
}

void TrimSegmentTransient::init(const wy3d::Plane& sketchPlane, const wy3d::SketchCurve* pSketchCurve)
{
    assert(pSketchCurve);
    if (!_trimSegment.isValid()) return;

    std::shared_ptr<SketchEntityLinearization> pLinearization;
    if (const wy3d::SketchLine* pSketchLine = wy3d::SketchLine::cast(pSketchCurve))
    {
        wy::Vector2 lineStartPnt = pSketchLine->getStartPoint();
        wy::Vector2 lineVec = pSketchLine->getEndPoint() - lineStartPnt;
        wy::Vector2 startPnt = lineStartPnt + lineVec * _trimSegment.startKnot.getParam();
        wy::Vector2 endPnt = lineStartPnt + lineVec * _trimSegment.endKnot.getParam();
        pLinearization = std::make_shared<SketchEntityLinearization>(startPnt, endPnt);
    }
    else if (const wy3d::SketchCenterLine* pSketchLine = wy3d::SketchCenterLine::cast(pSketchCurve))
    {
        wy::Vector2 lineStartPnt = pSketchLine->getStartPoint();
        wy::Vector2 lineVec = pSketchLine->getEndPoint() - lineStartPnt;
        wy::Vector2 startPnt = lineStartPnt + lineVec * _trimSegment.startKnot.getParam();
        wy::Vector2 endPnt = lineStartPnt + lineVec * _trimSegment.endKnot.getParam();
        pLinearization = std::make_shared<SketchEntityLinearization>(startPnt, endPnt);

        _lineStipple = new osg::LineStipple(CENTER_LINE_STIPPLE_FACTOR, CENTER_LINE_STIPPLE_PATTERN);
    }
    else if (const wy3d::SketchCircle* pSketchCircle = wy3d::SketchCircle::cast(pSketchCurve))
    {
        if (_trimSegment.startKnot.getParam() == 0.0 && _trimSegment.endKnot.getParam() == 1.0)
        {
            pLinearization = std::make_shared<SketchEntityLinearization>(pSketchCircle->getCenter(), pSketchCircle->getRadius());
        }
        else
        {
            pLinearization = std::make_shared<SketchEntityLinearization>(pSketchCircle->getCenter(), pSketchCircle->getRadius(),
                _trimSegment.startKnot.getParam() * wy3d::TWO_PI, _trimSegment.endKnot.getParam() * wy3d::TWO_PI);
        }
    }
    else if (const wy3d::SketchArc* pSketchArc = wy3d::SketchArc::cast(pSketchCurve))
    {
        if (_trimSegment.startKnot.getParam() == 0.0 && _trimSegment.endKnot.getParam() == 1.0)
        {
            pLinearization = std::make_shared<SketchEntityLinearization>(pSketchArc->getCenter(), pSketchArc->getRadius(),
                pSketchArc->getStartAngle(), pSketchArc->getEndAngle());
        }
        else
        {
            double startAngle = wy3d::normalizeRadian(pSketchArc->getStartAngle());
            double totalAngle = pSketchArc->getTotalAngle();
            pLinearization = std::make_shared<SketchEntityLinearization>(pSketchArc->getCenter(), pSketchArc->getRadius(),
                startAngle + _trimSegment.startKnot.getParam() * totalAngle, startAngle + _trimSegment.endKnot.getParam() * totalAngle);
        }
    }
    else if (const wy3d::SketchEllipse* pEllipse = wy3d::SketchEllipse::cast(pSketchCurve))
    {
        if (_trimSegment.startKnot.getParam() == 0.0 && _trimSegment.endKnot.getParam() == 1.0)
        {
            pLinearization = std::make_shared<SketchEntityLinearization>(pEllipse->getCenter(), pEllipse->getMajorAxis(), pEllipse->getRadiusRatio());
        }
        else
        {
            pLinearization = std::make_shared<SketchEntityLinearization>(pEllipse->getCenter(), pEllipse->getMajorAxis(), pEllipse->getRadiusRatio(),
                _trimSegment.startKnot.getParam() * wy3d::TWO_PI, _trimSegment.endKnot.getParam() * wy3d::TWO_PI);
        }
    }
    else if (const wy3d::SketchEllipseArc* pEllipseArc = wy3d::SketchEllipseArc::cast(pSketchCurve))
    {
        double startAngle = wy3d::normalizeRadian(pEllipseArc->getStartAngle());
        double totalAngle = pEllipseArc->getTotalAngle();
        pLinearization = std::make_shared<SketchEntityLinearization>(pEllipseArc->getCenter(), pEllipseArc->getMajorAxis(), pEllipseArc->getRadiusRatio(),
            startAngle + _trimSegment.startKnot.getParam() * totalAngle, startAngle + _trimSegment.endKnot.getParam() * totalAngle);
    }
    else if (const wy3d::SketchSpline* pSketchSpline = wy3d::SketchSpline::cast(pSketchCurve))
    {        pLinearization = std::make_shared<SketchEntityLinearization>(pSketchSpline->getOccSpline(),
            _trimSegment.startKnot.getParam(), _trimSegment.endKnot.getParam());
    }
    else
    {
        assert(false);
        return;
    }

    if (!pLinearization) return;
    const std::vector<wy::Vector2>& vertices2 = pLinearization->getVertices();
    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
    vertices->reserve(vertices2.size());
    for (const wy::Vector2& uv : vertices2)
    {
        vertices->push_back(MathUtils::toVec3(sketchPlane.value(uv)));
    }
    this->initGeom(vertices, pLinearization->getIndices());
}

void TrimSegmentTransient::initGeom(osg::ref_ptr<osg::Vec3Array> vertices, const std::vector<unsigned int>& indices)
{
    if (!vertices) return;
    if (vertices->empty() || indices.empty()) return;

    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
    // added by wangyao 2025.05.24 {
    // 不可Pick
    geom->setNodeMask(~ElementNodeType::All);
    // }
    geom->setUseDisplayList(false);
    geom->setUseVertexBufferObjects(true);
    // 顶点数组
    geom->setVertexArray(vertices);
    // 法向数组
    osg::ref_ptr<osg::Vec3Array> normals = new osg::Vec3Array();
    normals->push_back(osg::Vec3(0.0f, 0.0f, 1.0f));
    geom->setNormalArray(normals, osg::Array::Binding::BIND_OVERALL);
    // 颜色数组
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    colors->push_back(osg::Vec4(1.0f, 0.392f, 0.039f, 1.0f));
    geom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);
    // GL_LINES
    geom->addPrimitiveSet(new osg::DrawElementsUShort(GL_LINES, indices.cbegin(), indices.cend()));
    // 加粗
    geom->getOrCreateStateSet()->setAttribute(new osg::LineWidth(3.0));
    // 线样式
    if (_lineStipple)
    {
        geom->getOrCreateStateSet()->setAttributeAndModes(_lineStipple, osg::StateAttribute::ON);
    }
    // 添加到根节点
    _root->addChild(geom.get());
}

} // namespace wy3dApp
