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

#include <wy3dSketchPath.h>

#include <wydbDatabase.h>
#include <wy3dSketchEntity.h>
#include <wy3dSketchCurve.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>
#include <wy3dSketchEllipse.h>
#include <wy3dSketchEllipseArc.h>

NS_WY3D_BEG

SketchPath::SketchPath(const Sketch* pSketch, double tol)
    : _pSketch(pSketch), _tol(tol), _isValid(false)
{
    assert(_pSketch);
}

bool SketchPath::check()
{
    assert(_pSketch);
    if (_pSketch && this->init() && !_path.empty())
    {
        _isValid = true;
        return true;
    }
    else
    {
        return false;
    }
}

bool SketchPath::init()
{
    assert(_pSketch);
    const wydb::Database* pDb = _pSketch->getDatabase();
    if (!pDb)
    {
        assert(false);
        _pError = this->newErrorOfUndefined();
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

    // 没有曲线
    if (curves.empty())
    {
        _pError = std::make_shared<SketchError>();
        _pError->type = ErrorCode::PATH_NoCurves;
        return false;
    }

    // 初始化图
    SketchCurveGraph_Path curveGraph(curves, _tol);
    if (!curveGraph.isValid())
    {
        _pError = curveGraph.getError();
        return false;
    }

    // 查找路径
    if (!curveGraph.findPath())
    {
        _pError = curveGraph.getError();
        return false;
    }
    const std::vector<SketchCurveGraph::CurveEntry>& pathCurves = curveGraph.getPath();
    if (pathCurves.empty())
    {
        _pError = std::make_shared<SketchError>();
        _pError->type = ErrorCode::PATH_NoCurves;
        return false;
    }

    // 路径结果
    assert(_path.empty());
    _path.reserve(pathCurves.size());
    for (const SketchCurveGraph::CurveEntry& curveEntry : pathCurves)
    {
        bool orient = (curveEntry.orient == SketchCurveGraph::Orientation::Normal) ? true : false;
        assert(curveEntry.index >= 0 && curveEntry.index < curves.size());
        _path.emplace_back(BiCurve(curves[curveEntry.index], orient));
    }

    return true;
}

std::shared_ptr<SketchError> SketchPath::newErrorOfUndefined() const
{
    std::shared_ptr<SketchError> pError = std::make_shared<SketchError>();
    pError->type = ErrorCode::PATH_InvalidPath;
    return pError;
}

SketchCurveGraph_Path::SketchCurveGraph_Path(const std::vector<const SketchCurve*>& curves, double tol)
    : SketchCurveGraph(curves, tol)
{
}

bool SketchCurveGraph_Path::findPath()
{
    if (!_isValid)
    {
        assert(_pError);
        return false;
    }

    // 没有曲线
    if (_curves.empty())
    {
        _pError = std::make_shared<SketchError>();
        _pError->type = ErrorCode::PATH_NoCurves;
        return false;
    }

    // 查找连续的曲线路径
    assert(_curves.size() == _endPointAdjacency.size());
    assert(_curves.size() == _startPointAdjacency.size());
    std::vector<bool> visited(_curves.size(), false);

    // 从第一条曲线正向查找
    std::vector<CurveEntry> pathCurves1st;
    FindRet findRet = this->findPathImpl(visited, CurveEntry{ 0, Orientation::Normal }, pathCurves1st);
    if (FindRet::Finished == findRet)
    {
        _pathCurves.swap(pathCurves1st);
        return true;
    }
    else if (FindRet::Error == findRet)
    {
        assert(_pError);
        return false;
    }

    // 从第一条曲线逆向查找
    assert(FindRet::Continue == findRet);
    std::vector<CurveEntry> pathCurves2nd;
    findRet = this->findPathImpl(visited, CurveEntry{ 0, Orientation::Reversed }, pathCurves2nd);
    if (FindRet::Finished == findRet)
    {
        assert(pathCurves2nd.size() > 1);
        std::vector<CurveEntry> prefix;
        prefix.reserve(_curves.size());
        for (auto iter = pathCurves2nd.crbegin(); iter != pathCurves2nd.crend(); ++iter)
        {
            if (iter->index == 0)
            {
                assert(iter == std::prev(pathCurves2nd.crend()));
                break;
            }
            prefix.emplace_back(CurveEntry{ iter->index,
                iter->orient == Orientation::Normal ? Orientation::Reversed : Orientation::Normal });
        }
        _pathCurves.swap(prefix);
        _pathCurves.insert(_pathCurves.cend(), pathCurves1st.cbegin(), pathCurves1st.cend());
        return true;
    }
    else if (FindRet::Error == findRet)
    {
        assert(_pError);
        return false;
    }
    else if (FindRet::Continue == findRet)
    {
        _pError = this->newError(ErrorCode::PATH_MoreThanOneLoopIsNotAllowed,
            wydb::ElementId::kNull, {});
        return false;
    }
    else
    {
        assert(false);
    }

    return false;
}

SketchCurveGraph_Path::FindRet SketchCurveGraph_Path::findPathImpl(
    std::vector<bool>& visited,
    const CurveEntry& startCurveEntry,
    std::vector<CurveEntry>& pathCurves)
{
    assert(visited.size() == _curves.size());
    assert(!_curves.empty());
    assert(startCurveEntry.index >= 0 && startCurveEntry.index < _curves.size());
    assert(pathCurves.empty());

    visited[startCurveEntry.index] = true;
    pathCurves.emplace_back(startCurveEntry);
    while (true)
    {
        const CurveEntry& curveEntry = pathCurves.back();
        assert(curveEntry.index >= 0 && curveEntry.index < _curves.size());
        const SketchCurve* pCurve = _curves[curveEntry.index];
        assert(pCurve);

        // 闭合曲线
        if (pCurve->isClosed())
        {
            if (_curves.size() == 1)
            {
                return FindRet::Finished;
            }
            else
            {
                _pError = this->newError(ErrorCode::PATH_MoreThanOneLoopIsNotAllowed,
                    pCurve->getId(), {});
                return FindRet::Error;
            }
        }
        // 非闭合曲线
        else
        {
            const std::vector<CurveEntry>& adjs = (curveEntry.orient == Orientation::Normal) ?
                _endPointAdjacency[curveEntry.index] : _startPointAdjacency[curveEntry.index];
            if (0 == adjs.size())
            {
                break;
            }
            else if (1 == adjs.size())
            {
                const CurveEntry& nextCurveEntry = adjs.front();
                if (visited[nextCurveEntry.index]) // 闭环
                {
                    for (auto iter = visited.cbegin(); iter != visited.cend(); ++iter)
                    {
                        if (!(*iter)) // 有没有访问的曲线
                        {
                            _pError = this->newError(ErrorCode::PATH_MoreThanOneLoopIsNotAllowed,
                                pCurve->getId(), { CurveEntry{ *iter, Orientation::Normal } });
                            return FindRet::Error;
                        }
                    }

                    // 刚好是一个完整的闭环
                    return FindRet::Finished;
                }
                else
                {
                    visited[nextCurveEntry.index] = true;
                    pathCurves.emplace_back(nextCurveEntry);
                }
            }
            else // >1
            {
                _pError = this->newError(ErrorCode::SKETCH_MoreThanTwoCurvesAtOneEndPoint,
                    pCurve->getId(), adjs);
                return FindRet::Error;
            }
        }
    }

    // 至此访问到了一条连续的路径,需判断是结束还是继续
    for (auto iter = visited.cbegin(); iter != visited.cend(); ++iter)
    {
        if (!(*iter)) // 有没有访问的曲线
        {
            return FindRet::Continue;
        }
    }
    return FindRet::Finished;
}

NS_WY3D_END