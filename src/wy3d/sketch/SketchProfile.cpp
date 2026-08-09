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

#include <cassert>

#include <wyVector2.h>
#include <wy3dCurveIntersectionUtil.h>
#include <wy3dSketchProfile.h>
#include <wydbDatabase.h>
#include <wy3dSketchEntity.h>
#include <wy3dSketchCurve.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>
#include <wy3dSketchEllipse.h>
#include <wy3dSketchEllipseArc.h>
#include <wy3dSketchSpline.h>
#include <wy3dImpl.h>

NS_WY3D_BEG

SketchProfile::SketchProfile(const Sketch* pSketch, double tol)
    : _pSketch(pSketch), _tol(tol), _isValid(false)
{
    assert(_pSketch);
}

bool SketchProfile::check()
{
    assert(_pSketch);
    if (_pSketch && this->init() && !_faces.empty())
    {
        _isValid = true;
        return true;
    }
    else
    {
        return false;
    }
}

bool SketchProfile::init()
{
    assert(_pSketch);
    const wydb::Database* pDb = _pSketch->getDatabase();
    if (!pDb)
    {
        assert(false);
        _pError = this->newErrorOfUndefined();
        return false;
    }

    // 前置校验器
    if (!this->preValid(pDb))
    {
        if (!_pError)
        {
            assert(false);
            _pError = this->newErrorOfUndefined();
        }
        return false;
    }

    std::vector<const SketchCurve*> curves;
    curves.reserve(50);
    for (auto iter = _pSketch->createIterator(); !iter.isDone(); iter.moveNext())
    {
        wydb::ElementId id = iter.current();
        if (id.isNull())
        {
            assert(false);
            _pError = this->newErrorOfUndefined();
            return false;
        }
        const wydb::Element* pElem = pDb->getElement(id);
        const wy3d::SketchEntity* pSketchEntity = wy3d::SketchEntity::cast(pElem);
        if (!pSketchEntity)
        {
            assert(false);
            _pError = this->newErrorOfUndefined();
            return false;
        }

        const wy3d::SketchCurve* pSketchCurve = wy3d::SketchCurve::cast(pElem);
        if (!pSketchCurve) continue;
        // added by wangyao 2025.03.29 {
        // 过滤掉构造线
        if (pSketchCurve->isConstruction()) continue;
        // 过滤掉中心线
        if (pSketchCurve->isKindOf(wy3d::SketchCenterLine::classInfo())) continue;
        // }
        curves.emplace_back(pSketchCurve);
    }

    // 初始化图
    SketchCurveGraph_Profile curveGraph(curves, _tol);
    if (!curveGraph.isValid())
    {
        _pError = curveGraph.getError();
        return false;
    }

    // 查找闭合环
    if (!curveGraph.findClosedLoops())
    {
        _pError = curveGraph.getError();
        return false;
    }
    const std::vector<SketchCurveGraph_Profile::CurveLoopSPtr>& closedLoops = curveGraph.getClosedLoops();

    // 查找面
    if (closedLoops.empty())
    {
        _pError = std::make_shared<SketchError>();
        _pError->type = ErrorCode::PROFILE_NoClosedLoop;
        return false;
    }
    else if (closedLoops.size() == 1) // 只有一个环那必然是外环
    {
        SketchCurveGraph_Profile::CurveLoopSPtr pCurveLoop = closedLoops.front();
        assert(pCurveLoop);

        // 环
        LoopSPtr pLoop = std::make_shared<Loop>();
        pLoop->isClockWise = pCurveLoop->isClockWise();
        const std::vector<SketchCurveGraph::CurveEntry>& curveEntrys = pCurveLoop->curves();
        pLoop->curves.reserve(curveEntrys.size());
        for (const SketchCurveGraph::CurveEntry& curveEntry : curveEntrys)
        {
            bool orient = (curveEntry.orient == SketchCurveGraph::Orientation::Normal) ? true : false;
            assert(curveEntry.index < curves.size());
            pLoop->curves.push_back(BiCurve(curves[curveEntry.index], orient));
        }

        // 面
        FaceSPtr pFace = std::make_shared<Face>();
        pFace->loops.emplace_back(std::move(pLoop));
        _faces.emplace_back(std::move(pFace));

        return true;
    }
    else
    {
        // TODO 后续时间允许的情况下做以下判断
        // <1>闭合环不能自相交
        // <2>闭合环不能相互相交
        // <3>只有一个外环,其余皆为内环

        assert(closedLoops.size() > 1);

        // 区分外环与内环
        if (!curveGraph.distinguishFaces())
        {
            _pError = curveGraph.getError();
            return false;
        }

        // 面
        const std::vector<SketchCurveGraph_Profile::CurveFaceSPtr>& curveFaces = curveGraph.getFaces();
        _faces.reserve(curveFaces.size());
        for (const SketchCurveGraph_Profile::CurveFaceSPtr& pCurveFace : curveFaces)
        {
            assert(pCurveFace);
            FaceSPtr pFace = std::make_shared<Face>();
            pFace->loops.reserve(pCurveFace->loops.size());
            for (const SketchCurveGraph_Profile::CurveLoopSPtr& pCurveLoop : pCurveFace->loops)
            {
                assert(pCurveLoop);
                LoopSPtr pLoop = std::make_shared<Loop>();
                pLoop->isClockWise = pCurveLoop->isClockWise();

                const std::vector<SketchCurveGraph::CurveEntry>& curveEntrys = pCurveLoop->curves();
                pLoop->curves.reserve(curveEntrys.size());
                for (const SketchCurveGraph::CurveEntry& curveEntry : curveEntrys)
                {
                    bool orient = (curveEntry.orient == SketchCurveGraph::Orientation::Normal) ? true : false;
                    assert(curveEntry.index < curves.size());
                    pLoop->curves.push_back(BiCurve(curves[curveEntry.index], orient));
                }

                pFace->loops.emplace_back(std::move(pLoop));
            }
            _faces.emplace_back(std::move(pFace));
        }

        return true;
    }
}

std::shared_ptr<SketchError> SketchProfile::newErrorOfUndefined() const
{
    std::shared_ptr<SketchError> pError = std::make_shared<SketchError>();
    pError->type = ErrorCode::PROFILE_InvalidProfile;
    return pError;
}

static inline void circleLinearization(
    const wy::Vector2& center, double radius,
    std::vector<wy::Vector2>& vertices, size_t numVertices)
{
    vertices.reserve(numVertices);
    double delta = (wy3d::TWO_PI) / numVertices;
    for (unsigned int i = 0; i < numVertices; ++i)
    {
        vertices.emplace_back(wy::Vector2(
            std::cos(i * delta) * radius + center.x(),
            std::sin(i * delta) * radius + center.y()));
    }
}

static inline void arcLinearization(
    const wy::Vector2& center, double radius, double startAngle, double endAngle,
    std::vector<wy::Vector2>& vertices, size_t numVertices)
{
    startAngle = wy3d::normalizeRadian(startAngle);
    endAngle = wy3d::normalizeRadian(endAngle);
    if (endAngle < startAngle) endAngle += wy3d::TWO_PI;
    double totalAngle = endAngle - startAngle;
    assert(totalAngle >= 0 && totalAngle < wy3d::TWO_PI);

    vertices.reserve(numVertices);
    double delta = totalAngle / (numVertices - 1);
    double angle = startAngle;
    for (size_t i = 0; i < numVertices; ++i)
    {
        angle = startAngle + i * delta;
        vertices.emplace_back(wy::Vector2(
            std::cos(angle) * radius + center.x(),
            std::sin(angle) * radius + center.y()));
    }
}

static inline void arcLinearization_MinMax(
    const wy::Vector2& center, double radius, double startAngle, double endAngle,
    std::vector<wy::Vector2>& vertices, size_t minNum, size_t maxNum)
{
    assert(minNum >= 2);
    assert(maxNum > minNum);
    assert(maxNum < 500);

    startAngle = wy3d::normalizeRadian(startAngle);
    endAngle = wy3d::normalizeRadian(endAngle);
    if (endAngle < startAngle) endAngle += wy3d::TWO_PI;
    double totalAngle = endAngle - startAngle;
    assert(totalAngle >= 0 && totalAngle < wy3d::TWO_PI);

    size_t numVertices = maxNum * totalAngle / wy3d::TWO_PI;
    if (numVertices < minNum)
    {
        numVertices = minNum;
    }

    vertices.reserve(numVertices);
    double delta = totalAngle / (numVertices - 1);
    double angle = startAngle;
    for (size_t i = 0; i < numVertices; ++i)
    {
        angle = startAngle + i * delta;
        vertices.emplace_back(wy::Vector2(
            std::cos(angle) * radius + center.x(),
            std::sin(angle) * radius + center.y()));
    }
}

static inline void ellipseLinearization(
    const wy::Vector2& center, const wy::Vector2& majorAxis, double radiusRatio,
    std::vector<wy::Vector2>& vertices, size_t numVertices)
{
    double majorRadius = majorAxis.length();
    double minorRadius = radiusRatio * majorRadius;

    // 计算长轴与X轴的夹角
    double angle = std::atan2(majorAxis.y(), majorAxis.x());
    double cosAngle = std::cos(angle);
    double sinAngle = std::sin(angle);

    vertices.reserve(numVertices);
    double delta = (wy3d::TWO_PI) / numVertices;
    for (unsigned int i = 0; i < numVertices; ++i)
    {
        // 计算椭圆的每个点（长轴和短轴）
        double x = std::cos(i * delta) * majorRadius;
        double y = std::sin(i * delta) * minorRadius;

        // 使用旋转矩阵旋转点
        double xRot = x * cosAngle - y * sinAngle;
        double yRot = x * sinAngle + y * cosAngle;

        // 计算旋转后的点的位置并添加到顶点数组
        vertices.emplace_back(wy::Vector2(xRot + center.x(), yRot + center.y()));
    }
}

static inline void ellipseArcLinearization(
    const wy::Vector2& center, const wy::Vector2& majorAxis, double radiusRatio,
    double startAngle, double endAngle,
    std::vector<wy::Vector2>& vertices, size_t numVertices)
{
    double majorRadius = majorAxis.length();
    double minorRadius = radiusRatio * majorRadius;

    // 计算长轴与X轴的夹角
    double angle = std::atan2(majorAxis.y(), majorAxis.x());
    double cosAngle = std::cos(angle);
    double sinAngle = std::sin(angle);

    // 椭圆弧的几何角度
    startAngle = wy3d::normalizeRadian(startAngle);
    endAngle = wy3d::normalizeRadian(endAngle);
    if (endAngle < startAngle) endAngle += wy3d::TWO_PI;
    double totalAngle = endAngle - startAngle;

    // 转换为参数角度
    double twoPI = wy3d::PI * 2;
    startAngle = wy3d::ellipsePolarAngleToParametricAngle(startAngle, majorRadius, minorRadius);
    endAngle = wy3d::ellipsePolarAngleToParametricAngle(endAngle, majorRadius, minorRadius);
    if (endAngle < startAngle) endAngle += twoPI;
    totalAngle = endAngle - startAngle;
    assert(totalAngle >= 0 && totalAngle < twoPI);

    // 离散化
    vertices.reserve(numVertices);
    double delta = totalAngle / (numVertices - 1);
    for (size_t i = 0; i < numVertices; ++i)
    {
        // 计算椭圆的每个点（长轴和短轴）
        double x = std::cos((i * delta + startAngle)) * majorRadius;
        double y = std::sin((i * delta + startAngle)) * minorRadius;

        // 使用旋转矩阵旋转点
        double xRot = x * cosAngle - y * sinAngle;
        double yRot = x * sinAngle + y * cosAngle;

        // 计算旋转后的点的位置并添加到顶点数组
        vertices.emplace_back(wy::Vector2(xRot + center.x(), yRot + center.y()));
    }
}

static inline void splineLinearization(
    Handle(Geom2d_BSplineCurve) pBSpline,
    std::vector<wy::Vector2>& vertices, size_t numVertices)
{
    if (!pBSpline)
    {
        assert(false);
        return;
    }

    vertices.reserve(numVertices);
    double uMin = pBSpline->FirstParameter();
    double uMax = pBSpline->LastParameter();
    double du = (uMax - uMin) / (numVertices - 1);
    gp_Pnt2d pnt2d;
    for (size_t i = 0; i < numVertices; ++i)
    {
        double u = uMin + i * du;
        pBSpline->D0(u, pnt2d);
        vertices.emplace_back(wy::Vector2(pnt2d.X(), pnt2d.Y()));
    }
}

void SketchCurveGraph_Profile::CurveLoop::discretize(const std::vector<const SketchCurve*>& curves)
{
    if (!_polygonPnts.empty()) // 已经离散化了
    {
        return;
    }
    if (_curveEntries.empty())
    {
        return;
    }

    for (const CurveEntry& curveEntry : _curveEntries)
    {
        assert(curveEntry.index < curves.size());
        const SketchCurve* pCurve = curves[curveEntry.index];
        assert(pCurve);
        const wyrx::ClassInfo* classInfo = pCurve->getClassInfo();
        if (classInfo == wy3d::SketchLine::classInfo())
        {
            const wy3d::SketchLine* pLine = static_cast<const wy3d::SketchLine*>(pCurve);
            if (curveEntry.orient == Orientation::Normal)
            {
                _polygonPnts.emplace_back(pLine->getStartPoint());
            }
            else
            {
                _polygonPnts.emplace_back(pLine->getEndPoint());
            }
        }
        else if (classInfo == wy3d::SketchArc::classInfo())
        {
            const wy3d::SketchArc* pArc = static_cast<const wy3d::SketchArc*>(pCurve);
            std::vector<wy::Vector2> pnts;
            arcLinearization(pArc->getCenter(), pArc->getRadius(), pArc->getStartAngle(), pArc->getEndAngle(),
                pnts, 100);
            if (pnts.size() == 100)
            {
                if (curveEntry.orient == Orientation::Normal)
                {
                    _polygonPnts.insert(_polygonPnts.cend(), pnts.cbegin(), pnts.cbegin() + 100 - 2);
                }
                else
                {
                    _polygonPnts.insert(_polygonPnts.cend(), pnts.crbegin(), pnts.crbegin() + 100 - 2);
                }
            }
            else
            {
                assert(false);
                continue;
            }
        }
        else if (classInfo == wy3d::SketchEllipse::classInfo())
        {
            assert(_curveEntries.size() == 1);
            const wy3d::SketchEllipse* pEllipse = static_cast<const wy3d::SketchEllipse*>(pCurve);
            std::vector<wy::Vector2> pnts;
            ellipseLinearization(
                pEllipse->getCenter(), pEllipse->getMajorAxis(), pEllipse->getRadiusRatio(),
                pnts, 200);
            _polygonPnts.swap(pnts);
        }
        else if (classInfo == wy3d::SketchEllipseArc::classInfo())
        {
            const wy3d::SketchEllipseArc* pEllipseArc = static_cast<const wy3d::SketchEllipseArc*>(pCurve);
            std::vector<wy::Vector2> pnts;
            ellipseArcLinearization(pEllipseArc->getCenter(), pEllipseArc->getMajorAxis(), pEllipseArc->getRadiusRatio(),
                pEllipseArc->getStartAngle(), pEllipseArc->getEndAngle(), pnts, 100);
            if (pnts.size() == 100)
            {
                if (curveEntry.orient == Orientation::Normal)
                {
                    _polygonPnts.insert(_polygonPnts.cend(), pnts.cbegin(), pnts.cbegin() + 100 - 2);
                }
                else
                {
                    _polygonPnts.insert(_polygonPnts.cend(), pnts.crbegin(), pnts.crbegin() + 100 - 2);
                }
            }
            else
            {
                assert(false);
                continue;
            }
        }
        else if (classInfo == wy3d::SketchSpline::classInfo())
        {
            const wy3d::SketchSpline* pSpline = static_cast<const wy3d::SketchSpline*>(pCurve);
            Handle(Geom2d_BSplineCurve) pBSpline = pSpline->getOccSpline();
            if (!pBSpline)
            {
                assert(false);
                continue;
            }
            std::vector<wy::Vector2> pnts;
            splineLinearization(pBSpline, pnts, 100);
            if (pnts.size() == 100)
            {
                if (curveEntry.orient == Orientation::Normal)
                {
                    _polygonPnts.insert(_polygonPnts.cend(), pnts.cbegin(), pnts.cbegin() + 100 - 2);
                }
                else
                {
                    _polygonPnts.insert(_polygonPnts.cend(), pnts.crbegin(), pnts.crbegin() + 100 - 2);
                }
            }
            else
            {
                assert(false);
                continue;
            }
        }
        else
        {
            assert(false);
            continue;
        }
    }
}

SketchCurveGraph_Profile::SketchCurveGraph_Profile(const std::vector<const SketchCurve*>& curves, double tol)
    : SketchCurveGraph(curves, tol)
{
}

bool SketchCurveGraph_Profile::findClosedLoops()
{
    if (!_isValid)
    {
        assert(_pError);
        return false;
    }

    std::list<std::shared_ptr<CurveLoop>> closedLoops;
    assert(_curves.size() == _endPointAdjacency.size());
    assert(_curves.size() == _startPointAdjacency.size());
    size_t n = _curves.size();

    // 首先判断是否是有效
    std::vector<bool> degenerated(n, false);
    for (size_t i = 0; i < n; ++i)
    {
        const SketchCurve* pCurve = _curves[i];
        assert(pCurve);
        // 忽略退化的曲线
        if (pCurve->isDegenerate(_tol))
        {
            degenerated[i] = true;
            continue;
        }

        // 闭合曲线
        if (pCurve->isClosed())
        {
            if (_endPointAdjacency[i].size() != 0)
            {
                _pError = this->newError(ErrorCode::PROFILE_ClosedCurveIntersectWithOtherCurves, 
                    pCurve->getId(), _endPointAdjacency[i]);
                return false;
            }
            if (_startPointAdjacency[i].size() != 0)
            {
                _pError = this->newError(ErrorCode::PROFILE_ClosedCurveIntersectWithOtherCurves,
                    pCurve->getId(), _startPointAdjacency[i]);
                return false;
            }
        }
        else // 非闭合曲线
        {
            if (_endPointAdjacency[i].size() != 1)
            {
                if (_endPointAdjacency[i].size() == 0)
                {
                    _pError = this->newError(ErrorCode::PROFILE_ExistCurveNotInClosedLoop,
                        pCurve->getId(), {});
                }
                else
                {
                    _pError = this->newError(ErrorCode::SKETCH_MoreThanTwoCurvesAtOneEndPoint,
                        pCurve->getId(), _endPointAdjacency[i]);
                }
                return false;
            }
            if (_startPointAdjacency[i].size() != 1)
            {
                if (_startPointAdjacency[i].size() == 0)
                {
                    _pError = this->newError(ErrorCode::PROFILE_ExistCurveNotInClosedLoop,
                        pCurve->getId(), {});
                }
                else
                {
                    _pError = this->newError(ErrorCode::SKETCH_MoreThanTwoCurvesAtOneEndPoint,
                        pCurve->getId(), _startPointAdjacency[i]);
                }
                return false;
            }
        }
    }

    // 首先处理本身闭合的曲线
    std::vector<bool> used(n, false);
    for (size_t i = 0; i < _curves.size(); ++i)
    {
        // 跳过退化的曲线
        if (degenerated[i])
        {
            continue;
        }

        // 对于本身闭合的曲线，直接加入闭合环结果，并标记为已使用
        assert(_curves[i]);
        if (_curves[i]->isClosed())
        {
            std::shared_ptr<CurveLoop> pCurveLoop = std::make_shared<CurveLoop>();
            pCurveLoop->push_back(CurveEntry{ i, Orientation::Normal });
            closedLoops.emplace_back(std::move(pCurveLoop));
            used[i] = true;
        }
    }

    // 处理本身不闭合的曲线
    for (size_t i = 0; i < _curves.size(); ++i)
    {
        // 跳过退化的曲线
        if (degenerated[i])
        {
            continue;
        }

        // 跳过已经使用的曲线
        if (used[i])
        {
            continue;
        }

        std::shared_ptr<CurveLoop> pCurveLoop = std::make_shared<CurveLoop>();
        pCurveLoop->reserve(10);
        pCurveLoop->push_back(CurveEntry{ i, Orientation::Normal });
        used[i] = true;
        if (!dfsFindCycle(used, degenerated, pCurveLoop, closedLoops))
        {
            _pError = this->newError(ErrorCode::PROFILE_ExistCurveNotInClosedLoop,
                _curves[i]->getId(), {});
            closedLoops.clear();
            return false;
        }
    }

    _closedLoops.reserve(closedLoops.size());
    for (CurveLoopSPtr& pCurveLoop : closedLoops)
    {
        _closedLoops.emplace_back(std::move(pCurveLoop));
    }
    return true;
}

bool SketchCurveGraph_Profile::findLoops()
{
    if (!_isValid)
    {
        assert(_pError);
        return false;
    }

    std::list<std::shared_ptr<CurveLoop>> loops;
    assert(_curves.size() == _endPointAdjacency.size());
    assert(_curves.size() == _startPointAdjacency.size());
    size_t n = _curves.size();

    // Validate topology and pre-compute degree for each curve.
    // Degree: number of non-degenerate neighbor curves (sum over both endpoints).
    //   0  → isolated or self-closed (no connection at either end)
    //   1  → open end (exactly one side attached, the other free)
    //   2  → middle of a chain, or part of a closed loop
    std::vector<bool> degenerated(n, false);
    std::vector<int> degree(n, 0);
    for (size_t i = 0; i < n; ++i)
    {
        const SketchCurve* pCurve = _curves[i];
        assert(pCurve);
        if (pCurve->isDegenerate(_tol))
        {
            degenerated[i] = true;
            continue;
        }

        if (pCurve->isClosed())
        {
            if (_endPointAdjacency[i].size() != 0)
            {
                _pError = this->newError(ErrorCode::PROFILE_ClosedCurveIntersectWithOtherCurves,
                    _curves[i]->getId(), _endPointAdjacency[i]);
                return false;
            }
            if (_startPointAdjacency[i].size() != 0)
            {
                _pError = this->newError(ErrorCode::PROFILE_ClosedCurveIntersectWithOtherCurves,
                    _curves[i]->getId(), _startPointAdjacency[i]);
                return false;
            }
            degree[i] = 0;
        }
        else
        {
            int d = 0;

            const std::vector<CurveEntry>& endAdjs = _endPointAdjacency[i];
            if (endAdjs.size() > 1)
            {
                _pError = this->newError(ErrorCode::SKETCH_MoreThanTwoCurvesAtOneEndPoint,
                    pCurve->getId(), endAdjs);
                return false;
            }
            else if (endAdjs.size() == 1)
            {
                if (!degenerated[endAdjs[0].index]) ++d;
            }

            const std::vector<CurveEntry>& startAdjs = _startPointAdjacency[i];
            if (startAdjs.size() > 1)
            {
                _pError = this->newError(ErrorCode::SKETCH_MoreThanTwoCurvesAtOneEndPoint,
                    _curves[i]->getId(), _startPointAdjacency[i]);
                return false;
            }
            else if (startAdjs.size() == 1)
            {
                if (!degenerated[startAdjs[0].index]) ++d;
            }

            degree[i] = d;
        }
    }

    std::vector<bool> used(n, false);
    for (size_t i = 0; i < n; ++i)
    {
        if (degenerated[i]) continue;
        assert(_curves[i]);
        if (_curves[i]->isClosed())
        {
            std::shared_ptr<CurveLoop> pCurveLoop = std::make_shared<CurveLoop>();
            pCurveLoop->push_back(CurveEntry{ i, Orientation::Normal });
            loops.emplace_back(std::move(pCurveLoop));
            used[i] = true;
        }
    }

    for (size_t i = 0; i < n; ++i)
    {
        if (degenerated[i] || used[i]) continue;
        if (degree[i] != 1) continue;

        size_t cur = i;
        std::shared_ptr<CurveLoop> pLoop = std::make_shared<CurveLoop>();
        Orientation orient = Orientation::Normal;
        {
            bool hasEnd = false, hasStart = false;
            for (const auto& e : _endPointAdjacency[cur])
                if (!degenerated[e.index] && !used[e.index]) { hasEnd = true; break; }
            for (const auto& e : _startPointAdjacency[cur])
                if (!degenerated[e.index] && !used[e.index]) { hasStart = true; break; }
            if (hasStart)
            {
                orient = Orientation::Reversed;
            }
            else if (hasEnd)
            {
                orient = Orientation::Normal;
            }
            else
            {
                assert(false);
                continue;
            }
        }
        pLoop->push_back(CurveEntry{ cur, orient });
        used[cur] = true;

        for (;;)
        {
            const std::vector<CurveEntry>& adj = (pLoop->curves().back().orient == Orientation::Normal)
                ? _endPointAdjacency[cur] : _startPointAdjacency[cur];
            const CurveEntry* pNext = nullptr;
            for (const CurveEntry& entry : adj)
                if (!degenerated[entry.index] && !used[entry.index]) { pNext = &entry; break; }
            if (!pNext) break;
            pLoop->push_back(*pNext);
            cur = pNext->index;
            used[cur] = true;
        }
        loops.emplace_back(std::move(pLoop));
    }

    for (size_t i = 0; i < n; ++i)
    {
        if (degenerated[i] || used[i]) continue;
        if (degree[i] != 0) continue;
        auto pLoop = std::make_shared<CurveLoop>();
        pLoop->push_back(CurveEntry{ i, Orientation::Normal });
        loops.emplace_back(std::move(pLoop));
        used[i] = true;
    }

    for (size_t i = 0; i < n; ++i)
    {
        if (degenerated[i] || used[i]) continue;
        auto pLoop = std::make_shared<CurveLoop>();
        pLoop->push_back(CurveEntry{ i, Orientation::Normal });
        used[i] = true;
        std::list<std::shared_ptr<CurveLoop>> closedLoops;
        if (!dfsFindCycle(used, degenerated, pLoop, closedLoops))
        {
            assert(false);
            _pError = std::make_shared<SketchError>();
            _pError->type = ErrorCode::PROFILE_InvalidProfile;
            return false;
        }
        loops.splice(loops.end(), closedLoops);
    }

    _loops.clear();
    _loops.reserve(loops.size());
    for (std::shared_ptr<CurveLoop> loop : loops)
        _loops.emplace_back(std::move(loop));
    return true;
}

// 使用非递归 DFS 在邻接图中查找闭合环（不允许重复使用曲线）。
bool SketchCurveGraph_Profile::dfsFindCycle(
    std::vector<bool>& used,
    const std::vector<bool>& degenerated,
    std::shared_ptr<CurveLoop> pCurveLoop,
    std::list<std::shared_ptr<CurveLoop>>& closedLoops)
{
    assert(pCurveLoop);
    assert(pCurveLoop->curves().size() == 1);
    // 定义DFS栈中的状态
    struct DFSFrame {
        size_t nodeIndex;      // 曲线索引,即pCurveLoop->curves().back().index
        size_t nextChildIndex; // 下一个待尝试的邻接列表下标
    };
    std::vector<DFSFrame> stack;
    size_t initialIndex = pCurveLoop->curves().back().index;
    stack.push_back({ initialIndex, 0 });

    auto getAdjacencyChildren = [this, pCurveLoop](size_t index) -> const std::vector<CurveEntry>&
    {
        if (pCurveLoop->curves().back().orient == Orientation::Normal)
        {
            return _endPointAdjacency[index];
        }
        else
        {
            return _startPointAdjacency[index];
        }
    };

    while (!stack.empty())
    {
        // 检查是否形成闭合环
        if (pCurveLoop->curves().size() > 1 &&
            nearlyEqual(effectiveEndPoint(pCurveLoop->curves().back()), effectiveStartPoint(pCurveLoop->curves().front())))
        {
            closedLoops.emplace_back(std::move(pCurveLoop));
            return true;
        }

        DFSFrame& topFrame = stack.back();
        const std::vector<CurveEntry>& children = getAdjacencyChildren(topFrame.nodeIndex);

        // 如果当前节点还有未尝试的邻接曲线,则取出下一个候选项
        if (topFrame.nextChildIndex < children.size())
        {
            const CurveEntry& candidate = children[topFrame.nextChildIndex];
            topFrame.nextChildIndex++; // 为下次尝试更新候选下标

            // 忽略退化的曲线
            if (degenerated[candidate.index])
            {
                continue;
            }

            // 检查该候选曲线是否未被使用,并且连接条件满足
            if (!used[candidate.index] &&
                nearlyEqual(effectiveEndPoint(pCurveLoop->curves().back()), effectiveStartPoint(candidate)))
            {
                used[candidate.index] = true;
                pCurveLoop->push_back(candidate);
                // 新增候选曲线后,压入新状态,其邻接列表待从0开始
                stack.push_back({ candidate.index, 0 });
                continue; // 继续下一次循环,从新状态出发
            }
        }
        else
        {
            // 当前节点的所有邻接曲线均已尝试,则回溯
            // 如果当前栈中只有初始状态,则说明起始曲线已穷尽所有可能,退出DFS
            if (stack.size() == 1)
            {
                break;
            }
            // 弹出当前DFS状态,并将当前路径中最后添加的曲线恢复为未使用状态
            stack.pop_back();
            CurveEntry lastEntry = pCurveLoop->curves().back();
            pCurveLoop->pop_back();
            used[lastEntry.index] = false;
        }
    }

    return false;
}

bool SketchCurveGraph_Profile::distinguishFaces()
{
    if (!_isValid)
    {
        assert(_pError);
        return false;
    }

    assert(_curves.size() == _endPointAdjacency.size());
    assert(_curves.size() == _startPointAdjacency.size());
    size_t n = _curves.size();
    size_t numClosedLoops = _closedLoops.size();

    // 求出所有曲线的外包围盒 
    _curveBBoxs.resize(n);
    for (size_t i = 0; i < n; ++i)
    {
        assert(_curves[i]);
        _curveBBoxs[i] = _curves[i]->getBoundingBox();
    }

    // 计算所有Loop的外包围盒
    wy3d::BoundingBox2 totalBoundingBox;
    for (std::shared_ptr<SketchCurveGraph_Profile::CurveLoop>& pCurveLoop : _closedLoops)
    {
        assert(pCurveLoop);
        wy3d::BoundingBox2 bbox;
        for (const CurveEntry& curveEntry : pCurveLoop->curves())
        {
            assert(curveEntry.index < n);
            bbox.merge(_curveBBoxs[curveEntry.index]);
        }
        pCurveLoop->setBoundingBox(bbox);
        totalBoundingBox.merge(bbox);
    }
    _loopsTotalBBox = totalBoundingBox;

    // 计算Loop的有向面积
    for (size_t i = 0; i < numClosedLoops; ++i)
    {
        const std::shared_ptr<SketchCurveGraph_Profile::CurveLoop>& pCurveLoop = _closedLoops[i];
        assert(pCurveLoop);
        assert(pCurveLoop->curves().size() >= 1);

        double signedArea = this->computeSideArea(*pCurveLoop);
        pCurveLoop->setSignedArea(signedArea);
        pCurveLoop->setIsClockWise((signedArea < 0));
    }

    // 鉴别外环与内环
    size_t outerLoopIndex(-1);
    if (0) // 根据外包围盒
    {
        for (size_t i = 0; i < numClosedLoops; ++i)
        {
            CurveLoopSPtr& pCurveLoop = _closedLoops[i];
            assert(pCurveLoop);
            if (pCurveLoop->getBoundingBox() == _loopsTotalBBox)
            {
                if (outerLoopIndex == size_t(-1))
                {
                    outerLoopIndex = i;
                }
                else
                {
                    return false;
                }
            }
        }
    }
    else // 根据面积大小
    {
        // 根据面积大小逆序排序
        std::sort(_closedLoops.begin(), _closedLoops.end(), [](
            const CurveLoopSPtr& a, const CurveLoopSPtr& b) {
                return a->getArea() > b->getArea();
            });

        // 构建空间索引树
        RTree<size_t, double, 2> rtree;
        for (size_t i = 0; i < numClosedLoops; ++i)
        {
            CurveLoopSPtr& pCurveLoop = _closedLoops[i];
            assert(pCurveLoop);
            const wy3d::BoundingBox2& bbox = pCurveLoop->getBoundingBox();
            double min[2] = { bbox.min().x(), bbox.min().y() };
            double max[2] = { bbox.max().x(), bbox.max().y() };
            rtree.Insert(min, max, i);
        }

        // 找出所有的面
        _faces.clear();
        _faces.reserve(20);
        std::vector<bool> visited(numClosedLoops, false);
        while (true)
        {
            CurveFaceSPtr pFace = this->extractFace(_closedLoops, rtree, visited);
            if (!pFace)
            {
                break;
            }
            _faces.emplace_back(pFace);
        }

        return true;
    }

}

SketchCurveGraph_Profile::CurveFaceSPtr SketchCurveGraph_Profile::extractFace(
    const std::vector<CurveLoopSPtr>& closedLoops,
    const RTree<size_t, double, 2>& rtree,
    std::vector<bool>& visited) const
{
    assert(closedLoops.size() == visited.size());
    size_t num = closedLoops.size();

    size_t startIndex(-1);
    for (size_t i = 0; i < num; ++i)
    {
        if (!visited[i])
        {
            startIndex = i;
            break;
        }
    }
    if (-1 == startIndex) // 所有的面都提取完了
    {
        return nullptr;
    }

    visited[startIndex] = true;

    // 外环
    const CurveLoopSPtr& pOuterLoop = closedLoops[startIndex];
    assert(pOuterLoop);
    wy3d::BoundingBox2 bboxOuter = pOuterLoop->getBoundingBox();
    double min[2] = { bboxOuter.min().x(), bboxOuter.min().y() };
    double max[2] = { bboxOuter.max().x(), bboxOuter.max().y() };

    // 查找可能的内环
    std::list<size_t> possibleInnerLoops;
    rtree.Search(min, max, [&possibleInnerLoops](const size_t& index) {
        possibleInnerLoops.emplace_back(index);
        return true;
    });

    // 遍历进一步确定是否是内环
    std::vector<size_t> indices;
    indices.reserve(possibleInnerLoops.size());
    bboxOuter.expand(wy3d::TOL); // 外环包围盒相应地向外扩张一点
    for (size_t index : possibleInnerLoops)
    {
        if (index == startIndex)
        {
            continue; // 排除外环自身
        }

        const CurveLoopSPtr& pLoop = closedLoops[index];
        assert(pLoop);
        const wy3d::BoundingBox2& bbox = pLoop->getBoundingBox();

        // 包围盒不完全包含则排除掉
        if (!bboxOuter.contains(bbox))
        {
            continue;
        }

        // 进一步判断是否完全包含
        if (!this->isCurveLoopContains(pOuterLoop, pLoop, wy3d::TOL))
        {
            continue;
        }

        indices.emplace_back(index);
        visited[index] = true;
    }

    // 构建面
    CurveFaceSPtr pFace = std::make_shared<CurveFace>();
    pFace->loops.reserve(indices.size() + 1);
    pFace->loops.emplace_back(pOuterLoop);
    for (size_t index : indices)
    {
        pFace->loops.emplace_back(closedLoops[index]);
    }
    return pFace;
}

// 判断圆是否完全包含环
bool SketchCurveGraph_Profile::isCircleContains(
    const wy3d::SketchCircle* pOuterCircle,
    const CurveLoopSPtr& pLoop,
    double tol) const
{
    assert(pOuterCircle);
    wy::Vector2 center = pOuterCircle->getCenter();
    double radius = pOuterCircle->getRadius();

    const std::vector<CurveEntry>& curves = pLoop->curves();
    for (const CurveEntry& curveEntry : curves)
    {
        assert(curveEntry.index < _curves.size());
        const wy3d::SketchCurve* pSketchCurve = _curves[curveEntry.index];
        assert(pSketchCurve);
        const wyrx::ClassInfo* classInfo = pSketchCurve->getClassInfo();
        if (classInfo == wy3d::SketchLine::classInfo())
        {
            wy::Vector2 pnt = (curveEntry.orient == Orientation::Normal) ?
                pSketchCurve->getStartPoint() : pSketchCurve->getEndPoint();
            if ((pnt - center).length() > radius + tol)
            {
                return false;
            }
        }
        else if (classInfo == wy3d::SketchCircle::classInfo())
        {
            const wy3d::SketchCircle* pCircle = static_cast<const wy3d::SketchCircle*>(pSketchCurve);
            double distance = (pCircle->getCenter() - center).length();
            if (distance + pCircle->getRadius() > radius + tol) // 两个圆心之间的距离加上小圆的半径大于大圆的半径
            {
                return false;
            }
        }
        else if (classInfo == wy3d::SketchArc::classInfo())
        {
            const wy3d::SketchArc* pArc = static_cast<const wy3d::SketchArc*>(pSketchCurve);

            // 判断圆弧所在的圆是否完全在圆内
            double distance = (pArc->getCenter() - center).length();
            if (distance + pArc->getRadius() <= radius + tol) // 圆弧所在的圆完全在圆内
            {
                continue;
            }

            // 判断两个端点是否在圆内
            if ((pArc->getStartPoint() - center).length() > radius + tol ||
                (pArc->getEndPoint() - center).length() > radius + tol)
            {
                return false;
            }

            // 端点都在圆内则判断小圆距离大圆最远点
            double phi = wy::Vector2::rotationAngle(pArc->getCenter() - center, wy::Vector2::kXAxis);
            if (wy3d::isAngleInArc(phi, pArc->getStartAngle(), pArc->getEndAngle(), tol))
            {
                return false;
            }
        }
        else if (classInfo == wy3d::SketchEllipse::classInfo())
        {
            const wy3d::SketchEllipse* pEllipse = static_cast<const wy3d::SketchEllipse*>(pSketchCurve);

            // 判断外包围盒上的点是否完全在圆内
            bool isBBoxInCircle(true);
            const wy3d::BoundingBox2& bbox = _curveBBoxs[curveEntry.index];
            wy::Vector2 bboxPnts[4] = { bbox.min(), bbox.max(),
                wy::Vector2(bbox.min().x(), bbox.max().y()),
                wy::Vector2(bbox.max().x(), bbox.min().y())};
            for (unsigned int i = 0; i < 4; ++i)
            {
                if ((bboxPnts[i] - center).length() > radius + tol)
                {
                    isBBoxInCircle = false;
                    break;
                }
            }
            if (isBBoxInCircle) continue; // 外包围盒完全在圆内

            // 离散化椭圆,判断这些离散点是否在圆内
            wy::Vector2 ellipseCenter = pEllipse->getCenter();
            wy::Vector2 majorAxis = pEllipse->getMajorAxis();
            double majorRadius = majorAxis.length();
            double minorRadius = pEllipse->getRadiusRatio() * majorRadius;

            // 计算长轴与X轴的夹角
            double angle = std::atan2(majorAxis.y(), majorAxis.x());
            double cosAngle = std::cos(angle);
            double sinAngle = std::sin(angle);

            double delta = (wy3d::TWO_PI) / 100;
            for (unsigned int i = 0; i < 100; ++i)
            {
                // 计算椭圆的每个点（长轴和短轴）
                double x = std::cos(i * delta) * majorRadius;
                double y = std::sin(i * delta) * minorRadius;

                // 使用旋转矩阵旋转点
                double xRot = x * cosAngle - y * sinAngle;
                double yRot = x * sinAngle + y * cosAngle;

                // 计算旋转后的点的位置并判断是否在圆内
                wy::Vector2 pnt(xRot + ellipseCenter.x(), yRot + ellipseCenter.y());
                if ((pnt - center).length() > radius + tol)
                {
                    return false;
                }
            }
        }
        else if (classInfo == wy3d::SketchEllipseArc::classInfo())
        {
            const wy3d::SketchEllipseArc* pEllipseArc = static_cast<const wy3d::SketchEllipseArc*>(pSketchCurve);

            // 判断外包围盒上的点是否完全在圆内
            bool isBBoxInCircle(true);
            const wy3d::BoundingBox2& bbox = _curveBBoxs[curveEntry.index];
            wy::Vector2 bboxPnts[4] = { bbox.min(), bbox.max(),
                wy::Vector2(bbox.min().x(), bbox.max().y()),
                wy::Vector2(bbox.max().x(), bbox.min().y()) };
            for (unsigned int i = 0; i < 4; ++i)
            {
                if ((bboxPnts[i] - center).length() > radius + tol)
                {
                    isBBoxInCircle = false;
                    break;
                }
            }
            if (isBBoxInCircle) continue; // 外包围盒完全在圆内

            // 离散化椭圆弧,判断这些离散点是否在圆内
            wy::Vector2 ellipseCenter = pEllipseArc->getCenter();
            wy::Vector2 majorAxis = pEllipseArc->getMajorAxis();
            double majorRadius = majorAxis.length();
            double minorRadius = pEllipseArc->getRadiusRatio() * majorRadius;

            // 计算长轴与X轴的夹角
            double angle = std::atan2(majorAxis.y(), majorAxis.x());
            double cosAngle = std::cos(angle);
            double sinAngle = std::sin(angle);

            // 椭圆弧的几何角度
            double startAngle = wy3d::normalizeRadian(pEllipseArc->getStartAngle());
            double endAngle = wy3d::normalizeRadian(pEllipseArc->getEndAngle());
            if (endAngle < startAngle) endAngle += wy3d::TWO_PI;
            double totalAngle = endAngle - startAngle;

            // 转换为参数角度
            startAngle = wy3d::ellipsePolarAngleToParametricAngle(startAngle, majorRadius, minorRadius);
            endAngle = wy3d::ellipsePolarAngleToParametricAngle(endAngle, majorRadius, minorRadius);
            if (endAngle < startAngle) endAngle += wy3d::TWO_PI;
            totalAngle = endAngle - startAngle;
            assert(totalAngle >= 0 && totalAngle < wy3d::TWO_PI);

            // 离散化
            double delta = totalAngle / 99;
            for (size_t i = 0; i < 100; ++i)
            {
                // 计算椭圆的每个点（长轴和短轴）
                double x = std::cos((i * delta + startAngle)) * majorRadius;
                double y = std::sin((i * delta + startAngle)) * minorRadius;

                // 使用旋转矩阵旋转点
                double xRot = x * cosAngle - y * sinAngle;
                double yRot = x * sinAngle + y * cosAngle;

                // 计算旋转后的点的位置并判断是否在圆内
                wy::Vector2 pnt(xRot + ellipseCenter.x(), yRot + ellipseCenter.y());
                if ((pnt - center).length() > radius + tol)
                {
                    return false;
                }
            }
        }
        else if (classInfo == wy3d::SketchSpline::classInfo())
        {
            const wy3d::SketchSpline* pSpline = static_cast<const wy3d::SketchSpline*>(pSketchCurve);

            // 判断外包围盒上的点是否完全在圆内
            bool isBBoxInCircle(true);
            const wy3d::BoundingBox2& bbox = _curveBBoxs[curveEntry.index];
            wy::Vector2 bboxPnts[4] = { bbox.min(), bbox.max(),
                wy::Vector2(bbox.min().x(), bbox.max().y()),
                wy::Vector2(bbox.max().x(), bbox.min().y()) };
            for (unsigned int i = 0; i < 4; ++i)
            {
                if ((bboxPnts[i] - center).length() > radius + tol)
                {
                    isBBoxInCircle = false;
                    break;
                }
            }
            if (isBBoxInCircle) continue; // 外包围盒完全在圆内

            // 离散化样条曲线,判断这些离散点是否在圆内
            Handle(Geom2d_BSplineCurve) pBSpline = pSpline->getOccSpline();
            std::vector<wy::Vector2> pnts;
            splineLinearization(pBSpline, pnts, 100);
            for (const wy::Vector2& pnt : pnts)
            {
                if ((pnt - center).length() > radius + tol)
                {
                    return false;
                }
            }
        }
        else
        {
            assert(false);
            return false;
        }
    }

    return true;
}

// 判断椭圆是否完全包含环
bool SketchCurveGraph_Profile::isEllipseContains(
    const CurveLoopSPtr& pOuterLoop,
    const wy3d::SketchEllipse* pOuterEllipse,
    const CurveLoopSPtr& pLoop,
    double tol) const
{
    assert(pOuterLoop);
    assert(pOuterEllipse);
    const std::vector<CurveEntry>& curves = pLoop->curves();
    if (curves.empty())
    {
        assert(false);
        return false;
    }

    wy::Vector2 center = pOuterEllipse->getCenter();
    double majorRadius = pOuterEllipse->getMajorRadius();
    double minorRadius = majorRadius * pOuterEllipse->getRadiusRatio();
    if (minorRadius > majorRadius) std::swap(majorRadius, minorRadius);

    // 初始判断一下
    std::set<size_t> excludes;
    for (const CurveEntry& curveEntry : curves) 
    {
        assert(curveEntry.index < _curves.size());
        const wy3d::SketchCurve* pSketchCurve = _curves[curveEntry.index];
        assert(pSketchCurve);
        const wyrx::ClassInfo* classInfo = pSketchCurve->getClassInfo();
        if (classInfo == wy3d::SketchLine::classInfo())
        {
            wy::Vector2 pnt = (curveEntry.orient == Orientation::Normal) ?
                pSketchCurve->getStartPoint() : pSketchCurve->getEndPoint();
            if ((pnt - center).length() <= minorRadius + tol)
            {
                excludes.insert(curveEntry.index);
                continue;
            }
        }
        else if (classInfo == wy3d::SketchCircle::classInfo())
        {
            const wy3d::SketchCircle* pCircle = static_cast<const wy3d::SketchCircle*>(pSketchCurve);
            double distance = (pCircle->getCenter() - center).length();
            if (distance + pCircle->getRadius() <= minorRadius + tol)
            {
                excludes.insert(curveEntry.index);
                continue;
            }
        }
        else if (classInfo == wy3d::SketchArc::classInfo())
        {
            const wy3d::SketchArc* pArc = static_cast<const wy3d::SketchArc*>(pSketchCurve);
            double distance = (pArc->getCenter() - center).length();
            if (distance + pArc->getRadius() <= minorRadius + tol)
            {
                excludes.insert(curveEntry.index);
                continue;
            }
        }
        else if (classInfo == wy3d::SketchEllipse::classInfo())
        {
            const wy3d::SketchEllipse* pEllipse = static_cast<const wy3d::SketchEllipse*>(pSketchCurve);
            double distance = (pEllipse->getCenter() - center).length();
            if (distance + pEllipse->getMajorRadius() <= minorRadius + tol)
            {
                excludes.insert(curveEntry.index);
                continue;
            }
        }
        else if (classInfo == wy3d::SketchEllipseArc::classInfo())
        {
            const wy3d::SketchEllipseArc* pEllipseArc = static_cast<const wy3d::SketchEllipseArc*>(pSketchCurve);
            double distance = (pEllipseArc->getCenter() - center).length();
            if (distance + pEllipseArc->getMajorRadius() <= minorRadius + tol)
            {
                excludes.insert(curveEntry.index);
                continue;
            }
        }
        else if (classInfo == wy3d::SketchSpline::classInfo())
        {
            // 样条不做提前排除,留给后续离散化判断
        }
        else
        {
            assert(false);
            return false;
        }
    }

    // 离散化椭圆
    pOuterLoop->discretize(_curves);
    const std::vector<wy::Vector2>& outerLoopPnts = pOuterLoop->getPoints();
    if (outerLoopPnts.size() < 3)
    {
        assert(false);
        return false;
    }

    // 遍历判断
    // 这种判断方法肯定是不严密的,但是能解决大部分的场景
    for (const CurveEntry& curveEntry : curves)
    {
        assert(curveEntry.index < _curves.size());
        if (excludes.find(curveEntry.index) != excludes.cend())
        {
            continue;
        }
        const wy3d::SketchCurve* pSketchCurve = _curves[curveEntry.index];
        assert(pSketchCurve);
        wy::Vector2 pnt = (curveEntry.orient == Orientation::Normal) ?
            pSketchCurve->getStartPoint() : pSketchCurve->getEndPoint();
        if (-1 == this->isCurveLoopContainPoint(outerLoopPnts, pnt, tol)) // 点在环外
        {
            return false;
        }

        const wyrx::ClassInfo* classInfo = pSketchCurve->getClassInfo();
        std::vector<wy::Vector2> pnts;
        if (classInfo == wy3d::SketchCircle::classInfo())
        {
            const wy3d::SketchCircle* pCircle = static_cast<const wy3d::SketchCircle*>(pSketchCurve);
            circleLinearization(pCircle->getCenter(), pCircle->getRadius(), pnts, 20);
        }
        else if (classInfo == wy3d::SketchArc::classInfo())
        {
            const wy3d::SketchArc* pArc = static_cast<const wy3d::SketchArc*>(pSketchCurve);
            arcLinearization(pArc->getCenter(), pArc->getRadius(), pArc->getStartAngle(), pArc->getEndAngle(),
                pnts, 20);
        }
        else if (classInfo == wy3d::SketchEllipse::classInfo())
        {
            const wy3d::SketchEllipse* pEllipse = static_cast<const wy3d::SketchEllipse*>(pSketchCurve);
            ellipseLinearization(pEllipse->getCenter(), pEllipse->getMajorAxis(), pEllipse->getRadiusRatio(),
                pnts, 20);
        }
        else if (classInfo == wy3d::SketchEllipseArc::classInfo())
        {
            const wy3d::SketchEllipseArc* pEllipseArc = static_cast<const wy3d::SketchEllipseArc*>(pSketchCurve);
            ellipseArcLinearization(pEllipseArc->getCenter(), pEllipseArc->getMajorAxis(), pEllipseArc->getRadiusRatio(),
                pEllipseArc->getStartAngle(), pEllipseArc->getEndAngle(), pnts, 20);
        }
        else if (classInfo == wy3d::SketchSpline::classInfo())
        {
            const wy3d::SketchSpline* pSpline = static_cast<const wy3d::SketchSpline*>(pSketchCurve);
            Handle(Geom2d_BSplineCurve) pBSpline = pSpline->getOccSpline();
            splineLinearization(pBSpline, pnts, 80);
        }
        for (const wy::Vector2& pnt : pnts)
        {
            if (-1 == this->isCurveLoopContainPoint(outerLoopPnts, pnt, tol)) // 点在环外
            {
                return false;
            }
        }
    }

    return true;
}

// 判断环是否完全包含
bool SketchCurveGraph_Profile::isCurveLoopContains(
    const CurveLoopSPtr& pOuterLoop,
    const CurveLoopSPtr& pLoop,
    double tol) const
{
    assert(pOuterLoop);
    assert(pLoop);

    const std::vector<CurveEntry>& curveEntries = pOuterLoop->curves();
    size_t num = curveEntries.size();
    if (0 == num)
    {
        return false;
    }
    else if (1 == num) // 说明环本身是一条闭合曲线(圆或椭圆)
    {
        const CurveEntry& curveEntry = curveEntries.front();
        assert(curveEntry.index < _curves.size());
        const wy3d::SketchCurve* pSketchCurve = _curves[curveEntry.index];
        assert(pSketchCurve);
        const wyrx::ClassInfo* classInfo = pSketchCurve->getClassInfo();
        if (classInfo == wy3d::SketchCircle::classInfo())
        {
            const wy3d::SketchCircle* pOuterCircle = static_cast<const wy3d::SketchCircle*>(pSketchCurve);
            return this->isCircleContains(pOuterCircle, pLoop, tol);
        }
        else if (classInfo == wy3d::SketchEllipse::classInfo())
        {
            const wy3d::SketchEllipse* pOuterEllipse = static_cast<const wy3d::SketchEllipse*>(pSketchCurve);
            return this->isEllipseContains(pOuterLoop, pOuterEllipse, pLoop, tol);
        }
        else if (classInfo == wy3d::SketchSpline::classInfo())
        {
            // 啥也不做直接走后续的判断逻辑
        }
        else
        {
            assert(false);
        }
    }

    // 判断所有点是否都在环内
    const std::vector<CurveEntry>& curves = pLoop->curves();
    for (const CurveEntry& curveEntry : curves)
    {
        assert(curveEntry.index < _curves.size());
        const wy3d::SketchCurve* pSketchCurve = _curves[curveEntry.index];
        assert(pSketchCurve);
        wy::Vector2 pnt = (curveEntry.orient == Orientation::Normal) ?
            pSketchCurve->getStartPoint() : pSketchCurve->getEndPoint();
        if (-1 == this->isCurveLoopContainPoint(pOuterLoop, pnt, tol)) // 有一个点在环外
        {
            return false;
        }

        const std::vector<wy::Vector2>& outerLoopPnts = pOuterLoop->getPoints();
        if (outerLoopPnts.empty())
        {
            assert(false);
            return false;
        }

        // 先使用外包围盒初始过滤下
        const wy3d::BoundingBox2& bbox = _curveBBoxs[curveEntry.index];
        wy::Vector2 bboxPnts[4] = { bbox.min(), bbox.max(),
            wy::Vector2(bbox.min().x(), bbox.max().y()),
            wy::Vector2(bbox.max().x(), bbox.min().y()) };
        bool isBBoxTotallyInside(true);
        for (unsigned int i = 0; i < 4; ++i)
        {
            if (-1 == this->isCurveLoopContainPoint(outerLoopPnts, bboxPnts[i], tol)) // 点在环外
            {
                isBBoxTotallyInside = false;
                break;
            }
        }
        if (isBBoxTotallyInside) continue; // 外包围盒完全在圆内
        
        // 使用离散方法精确判断
        const wyrx::ClassInfo* classInfo = pSketchCurve->getClassInfo();
        std::vector<wy::Vector2> pnts;
        if (classInfo == wy3d::SketchLine::classInfo())
        {
            // 啥也不用做,因为在循环的开始判断了起点(或终点)
        }
        else if (classInfo == wy3d::SketchCircle::classInfo())
        {
            const wy3d::SketchCircle* pCircle = static_cast<const wy3d::SketchCircle*>(pSketchCurve);
            circleLinearization(pCircle->getCenter(), pCircle->getRadius(), pnts, 20);
        }
        else if (classInfo == wy3d::SketchArc::classInfo())
        {
            const wy3d::SketchArc* pArc = static_cast<const wy3d::SketchArc*>(pSketchCurve);
            arcLinearization(pArc->getCenter(), pArc->getRadius(), pArc->getStartAngle(), pArc->getEndAngle(),
                pnts, 20);
        }
        else if (classInfo == wy3d::SketchEllipse::classInfo())
        {
            const wy3d::SketchEllipse* pEllipse = static_cast<const wy3d::SketchEllipse*>(pSketchCurve);
            ellipseLinearization(pEllipse->getCenter(), pEllipse->getMajorAxis(), pEllipse->getRadiusRatio(),
                pnts, 20);
        }
        else if (classInfo == wy3d::SketchEllipseArc::classInfo())
        {
            const wy3d::SketchEllipseArc* pEllipseArc = static_cast<const wy3d::SketchEllipseArc*>(pSketchCurve);
            ellipseArcLinearization(pEllipseArc->getCenter(), pEllipseArc->getMajorAxis(), pEllipseArc->getRadiusRatio(),
                pEllipseArc->getStartAngle(), pEllipseArc->getEndAngle(), pnts, 20);
        }
        else if (classInfo == wy3d::SketchSpline::classInfo())
        {
            const wy3d::SketchSpline* pSpline = wy3d::SketchSpline::cast(pSketchCurve);
            Handle(Geom2d_BSplineCurve) pBSpline = pSpline->getOccSpline();
            if (!pBSpline)
            {
                assert(false);
                return false;
            }
            splineLinearization(pBSpline, pnts, 80);
        }
        else
        {
            assert(false);
            return false;
        }
        for (const wy::Vector2& pnt : pnts)
        {
            if (-1 == this->isCurveLoopContainPoint(outerLoopPnts, pnt, tol)) // 点在环外
            {
                return false;
            }
        }
    }
    return true;
}

// 原本是想通过射线法判断点是否在环内,但是写的过程中发现射线法在处理边界问题时很不好处理;
// <1>边界问题容易考虑不周<2>容差处理不好结果会不稳定
// 所以更改使用绕数法来判断
#ifdef RAY_TEST
// 已经判断了点在曲线的外包围盒内的情况下调用此函数判断点是否在曲线上
static inline bool isPointOnSketchCurve(
    const wy::Vector2& pnt,
    const wy3d::SketchCurve* pSketchCurve,
    double tol = wy3d::TOL)
{
    assert(pSketchCurve);
    const wyrx::ClassInfo* classInfo = pSketchCurve->getClassInfo();
    if (classInfo == wy3d::SketchLine::classInfo()) // 直线段
    {
        const wy3d::SketchLine* pSketchLine = static_cast<const wy3d::SketchLine*>(pSketchCurve);
        const wy::Vector2& startPnt = pSketchLine->getStartPoint();
        const wy::Vector2& endPnt = pSketchLine->getEndPoint();

        // 1. 处理线段退化为点的情况
        const wy::Vector2 lineVec = endPnt - startPnt;
        const double lineLen = lineVec.length();
        if (lineLen <= tol)
        {
            return (pnt - startPnt).length() <= tol;
        }

        // 2. 检查共线性（点必须在直线上）
        const wy::Vector2 pointVec = pnt - startPnt;
        if (std::fabs(pointVec.cross(lineVec)) > tol) // 叉积接近0表示共线
        {
            return false;
        }

        // 3. 检查点是否在线段范围内
        const double projLen = pointVec.dot(lineVec) / lineLen;
        return projLen >= -tol && projLen <= lineLen + tol;
    }
    else if (classInfo == wy3d::SketchCircle::classInfo()) // 圆
    {
        const wy3d::SketchCircle* pSketchCircle = static_cast<const wy3d::SketchCircle*>(pSketchCurve);
        wy::Vector2 center = pSketchCircle->getCenter();
        double radius = pSketchCircle->getRadius();

        // 处理退化情况：圆退化为点
        if (radius <= tol)
        {
            assert(false);
            return (pnt - center).length() <= tol;
        }

        return std::fabs((pnt - center).length() - radius) <= tol;
    }
    else if (classInfo == wy3d::SketchArc::classInfo()) // 圆弧
    {
        const wy3d::SketchArc* pSketchArc = static_cast<const wy3d::SketchArc*>(pSketchCurve);
        wy::Vector2 center = pSketchArc->getCenter();
        double radius = pSketchArc->getRadius();

        // 处理退化情况：圆退化为点
        if (radius <= tol)
        {
            assert(false);
            return (pnt - center).length() <= tol;
        }

        if (std::fabs((pnt - center).length() - radius) > tol)
        {
            return false;
        }

        // 判断角度是否在范围内,考虑容差
        double angle = wy::Vector2::rotationAngle(pnt - center, wy::Vector2::kXAxis);
        return wy3d::isAngleInArc(angle, pSketchArc->getStartAngle(), pSketchArc->getEndAngle(), tol);
    }
    else if (classInfo == wy3d::SketchEllipse::classInfo()) // 椭圆
    {
        const wy3d::SketchEllipse* pSketchEllipse = static_cast<const wy3d::SketchEllipse*>(pSketchCurve);
        wy::Vector2 center = pSketchEllipse->getCenter();
        wy::Vector2 majorAxis = pSketchEllipse->getMajorAxis();
        double radiusRatio = pSketchEllipse->getRadiusRatio();
        const double a = majorAxis.length();
        const double b = a * radiusRatio;

        // 处理退化情况：椭圆退化为点
        if (a <= tol)
        {
            assert(false);
            return (pnt - center).length() <= tol;
        }

        // 构建椭圆坐标系
        const wy::Vector2 u = majorAxis.normalized();        // 长轴单位方向
        const wy::Vector2 v(-u.y(), u.x());                  // 短轴单位方向（逆时针旋转90度）
        const wy::Vector2 pt = pnt - center;                 // 平移至椭圆坐标系

        // 计算归一化坐标分量
        const double x = pt.dot(u) / a;  // 长轴方向归一化坐标
        const double y = pt.dot(v) / b;  // 短轴方向归一化坐标

        // 椭圆方程：(x/a)^2 + (y/b)^2 = 1
        return std::fabs(x * x + y * y - 1.0) <= tol;
    }
    else if (classInfo == wy3d::SketchEllipseArc::classInfo()) // 椭圆弧
    {
        const wy3d::SketchEllipseArc* pSketchEllipseArc = static_cast<const wy3d::SketchEllipseArc*>(pSketchCurve);
        wy::Vector2 center = pSketchEllipseArc->getCenter();
        wy::Vector2 majorAxis = pSketchEllipseArc->getMajorAxis();
        double radiusRatio = pSketchEllipseArc->getRadiusRatio();
        const double a = majorAxis.length();
        const double b = a * radiusRatio;

        // 处理退化情况：椭圆退化为点
        if (a <= tol)
        {
            assert(false);
            return (pnt - center).length() <= tol;
        }

        // 构建椭圆坐标系
        const wy::Vector2 u = majorAxis.normalized();        // 长轴单位方向
        const wy::Vector2 v(-u.y(), u.x());                  // 短轴单位方向（逆时针旋转90度）
        const wy::Vector2 pt = pnt - center;                 // 平移至椭圆坐标系

        // 计算归一化坐标分量
        const double x = pt.dot(u) / a;  // 长轴方向归一化坐标
        const double y = pt.dot(v) / b;  // 短轴方向归一化坐标

        // 椭圆方程：(x/a)^2 + (y/b)^2 = 1
        if (std::fabs(x * x + y * y - 1.0) > tol)
        {
            return false;
        }

        // 计算参数角(考虑长轴方向)
        double angle = std::atan2(y, x); // [-PI, PI]
        angle = wy3d::normalizeRadian(angle);

        // 判断角度是否在范围内,考虑容差
        return wy3d::isAngleInArc(angle, pSketchEllipseArc->getStartAngle(), pSketchEllipseArc->getEndAngle(), tol);
    }
    else
    {
        assert(false);
        return false;
    }

    return false;
}

// 判断射线是否和曲线有交点
// 射线方向:水平向右
static inline bool isRayIntersectWith(
    const wy::Vector2& rayStart,
    const wy3d::SketchCurve* pSketchCurve,
    double tol = wy3d::TOL)
{
    assert(pSketchCurve);
    const wyrx::ClassInfo* classInfo = pSketchCurve->getClassInfo();
    if (classInfo == wy3d::SketchLine::classInfo()) // 直线段
    {
        const wy3d::SketchLine* pSketchLine = static_cast<const wy3d::SketchLine*>(pSketchCurve);
        const wy::Vector2& startPnt = pSketchLine->getStartPoint();
        const wy::Vector2& endPnt = pSketchLine->getEndPoint();

        // 线段退化判断
        if ((endPnt - startPnt).length() <= tol) return false;

        // 确保射线与线段的y坐标范围有重叠
        if ((startPnt.y() > rayStart.y() + tol && endPnt.y() > rayStart.y() + tol) ||
            (startPnt.y() < rayStart.y() - tol && endPnt.y() < rayStart.y() - tol)) {
            return false;
        }

        if (std::fabs(startPnt.y() - endPnt.y()) <= tol)
        {
            // 线段水平则不与水平射线相交
            return false;
        }

        // 参数方程求交点
        const double t = (rayStart.y() - startPnt.y()) / (endPnt.y() - startPnt.y());
        if (t < -tol || t > 1.0 + tol) return false; // 交点不在线段上

        // 是否在射线右侧
        const double x_intersect = startPnt.x() + t * (endPnt.x() - startPnt.x());
        return x_intersect >= rayStart.x() - tol;
    }
    else if (classInfo == wy3d::SketchCircle::classInfo()) // 圆
    {
        const wy3d::SketchCircle* pSketchCircle = static_cast<const wy3d::SketchCircle*>(pSketchCurve);
        wy::Vector2 center = pSketchCircle->getCenter();
        double radius = pSketchCircle->getRadius();

        // 垂直距离超过半径则无交点
        const double dy = center.y() - rayStart.y();
        if (std::fabs(dy) > radius + tol) return false;

        // 解水平线与圆的方程
        double temp = radius* radius - dy * dy;
        if (temp < 0.0) temp = 0.0;
        const double dx = std::sqrt(temp);
        const double x1 = center.x() - dx;
        const double x2 = center.x() + dx;

        // 是否在射线右侧
        return (x1 >= rayStart.x() - tol) || (x2 >= rayStart.x() - tol);
    }
    else if (classInfo == wy3d::SketchArc::classInfo()) // 圆弧
    {
        const wy3d::SketchArc* pSketchArc = static_cast<const wy3d::SketchArc*>(pSketchCurve);
        wy::Vector2 center = pSketchArc->getCenter();
        double radius = pSketchArc->getRadius();
        double startAngle = pSketchArc->getStartAngle();
        double endAngle = pSketchArc->getEndAngle();

        // 垂直距离超过半径则无交点
        const double dy = center.y() - rayStart.y();
        if (std::fabs(dy) > radius + tol) return false;

        // 解水平线与圆的方程
        double temp = radius * radius - dy * dy;
        if (temp < 0.0) temp = 0.0;
        const double dx = std::sqrt(temp);
        const double x1 = center.x() - dx;
        const double x2 = center.x() + dx;
        const double angle1 = wy::Vector2::rotationAngle(wy::Vector2(x1, rayStart.y()), wy::Vector2::kXAxis);
        const double angle2 = wy::Vector2::rotationAngle(wy::Vector2(x2, rayStart.y()), wy::Vector2::kXAxis);

        // 是否在射线右侧以及在圆弧角度范围内
        return (x1 >= rayStart.x() - tol && wy3d::isAngleInArc(angle1, startAngle, endAngle, tol)) 
            || (x2 >= rayStart.x() - tol && wy3d::isAngleInArc(angle2, startAngle, endAngle, tol));
    }
    else if (classInfo == wy3d::SketchEllipse::classInfo()) // 椭圆
    {
        const wy3d::SketchEllipse* pSketchEllipse = static_cast<const wy3d::SketchEllipse*>(pSketchCurve);
        wy::Vector2 center = pSketchEllipse->getCenter();
        wy::Vector2 majorAxis = pSketchEllipse->getMajorAxis();
        double radiusRatio = pSketchEllipse->getRadiusRatio();
        const double a = majorAxis.length();
        const double b = a * radiusRatio;

        // 长短轴方向
        const wy::Vector2 u = majorAxis.normalized();
        const wy::Vector2 v(-u.y(), u.x());

        // 将射线点转换到椭圆坐标系
        const double y_ellipse = (rayStart - center).dot(v);

        // 椭圆方程：(x/a)^2 + (y/b)^2 = 1
        if (std::fabs(y_ellipse) > b + tol) return false;

        // 解水平线方程y=y_ellipse的x坐标
        double temp = 1 - (y_ellipse * y_ellipse) / (b * b);
        if (temp < 0) temp = 0;
        const double x_scale = a * std::sqrt(temp);
        const double x1 = -x_scale;
        const double x2 = x_scale;

        // 将椭圆坐标系下的x坐标转换回世界坐标系
        const wy::Vector2 intersect1 = center + u * x1 + v * y_ellipse;
        const wy::Vector2 intersect2 = center + u * x2 + v * y_ellipse;

        // 验证交点的y坐标是否与射线一致（容差范围内）
        const bool valid1 = std::abs(intersect1.y() - rayStart.y()) <= tol;
        const bool valid2 = std::abs(intersect2.y() - rayStart.y()) <= tol;

        // 检查有效交点是否在射线右侧
        const bool right1 = valid1 && (intersect1.x() >= rayStart.x() - tol);
        const bool right2 = valid2 && (intersect2.x() >= rayStart.x() - tol);

        return right1 || right2;
    }
    else if (classInfo == wy3d::SketchEllipseArc::classInfo()) // 椭圆弧
    {
        const wy3d::SketchEllipseArc* pSketchEllipseArc = static_cast<const wy3d::SketchEllipseArc*>(pSketchCurve);
        wy::Vector2 center = pSketchEllipseArc->getCenter();
        wy::Vector2 majorAxis = pSketchEllipseArc->getMajorAxis();
        double radiusRatio = pSketchEllipseArc->getRadiusRatio();
        double startAngle = pSketchEllipseArc->getStartAngle();
        double endAngle = pSketchEllipseArc->getEndAngle();
        const double a = majorAxis.length();
        const double b = a * radiusRatio;

        // 长短轴方向
        const wy::Vector2 u = majorAxis.normalized();
        const wy::Vector2 v(-u.y(), u.x());

        // 将射线点转换到椭圆坐标系
        const double y_ellipse = (rayStart - center).dot(v);

        // 椭圆方程：(x/a)^2 + (y/b)^2 = 1
        if (std::fabs(y_ellipse) > b + tol) return false;

        // 解水平线方程y=y_ellipse的x坐标
        double temp = 1 - (y_ellipse * y_ellipse) / (b * b);
        if (temp < 0) temp = 0;
        const double x_scale = a * std::sqrt(temp);
        const double x1 = -x_scale;
        const double x2 = x_scale;

        // 将椭圆坐标系下的x坐标转换回世界坐标系
        const wy::Vector2 intersect1 = center + u * x1 + v * y_ellipse;
        const wy::Vector2 intersect2 = center + u * x2 + v * y_ellipse;

        // 验证交点的y坐标是否与射线一致（容差范围内）
        const bool valid1 = std::abs(intersect1.y() - rayStart.y()) <= tol;
        const bool valid2 = std::abs(intersect2.y() - rayStart.y()) <= tol;

        // 检查有效交点是否在射线右侧
        const bool right1 = valid1 && (intersect1.x() >= rayStart.x() - tol);
        const bool right2 = valid2 && (intersect2.x() >= rayStart.x() - tol);

        // 计算交点与椭圆中心连线和长轴正方向的夹角
        const double angle1 = wy::Vector2::rotationAngle(intersect1 - center, u);
        const double angle2 = wy::Vector2::rotationAngle(intersect2 - center, u);

        return (right1 && wy3d::isAngleInArc(angle1, startAngle, endAngle, tol)) 
            || (right2 && wy3d::isAngleInArc(angle2, startAngle, endAngle, tol));
    }
    else
    {
        assert(false);
        return false;
    }

    return false;
}
#endif

// 判断环是否完全包含点
// 返回值: 0 --- 点在环上;-1 --- 点在环外;1 --- 点在环内;
int SketchCurveGraph_Profile::isCurveLoopContainPoint(
    const CurveLoopSPtr& pLoop,
    const wy::Vector2& pnt,
    double tol) const
{
    assert(pLoop);
    assert(pLoop->curves().size() >= 1); // 有可能是闭合的样条曲线

    // 离散化
    pLoop->discretize(_curves); // 如果已经离散化了,函数内部会判断,不会重复离散化;
    return this->isCurveLoopContainPoint(pLoop->getPoints(), pnt, tol);
}

int SketchCurveGraph_Profile::isCurveLoopContainPoint(
    const std::vector<wy::Vector2>& pnts,
    const wy::Vector2& pnt,
    double tol) const
{
    if (pnts.size() < 3)
    {
        assert(false);
        return false;
    }

    // 使用绕数法判断
    double windingAngle = 0.0;
    size_t num = pnts.size();
    for (size_t i = 0; i < num; ++i)
    {
        const wy::Vector2& a = pnts[i];
        const wy::Vector2& b = pnts[(i + 1) % num];
        wy::Vector2 ap = pnt - a;
        wy::Vector2 bp = pnt - b;
        wy::Vector2 ab = b - a;

        // 点在边线上
        if (std::fabs(ap.cross(ab)) <= tol && ap.dot(ab) >= 0.0 && bp.dot(ab) <= 0.0)
        {
            return 0;
        }

        assert(std::atan2(ap.cross(bp), ap.dot(bp)) >= -wy3d::PI);
        assert(std::atan2(ap.cross(bp), ap.dot(bp)) <= wy3d::PI);
        windingAngle += std::atan2(ap.cross(bp), ap.dot(bp)); // [-PI,PI]
    }

    if (std::fabs(windingAngle) > 0.5) // 2PI or -2PI
    {
        assert(std::fabs(std::fabs(windingAngle) - wy3d::TWO_PI) <= 1e-3);
        return 1; // 内部
    }
    else // windingNumber == 0
    {
        assert(std::fabs(windingAngle) <= 1e-3);
        return -1; // 外部
    }
}

double SketchCurveGraph_Profile::computeSideArea(const CurveLoop& loop)
{
    double signedArea(0.0);
    const std::vector<CurveEntry>& curves = loop.curves();
    for (const CurveEntry& curveEntry : curves)
    {
        double area(0.0);
        assert(curveEntry.index < _curves.size());
        const SketchCurve* pSketchCurve = _curves[curveEntry.index];
        assert(pSketchCurve);
        if (const wy3d::SketchLine* pSketchLine = wy3d::SketchLine::cast(pSketchCurve))
        {
            wy::Vector2 startPoint = pSketchLine->getStartPoint();
            wy::Vector2 endPoint = pSketchLine->getEndPoint();
            area = (startPoint.x() * endPoint.y() - endPoint.x() * startPoint.y()) / 2;
        }
        else if (const SketchCircle* pSketchCircle = wy3d::SketchCircle::cast(pSketchCurve))
        {
            assert(curves.size() == 1);
            area = wy3d::PI * pSketchCircle->getRadius() * pSketchCircle->getRadius();
        }
        else if (const wy3d::SketchArc* pSketchArc = wy3d::SketchArc::cast(pSketchCurve))
        {
            wy::Vector2 centerPoint = pSketchArc->getCenter();
            double radius = pSketchArc->getRadius();
            std::vector<wy::Vector2> vertices;
            arcLinearization_MinMax(centerPoint, radius, pSketchArc->getStartAngle(), pSketchArc->getEndAngle(), vertices,
                10, 40);
            size_t num = vertices.size();
            if (num >= 2)
            {
                for (size_t i = 0; i <= num - 2; ++i)
                {
                    area += vertices[i].x() * vertices[i + 1].y() - vertices[i + 1].x() * vertices[i].y();
                }
            }
            else
            {
                assert(false);
            }
            area /= 2;
        }
        else if (const wy3d::SketchEllipse* pSketchEllipse = wy3d::SketchEllipse::cast(pSketchCurve))
        {
            assert(curves.size() == 1);
            area = wy3d::PI * pSketchEllipse->getMajorRadius() * pSketchEllipse->getMinorRadius();
        }
        else if (const wy3d::SketchEllipseArc* pSketchEllipseArc = wy3d::SketchEllipseArc::cast(pSketchCurve))
        {
            std::vector<wy::Vector2> vertices;
            ellipseArcLinearization(pSketchEllipseArc->getCenter(), pSketchEllipseArc->getMajorAxis(), pSketchEllipseArc->getRadiusRatio(),
                pSketchEllipseArc->getStartAngle(), pSketchEllipseArc->getEndAngle(), vertices, 40);
            size_t num = vertices.size();
            if (num >= 2)
            {
                for (size_t i = 0; i <= num - 2; ++i)
                {
                    area += vertices[i].x() * vertices[i + 1].y() - vertices[i + 1].x() * vertices[i].y();
                }
            }
            else
            {
                assert(false);
            }
            area /= 2;
        }
        else if (const wy3d::SketchSpline* pSketchSpline = wy3d::SketchSpline::cast(pSketchCurve))
        {
            Handle(Geom2d_BSplineCurve) pBSpline = pSketchSpline->getOccSpline();
            if (!pBSpline)
            {
                assert(false);
                area = 0.0;
            }
            else
            {
                // 获取参数范围
                double u0 = pBSpline->FirstParameter();
                double u1 = pBSpline->LastParameter();

                // 离散点数（可根据需要调整）
                int numSegments = 100;
                int numKnots = pBSpline->NbKnots();
                if (numKnots > 1)
                {
                    numSegments = (numKnots - 1) * 20;
                }
                double du = (u1 - u0) / numSegments;

                // 获取起点
                gp_Pnt2d prevPoint;
                pBSpline->D0(u0, prevPoint);

                double totalArea = 0.0;

                // 数值积分：将曲线离散为小线段
                for (int i = 1; i <= numSegments; ++i)
                {
                    double u = u0 + i * du;
                    gp_Pnt2d currentPoint;
                    pBSpline->D0(u, currentPoint);

                    // 计算当前线段与原点形成的三角形面积
                    totalArea += (prevPoint.X() * currentPoint.Y() - currentPoint.X() * prevPoint.Y());
                    prevPoint = currentPoint;
                }

                // 最终面积 = 所有小三角形面积之和 / 2
                area = totalArea / 2.0;
            }
        }
        else
        {
            // TODO:椭圆,椭圆弧,样条曲线,贝塞尔曲线
            assert(false);
            wy::Vector2 startPoint = pSketchCurve->getStartPoint();
            wy::Vector2 endPoint = pSketchCurve->getEndPoint();
            area = (startPoint.x() * endPoint.y() - endPoint.x() * startPoint.y()) / 2;
        }

        if (curveEntry.orient == Orientation::Normal)
            signedArea += area;
        else
            signedArea -= area;
    }

    return signedArea;
}

NS_WY3D_END