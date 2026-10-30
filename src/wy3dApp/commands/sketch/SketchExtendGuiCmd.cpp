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

#include "SketchExtendGuiCmd.h"
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
#include "SketchExtendGraph.h"
#include "scene/RenderConst.h"
#include <wy3dApp/ElementNodeType.h>
#include <wy3dApp/CommonSelFilters.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

SketchExtendGuiCmd::SketchExtendGuiCmd() : OsgGuiCommand()
{
}

SketchExtendGuiCmd::~SketchExtendGuiCmd()
{
}

void SketchExtendGuiCmd::onDatabaseChanged(
    const wydb::Database* pDb,
    const wydb::Transaction* pTransaction,
    const wydb::DatabaseChangeInfo& changeInfo)
{
    assert(pDb);

    // 延伸命令本身是不会新增与删除元素的
    // 如果存在新增与删除的元素,说明执行了Undo&Redo操作,需要重构数据.
    if (!changeInfo.addedIds.empty() || !changeInfo.erasedIds.empty())
    {
        this->initExtendGraph();
        _pTransient = nullptr;
        return;
    }

    // 修改的元素
    for (const wydb::ElementId& id : changeInfo.modifiedIds)
    {
        const wy3d::SketchCurve* pCurve = wy3d::SketchCurve::cast(pDb->getElement(id));
        if (!pCurve)
        {
            assert(false);
            continue;
        }
        SketchExtendNodeSPtr pNode = _pExtendGraph->getNode(id);
        assert(pNode);
        if (pNode) pNode->refresh(pDb);
    }
}

wyap::CmdExecution::StartResult SketchExtendGuiCmd::onStart()
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
    if (!this->initExtendGraph())
    {
        assert(false);
        return wyap::CmdExecution::StartResult::Failed;
    }

    // add database reactor
    wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return wyap::CmdExecution::StartResult::Failed;
    }
    pDb->addReactor(this);

    // 鼠标样式
    ApplicationImpl::instance().setCursor(CursorType::SelectElements);
    // 提示信息
    ApplicationImpl::instance().getStatusBar()->setTips(QCoreApplication::translate("Extend", "Select sketch entities to extend."));

    // 点选控制项
    _pointPickOption.allowedElemTypes = ElementNodeType::SketchEntity;
    _pointPickOption.selType = wy3d::SelectionType::Element;
    _pointPickOption.preFilter = CommonSelPreFilter::create<wy3d::SketchEntity>();

    return wyap::CmdExecution::StartResult::Succeeded;
}
void SketchExtendGuiCmd::onEnd()
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
void SketchExtendGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
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

bool SketchExtendGuiCmd::initExtendGraph()
{
    wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return false;
    }
    if (_sketchInfo.sketchId.isNull())
    {
        assert(false);
        return false;
    }
    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(_sketchInfo.sketchId));
    if (!pSketch)
    {
        assert(false);
        return false;
    }
    _pExtendGraph = std::make_unique<SketchExtendGraph>(pSketch, 1e-6);
    if (!_pExtendGraph->isValid())
    {
        assert(false);
        _pExtendGraph = nullptr;
        return false;
    }

    return true;
}

void SketchExtendGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (!this->getOsgView()) return;

    // 点选
    const wyap::Selection sel = this->pointPick(event.x, event.y, _pointPickOption);

    // 拾取延伸段
    pickExtendSegment(sel.getElementId(), sel.getPickPosition());

    // 设置鼠标样式
    if (!sel.getElementId().isNull() && !_pTransient)
        ApplicationImpl::instance().setCursor(CursorType::Forbid);
    else
        ApplicationImpl::instance().setCursor(CursorType::SelectElements);

    return;
}

void SketchExtendGuiCmd::pickExtendSegment(const wydb::ElementId& id, const wy::Vector3& pickPos3d)
{
    if (id.isNull())
    {
        _pTransient = nullptr;
    }
    else
    {
        wy::Vector2 pickPosUV = _sketchInfo.sketchPlane.uv(pickPos3d);
        assert(_pExtendGraph);
        SketchExtendNodeSPtr pNode = _pExtendGraph->getNode(id);
        // added by wangyao 2025.03.14 {
        // 有可能拾取到的是非草图曲线(比如后续有可能新增的尺寸,文本,标注等),此时是没有结点的.
        if (!pNode)
        {
            _pTransient = nullptr;
            return;
        }
        // }
        if (pNode->isClosed()) // 闭合曲线
        {
            _pTransient = nullptr;
            return;
        }

        // 拾取段
        SketchExtendSegment segment = pNode->pick(_pExtendGraph.get(), pickPosUV);
        if (!segment.isValid()) // 排除无效段(无法延伸)
        {
            _pTransient = nullptr;
            return;
        }

        // 延伸
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
            _pTransient = std::make_shared<ExtendSegmentTransient>(this, _sketchInfo.sketchPlane, pSketchCurve, segment);
        }
        else if (_pTransient->getId() != id
            || _pTransient->getSegment().startKnot.getParam() != segment.startKnot.getParam()
            || _pTransient->getSegment().endKnot.getParam() != segment.endKnot.getParam()) // 比较是否是相同的修剪段
        {
            _pTransient = std::make_shared<ExtendSegmentTransient>(this, _sketchInfo.sketchPlane, pSketchCurve, segment);
        }
    }
}

void SketchExtendGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    if (_pTransient)
    {
        wydb::ElementId id = _pTransient->getId();
        assert(_pTransient->getSegment().isValid());
        if (_pTransient->getSegment().isValid())
        {
            const SketchExtendKnot& startKnot = _pTransient->getSegment().startKnot;
            const SketchExtendKnot& endKnot = _pTransient->getSegment().endKnot;
            if (startKnot.getParam() == 0.0 && endKnot.getParam() == 1.0)
            {
                // 刚好是整条曲线则略过(比如草图直线段在起点可以延伸但在终点不能延伸,pick终点处就是此种情形)
            }
            else
            {
                bool extendRet = extend(id, startKnot, endKnot);
                assert(extendRet);
            }
        }
    }
    _pTransient = nullptr;
    ApplicationImpl::instance().setCursor(CursorType::SelectElements);

    return;
}

bool SketchExtendGuiCmd::extend(const wydb::ElementId curveId, const SketchExtendKnot& startKnot, const SketchExtendKnot& endKnot)
{
    double startParam = startKnot.getParam();
    if (std::isnan(startParam) || std::isinf(startParam) || startParam == DBL_MAX || startParam == -DBL_MAX)
    {
        assert(false);
        return false;
    }
    double endParam = endKnot.getParam();
    if (std::isnan(endParam) || std::isinf(endParam) || endParam == DBL_MAX || endParam == -DBL_MAX)
    {
        assert(false);
        return false;
    }

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
    wy3d::Sketch* pSketch = nullptr;
    wy3d::SketchCurve* pSketchCurve = nullptr;
    pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(pConstSketch->getId()));
    if (!pSketch) goto ABORT_TRANS;
    pSketchCurve = wy3d::SketchCurve::cast(pTrans->getElementForWrite(pConstCurve->getId()));
    if (!pSketchCurve) goto ABORT_TRANS;
    if (wy3d::SketchLine* pLine = wy3d::SketchLine::cast(pSketchCurve))
    {
        assert(endParam > startParam && (endParam - startParam) > 1.0);
        wy::Vector2 startPnt = pLine->getStartPoint();
        wy::Vector2 endPnt = pLine->getEndPoint();
        if (startParam < -wy3d::EPS)
        {
            if (wy::ErrorStatus::Ok != pLine->setStartPoint(startPnt + (endPnt - startPnt) * startParam)) goto ABORT_TRANS;
        }
        else if (endParam > 1.0 + wy3d::EPS)
        {
            if (wy::ErrorStatus::Ok != pLine->setEndPoint(startPnt + (endPnt - startPnt) * endParam)) goto ABORT_TRANS;
        }
        else
        {
            assert(false);
            goto ABORT_TRANS;
        }
    }
    else if (wy3d::SketchCenterLine* pLine = wy3d::SketchCenterLine::cast(pSketchCurve))
    {
        assert(endParam > startParam && (endParam - startParam) > 1.0);
        wy::Vector2 startPnt = pLine->getStartPoint();
        wy::Vector2 endPnt = pLine->getEndPoint();
        if (startParam < -wy3d::EPS)
        {
            if (wy::ErrorStatus::Ok != pLine->setStartPoint(startPnt + (endPnt - startPnt) * startParam)) goto ABORT_TRANS;
        }
        else if (endParam > 1.0 + wy3d::EPS)
        {
            if (wy::ErrorStatus::Ok != pLine->setEndPoint(startPnt + (endPnt - startPnt) * endParam)) goto ABORT_TRANS;
        }
        else
        {
            assert(false);
            goto ABORT_TRANS;
        }
    }
    else if (wy3d::SketchArc* pArc = wy3d::SketchArc::cast(pSketchCurve))
    {
        double startAngle = wy3d::normalizeRadian(pArc->getStartAngle());
        double totalAngle = pArc->getTotalAngle();
        if (endParam == 1.0)
        {
            assert(startParam > 1.0);
            double newStartAngle = wy3d::normalizeRadian(startAngle + startParam * totalAngle);
            double newEndAngle = wy3d::normalizeRadian(pArc->getEndAngle());
            if (newEndAngle < newStartAngle) newEndAngle += wy3d::TWO_PI;
            if (wy::ErrorStatus::Ok != pArc->setStartAngle(newStartAngle) ||
                wy::ErrorStatus::Ok != pArc->setEndAngle(newEndAngle))
            {
                goto ABORT_TRANS;
            }
        }
        else if (startParam == 0.0)
        {
            assert(endParam > 1.0);
            double newEndAngle = wy3d::normalizeRadian(startAngle + endParam * totalAngle);
            double newStartAngle = wy3d::normalizeRadian(pArc->getStartAngle());
            if (newEndAngle < newStartAngle) newEndAngle += wy3d::TWO_PI;
            if (wy::ErrorStatus::Ok != pArc->setStartAngle(newStartAngle) ||
                wy::ErrorStatus::Ok != pArc->setEndAngle(newEndAngle))
            {
                goto ABORT_TRANS;
            }
        }
        else
        {
            assert(false);
            goto ABORT_TRANS;
        }
    }
    else if (wy3d::SketchEllipseArc* pEllipseArc = wy3d::SketchEllipseArc::cast(pSketchCurve))
    {
        double startAngle = wy3d::normalizeRadian(pEllipseArc->getStartAngle());
        double totalAngle = pEllipseArc->getTotalAngle();
        if (endParam == 1.0)
        {
            assert(startParam > 1.0);
            double newStartAngle = wy3d::normalizeRadian(startAngle + startParam * totalAngle);
            double newEndAngle = wy3d::normalizeRadian(pEllipseArc->getEndAngle());
            if (newEndAngle < newStartAngle) newEndAngle += wy3d::TWO_PI;
            if (wy::ErrorStatus::Ok != pEllipseArc->setStartAngle(newStartAngle) ||
                wy::ErrorStatus::Ok != pEllipseArc->setEndAngle(newEndAngle))
            {
                goto ABORT_TRANS;
            }
        }
        else if (startParam == 0.0)
        {
            assert(endParam > 1.0);
            double newEndAngle = wy3d::normalizeRadian(startAngle + endParam * totalAngle);
            double newStartAngle = wy3d::normalizeRadian(pEllipseArc->getStartAngle());
            if (newEndAngle < newStartAngle) newEndAngle += wy3d::TWO_PI;
            if (wy::ErrorStatus::Ok != pEllipseArc->setStartAngle(newStartAngle) ||
                wy::ErrorStatus::Ok != pEllipseArc->setEndAngle(newEndAngle))
            {
                goto ABORT_TRANS;
            }
        }
        else
        {
            assert(false);
            goto ABORT_TRANS;
        }
    }
    else if (wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pSketchCurve))
    {        Handle(Geom2d_BSplineCurve) pBSpline = pSpline->getOccSpline();
        if (!pBSpline)
        {
            assert(false);
            goto ABORT_TRANS;
        }

        Handle(Geom2d_BSplineCurve) pNewBSpline(nullptr);
        if (startKnot.getParam() < 0.0)
        {
            pNewBSpline = SplineUtil::addLineSegmentToBSpline(pBSpline, startKnot.getPosition(), true);
        }
        else if (endKnot.getParam() > 1.0)
        {
            pNewBSpline = SplineUtil::addLineSegmentToBSpline(pBSpline, endKnot.getPosition(), false);
        }
    
        if (!pNewBSpline)
        {
            assert(false);
            goto ABORT_TRANS;
        }
        
        unsigned int degree(0);
        std::vector<wy::Vector2> controlPoints;
        std::vector<double> knots;
        std::vector<unsigned int> multiplicities;
        if (SplineUtil::getBSplineData(pNewBSpline, degree, controlPoints, knots, multiplicities))
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
    error = pDb->getTransactionManager()->endTransaction();
    assert(wy::ErrorStatus::Ok == error);
    return true;

ABORT_TRANS:
    assert(false);
    pDb->getTransactionManager()->abortTransaction();
    return false;
}

ExtendSegmentTransient::ExtendSegmentTransient(GuiCommand* pGuiCmd, const wy3d::Plane& sketchPlane, const wy3d::SketchCurve* pSketchCurve,
    const SketchExtendSegment& segment) : _segment(segment)
{
    assert(pGuiCmd);
    assert(pSketchCurve);
    assert(_segment.isValid());
    _id = pSketchCurve->getId();

    this->init(sketchPlane, pSketchCurve);
}

ExtendSegmentTransient::~ExtendSegmentTransient()
{
}

void ExtendSegmentTransient::init(const wy3d::Plane& sketchPlane, const wy3d::SketchCurve* pSketchCurve)
{
    assert(pSketchCurve);
    if (!_segment.isValid()) return;

    std::shared_ptr<SketchEntityLinearization> pLinearization;
    if (const wy3d::SketchLine* pSketchLine = wy3d::SketchLine::cast(pSketchCurve))
    {
        wy::Vector2 lineStartPnt = pSketchLine->getStartPoint();
        wy::Vector2 lineVec = pSketchLine->getEndPoint() - lineStartPnt;
        wy::Vector2 startPnt = lineStartPnt + lineVec * _segment.startKnot.getParam();
        wy::Vector2 endPnt = lineStartPnt + lineVec * _segment.endKnot.getParam();
        pLinearization = std::make_shared<SketchEntityLinearization>(startPnt, endPnt);
    }
    else if (const wy3d::SketchCenterLine* pSketchLine = wy3d::SketchCenterLine::cast(pSketchCurve))
    {
        wy::Vector2 lineStartPnt = pSketchLine->getStartPoint();
        wy::Vector2 lineVec = pSketchLine->getEndPoint() - lineStartPnt;
        wy::Vector2 startPnt = lineStartPnt + lineVec * _segment.startKnot.getParam();
        wy::Vector2 endPnt = lineStartPnt + lineVec * _segment.endKnot.getParam();
        pLinearization = std::make_shared<SketchEntityLinearization>(startPnt, endPnt);

        _lineStipple = new osg::LineStipple(CENTER_LINE_STIPPLE_FACTOR, CENTER_LINE_STIPPLE_PATTERN);
    }
    else if (const wy3d::SketchCircle* pSketchCircle = wy3d::SketchCircle::cast(pSketchCurve))
    {
        if (_segment.startKnot.getParam() == 0.0 && _segment.endKnot.getParam() == 1.0)
        {
            pLinearization = std::make_shared<SketchEntityLinearization>(pSketchCircle->getCenter(), pSketchCircle->getRadius());
        }
        else
        {
            pLinearization = std::make_shared<SketchEntityLinearization>(pSketchCircle->getCenter(), pSketchCircle->getRadius(),
                _segment.startKnot.getParam() * wy3d::TWO_PI, _segment.endKnot.getParam() * wy3d::TWO_PI);
        }
    }
    else if (const wy3d::SketchArc* pSketchArc = wy3d::SketchArc::cast(pSketchCurve))
    {
        if (_segment.startKnot.getParam() == 0.0 && _segment.endKnot.getParam() == 1.0)
        {
            pLinearization = std::make_shared<SketchEntityLinearization>(pSketchArc->getCenter(), pSketchArc->getRadius(),
                pSketchArc->getStartAngle(), pSketchArc->getEndAngle());
        }
        else
        {
            double startAngle = wy3d::normalizeRadian(pSketchArc->getStartAngle());
            double totalAngle = pSketchArc->getTotalAngle();
            pLinearization = std::make_shared<SketchEntityLinearization>(pSketchArc->getCenter(), pSketchArc->getRadius(),
                startAngle + _segment.startKnot.getParam() * totalAngle, startAngle + _segment.endKnot.getParam() * totalAngle);
        }
    }
    else if (const wy3d::SketchEllipse* pEllipse = wy3d::SketchEllipse::cast(pSketchCurve))
    {
        if (_segment.startKnot.getParam() == 0.0 && _segment.endKnot.getParam() == 1.0)
        {
            pLinearization = std::make_shared<SketchEntityLinearization>(pEllipse->getCenter(), pEllipse->getMajorAxis(), pEllipse->getRadiusRatio());
        }
        else
        {
            pLinearization = std::make_shared<SketchEntityLinearization>(pEllipse->getCenter(), pEllipse->getMajorAxis(), pEllipse->getRadiusRatio(),
                _segment.startKnot.getParam() * wy3d::TWO_PI, _segment.endKnot.getParam() * wy3d::TWO_PI);
        }
    }
    else if (const wy3d::SketchEllipseArc* pEllipseArc = wy3d::SketchEllipseArc::cast(pSketchCurve))
    {
        double startAngle = wy3d::normalizeRadian(pEllipseArc->getStartAngle());
        double totalAngle = pEllipseArc->getTotalAngle();
        pLinearization = std::make_shared<SketchEntityLinearization>(pEllipseArc->getCenter(), pEllipseArc->getMajorAxis(), pEllipseArc->getRadiusRatio(),
            startAngle + _segment.startKnot.getParam() * totalAngle, startAngle + _segment.endKnot.getParam() * totalAngle);
    }
    else if (const wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pSketchCurve))
    {        pLinearization = std::make_shared<SketchEntityLinearization>(pSpline->getOccSpline(),
            _segment.startKnot.getParam(), _segment.endKnot.getParam());
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

void ExtendSegmentTransient::initGeom(osg::ref_ptr<osg::Vec3Array> vertices, const std::vector<unsigned int>& indices)
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
