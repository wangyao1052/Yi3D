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

#include "SketchExtendNode.h"
#include <set>
#include <algorithm>
#include <cassert>
#include <wyVector2.h>
#include <wydbDatabase.h>
#include <wy3dSketchCurve.h>
#include <wy3dSketchCurve.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>
#include <wy3dSketchEllipse.h>
#include <wy3dSketchEllipseArc.h>

#include <wy3dApp/Application.h>
#include "SketchExtendGraph.h"
#include "SketchTrimExtendUtil.h"

#define TOL 1e-7

static const wy::Vector2 kAxisX(1.0, 0.0);

inline bool isInValidRange(double t)
{
    return t >= -TOL && t <= 1.0 + TOL;
}

SketchExtendNode::SketchExtendNode(const wydb::ElementId& id) : _id(id), _isClosed(false)
{
}

double SketchExtendNode::isOnCurve(const wydb::Database& db, const wy::Vector2& pos) const
{
    // 草图曲线有可能被删除掉了
    const wy3d::SketchCurve* pSketchCurve = wy3d::SketchCurve::cast(db.getElement(_id));
    if (!pSketchCurve || pSketchCurve->isErased())
    {
        return false;
    }

    // 获取参数值
    double param(DBL_MAX);
    if (const wy3d::SketchLine* pSketchLine = wy3d::SketchLine::cast(pSketchCurve))
    {
        param = SketchTrimExtendUtil::getParamOfLine(pSketchLine, pos);
    }
    else if (const wy3d::SketchCenterLine* pSketchLine = wy3d::SketchCenterLine::cast(pSketchCurve))
    {
        param = SketchTrimExtendUtil::getParamOfCenterLine(pSketchLine, pos);
    }
    else if (const wy3d::SketchCircle* pSketchCircle = wy3d::SketchCircle::cast(pSketchCurve))
    {
        param = SketchTrimExtendUtil::getParamOfCircle(pSketchCircle, pos);
    }
    else if (const wy3d::SketchArc* pSketchArc = wy3d::SketchArc::cast(pSketchCurve))
    {
        param = SketchTrimExtendUtil::getParamOfArc(pSketchArc, pos);
    }
    else if (const wy3d::SketchEllipse* pSketchEllipse = wy3d::SketchEllipse::cast(pSketchCurve))
    {
        param = SketchTrimExtendUtil::getParamOfEllipse(pSketchEllipse, pos);
    }
    else if (const wy3d::SketchEllipseArc* pSketchEllipseArc = wy3d::SketchEllipseArc::cast(pSketchCurve))
    {
        param = SketchTrimExtendUtil::getParamOfEllipseArc(pSketchEllipseArc, pos);
    }
    else if (const wy3d::SketchSpline* pSketchSpline = wy3d::SketchSpline::cast(pSketchCurve))
    {
        param = SketchTrimExtendUtil::getParamOfSpline(pSketchSpline, pos);
    }
    else
    {
        assert(false);
        param = DBL_MAX;
    }

    return param >= 0.0 && param <= 1.0;
}

void SketchExtendNode::refreshLine(const wy::Vector2& startPnt, const wy::Vector2& endPnt)
{
    double lineLength = (endPnt - startPnt).length();
    if (lineLength <= TOL)
    {
        for (SketchExtendKnot& knot : _knots)
        {
            knot.setParam(DBL_MAX);
        }
    }
    else
    {
        wy::Vector2 lineVec = endPnt - startPnt;
        wy::Vector2 vec;
        double t(0.0);
        for (SketchExtendKnot& knot : _knots)
        {
            vec = knot.getPosition() - startPnt;
            t = vec.length() / lineLength;
            if (vec.dot(lineVec) < 0) t = -t;
            t = SketchTrimExtendUtil::reviseT(t);
            knot.setParam(t);
        }
    }
}

void SketchExtendNode::refreshSpline(const wy3d::SketchSpline& sketchSpline)
{    Handle(Geom2d_BSplineCurve) pBSpline = sketchSpline.getOccSpline();
    if (pBSpline.IsNull())
    {
        assert(false);
        for (SketchExtendKnot& knot : _knots)
        {
            knot.setParam(DBL_MAX);
        }
        return;
    }

    double firstParam = pBSpline->FirstParameter();
    double lastParam = pBSpline->LastParameter();
    double paramRange = lastParam - firstParam;
    if (paramRange <= TOL)
    {
        assert(false);
        for (SketchExtendKnot& knot : _knots)
        {
            knot.setParam(DBL_MAX);
        }
        return;
    }

    wy::Vector2 startPnt, startDir;
    wy::Vector2 endPnt, endDir;
    if (!SketchTrimExtendUtil::getBSplineInfo(pBSpline, startPnt, startDir, endPnt, endDir))
    {
        assert(false);
        for (SketchExtendKnot& knot : _knots)
        {
            knot.setParam(DBL_MAX);
        }
        return;
    }
    
    for (SketchExtendKnot& knot : _knots)
    {
        Geom2dAPI_ProjectPointOnCurve projector(
            gp_Pnt2d(knot.getPosition().x(), knot.getPosition().y()),
            pBSpline);
        if (projector.NbPoints() > 0 && projector.LowerDistance() <= TOL)
        {
            // 对于样条曲线而言还需要判断距离是否在容差内
            // 比如:样条曲线和直线段在样条曲线的中间处相交,在修剪掉样条曲线的[0.5,1.0]段之后;
            // 原来样条曲线的终点投影到现在的样条曲线上可能为0.97在有效范围内,但这是不合理的,会导致后续的assert.
            double t = (projector.LowerDistanceParameter() - firstParam) / paramRange;
            knot.setParam(SketchTrimExtendUtil::reviseT(t));
        }
        else // 说明是在样条曲线起点和终点处的延长线上
        {
            wy::Vector2 startVec = knot.getPosition() - startPnt;
            wy::Vector2 endVec = knot.getPosition() - endPnt;
            if (std::fabs(startVec.cross(startDir)) <= TOL && startVec.dot(startDir) < 0.0)
            {
                knot.setParam(startVec.dot(startDir));
            }
            else if (std::fabs(endVec.cross(endDir)) <= TOL && endVec.dot(endDir) > 0.0)
            {
                knot.setParam(1.0 + endVec.dot(endDir));
            }
            else
            {
                knot.setParam(DBL_MAX);
            }
        }
    }
}

void SketchExtendNode::refresh(const wydb::Database* pDb)
{
    assert(pDb);

    static const wy::Vector2 kAxisX(1.0, 0.0);
    const wy3d::SketchCurve* pSketchCurve = wy3d::SketchCurve::cast(pDb->getElement(_id));
    if (const wy3d::SketchLine* pSketchLine = wy3d::SketchLine::cast(pSketchCurve))
    {
        this->refreshLine(pSketchLine->getStartPoint(), pSketchLine->getEndPoint());
    }
    else if (const wy3d::SketchCenterLine* pSketchCenterLine = wy3d::SketchCenterLine::cast(pSketchCurve))
    {
        this->refreshLine(pSketchCenterLine->getStartPoint(), pSketchCenterLine->getEndPoint());
    }
    else if (const wy3d::SketchCircle* pSketchCircle = wy3d::SketchCircle::cast(pSketchCurve))
    {
        wy::Vector2 center = pSketchCircle->getCenter();
        for (SketchExtendKnot& knot : _knots)
        {
            double t = wy::Vector2::rotationAngle(kAxisX, knot.getPosition() - center) / wy3d::TWO_PI;
            assert(t >= 0.0 && t < 1.0);
            knot.setParam(t);
        }
    }
    else if (const wy3d::SketchArc* pSketchArc = wy3d::SketchArc::cast(pSketchCurve))
    {
        double totalAngle = pSketchArc->getTotalAngle();
        if (totalAngle <= TOL)
        {
            for (SketchExtendKnot& knot : _knots)
            {
                knot.setParam(DBL_MAX);
            }
        }
        else
        {
            wy::Vector2 center = pSketchArc->getCenter();
            double startAngle = wy3d::normalizeRadian(pSketchArc->getStartAngle());
            double endAngle = startAngle + totalAngle;
            for (SketchExtendKnot& knot : _knots)
            {
                double angle = wy::Vector2::rotationAngle(kAxisX, knot.getPosition() - center);
                angle = SketchTrimExtendUtil::reviseAngle(angle, startAngle, endAngle);
                double t = (angle - startAngle) / totalAngle;
                knot.setParam(t);
            }
        }
    }
    else if (const wy3d::SketchEllipse* pSketchEllipse = wy3d::SketchEllipse::cast(pSketchCurve))
    {
        wy::Vector2 center = pSketchEllipse->getCenter();
        wy::Vector2 majorAxis = pSketchEllipse->getMajorAxis();
        for (SketchExtendKnot& knot : _knots)
        {
            double t = wy::Vector2::rotationAngle(majorAxis, knot.getPosition() - center) / wy3d::TWO_PI;
            assert(t >= 0.0 && t < 1.0);
            knot.setParam(t);
        }
    }
    else if (const wy3d::SketchEllipseArc* pSketchEllipseArc = wy3d::SketchEllipseArc::cast(pSketchCurve))
    {
        double totalAngle = pSketchEllipseArc->getTotalAngle();
        if (totalAngle <= TOL)
        {
            for (SketchExtendKnot& knot : _knots)
            {
                knot.setParam(DBL_MAX);
            }
        }
        else
        {
            wy::Vector2 center = pSketchEllipseArc->getCenter();
            wy::Vector2 majorAxis = pSketchEllipseArc->getMajorAxis();
            double startAngle = wy3d::normalizeRadian(pSketchEllipseArc->getStartAngle());
            double endAngle = startAngle + totalAngle;
            for (SketchExtendKnot& knot : _knots)
            {
                double angle = wy::Vector2::rotationAngle(majorAxis, knot.getPosition() - center);
                angle = SketchTrimExtendUtil::reviseAngle(angle, startAngle, endAngle);
                double t = (angle - startAngle) / totalAngle;
                knot.setParam(t);
            }
        }
    }
    else if (const wy3d::SketchSpline* pSketchSpline = wy3d::SketchSpline::cast(pSketchCurve))
    {
        this->refreshSpline(*pSketchSpline);
    }
    else
    {
        assert(false);
        for (SketchExtendKnot& knot : _knots)
        {
            knot.setParam(DBL_MAX);
        }
    }

    // 排序
    std::sort(_knots.begin(), _knots.end(), [](const SketchExtendKnot& lhs, const SketchExtendKnot& rhs) {
        return lhs.getParam() < rhs.getParam(); });
}

SketchExtendSegment SketchExtendNode::pick(SketchExtendGraph* pGraph, const wy::Vector2& position, double tol)
{
    assert(pGraph);
    SketchExtendSegment segment; // 默认是无效的曲线段
    
    // 闭合曲线结点直接返回无效的曲线段
    if (this->isClosed())
    {
        return segment;
    }

    // 获取曲线
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return segment; // 无效曲线段
    }
    const wy3d::SketchCurve* pSketchCurve = wy3d::SketchCurve::cast(pDb->getElement(_id));
    if (!pSketchCurve)
    {
        assert(false);
        return segment; // 无效曲线段
    }
    if (pSketchCurve->isErased())
    {
        assert(false);
        return segment; // 无效曲线段
    }
    if (pSketchCurve->isClosed())
    {
        assert(false);
        return segment; // 无效曲线段
    }

    // 判断曲线可不可以延伸
    bool extendable(false);
    for (const SketchExtendKnot& knot : _knots)
    {
        if (knot.getParam() >= -wy3d::EPS && knot.getParam() <= 1.0 + wy3d::EPS)
        {
            continue;
        }

        if (this->isOnOtherCurve(*pDb, pGraph, knot))
        {
            extendable = true;
            break;
        }
    }
    if (!extendable) // 曲线不可以延伸
    {
        return segment; // 无效曲线段
    }

    // 当前鼠标Pick点位置在曲线上的参数值
    double pickParam(0.0);
    if (const wy3d::SketchLine* pSketchLine = wy3d::SketchLine::cast(pSketchCurve))
    {
        pickParam = SketchTrimExtendUtil::getParamOfLine(pSketchLine, position);
    }
    else if (const wy3d::SketchCenterLine* pSketchLine = wy3d::SketchCenterLine::cast(pSketchCurve))
    {
        pickParam = SketchTrimExtendUtil::getParamOfCenterLine(pSketchLine, position);
    }
    else if (const wy3d::SketchArc* pSketchArc = wy3d::SketchArc::cast(pSketchCurve))
    {
        pickParam = SketchTrimExtendUtil::getParamOfArc(pSketchArc, position);
    }
    else if (const wy3d::SketchEllipseArc* pSketchEllipseArc = wy3d::SketchEllipseArc::cast(pSketchCurve))
    {
        pickParam = SketchTrimExtendUtil::getParamOfEllipseArc(pSketchEllipseArc, position);
    }
    else if (const wy3d::SketchSpline* pSketchSpline = wy3d::SketchSpline::cast(pSketchCurve))
    {
        pickParam = SketchTrimExtendUtil::getPickParamOfSpline(*pSketchSpline, position);
    }
    else
    {
        assert(false);
        return segment; // 返回无效的曲线段
    }

    // 是否是正向延伸
    bool forward = (pickParam >= 0.5);

    // 整条曲线
    segment.startKnot.setParam(0.0);
    segment.startKnot.setPosition(pSketchCurve->getStartPoint());
    segment.endKnot.setParam(1.0);
    segment.endKnot.setPosition(pSketchCurve->getEndPoint());

    // 直线段
    const wy3d::SketchLine* pSketchLine = wy3d::SketchLine::cast(pSketchCurve);
    const wy3d::SketchCenterLine* pSketchCenterLine = wy3d::SketchCenterLine::cast(pSketchCurve);
    const wy3d::SketchSpline* pSketchSpline = wy3d::SketchSpline::cast(pSketchCurve);
    if (pSketchLine || pSketchCenterLine || pSketchSpline)
    {
        // 正向延伸
        if (forward)
        {
            for (const SketchExtendKnot& knot : _knots)
            {
                if (knot.getParam() > 1.0 + wy3d::EPS && this->isOnOtherCurve(*pDb, pGraph, knot))
                {
                    segment.endKnot = knot;
                    break;
                }
            }
        }
        // 负向延伸
        else
        {
            for (auto riter = _knots.crbegin(); riter != _knots.crend(); ++riter)
            {
                if (riter->getParam() < -wy3d::EPS && this->isOnOtherCurve(*pDb, pGraph, *riter))
                {
                    segment.startKnot = *riter;
                    break;
                }
            }
        }
    }
    else // 周期性的曲线
    {
        double startAngle(0.0), endAngle(0.0), totalAngle(0.0);
        if (const wy3d::SketchArc* pArc = wy3d::SketchArc::cast(pSketchCurve))
        {
            startAngle = pArc->getStartAngle();
            endAngle = pArc->getEndAngle();
            totalAngle = pArc->getTotalAngle();
        }
        else if (const wy3d::SketchEllipseArc* pEllipseArc = wy3d::SketchEllipseArc::cast(pSketchCurve))
        {
            startAngle = pEllipseArc->getStartAngle();
            endAngle = pEllipseArc->getEndAngle();
            totalAngle = pEllipseArc->getTotalAngle();
        }
        else
        {
            assert(false);
            return segment;
        }
        startAngle = wy3d::normalizeRadian(startAngle);
        endAngle = wy3d::normalizeRadian(endAngle);

        // 正向延伸
        if (forward)
        {
            double minDisAngle = DBL_MAX;
            size_t minDisIndex = -1;
            double curAngle(0.0), param(0.0);
            for (size_t i = 0; i < _knots.size(); ++i)
            {
                param = _knots[i].getParam();
                if (param >= -wy3d::EPS && param <= 1.0 + wy3d::EPS) continue;
                curAngle = wy3d::normalizeRadian(startAngle + param * totalAngle);
                if (curAngle < endAngle) curAngle += wy3d::TWO_PI;
                if ((curAngle - endAngle) < minDisAngle && this->isOnOtherCurve(*pDb, pGraph, _knots[i]))
                {
                    minDisAngle = curAngle - endAngle;
                    minDisIndex = i;
                }
            }

            if (-1 != minDisIndex)
            {
                segment.endKnot = _knots[minDisIndex];
            }
            else
            {
                assert(false);
            }
        }
        // 负向延伸
        else
        {
            double minDisAngle = DBL_MAX;
            size_t minDisIndex = -1;
            double curAngle(0.0), param(0.0);
            for (size_t i = 0; i < _knots.size(); ++i)
            {
                param = _knots[i].getParam();
                if (param >= -wy3d::EPS && param <= 1.0 + wy3d::EPS) continue;
                curAngle = wy3d::normalizeRadian(startAngle + param * totalAngle);
                if (curAngle > startAngle) curAngle -= wy3d::TWO_PI;
                if ((startAngle - curAngle) < minDisAngle && this->isOnOtherCurve(*pDb, pGraph, _knots[i]))
                {
                    minDisAngle = startAngle - curAngle;
                    minDisIndex = i;
                }
            }

            if (-1 != minDisIndex)
            {
                segment.startKnot = _knots[minDisIndex];
            }
            else
            {
                assert(false);
            }
        }
    }

    return segment;
}

bool SketchExtendNode::isOnOtherCurve(const wydb::Database& db, SketchExtendGraph* pGraph, const SketchExtendKnot& knot)
{
    assert(pGraph);
    SketchExtendNodeSPtr pOtherNode = pGraph->getNode(knot.getOtherOwner());
    if (!pOtherNode)
    {
        assert(false);
        return false;
    }

    if (pOtherNode->isClosed())
    {
        return true;
    }
    else
    {
        return pOtherNode->isOnCurve(db, knot.getPosition());
    }
}
