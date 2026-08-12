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

#include "SketchDrawLineTangentGuiCmd.h"

#include <QCoreApplication>

#include <wyVector2.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapSelManager.h>
#include <wy3dImpl.h>
#include <wy3dSketch.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>
#include <wy3dSketchSpline.h>

#include "application/Application.h"
#include "snap/SketchSnapSystem.h"
#include "commands/sketch/SketchTrimExtendUtil.h"
#include "utils/MathUtils.h"
#include "scene/nodes/ElementNodeType.h"

class DrawLineTangentPreFilter : public SelectPreFilterFunctor
{
public:
    DrawLineTangentPreFilter(const wydb::ElementId& firstId,
        std::map<TangentLineIdPair, std::vector<TangentLine>>& cachedTangentLines)
        : _firstId(firstId), _cachedTangentLines(cachedTangentLines) {}

    virtual SelectFilterStatus operator()(
        const wydb::Database* pDb,
        const wydb::ElementId& id,
        SelectAction selectAction) const override
    {
        assert(pDb);

        if (id.isNull()) return SelectFilterStatus::Continue;

        const wydb::Element* pElem = pDb->getElement(id);
        if (!pElem) return SelectFilterStatus::Continue;
        const wyrx::ClassInfo* classInfo = pElem->getClassInfo();
        if (classInfo != wy3d::SketchCircle::classInfo() &&
            classInfo != wy3d::SketchArc::classInfo() &&
            classInfo != wy3d::SketchEllipse::classInfo() &&
            classInfo != wy3d::SketchEllipseArc::classInfo())
        {
            return SelectFilterStatus::Continue;
        }

        if (_firstId.isNull())
        {
            return SelectFilterStatus::Ok;
        }
        if (id == _firstId)
        {
            return SelectFilterStatus::Continue;
        }

        // 查看是否有缓存
        if (_cachedTangentLines.find(TangentLineIdPair(_firstId, id)) != _cachedTangentLines.cend())
        {
            return SelectFilterStatus::Ok;
        }

        // 没有缓存需要计算
        const wydb::Element* pFirstElem = pDb->getElement(_firstId);
        if (!pFirstElem)
        {
            assert(false);
            return SelectFilterStatus::Break;
        }

        const wyrx::ClassInfo* firstClassInfo = pFirstElem->getClassInfo();
        if (firstClassInfo == wy3d::SketchCircle::classInfo())
        {
            const wy3d::SketchCircle* pFirstCircle = wy3d::SketchCircle::cast(pFirstElem);
            assert(pFirstCircle);

            if (classInfo == wy3d::SketchCircle::classInfo())
            {
                return computeTangentLines(pFirstCircle, wy3d::SketchCircle::cast(pElem));
            }
            else if (classInfo == wy3d::SketchArc::classInfo())
            {
                return computeTangentLines(pFirstCircle, wy3d::SketchArc::cast(pElem));
            }
            else
            {
                return SelectFilterStatus::Continue;
            }
        }
        else if (firstClassInfo == wy3d::SketchArc::classInfo())
        {
            const wy3d::SketchArc* pFirstArc = wy3d::SketchArc::cast(pFirstElem);
            assert(pFirstArc);

            if (classInfo == wy3d::SketchCircle::classInfo())
            {
                return computeTangentLines(wy3d::SketchCircle::cast(pElem), pFirstArc);
            }
            else if (classInfo == wy3d::SketchArc::classInfo())
            {
                return computeTangentLines(wy3d::SketchArc::cast(pElem), pFirstArc);
            }
            else
            {
                return SelectFilterStatus::Continue;
            }
        }
        else if (firstClassInfo == wy3d::SketchEllipse::classInfo())
        {
            return SelectFilterStatus::Continue;
        }
        else if (firstClassInfo == wy3d::SketchEllipseArc::classInfo())
        {
            return SelectFilterStatus::Continue;
        }
        else
        {
            assert(false);
            return SelectFilterStatus::Break;
        }
    }

private:
    std::vector<TangentLine> convertToTangentLines(
        wydb::ElementId startElemId, wydb::ElementId endElemId,
        const std::vector<std::pair<wy::Vector2, wy::Vector2>>& tangentLines) const
    {
        std::vector<TangentLine> result;
        result.reserve(tangentLines.size());
        for (const auto& pair : tangentLines)
        {
            TangentLine item;
            item.startElementId = startElemId;
            item.startPoint = pair.first;
            item.endElementId = endElemId;
            item.endPoint = pair.second;
            result.emplace_back(item);
        }
        return result;
    }

    SelectFilterStatus computeTangentLines(
        const wy3d::SketchCircle* pCircle1st,
        const wy3d::SketchCircle* pCircle2nd) const
    {
        if (!pCircle1st || !pCircle2nd)
        {
            assert(false);
            return SelectFilterStatus::Break;
        }

        std::vector<std::pair<wy::Vector2, wy::Vector2>> tangentLines;
        if (MathUtils::tangentLinesOfCircleCircle(pCircle1st->getCenter(), pCircle1st->getRadius(),
            pCircle2nd->getCenter(), pCircle2nd->getRadius(), tangentLines, wy3d::TOL) && !tangentLines.empty())
        {
            _cachedTangentLines[TangentLineIdPair(pCircle1st->getId(), pCircle2nd->getId())] = convertToTangentLines(
                pCircle1st->getId(), pCircle2nd->getId(), tangentLines);
            return SelectFilterStatus::Ok;
        }
        else
        {
            return SelectFilterStatus::Break;
        }
    }

    SelectFilterStatus computeTangentLines(
        const wy3d::SketchCircle* pCircle,
        const wy3d::SketchArc* pArc) const
    {
        if (!pCircle || !pArc)
        {
            assert(false);
            return SelectFilterStatus::Break;
        }

        std::vector<std::pair<wy::Vector2, wy::Vector2>> tangentLines;
        if (MathUtils::tangentLinesOfCircleArc(pCircle->getCenter(), pCircle->getRadius(),
            pArc->getCenter(), pArc->getRadius(), pArc->getStartAngle(), pArc->getEndAngle(),
            tangentLines, wy3d::TOL) && !tangentLines.empty())
        {
            _cachedTangentLines[TangentLineIdPair(pCircle->getId(), pArc->getId())] = convertToTangentLines(
                pCircle->getId(), pArc->getId(), tangentLines);
            return SelectFilterStatus::Ok;
        }
        else
        {
            return SelectFilterStatus::Break;
        }
    }

    SelectFilterStatus computeTangentLines(
        const wy3d::SketchArc* pArc1st,
        const wy3d::SketchArc* pArc2nd) const
    {
        if (!pArc1st || !pArc2nd)
        {
            assert(false);
            return SelectFilterStatus::Break;
        }

        std::vector<std::pair<wy::Vector2, wy::Vector2>> tangentLines;
        if (MathUtils::tangentLinesOfArcArc(
            pArc1st->getCenter(), pArc1st->getRadius(), pArc1st->getStartAngle(), pArc1st->getEndAngle(),
            pArc2nd->getCenter(), pArc2nd->getRadius(), pArc2nd->getStartAngle(), pArc2nd->getEndAngle(),
            tangentLines, wy3d::TOL) && !tangentLines.empty())
        {
            _cachedTangentLines[TangentLineIdPair(pArc1st->getId(), pArc2nd->getId())] = convertToTangentLines(
                pArc1st->getId(), pArc2nd->getId(), tangentLines);
            return SelectFilterStatus::Ok;
        }
        else
        {
            return SelectFilterStatus::Break;
        }
    }

private:
    wydb::ElementId _firstId;
    std::map<TangentLineIdPair, std::vector<TangentLine>>& _cachedTangentLines;
};


SketchDrawLineTangentGuiCmd::SketchDrawLineTangentGuiCmd() : OsgGuiCommand(),
    _step(Step::Undefined), _startElementId(wydb::ElementId::kNull), _startPickPos(), _endElementId(wydb::ElementId::kNull), _endPickPos()
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

wyap::CmdExecution::StartResult SketchDrawLineTangentGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = GuiCommand::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);
 
    _sketchInfo = GuiCommandUtil::initSketchInfo();
    if (_sketchInfo.pSketchSnapSys) _sketchInfo.pSketchSnapSys->clearSnapResult();

    this->gotoStep(Step::SelectStartEntity);

    return wyap::CmdExecution::StartResult::Succeeded;
}
void SketchDrawLineTangentGuiCmd::onEnd()
{
    GuiCommand::onEnd();

    _pLineTransient = nullptr;
    _pPreview = nullptr;
    _pSelSetHighlightor = nullptr;

}
void SketchDrawLineTangentGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    GuiCommand::onAbort(cause);

    _pLineTransient = nullptr;
    _pPreview = nullptr;
    _pSelSetHighlightor = nullptr;

}

void SketchDrawLineTangentGuiCmd::reset()
{
    _step = Step::Undefined;
    _startElementId = wydb::ElementId::kNull;
    _startPickPos.set(0.0, 0.0);
    _endElementId = wydb::ElementId::kNull;
    _endPickPos.set(0.0, 0.0);
    _pLineTransient = nullptr;
    _pPreview = nullptr;
    _pSelSetHighlightor = nullptr;

    this->gotoStep(Step::SelectStartEntity);
}

void SketchDrawLineTangentGuiCmd::onEscapeKey()
{
    if (_step == Step::SelectStartEntity || _step == Step::Undefined)
    {
        this->requestAbort(AbortCause::UserCancel);
    }
    else
    {
        this->reset();
    }
}

bool SketchDrawLineTangentGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SelectStartEntity:
    {
        // 绘制线
        _pLineTransient = std::make_shared<LineTransient>();
        _pLineTransient->hide();
        _pLineTransient->update(_sketchInfo.sketchPlane, wy::Vector2(), wy::Vector2());

        // 下一步
        this->gotoStep(Step::SelectEndEntity);
        return true;
    }
    break;

    case Step::SelectEndEntity:
    {
        // 取消预览&高亮
        _pSelSetHighlightor = nullptr;
        _pPreview = nullptr;

        // 查找匹配的切线:确定匹配的切线的起点和终点
        wy::Vector2 startPnt, endPnt;
        if (!this->findMatchedTangentLine(_startElementId, _startPickPos, _endElementId, _endPickPos, startPnt, endPnt))
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }
        
        // 创建切线
        if (!this->createLine(startPnt, endPnt))
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }

        // 循环执行第一步
        this->reset();
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

void SketchDrawLineTangentGuiCmd::gotoStep(Step step)
{
    _step = step;

    switch (step)
    {
    case Step::SelectStartEntity:
    {
        // 禁止输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawLineTangent",
            "Select the start position on a circle or arc."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);

        // 点选选项
        _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::SketchEntity);
        _pointPickOption.selType = wy3d::SelectionType::Element;
        _pointPickOption.pSelPreFilter = std::make_shared<DrawLineTangentPreFilter>(wydb::ElementId::kNull, _cachedTangentLines);

        // 选择集高亮器
        _pSelSetHighlightor = std::make_shared<SelectionSetHighlightor>();
    }
    break;

    case Step::SelectEndEntity:
    {
        // 禁止输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchDrawLineTangent",
            "Select the end position on a circle or arc."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);

        // 点选选项
        _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::SketchEntity);
        _pointPickOption.selType = wy3d::SelectionType::Element;
        _pointPickOption.pSelPreFilter = std::make_shared<DrawLineTangentPreFilter>(_startElementId, _cachedTangentLines);
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

void SketchDrawLineTangentGuiCmd::onMouseMove(const MouseEvent& event)
{
    switch (_step)
    {
    case Step::SelectStartEntity:
    {
        this->mouseMovePointPickPreview(event.x, event.y, _pointPickOption, _pPreview);
        return;
    }
    break;

    case Step::SelectEndEntity:
    {
        this->mouseMovePointPickPreview(event.x, event.y, _pointPickOption, _pPreview);
        if (_pLineTransient) _pLineTransient->hide();
        if (_pPreview)
        {
            const wyap::Selection& sel = _pPreview->getSelection();
            wydb::ElementId endElementId = sel.getElementId();
            wy::Vector2 endPickPos = this->computePosition2dWithoutSnap(event.x, event.y, _sketchInfo.sketchPlane);

            wy::Vector2 startPnt, endPnt;
            if (this->findMatchedTangentLine(_startElementId, _startPickPos, endElementId, endPickPos, startPnt, endPnt))
            {
                if (_pLineTransient)
                {
                    _pLineTransient->show();
                    _pLineTransient->update(_sketchInfo.sketchPlane, startPnt, endPnt);
                }
            }
        }
        return;
    }
    break;

    default:
    {
        return;
    }
    break;
    }

    return;
}

void SketchDrawLineTangentGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    switch (_step)
    {
    case Step::SelectStartEntity:
    {
        if (_pPreview && _pSelSetHighlightor)
        {
            assert(_pSelSetHighlightor->getSelectionSet().isEmpty());
            const wyap::Selection& sel = _pPreview->getSelection();
            _pSelSetHighlightor->addSelection(sel);
            _startElementId = sel.getElementId();
            _startPickPos = this->computePosition2dWithoutSnap(event.x, event.y, _sketchInfo.sketchPlane);
            _pPreview = nullptr;
            this->finishStep(_step);
        }
        return;
    }
    break;

    case Step::SelectEndEntity:
    {
        if (_pPreview && _pSelSetHighlightor)
        {
            assert(!_pSelSetHighlightor->getSelectionSet().isEmpty());
            const wyap::Selection& sel = _pPreview->getSelection();
            _pSelSetHighlightor->addSelection(sel);
            _endElementId = sel.getElementId();
            _endPickPos = this->computePosition2dWithoutSnap(event.x, event.y, _sketchInfo.sketchPlane);
            _pPreview = nullptr;
            this->finishStep(_step);
        }
        return;
    }
    break;

    default:
    {
        return;
    }
    break;
    }

    return;
}

bool SketchDrawLineTangentGuiCmd::createLine(const wy::Vector2& startPnt, const wy::Vector2& endPnt)
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return false;
    }

    wy3d::SketchLine* pSketchLine(nullptr);
    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    wy3d::Sketch* pSketch = nullptr;
    wydb::Element* pSketchElem = pTrans->getElementForWrite(_sketchInfo.sketchId);
    if (!pSketchElem) goto ABORT_TRANS;
    pSketch = wy3d::Sketch::cast(pSketchElem);
    if (!pSketch) goto ABORT_TRANS;
    if (wy::ErrorStatus::Ok != wy3d::SketchLine::create(pTrans, startPnt, endPnt, pSketchLine) || !pSketchLine)
    {
        goto ABORT_TRANS;
    }
    if (wy::ErrorStatus::Ok != pSketch->addEntity(pSketchLine))
    {
        goto ABORT_TRANS;
    }
    pDb->getTransactionManager()->endTransaction();
    return true;

ABORT_TRANS:
    assert(false);
    pDb->getTransactionManager()->abortTransaction();
    pSketchLine = nullptr;
    return false;
}

bool SketchDrawLineTangentGuiCmd::findMatchedTangentLine(
    const wydb::ElementId& startElemId, const wy::Vector2& startPickPos,
    const wydb::ElementId& endElemId, const wy::Vector2& endPickPos,
    wy::Vector2& startPnt, wy::Vector2& endPnt)
{
    // 从缓存中查找候选的切线集合
    auto iter = _cachedTangentLines.find(TangentLineIdPair(startElemId, endElemId));
    if (iter == _cachedTangentLines.cend())
    {
        return false;
    }
    const std::vector<TangentLine>& candidates = iter->second;
    if (candidates.empty())
    {
        assert(false);
        return false;
    }

    // 从候选的切线集合中找出最匹配的切线
    double nearestDis(DBL_MAX);
    size_t nearestIdx(-1);
    for (size_t i = 0; i < candidates.size(); ++i)
    {
        const TangentLine& tangentLine = candidates[i];
        double dis(0.0);
        if (startElemId == tangentLine.startElementId)
        {
            dis = (startPickPos - tangentLine.startPoint).length()
                + (endPickPos - tangentLine.endPoint).length();
        }
        else
        {
            dis = (startPickPos - tangentLine.endPoint).length()
                + (endPickPos - tangentLine.startPoint).length();
        }
        if (dis < nearestDis)
        {
            nearestIdx = i;
            nearestDis = dis;
        }
    }
    if (nearestIdx == size_t(-1) || nearestIdx >= candidates.size())
    {
        assert(false);
        return false;
    }

    // 返回结果
    startPnt = candidates[nearestIdx].startPoint;
    endPnt = candidates[nearestIdx].endPoint;
    return true;
}