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

#include "SketchFilletGuiCmd.h"
#include "snap/SketchSnapSystem.h"
#include <cassert>
#include <QCoreApplication>
#include <wyVector2.h>
#include <wyVector3.h>
#include <utils/wy3dCurveIntersectionUtil.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapSelManager.h>
#include <wy3dImpl.h>
#include <wy3dSketch.h>
#include "application/Application.h"
#include "commands/UndoRedoCommands.h"
#include "commands/sketch/SketchTrimExtendUtil.h"
#include "utils/MathUtils.h"
#include <utils/wy3dSketchFilletAlgo.h>
#include "select/filters/CommonSelFilters.h"
#include "scene/nodes/ElementNodeType.h"
#include "utils/SplineUtil.h"
#include "commands/dialogs/DoubleValueInputDialog.h"

#include <wy3dSketchLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>
#include <wy3dSketchEllipse.h>
#include <wy3dSketchEllipseArc.h>
#include <wy3dSketchSpline.h>


double SketchFilletGuiCmd::_R = 10.0;  // 圆角半径

SketchFilletGuiCmd::SketchFilletGuiCmd() 
    : OsgGuiCommand(), _step(Step::First), _pickPos1st(), _pickPos2nd()
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

SketchFilletGuiCmd::~SketchFilletGuiCmd()
{
}

wyap::CmdExecution::StartResult SketchFilletGuiCmd::onStart()
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

    // 弹出对话框:圆角半径
    DoubleValueInputDialog dialog(10.0,
        QCoreApplication::translate("SketchFillet", "Fillet"),
        QCoreApplication::translate("SketchFillet", "Radius"));
    if (QDialog::Accepted != dialog.exec())
    {
        return wyap::CmdExecution::StartResult::Rejected;
    }
    _R = std::fabs(dialog.getValue());

    // 第一步
    _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::SketchEntity);
    _pointPickOption.selType = wy3d::SelectionType::Element;
    this->gotoStep(Step::First);

    return wyap::CmdExecution::StartResult::Succeeded;
}
void SketchFilletGuiCmd::onEnd()
{
    GuiCommand::onEnd();

}
void SketchFilletGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    GuiCommand::onAbort(cause);

}

void SketchFilletGuiCmd::reset()
{
    _pickPos1st.set(0.0, 0.0);
    _pCurveTransient1st = nullptr;

    _pickPos2nd.set(0.0, 0.0);
    _pCurveTransient2nd = nullptr;

    _pFilletTransient = nullptr;
    _pFilletData = nullptr;

    this->gotoStep(Step::First);
}

void SketchFilletGuiCmd::gotoStep(Step step)
{
    _step = step;

    switch (step)
    {
    case Step::First:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchFillet", "Select the first sketch curve."));
        Application::instance().setCursor(CursorType::SelectElements);

        // 前置选择过滤器
        _pointPickOption.pSelPreFilter = std::make_shared<CommonPreSelFilterForPointPick>(
            wy3d::SketchEntity::classInfo(), wydb::ElementId::kNull);
    }
    break;

    case Step::Second:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchFillet", "Select the second sketch curve."));
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

void SketchFilletGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (!this->getOsgView()) return;

    // 点选
    //wydb::ElementId excludeId = wydb::ElementId::kNull;
    //if (Step::Second == _step)
    //{
    //    assert(_pCurveTransient1st);
    //    if (_pCurveTransient1st) excludeId = _pCurveTransient1st->getId();
    //}
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
    else // 第二步
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

void SketchFilletGuiCmd::onLeftMouseDown(const MouseEvent& event)
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
    else
    {
        assert(_pCurveTransient1st);
        if (_pCurveTransient2nd)
        {
            fillet(_pFilletData.get());
            this->reset();
        }
        else
        {
            // 如果单击的是选中的第一条曲线则重置
            if (_pCurveTransient1st)
            {
                std::pair<wydb::ElementId, wy::Vector3> pickRet = this->pointPickElement(event.x, event.y, _pointPickOption);
                if (pickRet.first == _pCurveTransient1st->getId())
                {
                    this->reset();
                }
            }
        }
    }

    return;
}

void SketchFilletGuiCmd::preview(wydb::ElementId id, const wy::Vector3& pickPos)
{
    // 第一步
    if (Step::First == _step)
    {
        const wy3d::SketchCurve* pSketchCurve = this->getSketchCurve(id);
        if (!pSketchCurve)
        {
            _pCurveTransient1st = nullptr;
            return;
        }

        // 排除椭圆和椭圆弧以及中心线(目前不支持)
        if (pSketchCurve->isKindOf(wy3d::SketchEllipse::classInfo()) ||
            pSketchCurve->isKindOf(wy3d::SketchEllipseArc::classInfo()) ||
            pSketchCurve->isKindOf(wy3d::SketchCenterLine::classInfo()))
        {
            _pCurveTransient1st = nullptr;
            return;
        }

        _pickPos1st = _sketchInfo.sketchPlane.uv(pickPos);
        if (_pCurveTransient1st)
        {
            if (_pCurveTransient1st->getId() != id)
            {
                _pCurveTransient1st = std::make_shared<SketchCurveTransient>(pSketchCurve, 0.0, 1.0);
            }
            else
            {
                // 维持不变
            }
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
            _pFilletTransient = nullptr;
            _pFilletData = nullptr;
            return;
        }

        // 不满足求圆角的图元
        const wy3d::SketchCurve* pSketchCurve = this->getSketchCurve(id);
        if (!pSketchCurve)
        {
            _pCurveTransient2nd = nullptr;
            _pFilletTransient = nullptr;
            _pFilletData = nullptr;
            return;
        }

        // 排除椭圆和椭圆弧以及中心线(目前不支持)
        if (pSketchCurve->isKindOf(wy3d::SketchEllipse::classInfo()) ||
            pSketchCurve->isKindOf(wy3d::SketchEllipseArc::classInfo()) ||
            pSketchCurve->isKindOf(wy3d::SketchCenterLine::classInfo()))
        {
            _pCurveTransient2nd = nullptr;
            _pFilletTransient = nullptr;
            _pFilletData = nullptr;
            return;
        }

        // 拾取点坐标
        _pickPos2nd = _sketchInfo.sketchPlane.uv(pickPos);

        // 计算圆角
        std::shared_ptr<FilletData> pFilletData = filletPreview(_pCurveTransient1st->getId(), _pickPos1st, id, _pickPos2nd);
        if (pFilletData)
        {
            if (!_pFilletData || (_pFilletData && *pFilletData != *_pFilletData))
            {
                _pCurveTransient1st = std::make_shared<SketchCurveTransient>(pFilletData->id1st, pFilletData->startParam1st, pFilletData->endParam1st);
                _pCurveTransient2nd = std::make_shared<SketchCurveTransient>(pFilletData->id2nd, pFilletData->startParam2nd, pFilletData->endParam2nd);
                _pFilletTransient = std::make_shared<SketchCurveTransient>(_sketchInfo.sketchPlane, pFilletData->filletCenter, pFilletData->filletRadius,
                    pFilletData->filletStartAngle, pFilletData->filletEndAngle);
                _pFilletData = pFilletData;
            }
        }
        else
        {
            _pCurveTransient2nd = nullptr;
            _pFilletTransient = nullptr;
            _pFilletData = pFilletData;
        }
        
        return;
    }
}

std::shared_ptr<FilletData> SketchFilletGuiCmd::filletPreview(
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
            return this->filletPreviewLineLine(pLine1st, t1, pLine2nd, t2);
        }
        else if (const wy3d::SketchCircle* pCircle = wy3d::SketchCircle::cast(pCurve2nd))
        {
            double t2 = SketchTrimExtendUtil::getParamOfCircle(pCircle, pickPos2nd);
            t2 = std::clamp(t2, 0.0, 1.0);
            return this->filletPreviewLineCircle(pLine1st, t1, pCircle, t2);
        }
        else if (const wy3d::SketchArc* pArc = wy3d::SketchArc::cast(pCurve2nd))
        {
            double t2 = SketchTrimExtendUtil::getParamOfArc(pArc, pickPos2nd);
            t2 = std::clamp(t2, 0.0, 1.0);
            return this->filletPreviewLineArc(pLine1st, t1, pArc, t2);
        }
        else if (const wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pCurve2nd))
        {
            double t2 = SketchTrimExtendUtil::getPickParamOfSpline(*pSpline, pickPos2nd);
            if (t2 >= 0.0 && t2 <= 1.0)
            {
                return this->filletPreviewLineSpline(pLine1st, t1, pSpline, t2);
            }
            else
            {
                assert(false);
                return nullptr;
            }
        }
    }
    else if (const wy3d::SketchCircle* pCircle = wy3d::SketchCircle::cast(pCurve1st))
    {
        double t1 = SketchTrimExtendUtil::getParamOfCircle(pCircle, pickPos1st);
        t1 = std::clamp(t1, 0.0, 1.0);

        if (const wy3d::SketchLine* pLine = wy3d::SketchLine::cast(pCurve2nd))
        {
            double t2 = SketchTrimExtendUtil::getParamOfLine(pLine, pickPos2nd);
            t2 = std::clamp(t2, 0.0, 1.0);
            return this->filletPreviewCircleLine(pCircle, t1, pLine, t2);
        }
        else if (const wy3d::SketchCircle* pCircle2nd = wy3d::SketchCircle::cast(pCurve2nd))
        {
            double t2 = SketchTrimExtendUtil::getParamOfCircle(pCircle2nd, pickPos2nd);
            t2 = std::clamp(t2, 0.0, 1.0);
            return this->filletPreviewCircleCircle(pCircle, t1, pCircle2nd, t2);
        }
        else if (const wy3d::SketchArc* pArc = wy3d::SketchArc::cast(pCurve2nd))
        {
            double t2 = SketchTrimExtendUtil::getParamOfArc(pArc, pickPos2nd);
            t2 = std::clamp(t2, 0.0, 1.0);
            return this->filletPreviewCircleArc(pCircle, t1, pArc, t2);
        }
        else if (const wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pCurve2nd))
        {
            double t2 = SketchTrimExtendUtil::getPickParamOfSpline(*pSpline, pickPos2nd);
            if (t2 >= 0.0 && t2 <= 1.0)
            {
                return this->filletPreviewCircleSpline(pCircle, t1, pSpline, t2);
            }
            else
            {
                assert(false);
                return nullptr;
            }
        }
    }
    else if (const wy3d::SketchArc* pArc = wy3d::SketchArc::cast(pCurve1st))
    {
        double t1 = SketchTrimExtendUtil::getParamOfArc(pArc, pickPos1st);
        t1 = std::clamp(t1, 0.0, 1.0);

        if (const wy3d::SketchLine* pLine = wy3d::SketchLine::cast(pCurve2nd))
        {
            double t2 = SketchTrimExtendUtil::getParamOfLine(pLine, pickPos2nd);
            t2 = std::clamp(t2, 0.0, 1.0);
            return this->filletPreviewArcLine(pArc, t1, pLine, t2);
        }
        else if (const wy3d::SketchCircle* pCircle = wy3d::SketchCircle::cast(pCurve2nd))
        {
            double t2 = SketchTrimExtendUtil::getParamOfCircle(pCircle, pickPos2nd);
            t2 = std::clamp(t2, 0.0, 1.0);
            return this->filletPreviewArcCircle(pArc, t1, pCircle, t2);
        }
        else if (const wy3d::SketchArc* pArc2nd = wy3d::SketchArc::cast(pCurve2nd))
        {
            double t2 = SketchTrimExtendUtil::getParamOfArc(pArc2nd, pickPos2nd);
            t2 = std::clamp(t2, 0.0, 1.0);
            return this->filletPreviewArcArc(pArc, t1, pArc2nd, t2);
        }
        else if (const wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pCurve2nd))
        {
            double t2 = SketchTrimExtendUtil::getPickParamOfSpline(*pSpline, pickPos2nd);
            if (t2 >= 0.0 && t2 <= 1.0)
            {
                return this->filletPreviewArcSpline(pArc, t1, pSpline, t2);
            }
            else
            {
                assert(false);
                return nullptr;
            }
        }
    }
    else if (const wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pCurve1st))
    {
        double t1 = SketchTrimExtendUtil::getPickParamOfSpline(*pSpline, pickPos1st);
        if (t1 < 0.0 || t1 > 1.0)
        {
            assert(false);
            return nullptr;
        }

        if (const wy3d::SketchLine* pLine = wy3d::SketchLine::cast(pCurve2nd))
        {
            double t2 = SketchTrimExtendUtil::getParamOfLine(pLine, pickPos2nd);
            t2 = std::clamp(t2, 0.0, 1.0);
            return this->filletPreviewSplineLine(pSpline, t1, pLine, t2);
        }
        else if (const wy3d::SketchCircle* pCircle = wy3d::SketchCircle::cast(pCurve2nd))
        {
            double t2 = SketchTrimExtendUtil::getParamOfCircle(pCircle, pickPos2nd);
            t2 = std::clamp(t2, 0.0, 1.0);
            return this->filletPreviewSplineCircle(pSpline, t1, pCircle, t2);
        }
        else if (const wy3d::SketchArc* pArc = wy3d::SketchArc::cast(pCurve2nd))
        {
            double t2 = SketchTrimExtendUtil::getParamOfArc(pArc, pickPos2nd);
            t2 = std::clamp(t2, 0.0, 1.0);
            return this->filletPreviewSplineArc(pSpline, t1, pArc, t2);
        }
        else if (const wy3d::SketchSpline* pSpline2nd = wy3d::SketchSpline::cast(pCurve2nd))
        {
            double t2 = SketchTrimExtendUtil::getPickParamOfSpline(*pSpline2nd, pickPos2nd);
            if (t2 >= 0.0 && t2 <= 1.0)
            {
                return this->filletPreviewSplineSpline(pSpline, t1, pSpline2nd, t2);
            }
            else
            {
                assert(false);
                return nullptr;
            }
        }
    }

    return nullptr;
}

static std::shared_ptr<FilletData> makeFilletData(std::shared_ptr<wy3d::SketchFilletData> pSketchFilletData)
{
    std::shared_ptr<FilletData> pFilletData = std::make_shared<FilletData>();
    // curve 1
    pFilletData->startParam1st = pSketchFilletData->startParam1st;
    pFilletData->endParam1st = pSketchFilletData->endParam1st;
    // curve 2
    pFilletData->startParam2nd = pSketchFilletData->startParam2nd;
    pFilletData->endParam2nd = pSketchFilletData->endParam2nd;
    // fillet
    pFilletData->filletCenter = pSketchFilletData->filletCenter;
    pFilletData->filletRadius = pSketchFilletData->filletRadius;
    pFilletData->filletStartAngle = pSketchFilletData->filletStartAngle;
    pFilletData->filletEndAngle = pSketchFilletData->filletEndAngle;

    return pFilletData;
}

std::shared_ptr<FilletData> SketchFilletGuiCmd::filletPreviewLineLine(
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

    std::shared_ptr<wy3d::SketchFilletData> pSketchFilletData = wy3d::SketchFilletAlgo::filletLineLine(_R, wy3d::TOL,
        startPnt1st, endPnt1st, pickPosOnLine1st,
        startPnt2nd, endPnt2nd, pickPosOnLine2nd);
    if (!pSketchFilletData) return nullptr;

    std::shared_ptr<FilletData> pFilletData = makeFilletData(pSketchFilletData);
    pFilletData->id1st = pLine1st->getId();
    pFilletData->id2nd = pLine2nd->getId();

    return pFilletData;
}

std::shared_ptr<FilletData> SketchFilletGuiCmd::filletPreviewLineArc(
    const wy3d::SketchLine* pLine, double paramPickPosOnLine,
    const wy3d::SketchArc* pArc, double paramPickPosOnArc,
    bool isSecondPickPosMajor)
{
    assert(pLine);
    assert(pArc);

    wy::Vector2 lineStartPnt = pLine->getStartPoint();
    wy::Vector2 lineEndPnt = pLine->getEndPoint();
    wy::Vector2 pickPosOnLine = lineStartPnt + (lineEndPnt - lineStartPnt) * paramPickPosOnLine;

    wy::Vector2 arcCenter = pArc->getCenter();
    double arcRadius = pArc->getRadius();
    double arcStartAngle = pArc->getStartAngle();
    double angle = arcStartAngle + pArc->getTotalAngle() * paramPickPosOnArc;
    wy::Vector2 pickPosOnArc = arcCenter + wy::Vector2(arcRadius * std::cos(angle), arcRadius * std::sin(angle));

    std::shared_ptr<wy3d::SketchFilletData> pSketchFilletData = wy3d::SketchFilletAlgo::filletLineArc(_R, wy3d::TOL,
        lineStartPnt, lineEndPnt, pickPosOnLine,
        arcCenter, arcRadius, arcStartAngle, pArc->getEndAngle(), pickPosOnArc,
        isSecondPickPosMajor);
    if (!pSketchFilletData) return nullptr;

    std::shared_ptr<FilletData> pFilletData = makeFilletData(pSketchFilletData);
    pFilletData->id1st = pLine->getId();
    pFilletData->id2nd = pArc->getId();

    return pFilletData;
}

std::shared_ptr<FilletData> SketchFilletGuiCmd::filletPreviewArcLine(
    const wy3d::SketchArc* pArc, double refParamArc,
    const wy3d::SketchLine* pLine, double refParamLine)
{
    std::shared_ptr<FilletData> pFilletData = this->filletPreviewLineArc(pLine, refParamLine, pArc, refParamArc, false);
    if (pFilletData)
    {
        pFilletData->swap();
    }
    return pFilletData;
}

std::shared_ptr<FilletData> SketchFilletGuiCmd::filletPreviewLineCircle(
    const wy3d::SketchLine* pLine, double paramPickPosOnLine,
    const wy3d::SketchCircle* pCircle, double paramPickPosOnCircle,
    bool isSecondPickPosMajor)
{
    assert(pLine);
    assert(pCircle);

    wy::Vector2 lineStartPnt = pLine->getStartPoint();
    wy::Vector2 lineEndPnt = pLine->getEndPoint();
    wy::Vector2 pickPosOnLine = lineStartPnt + (lineEndPnt - lineStartPnt) * paramPickPosOnLine;

    wy::Vector2 center = pCircle->getCenter();
    double radius = pCircle->getRadius();
    double angle = wy3d::TWO_PI * paramPickPosOnCircle;
    wy::Vector2 pickPosOnCircle = center + wy::Vector2(radius * std::cos(angle), radius * std::sin(angle));

    std::shared_ptr<wy3d::SketchFilletData> pSketchFilletData = wy3d::SketchFilletAlgo::filletLineCircle(_R, wy3d::TOL,
        lineStartPnt, lineEndPnt, pickPosOnLine,
        center, radius, pickPosOnCircle,
        isSecondPickPosMajor);
    if (!pSketchFilletData) return nullptr;

    std::shared_ptr<FilletData> pFilletData = makeFilletData(pSketchFilletData);
    pFilletData->id1st = pLine->getId();
    pFilletData->id2nd = pCircle->getId();

    return pFilletData;
}

std::shared_ptr<FilletData> SketchFilletGuiCmd::filletPreviewCircleLine(
    const wy3d::SketchCircle* pCircle, double paramPickPosOnCircle,
    const wy3d::SketchLine* pLine, double paramPickPosOnLine)
{
    std::shared_ptr<FilletData> pFilletData = this->filletPreviewLineCircle(
        pLine, paramPickPosOnLine, pCircle, paramPickPosOnCircle, false);
    if (pFilletData)
    {
        pFilletData->swap();
    }
    return pFilletData;
}

std::shared_ptr<FilletData> SketchFilletGuiCmd::filletPreviewLineSpline(
    const wy3d::SketchLine* pLine, double paramPickPosOnLine,
    const wy3d::SketchSpline* pSpline, double paramPickPosOnSpline,
    bool isSecondPickPosMajor)
{
    assert(pLine);
    assert(pSpline);

    wy::Vector2 lineStartPnt = pLine->getStartPoint();
    wy::Vector2 lineEndPnt = pLine->getEndPoint();
    wy::Vector2 pickPosOnLine = lineStartPnt + (lineEndPnt - lineStartPnt) * paramPickPosOnLine;    Handle(Geom2d_BSplineCurve) pBSpline = pSpline->getOccSpline();
    if (!pBSpline)
    {
        assert(false);
        return nullptr;
    }
    gp_Pnt2d pos2d;
    double realParam = pBSpline->FirstParameter()
        + paramPickPosOnSpline * (pBSpline->LastParameter() - pBSpline->FirstParameter());
    pBSpline->D0(realParam, pos2d);
    wy::Vector2 pickPosOnSpline(pos2d.X(), pos2d.Y());

    std::shared_ptr<wy3d::SketchFilletData> pSketchFilletData = wy3d::SketchFilletAlgo::filletLineSpline(_R, wy3d::TOL,
        lineStartPnt, lineEndPnt, pickPosOnLine,
        pBSpline, pickPosOnSpline,
        isSecondPickPosMajor);
    if (!pSketchFilletData) return nullptr;

    std::shared_ptr<FilletData> pFilletData = makeFilletData(pSketchFilletData);
    pFilletData->id1st = pLine->getId();
    pFilletData->id2nd = pSpline->getId();

    return pFilletData;
}

std::shared_ptr<FilletData> SketchFilletGuiCmd::filletPreviewSplineLine(
    const wy3d::SketchSpline* pSpline, double refParamSpline,
    const wy3d::SketchLine* pLine, double refParamLine)
{
    std::shared_ptr<FilletData> pFilletData = this->filletPreviewLineSpline(
        pLine, refParamLine, pSpline, refParamSpline, false);
    if (pFilletData)
    {
        pFilletData->swap();
    }
    return pFilletData;
}

std::shared_ptr<FilletData> SketchFilletGuiCmd::filletPreviewCircleCircle(
    const wy3d::SketchCircle* pCircle1st, double paramPickPos1st,
    const wy3d::SketchCircle* pCircle2nd, double paramPickPos2nd)
{
    assert(pCircle1st);
    assert(pCircle2nd);

    wy::Vector2 center1 = pCircle1st->getCenter();
    double radius1 = pCircle1st->getRadius();
    double angle1 = wy3d::TWO_PI * paramPickPos1st;
    wy::Vector2 pickPos1st = center1 + wy::Vector2(radius1 * std::cos(angle1), radius1 * std::sin(angle1));

    wy::Vector2 center2 = pCircle2nd->getCenter();
    double radius2 = pCircle2nd->getRadius();
    double angle2 = wy3d::TWO_PI * paramPickPos2nd;
    wy::Vector2 pickPos2nd = center2 + wy::Vector2(radius2 * std::cos(angle2), radius2 * std::sin(angle2));

    std::shared_ptr<wy3d::SketchFilletData> pSketchFilletData = wy3d::SketchFilletAlgo::filletCircleCircle(_R, wy3d::TOL,
        center1, radius1, pickPos1st,
        center2, radius2, pickPos2nd);
    if (!pSketchFilletData) return nullptr;

    std::shared_ptr<FilletData> pFilletData = makeFilletData(pSketchFilletData);
    pFilletData->id1st = pCircle1st->getId();
    pFilletData->id2nd = pCircle2nd->getId();

    return pFilletData;
}

std::shared_ptr<FilletData> SketchFilletGuiCmd::filletPreviewCircleArc(
    const wy3d::SketchCircle* pCircle, double paramPickPosOnCircle,
    const wy3d::SketchArc* pArc, double paramPickPosOnArc,
    bool isSecondPickPosMajor)
{
    assert(pCircle);
    assert(pArc);

    wy::Vector2 center1 = pCircle->getCenter();
    double radius1 = pCircle->getRadius();
    double angle1 = wy3d::TWO_PI * paramPickPosOnCircle;
    wy::Vector2 pickPos1st = center1 + wy::Vector2(radius1 * std::cos(angle1), radius1 * std::sin(angle1));

    wy::Vector2 center2 = pArc->getCenter();
    double radius2 = pArc->getRadius();
    double startAngle = pArc->getStartAngle();
    double endAngle = pArc->getEndAngle();
    double angle2 = startAngle + pArc->getTotalAngle() * paramPickPosOnArc;
    wy::Vector2 pickPos2nd = center2 + wy::Vector2(radius2 * std::cos(angle2), radius2 * std::sin(angle2));

    std::shared_ptr<wy3d::SketchFilletData> pSketchFilletData = wy3d::SketchFilletAlgo::filletCircleArc(_R, wy3d::TOL,
        center1, radius1, pickPos1st,
        center2, radius2, startAngle, endAngle, pickPos2nd,
        isSecondPickPosMajor);
    if (!pSketchFilletData) return nullptr;

    std::shared_ptr<FilletData> pFilletData = makeFilletData(pSketchFilletData);
    pFilletData->id1st = pCircle->getId();
    pFilletData->id2nd = pArc->getId();

    return pFilletData;
}

std::shared_ptr<FilletData> SketchFilletGuiCmd::filletPreviewArcCircle(
    const wy3d::SketchArc* pArc, double paramPickPosOnArc,
    const wy3d::SketchCircle* pCircle, double paramPickPosOnCircle)
{
    std::shared_ptr<FilletData> pFilletData = this->filletPreviewCircleArc(
        pCircle, paramPickPosOnCircle, pArc, paramPickPosOnArc, false);
    if (pFilletData)
    {
        pFilletData->swap();
    }
    return pFilletData;
}

std::shared_ptr<FilletData> SketchFilletGuiCmd::filletPreviewCircleSpline(
    const wy3d::SketchCircle* pCircle, double paramPickPosOnCircle,
    const wy3d::SketchSpline* pSpline, double paramPickPosOnSpline,
    bool isSecondPickPosMajor)
{
    assert(pCircle);
    assert(pSpline);

    wy::Vector2 center = pCircle->getCenter();
    double radius = pCircle->getRadius();
    double angle = wy3d::TWO_PI * paramPickPosOnCircle;
    wy::Vector2 pickPosOnCircle = center + radius * wy::Vector2(std::cos(angle), std::sin(angle));    Handle(Geom2d_BSplineCurve) pBSpline = pSpline->getOccSpline();
    if (!pBSpline)
    {
        assert(false);
        return nullptr;
    }
    gp_Pnt2d pos2d;
    double realParam = pBSpline->FirstParameter()
        + paramPickPosOnSpline * (pBSpline->LastParameter() - pBSpline->FirstParameter());
    pBSpline->D0(realParam, pos2d);
    wy::Vector2 pickPosOnSpline(pos2d.X(), pos2d.Y());

    std::shared_ptr<wy3d::SketchFilletData> pSketchFilletData = wy3d::SketchFilletAlgo::filletCircleSpline(_R, wy3d::TOL,
        center, radius, pickPosOnCircle,
        pBSpline, pickPosOnSpline,
        isSecondPickPosMajor);
    if (!pSketchFilletData) return nullptr;

    std::shared_ptr<FilletData> pFilletData = makeFilletData(pSketchFilletData);
    pFilletData->id1st = pCircle->getId();
    pFilletData->id2nd = pSpline->getId();

    return pFilletData;
}

std::shared_ptr<FilletData> SketchFilletGuiCmd::filletPreviewSplineCircle(
    const wy3d::SketchSpline* pSpline, double paramPickPosOnSpline,
    const wy3d::SketchCircle* pCircle, double paramPickPosOnCircle)
{
    std::shared_ptr<FilletData> pFilletData = this->filletPreviewCircleSpline(
        pCircle, paramPickPosOnCircle, pSpline, paramPickPosOnSpline, false);
    if (pFilletData)
    {
        pFilletData->swap();
    }
    return pFilletData;
}

std::shared_ptr<FilletData> SketchFilletGuiCmd::filletPreviewArcArc(
    const wy3d::SketchArc* pArc1, double paramPickPos1,
    const wy3d::SketchArc* pArc2, double paramPickPos2)
{
    assert(pArc1);
    assert(pArc2);

    wy::Vector2 center1 = pArc1->getCenter();
    double radius1 = pArc1->getRadius();
    double startAngle1 = pArc1->getStartAngle();
    double endAngle1 = pArc1->getEndAngle();
    double angle1 = startAngle1 + pArc1->getTotalAngle() * paramPickPos1;
    wy::Vector2 pickPos1st = center1 + wy::Vector2(radius1 * std::cos(angle1), radius1 * std::sin(angle1));

    wy::Vector2 center2 = pArc2->getCenter();
    double radius2 = pArc2->getRadius();
    double startAngle2 = pArc2->getStartAngle();
    double endAngle2 = pArc2->getEndAngle();
    double angle2 = startAngle2 + pArc2->getTotalAngle() * paramPickPos2;
    wy::Vector2 pickPos2nd = center2 + wy::Vector2(radius2 * std::cos(angle2), radius2 * std::sin(angle2));

    std::shared_ptr<wy3d::SketchFilletData> pSketchFilletData = wy3d::SketchFilletAlgo::filletArcArc(_R, wy3d::TOL,
        center1, radius1, startAngle1, endAngle1, pickPos1st,
        center2, radius2, startAngle2, endAngle2, pickPos2nd);
    if (!pSketchFilletData) return nullptr;

    std::shared_ptr<FilletData> pFilletData = makeFilletData(pSketchFilletData);
    pFilletData->id1st = pArc1->getId();
    pFilletData->id2nd = pArc2->getId();

    return pFilletData;
}

std::shared_ptr<FilletData> SketchFilletGuiCmd::filletPreviewArcSpline(
    const wy3d::SketchArc* pArc, double paramPickPosOnArc,
    const wy3d::SketchSpline* pSpline, double paramPickPosOnSpline,
    bool isSecondPickPosMajor)
{
    assert(pArc);
    assert(pSpline);

    wy::Vector2 center = pArc->getCenter();
    double radius = pArc->getRadius();
    double startAngle = pArc->getStartAngle();
    double endAngle = pArc->getEndAngle();
    double angle = startAngle + pArc->getTotalAngle() * paramPickPosOnArc;
    wy::Vector2 pickPosOnArc = center + radius * wy::Vector2(std::cos(angle), std::sin(angle));    Handle(Geom2d_BSplineCurve) pBSpline = pSpline->getOccSpline();
    if (!pBSpline)
    {
        assert(false);
        return nullptr;
    }
    gp_Pnt2d pos2d;
    double realParam = pBSpline->FirstParameter()
        + paramPickPosOnSpline * (pBSpline->LastParameter() - pBSpline->FirstParameter());
    pBSpline->D0(realParam, pos2d);
    wy::Vector2 pickPosOnSpline(pos2d.X(), pos2d.Y());

    std::shared_ptr<wy3d::SketchFilletData> pSketchFilletData = wy3d::SketchFilletAlgo::filletArcSpline(_R, wy3d::TOL,
        center, radius, startAngle, endAngle, pickPosOnArc,
        pBSpline, pickPosOnSpline,
        isSecondPickPosMajor);
    if (!pSketchFilletData) return nullptr;

    std::shared_ptr<FilletData> pFilletData = makeFilletData(pSketchFilletData);
    pFilletData->id1st = pArc->getId();
    pFilletData->id2nd = pSpline->getId();

    return pFilletData;
}

std::shared_ptr<FilletData> SketchFilletGuiCmd::filletPreviewSplineArc(
    const wy3d::SketchSpline* pSpline, double paramPickPosOnSpline,
    const wy3d::SketchArc* pArc, double paramPickPosOnArc)
{
    std::shared_ptr<FilletData> pFilletData = this->filletPreviewArcSpline(
        pArc, paramPickPosOnArc, pSpline, paramPickPosOnSpline, false);
    if (pFilletData)
    {
        pFilletData->swap();
    }
    return pFilletData;
}

std::shared_ptr<FilletData> SketchFilletGuiCmd::filletPreviewSplineSpline(
    const wy3d::SketchSpline* pSpline1st, double paramPickPosOnSpline1st,
    const wy3d::SketchSpline* pSpline2nd, double paramPickPosOnSpline2nd)
{
    assert(pSpline1st);
    assert(pSpline2nd);    Handle(Geom2d_BSplineCurve) pBSpline1st = pSpline1st->getOccSpline();
    if (!pBSpline1st)
    {
        assert(false);
        return nullptr;
    }
    wy::Vector2 pickPosOnSpline1st;
    {
        gp_Pnt2d pos2d;
        double realParam = pBSpline1st->FirstParameter()
            + paramPickPosOnSpline1st * (pBSpline1st->LastParameter() - pBSpline1st->FirstParameter());
        pBSpline1st->D0(realParam, pos2d);
        pickPosOnSpline1st.set(pos2d.X(), pos2d.Y());
    }    Handle(Geom2d_BSplineCurve) pBSpline2nd = pSpline2nd->getOccSpline();
    if (!pBSpline2nd)
    {
        assert(false);
        return nullptr;
    }
    wy::Vector2 pickPosOnSpline2nd;
    {
        gp_Pnt2d pos2d;
        double realParam = pBSpline2nd->FirstParameter()
            + paramPickPosOnSpline2nd * (pBSpline2nd->LastParameter() - pBSpline2nd->FirstParameter());
        pBSpline2nd->D0(realParam, pos2d);
        pickPosOnSpline2nd.set(pos2d.X(), pos2d.Y());
    }

    std::shared_ptr<wy3d::SketchFilletData> pSketchFilletData = wy3d::SketchFilletAlgo::filletSplineSpline(_R, wy3d::TOL,
        pBSpline1st, pickPosOnSpline1st,
        pBSpline2nd, pickPosOnSpline2nd);
    if (!pSketchFilletData) return nullptr;

    std::shared_ptr<FilletData> pFilletData = makeFilletData(pSketchFilletData);
    pFilletData->id1st = pSpline1st->getId();
    pFilletData->id2nd = pSpline2nd->getId();

    return pFilletData;
}

const wy3d::SketchCurve* SketchFilletGuiCmd::getSketchCurve(const wydb::ElementId& id) const
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return nullptr;
    const wy3d::SketchCurve* pSketchCurve = wy3d::SketchCurve::cast(pDb->getElement(id));
    if (!pSketchCurve) return nullptr;

    return pSketchCurve;
}

bool SketchFilletGuiCmd::fillet(const FilletData* pFilletData)
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return false;

    wydb::Transaction* pTransGroup = pDb->getTransactionManager()->startTransactionGroup();
    if (!pTransGroup) return false;
    {
        if (!filletItem(pDb, pFilletData->id1st, pFilletData->startParam1st, pFilletData->endParam1st))
        {
            goto ABORT_TRANS;
        }
        if (!filletItem(pDb, pFilletData->id2nd, pFilletData->startParam2nd, pFilletData->endParam2nd))
        {
            goto ABORT_TRANS;
        }
        if (!filletArc(pDb, pFilletData->filletCenter, pFilletData->filletRadius, pFilletData->filletStartAngle, pFilletData->filletEndAngle))
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

bool SketchFilletGuiCmd::filletItem(wydb::Database* pDb, const wydb::ElementId& id, double startParam, double endParam)
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
    else if (wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pCurve))
    {        unsigned int degree(0);
        std::vector<wy::Vector2> controlPoints;
        std::vector<double> knots;
        std::vector<unsigned int> multiplicities;
        if (SplineUtil::segment(pSpline, startParam, endParam,
            degree, controlPoints, knots, multiplicities))
        {
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

bool SketchFilletGuiCmd::filletArc(wydb::Database* pDb, const wy::Vector2& center, double radius, double startAngle, double endAngle)
{
    assert(pDb);

    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans) return false;
    wy3d::SketchArc* pArc = nullptr;

    wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(_sketchInfo.sketchId));
    if (!pSketch) goto ABORT_TRANS;
    if (wy::ErrorStatus::Ok != wy3d::SketchArc::create(pTrans, center, radius, startAngle, endAngle, pArc)
        || !pArc)
    {
        goto ABORT_TRANS;
    }
    pSketch->addEntity(pArc);

    pDb->getTransactionManager()->endTransaction();
    return true;

ABORT_TRANS:
    pDb->getTransactionManager()->abortTransaction();
    return false;
}