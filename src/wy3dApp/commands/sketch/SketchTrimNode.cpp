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

#include "SketchTrimNode.h"
#include <set>
#include <algorithm>
#include <cassert>
#include <Geom2dAPI_ProjectPointOnCurve.hxx>
#include <wyVector2.h>
#include <wydbDatabase.h>
#include <wy3dSketchCurve.h>
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
#include "SketchTrimGraph.h"
#include "SketchTrimExtendUtil.h"
#include <wy3dApp/wy3dAppDefs.h>

#define TOL 1e-7

namespace wy3dApp {

static const wy::Vector2 kAxisX(1.0, 0.0);

inline bool isInValidRange(double t)
{
    return t >= -TOL && t <= 1.0 + TOL;
}

SketchTrimNode::SketchTrimNode(const wydb::ElementId& id) : _id(id)
{
}

std::shared_ptr<SketchTrimNode>  SketchTrimNode::clone(const wydb::ElementId& id)
{
    SketchTrimNodeSPtr pCopy = std::make_shared<SketchTrimNode>(id);
    pCopy->_knots = _knots;
    return pCopy;
}

double SketchTrimNode::getParam(wydb::Database* pDb, const wy::Vector2& pos) const
{
    assert(pDb);

    // 草图曲线有可能被删除掉了
    const wy3d::SketchCurve* pSketchCurve = wy3d::SketchCurve::cast(pDb->getElement(_id));
    if (!pSketchCurve || pSketchCurve->isErased())
    {
        return DBL_MAX;
    }

    // 获取参数值
    if (const wy3d::SketchLine* pSketchLine = wy3d::SketchLine::cast(pSketchCurve))
    {
        return SketchTrimExtendUtil::getParamOfLine(pSketchLine, pos);
    }
    else if (const wy3d::SketchCenterLine* pSketchCenterLine = wy3d::SketchCenterLine::cast(pSketchCurve))
    {
        return SketchTrimExtendUtil::getParamOfCenterLine(pSketchCenterLine, pos);
    }
    else if (const wy3d::SketchCircle* pSketchCircle = wy3d::SketchCircle::cast(pSketchCurve))
    {
        return SketchTrimExtendUtil::getParamOfCircle(pSketchCircle, pos);
    }
    else if (const wy3d::SketchArc* pSketchArc = wy3d::SketchArc::cast(pSketchCurve))
    {
        return SketchTrimExtendUtil::getParamOfArc(pSketchArc, pos);
    }
    else if (const wy3d::SketchEllipse* pSketchEllipse = wy3d::SketchEllipse::cast(pSketchCurve))
    {
        return SketchTrimExtendUtil::getParamOfEllipse(pSketchEllipse, pos);
    }
    else if (const wy3d::SketchEllipseArc* pSketchEllipseArc = wy3d::SketchEllipseArc::cast(pSketchCurve))
    {
        return SketchTrimExtendUtil::getParamOfEllipseArc(pSketchEllipseArc, pos);
    }
    else if (const wy3d::SketchSpline* pSketchSpline = wy3d::SketchSpline::cast(pSketchCurve))
    {
        return SketchTrimExtendUtil::getParamOfSpline(pSketchSpline, pos);
    }
    else
    {
        assert(false);
        return DBL_MAX;
    }
}

void SketchTrimNode::refreshLine(const wy::Vector2& startPnt, const wy::Vector2& endPnt)
{
    double lineLength = (endPnt - startPnt).length();
    if (lineLength <= TOL)
    {
        for (SketchTrimKnot& knot : _knots)
        {
            knot.setParam(DBL_MAX);
        }
    }
    else
    {
        wy::Vector2 lineVec = endPnt - startPnt;
        wy::Vector2 vec;
        double t(0.0);
        for (SketchTrimKnot& knot : _knots)
        {
            vec = knot.getPosition() - startPnt;
            t = vec.length() / lineLength;
            if (vec.dot(lineVec) < 0) t = -t;
            t = SketchTrimExtendUtil::reviseT(t);
            knot.setParam(t);
        }
    }
}

void SketchTrimNode::refreshSpline(const wy3d::SketchSpline& sketchSpline)
{    Handle(Geom2d_BSplineCurve) pBSpline = sketchSpline.getOccSpline();
    if (pBSpline.IsNull())
    {
        assert(false);
        for (SketchTrimKnot& knot : _knots)
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
        for (SketchTrimKnot& knot : _knots)
        {
            knot.setParam(DBL_MAX);
        }
        return;
    }

    for (SketchTrimKnot& knot : _knots)
    {
        Geom2dAPI_ProjectPointOnCurve projector(
            gp_Pnt2d(knot.getPosition().x(), knot.getPosition().y()),
            pBSpline);
        if (projector.NbPoints() > 0)
        {
            // 对于样条曲线而言还需要判断距离是否在容差内
            // 比如:样条曲线和直线段在样条曲线的中间处相交,在修剪掉样条曲线的[0.5,1.0]段之后;
            // 原来样条曲线的终点投影到现在的样条曲线上可能为0.97在有效范围内,但这是不合理的,会导致后续的assert.
            if (projector.LowerDistance() <= 1e-5)
            {
                double t = (projector.LowerDistanceParameter() - firstParam) / paramRange;
                knot.setParam(SketchTrimExtendUtil::reviseT(t));
            }
            else
            {
                knot.setParam(DBL_MAX);
            }
        }
        else
        {
            knot.setParam(DBL_MAX);
        }
    }
}

void SketchTrimNode::refresh(const wydb::Database* pDb)
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
        for (SketchTrimKnot& knot : _knots)
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
            for (SketchTrimKnot& knot : _knots)
            {
                knot.setParam(DBL_MAX);
            }
        }
        else
        {
            wy::Vector2 center = pSketchArc->getCenter();
            double startAngle = wy3d::normalizeRadian(pSketchArc->getStartAngle());
            double endAngle = startAngle + totalAngle;
            for (SketchTrimKnot& knot : _knots)
            {
                double angle = wy::Vector2::rotationAngle(kAxisX, knot.getPosition() - center);
                angle = SketchTrimExtendUtil::reviseAngle(angle, startAngle, endAngle);
                double t = (angle - startAngle) / totalAngle;
                t = SketchTrimExtendUtil::reviseT(t);
                knot.setParam(t);
            }
        }
    }
    else if (const wy3d::SketchEllipse* pSketchEllipse = wy3d::SketchEllipse::cast(pSketchCurve))
    {
        wy::Vector2 center = pSketchEllipse->getCenter();
        wy::Vector2 majorAxis = pSketchEllipse->getMajorAxis();
        for (SketchTrimKnot& knot : _knots)
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
            for (SketchTrimKnot& knot : _knots)
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
            for (SketchTrimKnot& knot : _knots)
            {
                double angle = wy::Vector2::rotationAngle(majorAxis, knot.getPosition() - center);
                angle = SketchTrimExtendUtil::reviseAngle(angle, startAngle, endAngle);
                double t = (angle - startAngle) / totalAngle;
                t = SketchTrimExtendUtil::reviseT(t);
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
        for (SketchTrimKnot& knot : _knots)
        {
            knot.setParam(DBL_MAX);
        }
    }

    // 排序
    std::sort(_knots.begin(), _knots.end(), [](const SketchTrimKnot& lhs, const SketchTrimKnot& rhs) {
        return lhs.getParam() < rhs.getParam(); });
}

SketchTrimSegment SketchTrimNode::pick(SketchTrimGraph* pGraph, const wy::Vector2& position, double tol)
{
    assert(pGraph);
    SketchTrimSegment segment; // 默认是无效的曲线段

    // 获取曲线
    wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return segment;
    }
    const wy3d::SketchCurve* pSketchCurve = wy3d::SketchCurve::cast(pDb->getElement(_id));
    if (!pSketchCurve)
    {
        assert(false);
        return segment;
    }
    if (pSketchCurve->isErased())
    {
        assert(false);
        return segment;
    }

    // 初筛:判断是否选取曲线整体
    bool isSelectTheWhole(false);
    if (pSketchCurve->isClosed()) // 闭合曲线
    {
        size_t num(0);
        size_t lastIdx(-1), firstIdx(-1);
        double firstParam(DBL_MAX);
        for (size_t i = 0; i < _knots.size(); ++i)
        {
            double t = _knots[i].getParam();
            if (!isInValidRange(t)) // 排除无效的交点
            {
                assert(false);
                continue;
            }
            if (SketchTrimNodeSPtr pOtherNode = pGraph->getNode(_knots[i].getOtherOwner())) // 判断交点是否依然还有效
            {
                if (!pOtherNode->isValidKnotPosition(pDb, _knots[i].getPosition())) continue;
            }
            if (-1 == lastIdx)
            {
                // 第一个有效交点
                ++num;
                lastIdx = i;
                firstIdx = i;
                firstParam = t;
            }
            else if (std::fabs(t - _knots[lastIdx].getParam()) > tol) // 不重合的交点
            {
                if (t > 0.99 && firstParam < 0.01) // 闭合曲线对于靠近终点处需要判断与起点的距离
                {
                    if (std::fabs(t - 1.0 - firstParam) > tol)
                    {
                        ++num;
                        lastIdx = i;
                    }
                }
                else
                {
                    ++num;
                    lastIdx = i;
                }
            }
        }
        
        if (num <= 1) // 曲线整体
        {
            isSelectTheWhole = true;
        }
    }
    else // 非闭合曲线
    {
        assert(_knots.size() >= 2); // 初始化的时候加入了起点与终点
        size_t num(0);
        size_t lastIdx(-1);
        for (size_t i = 0; i < _knots.size(); ++i)
        {
            double t = _knots[i].getParam();
            if (!isInValidRange(t)) // 排除无效的交点
            {
                continue;
            }
            if (t != 0.0 && t != 1.0) // 非起点终点处判断交点是否依然还有效
            {
                if (SketchTrimNodeSPtr pOtherNode = pGraph->getNode(_knots[i].getOtherOwner()))
                {
                    if (!pOtherNode->isValidKnotPosition(pDb, _knots[i].getPosition())) continue;
                }
            }
            if (-1 == lastIdx)
            {
                ++num;
                lastIdx = i;
            }
            else if (std::fabs(t - _knots[lastIdx].getParam()) > tol)
            {
                ++num;
            }
        }

        if (num <= 2) // 曲线整体
        {
            isSelectTheWhole = true;
        }
    }
    if (isSelectTheWhole)
    {
        segment.startKnot.setParam(0.0);
        segment.startKnot.setPosition(pSketchCurve->getStartPoint());
        segment.endKnot.setParam(1.0);
        segment.endKnot.setPosition(pSketchCurve->getEndPoint());
        return segment;
    }

    // 选取曲线段
    double t(0.0);
    if (const wy3d::SketchLine* pSketchLine = wy3d::SketchLine::cast(pSketchCurve))
    {
        assert(_knots.size() > 2);
        t = SketchTrimExtendUtil::getParamOfLine(pSketchLine, position);
    }
    else if (const wy3d::SketchCenterLine* pSketchCenterLine = wy3d::SketchCenterLine::cast(pSketchCurve))
    {
        assert(_knots.size() > 2);
        t = SketchTrimExtendUtil::getParamOfCenterLine(pSketchCenterLine, position);
    }
    else if (const wy3d::SketchCircle* pSketchCircle = wy3d::SketchCircle::cast(pSketchCurve))
    {
        assert(_knots.size() > 1);
        t = SketchTrimExtendUtil::getParamOfCircle(pSketchCircle, position);
    }
    else if (const wy3d::SketchArc* pSketchArc = wy3d::SketchArc::cast(pSketchCurve))
    {
        assert(_knots.size() > 2);
        t = SketchTrimExtendUtil::getParamOfArc(pSketchArc, position);
    }
    else if (const wy3d::SketchEllipse* pSketchEllipse = wy3d::SketchEllipse::cast(pSketchCurve))
    {
        assert(_knots.size() > 1);
        t = SketchTrimExtendUtil::getParamOfEllipse(pSketchEllipse, position);
    }
    else if (const wy3d::SketchEllipseArc* pSketchEllipseArc = wy3d::SketchEllipseArc::cast(pSketchCurve))
    {
        assert(_knots.size() > 2);
        t = SketchTrimExtendUtil::getParamOfEllipseArc(pSketchEllipseArc, position);
    }
    else if (const wy3d::SketchSpline* pSketchSpline = wy3d::SketchSpline::cast(pSketchCurve))
    {
        assert(_knots.size() > 2);
        t = SketchTrimExtendUtil::getPickParamOfSpline(*pSketchSpline, position);
    }
    else
    {
        assert(false);
        t = DBL_MAX;
    }

    size_t theFirstIndex(-1);
    size_t theLastIndex(-1);
    size_t startIndex(-1);
    size_t endIndex(-1);
    for (size_t i = 0; i < _knots.size(); ++i)
    {
        double currParam = _knots[i].getParam();
        if (!isInValidRange(currParam)) continue; // 跳过无效的点

        // 是否还是有效的交点需要根据另外一个曲线主体来判断
        if (currParam != 0.0 && currParam != 1.0)
        {
            SketchTrimNodeSPtr pOtherOwner = pGraph->getNode(_knots[i].getOtherOwner());
            assert(pOtherOwner);
            if (pOtherOwner && !pOtherOwner->isValidKnotPosition(pDb, _knots[i].getPosition()))
            {
                continue;
            }
        }

        if (-1 == theFirstIndex) theFirstIndex = i;
        theLastIndex = i;

        if (-1 == startIndex)
        {
            if (t >= currParam)
            {
                segment.startKnot = _knots[i];
                startIndex = i;
                continue;
            }
        }
        else if (std::fabs(currParam - segment.startKnot.getParam()) > tol) // 跳过重叠的点
        {
            if (t <= currParam)
            {
                segment.endKnot = _knots[i];
                endIndex = i;
                break;
            }
            else
            {
                segment.startKnot = _knots[i];
                startIndex = i;
                continue;
            }
        }
    }
    assert(theFirstIndex != -1 && theLastIndex != -1 && theFirstIndex != theLastIndex);

    // 闭合曲线
    if (pSketchCurve->isClosed())
    {
        if (segment.startKnot.getParam() == DBL_MAX)
        {
            if (theLastIndex == -1)
            {
                assert(false);
                return segment;
            }
            segment.startKnot = _knots[theLastIndex];
        }

        if (segment.endKnot.getParam() == DBL_MAX)
        {
            if (theFirstIndex == -1)
            {
                assert(false);
                return segment;
            }
            segment.endKnot = _knots[theFirstIndex];
        }
    }
    else // 非闭合曲线
    {
        assert(segment.startKnot.getParam() != DBL_MAX && segment.endKnot.getParam() != DBL_MAX);
    }

    return segment;
}

bool SketchTrimNode::isValidKnotPosition(wydb::Database* pDb, const wy::Vector2& pos) const
{
    assert(pDb);
    double param = this->getParam(pDb, pos);
    if (isInValidRange(param))
    {
        return true;
    }

    for (const std::shared_ptr<SketchTrimNode>& pChildNode : _children)
    {
        assert(pChildNode);
        if (pChildNode->isValidKnotPosition(pDb, pos))
        {
            return true;
        }
    }

    return false;
}

} // namespace wy3dApp
