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

#include "SketchExtendGraph.h"
#include <set>
#include <algorithm>
#include <cassert>
#include <wyVector2.h>
#include <utils/wy3dCurveIntersectionUtil.h>
#include <wydbDatabase.h>
#include <wy3dSketchCurve.h>
#include <wy3dSketchCurve.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>
#include <wy3dSketchEllipse.h>
#include <wy3dSketchEllipseArc.h>
#include <wy3dSketchSpline.h>
#include <wy3dSketchSpline.h>

#include "application/Application.h"
#include "SketchTrimExtendUtil.h"

#define MAKE_ID_PAIR(id1st, id2st) std::pair<wydb::ElementId, wydb::ElementId>(id1st, id2st)

SketchExtendGraph::SketchExtendGraph(const wy3d::Sketch* pSketch, double tol) : _pSketch(pSketch), _tol(tol), _isValid(false)
{
    assert(_pSketch);
    if (_pSketch && this->init())
    {
        _isValid = true;
    }
}

static inline wy3d::geom::BoundingBox2 getBoundingBoxOfCircle(const wy::Vector2& center, double radius)
{
    return wy3d::geom::BoundingBox2(
        wy::Vector2(center.x() - radius, center.y() - radius),
        wy::Vector2(center.x() + radius, center.y() + radius));
}

static wy3d::geom::BoundingBox2 getBoundingBoxOfEllipse(const wy::Vector2& center, const wy::Vector2& majorAxis, double radiusRatio)
{
    // 获取椭圆的长轴和短轴的半径
    double majorRadius = majorAxis.length();
    double minorRadius = majorRadius * radiusRatio;

    // 计算长轴与x轴的夹角
    double angle = std::atan2(majorAxis.y(), majorAxis.x());

    // 计算旋转矩阵的cos和sin值
    double cosAngle = std::cos(angle);
    double sinAngle = std::sin(angle);

    // 旋转后的长轴和短轴在边界框中的影响
    double dx = majorRadius * std::abs(cosAngle) + minorRadius * std::abs(sinAngle);
    double dy = majorRadius * std::abs(sinAngle) + minorRadius * std::abs(cosAngle);

    // 计算并返回椭圆的包围盒
    return wy3d::geom::BoundingBox2(
        wy::Vector2(center.x() - dx, center.y() - dy),
        wy::Vector2(center.x() + dx, center.y() + dy));
}

bool SketchExtendGraph::init()
{
    assert(_pSketch);
    wydb::Database* pDb = _pSketch->getDatabase();
    if (!pDb)
    {
        assert(false);
        return false;
    }

    // 收集所有的草图曲线
    std::vector<const wy3d::SketchCurve*> curves;
    curves.reserve(100);
    struct LINE
    {
        wydb::ElementId id;
        wy::Vector2 startPnt;
        wy::Vector2 endPnt;

        LINE(const wydb::ElementId& argId, const wy::Vector2& argStartPnt, const wy::Vector2& argEndPnt)
            : id(argId), startPnt(argStartPnt), endPnt(argEndPnt) {}
    };
    std::vector<LINE> lines; // 单独收集线
    lines.reserve(100);
    struct SPLINE
    {
        wydb::ElementId id;
        wy::Vector2 startPnt;
        wy::Vector2 startDir;
        wy::Vector2 endPnt;
        wy::Vector2 endDir;
        const wy3d::SketchSpline* pSketchSpline;

        SPLINE() : id(wydb::ElementId::kNull), pSketchSpline(nullptr) {}
    };
    std::vector<SPLINE> splines; // 单独收集样条曲线
    splines.reserve(20);
    for (auto iter = _pSketch->createIterator(); !iter.isDone(); iter.moveNext())
    {
        const wy3d::SketchCurve* pSketchCurve = wy3d::SketchCurve::cast(pDb->getElement(iter.current()));
        if (!pSketchCurve) continue;
        curves.emplace_back(pSketchCurve);
        if (const wy3d::SketchLine* pSketchLine = wy3d::SketchLine::cast(pSketchCurve))
        {
            lines.emplace_back(LINE(pSketchLine->getId(), pSketchLine->getStartPoint(), pSketchLine->getEndPoint()));
        }
        else if (const wy3d::SketchCenterLine* pSketchCenterLine = wy3d::SketchCenterLine::cast(pSketchCurve))
        {
            lines.emplace_back(LINE(pSketchCenterLine->getId(), pSketchCenterLine->getStartPoint(), pSketchCenterLine->getEndPoint()));
        }
        else if (const wy3d::SketchSpline* pSketchSpline = wy3d::SketchSpline::cast(pSketchCurve))
        {            Handle(Geom2d_BSplineCurve) pBSpline = pSketchSpline->getOccSpline();
            if (!pBSpline) continue;

            SPLINE spline;
            spline.id = pSketchSpline->getId();
            if (!SketchTrimExtendUtil::getBSplineInfo(pBSpline, spline.startPnt, spline.startDir, spline.endPnt, spline.endDir))
            {
                continue;
            }
            spline.pSketchSpline = pSketchSpline;
            splines.emplace_back(spline);
        }
    }

    // 初始化结点
    for (const wy3d::SketchCurve* pSketchCurve : curves)
    {
        SketchExtendNodeSPtr pNode = std::make_shared<SketchExtendNode>(pSketchCurve->getId());
        if (pSketchCurve->isClosed())
        {
            pNode->setIsClosed(true); // 标记闭合曲线
        }
        _id2Node[pSketchCurve->getId()] = std::move(pNode);
    }

    // 构建区域树
    // 只构建圆&圆弧&椭圆&椭圆弧的区域树,直线段由于扩展成直线后的包围盒理论上是无限大的,所以略过.
    struct RTreeNode
    {
        wydb::ElementId id;
        size_t index;

        RTreeNode(wydb::ElementId inId = wydb::ElementId::kNull, size_t inIndex = -1) : id(inId), index(inIndex) {}
    };
    RTree<RTreeNode, double, 2> rtree;
    std::vector<wy3d::geom::BoundingBox2> bboxs;
    bboxs.resize(curves.size());
    wy3d::geom::BoundingBox2 bbox;
    for (size_t i = 0; i < curves.size(); ++i)
    {
        const wy3d::SketchCurve* pSketchCurve = curves[i];
        if (const wy3d::SketchLine* pSketchLine = wy3d::SketchLine::cast(pSketchCurve))
        {
            // 直线段扩展为直线后包围盒理论上是无限大的
            continue;
        }
        else if (const wy3d::SketchCenterLine* pSketchCenterLine = wy3d::SketchCenterLine::cast(pSketchCurve))
        {
            // 直线段扩展为直线后包围盒理论上是无限大的
            continue;
        }
        else if (const wy3d::SketchCircle* pCircle = wy3d::SketchCircle::cast(pSketchCurve))
        {
            bbox = getBoundingBoxOfCircle(pCircle->getCenter(), pCircle->getRadius());
        }
        else if (const wy3d::SketchArc* pArc = wy3d::SketchArc::cast(pSketchCurve))
        {
            bbox = getBoundingBoxOfCircle(pArc->getCenter(), pArc->getRadius());
        }
        else if (const wy3d::SketchEllipse* pEllipse = wy3d::SketchEllipse::cast(pSketchCurve))
        {
            bbox = getBoundingBoxOfEllipse(pEllipse->getCenter(), pEllipse->getMajorAxis(), pEllipse->getRadiusRatio());
        }
        else if (const wy3d::SketchEllipseArc* pEllipseArc = wy3d::SketchEllipseArc::cast(pSketchCurve))
        {
            bbox = getBoundingBoxOfEllipse(pEllipseArc->getCenter(), pEllipseArc->getMajorAxis(), pEllipseArc->getRadiusRatio());
        }
        else if (const wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pSketchCurve))
        {            bbox = pSpline->getBoundingBox();
        }
        else
        {
            assert(false);
            continue;
        }
        if (bbox.isEmpty()) continue;
        bboxs[i] = bbox;
        double min[2] = { bbox.min().x(), bbox.min().y() };
        double max[2] = { bbox.max().x(), bbox.max().y() };
        rtree.Insert(min, max, RTreeNode(pSketchCurve->getId(), i));
    }

    // 首先遍历草绘直线段求出交点信息
    std::set<std::pair<wydb::ElementId, wydb::ElementId>> intersected;
    for (size_t i = 0; i < lines.size(); ++i)
    {
        const LINE& line = lines[i];
        auto iterNode = _id2Node.find(line.id);
        if (iterNode == _id2Node.cend())
        {
            assert(false);
            continue;
        }
        SketchExtendNodeSPtr pNode = iterNode->second;
        assert(pNode);

        wy::Vector2 intPnt1, intPnt2;
        unsigned int numIntPnts(0);
        for (size_t j = 0; j < curves.size(); ++j)
        {
            const wy3d::SketchCurve* pOtherCurve = curves[j];
            assert(pOtherCurve);
            if (intersected.find(MAKE_ID_PAIR(line.id, pOtherCurve->getId())) != intersected.cend())
                continue; // 已经求过交点了

            SketchExtendNodeSPtr pOtherNode;
            auto iterOtherNode = _id2Node.find(pOtherCurve->getId());
            if (iterOtherNode != _id2Node.cend())
            {
                pOtherNode = iterOtherNode->second;
            }
            assert(pOtherNode);

            numIntPnts = 0;
            if (const wy3d::SketchLine* pOtherLine = wy3d::SketchLine::cast(pOtherCurve))
            {
                if (line.id == pOtherLine->getId()) continue; // 排除与自身求交
                if (wy3d::intersectLineLine(line.startPnt, line.endPnt,
                    pOtherLine->getStartPoint(), pOtherLine->getEndPoint(), intPnt1))
                {
                    numIntPnts = 1;
                }
            }
            else if (const wy3d::SketchCenterLine* pOtherCenterLine = wy3d::SketchCenterLine::cast(pOtherCurve))
            {
                if (line.id == pOtherCenterLine->getId()) continue; // 排除与自身求交
                if (wy3d::intersectLineLine(line.startPnt, line.endPnt,
                    pOtherCenterLine->getStartPoint(), pOtherCenterLine->getEndPoint(), intPnt1))
                {
                    numIntPnts = 1;
                }
            }
            else if (const wy3d::SketchCircle* pOtherCircle = wy3d::SketchCircle::cast(pOtherCurve))
            {
                numIntPnts = wy3d::intersectLineCircle(line.startPnt, line.endPnt,
                    pOtherCircle->getCenter(), pOtherCircle->getRadius(), intPnt1, intPnt2);
            }
            else if (const wy3d::SketchArc* pOtherArc = wy3d::SketchArc::cast(pOtherCurve))
            {
                numIntPnts = wy3d::intersectLineCircle(line.startPnt, line.endPnt,
                    pOtherArc->getCenter(), pOtherArc->getRadius(), intPnt1, intPnt2);
            }
            else if (const wy3d::SketchEllipse* pOtherEllipse = wy3d::SketchEllipse::cast(pOtherCurve))
            {
                numIntPnts = wy3d::intersectLineEllipse(line.startPnt, line.endPnt,
                    pOtherEllipse->getCenter(), pOtherEllipse->getMajorAxis(), pOtherEllipse->getRadiusRatio(), intPnt1, intPnt2);
            }
            else if (const wy3d::SketchEllipseArc* pOtherEllipseArc = wy3d::SketchEllipseArc::cast(pOtherCurve))
            {
                numIntPnts = wy3d::intersectLineEllipse(line.startPnt, line.endPnt,
                    pOtherEllipseArc->getCenter(), pOtherEllipseArc->getMajorAxis(), pOtherEllipseArc->getRadiusRatio(), intPnt1, intPnt2);
            }
            else if (const wy3d::SketchSpline* pOtherSpline = wy3d::SketchSpline::cast(pOtherCurve))
            {
                // B样条
                Handle(Geom2d_BSplineCurve) pBSpline = pOtherSpline->getOccSpline();
                if (!pBSpline) continue;

                // 获取B样条的数据:起点+起点方向向量;终点+终点方向向量
                wy::Vector2 startPnt, startDir;
                wy::Vector2 endPnt, endDir;
                if (!SketchTrimExtendUtil::getBSplineInfo(pBSpline, startPnt, startDir, endPnt, endDir))
                {
                    assert(false);
                    continue;
                }

                // 直线与B样条起点射线求交
                assert(0 == numIntPnts);
                if (wy3d::intersectLineRayLine(line.startPnt, line.endPnt,
                    startPnt, startPnt - startDir, intPnt1))
                {
                    numIntPnts = 1;
                }
                
                // 直线与B样条终点射线求交
                wy::Vector2& intPntRef = (1 == numIntPnts) ? intPnt2 : intPnt1;
                if (wy3d::intersectLineRayLine(line.startPnt, line.endPnt,
                    endPnt, endPnt + endDir, intPntRef))
                {
                    ++numIntPnts;
                }

                // added by wangyao 2025.06.23 {
                // 单独处理直线与B样条本身求交(游离于框架之外所以直接调用了appendKnot)
                std::vector<wy::Vector2> intPnts;
                if (wy3d::intersectLineSpline(line.startPnt, line.endPnt, pBSpline, intPnts))
                {
                    for (const wy::Vector2& intPnt : intPnts)
                    {
                        pNode->appendKnot(intPnt, pOtherCurve->getId());
                        if (pOtherNode && !pOtherNode->isClosed())
                        {
                            pOtherNode->appendKnot(intPnt, line.id);
                        }
                    }
                }
                // }
            }
            else
            {
                assert(false);
                continue;
            }
            intersected.insert(MAKE_ID_PAIR(pOtherCurve->getId(), line.id));

            if (numIntPnts >= 1)
            {
                pNode->appendKnot(intPnt1, pOtherCurve->getId());
                if (pOtherNode && !pOtherNode->isClosed())
                {
                    pOtherNode->appendKnot(intPnt1, line.id);
                }
            }
            if (numIntPnts >= 2)
            {
                pNode->appendKnot(intPnt2, pOtherCurve->getId());
                if (pOtherNode && !pOtherNode->isClosed())
                {
                    pOtherNode->appendKnot(intPnt2, line.id);
                }
            }
        }
    }

    // 然后遍历样条曲线求出交点信息
    auto intersectCircleBSpline = [](const wy::Vector2& center, double radius,
        Handle(Geom2d_BSplineCurve) pBSpline,
        const wy::Vector2& startPnt, const wy::Vector2& startDir,
        const wy::Vector2& endPnt, const wy::Vector2& endDir,
        std::vector<wy::Vector2>& intPnts, std::vector<wy::Vector2>& intPntsTemp) -> unsigned int
    {
        intPnts.clear();

        // 圆与B样条求交
        //intPntsTemp.clear();
        //if (wy3d::intersectCircleSpline(center, radius, pBSpline, intPntsTemp))
        //{
        //    intPnts.insert(intPnts.cend(), intPntsTemp.cbegin(), intPntsTemp.cend());
        //}

        // 圆与B样条起点射线求交
        intPntsTemp.clear();
        if (wy3d::intersectCircleRayLine(center, radius,
            startPnt, startPnt - startDir, intPntsTemp))
        {
            intPnts.insert(intPnts.cend(), intPntsTemp.cbegin(), intPntsTemp.cend());
        }

        // 圆与B样条终点射线求交
        intPntsTemp.clear();
        if (wy3d::intersectCircleRayLine(center, radius,
            endPnt, endPnt + endDir, intPntsTemp))
        {
            intPnts.insert(intPnts.cend(), intPntsTemp.cbegin(), intPntsTemp.cend());
        }

        return intPnts.size();
    };
    auto intersectEllipseBSpline = [](const wy::Vector2& center, const wy::Vector2& majorAxis, double radiusRatio,
        Handle(Geom2d_BSplineCurve) pBSpline,
        const wy::Vector2& startPnt, const wy::Vector2& startDir,
        const wy::Vector2& endPnt, const wy::Vector2& endDir,
        std::vector<wy::Vector2>& intPnts, std::vector<wy::Vector2>& intPntsTemp) -> unsigned int
    {
        intPnts.clear();

        // 椭圆与B样条求交
        //intPntsTemp.clear();
        //if (wy3d::intersectEllipseSpline(center, majorAxis, radiusRatio, pBSpline, intPntsTemp))
        //{
        //    intPnts.insert(intPnts.cend(), intPntsTemp.cbegin(), intPntsTemp.cend());
        //}

        // 椭圆与B样条起点射线求交
        intPntsTemp.clear();
        if (wy3d::intersectEllipseRayLine(center, majorAxis, radiusRatio,
            startPnt, startPnt - startDir, intPntsTemp))
        {
            intPnts.insert(intPnts.cend(), intPntsTemp.cbegin(), intPntsTemp.cend());
        }

        // 椭圆与B样条终点射线求交
        intPntsTemp.clear();
        if (wy3d::intersectEllipseRayLine(center, majorAxis, radiusRatio,
            endPnt, endPnt + endDir, intPntsTemp))
        {
            intPnts.insert(intPnts.cend(), intPntsTemp.cbegin(), intPntsTemp.cend());
        }

        return intPnts.size();
    };
    auto intersectBSplineBSpline = [](
        Handle(Geom2d_BSplineCurve) pBSpline1st,
        const wy::Vector2& startPnt1st, const wy::Vector2& startDir1st,
        const wy::Vector2& endPnt1st, const wy::Vector2& endDir1st,
        Handle(Geom2d_BSplineCurve) pBSpline2nd,
        const wy::Vector2& startPnt2nd, const wy::Vector2& startDir2nd,
        const wy::Vector2& endPnt2nd, const wy::Vector2& endDir2nd,
        std::vector<wy::Vector2>& intPnts, std::vector<wy::Vector2>& intPntsTemp) -> unsigned int
    {
        intPnts.clear();

        // 样条曲线1与样条曲线2的起点射线求交
        intPntsTemp.clear();
        if (wy3d::intersectSplineRayLine(pBSpline1st, startPnt2nd, startPnt2nd - startDir2nd, intPntsTemp))
        {
            intPnts.insert(intPnts.cend(), intPntsTemp.cbegin(), intPntsTemp.cend());
        }

        // 样条曲线1与样条曲线2的终点射线求交
        intPntsTemp.clear();
        if (wy3d::intersectSplineRayLine(pBSpline1st, endPnt2nd, endPnt2nd + endDir2nd, intPntsTemp))
        {
            intPnts.insert(intPnts.cend(), intPntsTemp.cbegin(), intPntsTemp.cend());
        }

        // 样条曲线2与样条曲线1的起点射线求交
        intPntsTemp.clear();
        if (wy3d::intersectSplineRayLine(pBSpline2nd, startPnt1st, startPnt1st - startDir1st, intPntsTemp))
        {
            intPnts.insert(intPnts.cend(), intPntsTemp.cbegin(), intPntsTemp.cend());
        }

        // 样条曲线2与样条曲线1的终点射线求交
        intPntsTemp.clear();
        if (wy3d::intersectSplineRayLine(pBSpline2nd, endPnt1st, endPnt1st + endDir1st, intPntsTemp))
        {
            intPnts.insert(intPnts.cend(), intPntsTemp.cbegin(), intPntsTemp.cend());
        }

        wy::Vector2 intPnt;

        // 样条曲线1的起点射线与样条曲线2的起点射线
        intPntsTemp.clear();
        if (wy3d::intersectRayLineRayLine(startPnt1st, startPnt1st - startDir1st, startPnt2nd, startPnt2nd - startDir2nd, intPnt))
        {
            intPnts.emplace_back(intPnt);
        }

        // 样条曲线1的起点射线与样条曲线2的终点射线
        intPntsTemp.clear();
        if (wy3d::intersectRayLineRayLine(startPnt1st, startPnt1st - startDir1st, endPnt2nd, endPnt2nd + endDir2nd, intPnt))
        {
            intPnts.emplace_back(intPnt);
        }

        // 样条曲线1的终点射线与样条曲线2的起点射线
        intPntsTemp.clear();
        if (wy3d::intersectRayLineRayLine(endPnt1st, endPnt1st + endDir1st, startPnt2nd, startPnt2nd - startDir2nd, intPnt))
        {
            intPnts.emplace_back(intPnt);
        }

        // 样条曲线1的终点射线与样条曲线2的终点射线
        intPntsTemp.clear();
        if (wy3d::intersectRayLineRayLine(endPnt1st, endPnt1st + endDir1st, endPnt2nd, endPnt2nd + endDir2nd, intPnt))
        {
            intPnts.emplace_back(intPnt);
        }

        return intPnts.size();
    };
    for (size_t i = 0; i < splines.size(); ++i)
    {
        const SPLINE& spline = splines[i];
        auto iterNode = _id2Node.find(spline.id);
        if (iterNode == _id2Node.cend())
        {
            assert(false);
            continue;
        }
        SketchExtendNodeSPtr pNode = iterNode->second;
        assert(pNode);

        if (!spline.pSketchSpline)
        {
            assert(false);
            continue;
        }
        Handle(Geom2d_BSplineCurve) pBSpline = spline.pSketchSpline->getOccSpline();
        if (!pBSpline)
        {
            assert(false);
            continue;
        }

        // 获取B样条的数据:起点+起点方向向量;终点+终点方向向量
        wy::Vector2 startPnt, startDir;
        wy::Vector2 endPnt, endDir;
        if (!SketchTrimExtendUtil::getBSplineInfo(pBSpline, startPnt, startDir, endPnt, endDir))
        {
            assert(false);
            continue;
        }

        std::vector<wy::Vector2> intPnts;
        intPnts.reserve(10);
        unsigned int numIntPnts(0);
        std::vector<wy::Vector2> intPntsTemp;
        intPntsTemp.reserve(10);

        for (size_t j = 0; j < curves.size(); ++j)
        {
            const wy3d::SketchCurve* pOtherCurve = curves[j];
            assert(pOtherCurve);
            if (intersected.find(MAKE_ID_PAIR(spline.id, pOtherCurve->getId())) != intersected.cend())
                continue; // 已经求过交点了

            SketchExtendNodeSPtr pOtherNode;
            auto iterOtherNode = _id2Node.find(pOtherCurve->getId());
            if (iterOtherNode != _id2Node.cend())
            {
                pOtherNode = iterOtherNode->second;
            }
            assert(pOtherNode);

            intPnts.clear();
            numIntPnts = 0;
            if (const wy3d::SketchLine* pOtherLine = wy3d::SketchLine::cast(pOtherCurve))
            {
                continue; // 已经求过交点了
            }
            else if (const wy3d::SketchCenterLine* pOtherCenterLine = wy3d::SketchCenterLine::cast(pOtherCurve))
            {
                continue; // 已经求过交点了
            }
            else if (const wy3d::SketchCircle* pOtherCircle = wy3d::SketchCircle::cast(pOtherCurve))
            {
                numIntPnts = intersectCircleBSpline(pOtherCircle->getCenter(), pOtherCircle->getRadius(), pBSpline,
                    startPnt, startDir, endPnt, endDir, intPnts, intPntsTemp);
            }
            else if (const wy3d::SketchArc* pOtherArc = wy3d::SketchArc::cast(pOtherCurve))
            {
                numIntPnts = intersectCircleBSpline(pOtherArc->getCenter(), pOtherArc->getRadius(), pBSpline,
                    startPnt, startDir, endPnt, endDir, intPnts, intPntsTemp);
            }
            else if (const wy3d::SketchEllipse* pOtherEllipse = wy3d::SketchEllipse::cast(pOtherCurve))
            {
                numIntPnts = intersectEllipseBSpline(pOtherEllipse->getCenter(), pOtherEllipse->getMajorAxis(), pOtherEllipse->getRadiusRatio(),
                    pBSpline, startPnt, startDir, endPnt, endDir, intPnts, intPntsTemp);
            }
            else if (const wy3d::SketchEllipseArc* pOtherEllipseArc = wy3d::SketchEllipseArc::cast(pOtherCurve))
            {
                numIntPnts = intersectEllipseBSpline(pOtherEllipseArc->getCenter(), pOtherEllipseArc->getMajorAxis(), pOtherEllipseArc->getRadiusRatio(),
                    pBSpline, startPnt, startDir, endPnt, endDir, intPnts, intPntsTemp);
            }
            else if (const wy3d::SketchSpline* pOtherSpline = wy3d::SketchSpline::cast(pOtherCurve))
            {
                if (spline.id == pOtherSpline->getId()) continue; // 排除与自身求交
                Handle(Geom2d_BSplineCurve) pOtherBSpline = pOtherSpline->getOccSpline();
                if (!pOtherBSpline) continue;

                wy::Vector2 startPntOther, startDirOther;
                wy::Vector2 endPntOther, endDirOther;
                if (!SketchTrimExtendUtil::getBSplineInfo(pOtherBSpline, startPntOther, startDirOther, endPntOther, endDirOther))
                {
                    assert(false);
                    continue;
                }

                numIntPnts = intersectBSplineBSpline(
                    pBSpline, startPnt, startDir, endPnt, endDir,
                    pOtherBSpline, startPntOther, startDirOther, endPntOther, endDirOther,
                    intPnts, intPntsTemp);
            }
            else
            {
                assert(false);
                continue;
            }
            intersected.insert(MAKE_ID_PAIR(pOtherCurve->getId(), spline.id));

            for (const wy::Vector2& intPnt : intPnts)
            {
                pNode->appendKnot(intPnt, pOtherCurve->getId());
                if (pOtherNode && !pOtherNode->isClosed())
                {
                    pOtherNode->appendKnot(intPnt, spline.id);
                }
            }
        }
    }
    
    // 最后遍历除草绘直线段之外的非闭合曲线
    std::vector<size_t> candidates;
    candidates.reserve(curves.size());
    std::set<std::pair<size_t, size_t>> intersectedIndices;
    for (size_t i = 0; i < curves.size(); ++i)
    {
        const wy3d::SketchCurve* pSketchCurve = curves[i];
        assert(pSketchCurve);
        if (pSketchCurve->isClosed()) continue; // 排除闭合曲线
        if (const wy3d::SketchLine* pSketchLine = wy3d::SketchLine::cast(pSketchCurve)) continue; // 排除草绘直线段
        if (const wy3d::SketchSpline* pSketchSpline = wy3d::SketchSpline::cast(pSketchCurve)) continue; // 排除草绘样条曲线
        
        wydb::ElementId id = pSketchCurve->getId();
        auto iterNode = _id2Node.find(id);
        if (iterNode == _id2Node.cend())
        {
            assert(false);
            continue;
        }
        SketchExtendNodeSPtr pNode = iterNode->second;
        assert(pNode);
       
        // 空间搜索快速找出候选曲线
        candidates.clear();
        const wy3d::geom::BoundingBox2& bbox = bboxs[i];
        if (bbox.isEmpty()) continue;
        double min[2] = { bbox.min().x(), bbox.min().y() };
        double max[2] = { bbox.max().x(), bbox.max().y() };
        rtree.Search(min, max, [&candidates](const RTreeNode& rtreeNode) {
            candidates.emplace_back(rtreeNode.index);
            return true; }
        );

        // 圆弧
        if (const wy3d::SketchArc* pArc = wy3d::SketchArc::cast(pSketchCurve))
        {
            std::vector<wy::Vector2> intPnts; // 专门用于与样条曲线求交
            unsigned int numIntPnts(0);
            wy::Vector2 intPnt1, intPnt2, intPnt3, intPnt4;
            for (size_t index : candidates)
            {
                // 排除本身
                if (index == i) continue;
                // 之前已经求过交点了
                if (intersectedIndices.find(std::pair<size_t, size_t>(i, index)) != intersectedIndices.cend())
                {
                    continue;
                }
                // 求交
                intPnts.clear();
                numIntPnts = 0;
                const wy3d::SketchCurve* pOtherCurve = curves[index];
                assert(pOtherCurve);
                if (const wy3d::SketchCircle* pOtherCircle = wy3d::SketchCircle::cast(pOtherCurve))
                {
                    numIntPnts = wy3d::intersectCircleCircle(pArc->getCenter(), pArc->getRadius(),
                        pOtherCircle->getCenter(), pOtherCircle->getRadius(), intPnt1, intPnt2);
                }
                else if (const wy3d::SketchArc* pOtherArc = wy3d::SketchArc::cast(pOtherCurve))
                {
                    numIntPnts = wy3d::intersectCircleCircle(pArc->getCenter(), pArc->getRadius(),
                        pOtherArc->getCenter(), pOtherArc->getRadius(), intPnt1, intPnt2);
                }
                else if (const wy3d::SketchEllipse* pOtherEllipse = wy3d::SketchEllipse::cast(pOtherCurve))
                {
                    numIntPnts = wy3d::intersectCircleEllipse(pArc->getCenter(), pArc->getRadius(),
                        pOtherEllipse->getCenter(), pOtherEllipse->getMajorAxis(), pOtherEllipse->getRadiusRatio(), intPnt1, intPnt2, intPnt3, intPnt4);
                }
                else if (const wy3d::SketchEllipseArc* pOtherEllipseArc = wy3d::SketchEllipseArc::cast(pOtherCurve))
                {
                    numIntPnts = wy3d::intersectCircleEllipse(pArc->getCenter(), pArc->getRadius(),
                        pOtherEllipseArc->getCenter(), pOtherEllipseArc->getMajorAxis(), pOtherEllipseArc->getRadiusRatio(), intPnt1, intPnt2, intPnt3, intPnt4);
                }
                else if (const wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pOtherCurve))
                {                    Handle(Geom2d_BSplineCurve) pBSpline = pSpline->getOccSpline();
                    if (!pBSpline)
                    {
                        assert(false);
                        continue;
                    }
                    wy3d::intersectCircleSpline(pArc->getCenter(), pArc->getRadius(), pBSpline, intPnts);
                }
                else
                {
                    assert(false);
                    continue;
                }
                
                SketchExtendNodeSPtr pOtherNode;
                auto iterOtherNode = _id2Node.find(pOtherCurve->getId());
                if (iterOtherNode != _id2Node.cend())
                {
                    pOtherNode = iterOtherNode->second;
                    assert(pOtherNode);
                }

                // 结果
                intersectedIndices.insert(std::pair<size_t, size_t>(index, i)); // 标记已经求过交点了

                // added by wangyao 2025.06.24 {
                // 专门用来处理与样条曲线求交的逻辑
                for (const wy::Vector2& intPnt : intPnts)
                {
                    pNode->appendKnot(intPnt, pOtherCurve->getId());
                    if (pOtherNode && !pOtherNode->isClosed())
                    {
                        pOtherNode->appendKnot(intPnt, id);
                    }
                }
                // }

                if (0 == numIntPnts) continue;

                if (numIntPnts >= 1)
                {
                    pNode->appendKnot(intPnt1, pOtherCurve->getId());
                    if (pOtherNode && !pOtherNode->isClosed())
                    {
                        pOtherNode->appendKnot(intPnt1, id);
                    }
                }
                if (numIntPnts >= 2)
                {
                    pNode->appendKnot(intPnt2, pOtherCurve->getId());
                    if (pOtherNode && !pOtherNode->isClosed())
                    {
                        pOtherNode->appendKnot(intPnt2, id);
                    }
                }
                if (numIntPnts >= 3)
                {
                    pNode->appendKnot(intPnt3, pOtherCurve->getId());
                    if (pOtherNode && !pOtherNode->isClosed())
                    {
                        pOtherNode->appendKnot(intPnt3, id);
                    }
                }
                if (numIntPnts >= 4)
                {
                    pNode->appendKnot(intPnt4, pOtherCurve->getId());
                    if (pOtherNode && !pOtherNode->isClosed())
                    {
                        pOtherNode->appendKnot(intPnt4, id);
                    }
                }
            }
        }
        // 椭圆弧
        else if (const wy3d::SketchEllipseArc* pEllipseArc = wy3d::SketchEllipseArc::cast(pSketchCurve))
        {
            std::vector<wy::Vector2> intPnts; // 专门用于与样条曲线求交
            unsigned int numIntPnts(0);
            wy::Vector2 intPnt1, intPnt2, intPnt3, intPnt4;
            for (size_t index : candidates)
            {
                // 排除本身
                if (index == i) continue;
                // 之前已经求过交点了
                if (intersectedIndices.find(std::pair<size_t, size_t>(i, index)) != intersectedIndices.cend())
                {
                    continue;
                }
                // 求交
                intPnts.clear();
                numIntPnts = 0;
                const wy3d::SketchCurve* pOtherCurve = curves[index];
                assert(pOtherCurve);
                if (const wy3d::SketchCircle* pOtherCircle = wy3d::SketchCircle::cast(pOtherCurve))
                {
                    numIntPnts = wy3d::intersectCircleEllipse(pOtherCircle->getCenter(), pOtherCircle->getRadius(),
                        pEllipseArc->getCenter(), pEllipseArc->getMajorAxis(), pEllipseArc->getRadiusRatio(),
                        intPnt1, intPnt2, intPnt3, intPnt4);
                }
                else if (const wy3d::SketchArc* pOtherArc = wy3d::SketchArc::cast(pOtherCurve))
                {
                    numIntPnts = wy3d::intersectCircleEllipse(pOtherArc->getCenter(), pOtherArc->getRadius(),
                        pEllipseArc->getCenter(), pEllipseArc->getMajorAxis(), pEllipseArc->getRadiusRatio(),
                        intPnt1, intPnt2, intPnt3, intPnt4);
                }
                else if (const wy3d::SketchEllipse* pOtherEllipse = wy3d::SketchEllipse::cast(pOtherCurve))
                {
                    numIntPnts = wy3d::intersectEllipseEllipse(pEllipseArc->getCenter(), pEllipseArc->getMajorAxis(), pEllipseArc->getRadiusRatio(),
                        pOtherEllipse->getCenter(), pOtherEllipse->getMajorAxis(), pOtherEllipse->getRadiusRatio(),
                        intPnt1, intPnt2, intPnt3, intPnt4);
                }
                else if (const wy3d::SketchEllipseArc* pOtherEllipseArc = wy3d::SketchEllipseArc::cast(pOtherCurve))
                {
                    numIntPnts = wy3d::intersectEllipseEllipse(pEllipseArc->getCenter(), pEllipseArc->getMajorAxis(), pEllipseArc->getRadiusRatio(),
                        pOtherEllipseArc->getCenter(), pOtherEllipseArc->getMajorAxis(), pOtherEllipseArc->getRadiusRatio(),
                        intPnt1, intPnt2, intPnt3, intPnt4);
                }
                else if (const wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pOtherCurve))
                {                    Handle(Geom2d_BSplineCurve) pBSpline = pSpline->getOccSpline();
                    if (!pBSpline)
                    {
                        assert(false);
                        continue;
                    }
                    wy3d::intersectEllipseSpline(pEllipseArc->getCenter(), pEllipseArc->getMajorAxis(), pEllipseArc->getRadiusRatio(),
                        pBSpline, intPnts);
                }
                else
                {
                    assert(false);
                    continue;
                }

                SketchExtendNodeSPtr pOtherNode;
                auto iterOtherNode = _id2Node.find(pOtherCurve->getId());
                if (iterOtherNode != _id2Node.cend())
                {
                    pOtherNode = iterOtherNode->second;
                    assert(pOtherNode);
                }

                // 结果
                intersectedIndices.insert(std::pair<size_t, size_t>(index, i)); // 标记已经求过交点了

                // added by wangyao 2025.06.24 {
                // 专门用来处理与样条曲线求交的逻辑
                for (const wy::Vector2& intPnt : intPnts)
                {
                    pNode->appendKnot(intPnt, pOtherCurve->getId());
                    if (pOtherNode && !pOtherNode->isClosed())
                    {
                        pOtherNode->appendKnot(intPnt, id);
                    }
                }
                // }

                if (0 == numIntPnts) continue;
                if (numIntPnts >= 1)
                {
                    pNode->appendKnot(intPnt1, pOtherCurve->getId());
                    if (pOtherNode && !pOtherNode->isClosed())
                    {
                        pOtherNode->appendKnot(intPnt1, id);
                    }
                }
                if (numIntPnts >= 2)
                {
                    pNode->appendKnot(intPnt2, pOtherCurve->getId());
                    if (pOtherNode && !pOtherNode->isClosed())
                    {
                        pOtherNode->appendKnot(intPnt2, id);
                    }
                }
                if (numIntPnts >= 3)
                {
                    pNode->appendKnot(intPnt3, pOtherCurve->getId());
                    if (pOtherNode && !pOtherNode->isClosed())
                    {
                        pOtherNode->appendKnot(intPnt3, id);
                    }
                }
                if (numIntPnts >= 4)
                {
                    pNode->appendKnot(intPnt4, pOtherCurve->getId());
                    if (pOtherNode && !pOtherNode->isClosed())
                    {
                        pOtherNode->appendKnot(intPnt4, id);
                    }
                }
            }
        }
        else
        {
            assert(false);
            continue;
        }
    }

    // 刷新标记点参数
    for (auto& kvp : _id2Node)
    {
        kvp.second->refresh(pDb);
    }

    return true;
}

SketchExtendNodeSPtr SketchExtendGraph::getNode(const wydb::ElementId& id) const
{
    auto iter = _id2Node.find(id);
    if (iter == _id2Node.cend())
    {
        return nullptr;
    }
    return iter->second;
}

bool SketchExtendGraph::addNode(SketchExtendNodeSPtr pNode)
{
    if (!pNode) return false;
    wydb::ElementId id = pNode->getId();
    auto iter = _id2Node.find(id);
    if (iter != _id2Node.cend())
    {
        return false;
    }
    _id2Node[id] = pNode;
    return true;
}

SketchExtendSegment SketchExtendGraph::pick(const wydb::ElementId& id, const wy::Vector2& position)
{
    auto iter = _id2Node.find(id);
    if (iter == _id2Node.cend())
    {
        // 返回无效的修剪段
        SketchExtendSegment segment;
        return segment;
    }

    return iter->second->pick(this, position);
}