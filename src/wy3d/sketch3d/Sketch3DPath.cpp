///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 Wang Yao <wangyao1052@163.com>
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
#include <cstddef>
#include <iterator>
#include <vector>

#include <wydbDatabase.h>
#include <wy3dErrorCode.h>
#include <wy3dSketch3DPath.h>
#include <wy3dSketchCurve3D.h>

#include "Sketch3DCurveGraph.h"

NS_WY3D_BEG

namespace
{
    // A curve of the path together with its orientation along it
    struct PathCurveEntry
    {
        int index;
        bool forward;
    };

    enum class FindRet
    {
        Finished = 0,
        Continue = 1,
        Error    = 2,
    };

    struct FindResult
    {
        FindResult() : ret(FindRet::Finished), errorType(ErrorCode::NoError) {}

        FindRet ret;
        // valid when ret == Error
        ErrorCode errorType;
        std::vector<int> errorCurves;
    };

    // The 3D counterpart of the 2D SketchCurveGraph::_endPointAdjacency /
    // _startPointAdjacency: the curves connected at the given vertex, the ones
    // starting there first (forward), then the ones ending there (reversed);
    // the curve itself is excluded
    std::vector<PathCurveEntry> adjacencyAt(
        const Sketch3DCurveGraph& graph, int vertex, int excludeIndex)
    {
        assert(vertex >= 0);
        assert(static_cast<std::size_t>(vertex) < graph.getVertexInfos().size());

        const Sketch3DCurveGraph::VertexInfo& info = graph.getVertexInfos()[static_cast<std::size_t>(vertex)];
        std::vector<PathCurveEntry> adjs;
        adjs.reserve(info.outgoingCurves.size() + info.incomingCurves.size());
        for (int j : info.outgoingCurves)
        {
            if (j != excludeIndex) adjs.emplace_back(PathCurveEntry{ j, true });
        }
        for (int j : info.incomingCurves)
        {
            if (j != excludeIndex) adjs.emplace_back(PathCurveEntry{ j, false });
        }
        return adjs;
    }

    // Mirrors the 2D SketchCurveGraph_Path::findPathImpl
    FindResult findPathImpl(
        const Sketch3DCurveGraph& graph,
        std::vector<bool>& visited,
        const PathCurveEntry& startCurveEntry,
        std::vector<PathCurveEntry>& pathCurves)
    {
        const std::vector<const SketchCurve3D*>& curves = graph.getCurves();
        const std::vector<Sketch3DCurveGraph::CurveEnds>& curveEnds = graph.getCurveEnds();

        assert(visited.size() == curves.size());
        assert(!curves.empty());
        assert(startCurveEntry.index >= 0 && static_cast<std::size_t>(startCurveEntry.index) < curves.size());
        assert(pathCurves.empty());

        FindResult result;
        visited[static_cast<std::size_t>(startCurveEntry.index)] = true;
        pathCurves.emplace_back(startCurveEntry);
        while (true)
        {
            const PathCurveEntry& curveEntry = pathCurves.back();
            assert(curveEntry.index >= 0 && static_cast<std::size_t>(curveEntry.index) < curves.size());
            const SketchCurve3D* pCurve = curves[static_cast<std::size_t>(curveEntry.index)];
            assert(pCurve);

            // 闭合曲线
            if (pCurve->isClosed())
            {
                if (1 == curves.size())
                {
                    result.ret = FindRet::Finished;
                    return result;
                }
                else
                {
                    // 闭合曲线自身成一环, 与其它曲线不可能连成单一环
                    result.ret = FindRet::Error;
                    result.errorType = ErrorCode::PATH_MoreThanOneLoopIsNotAllowed;
                    return result;
                }
            }
            // 非闭合曲线
            else
            {
                const Sketch3DCurveGraph::CurveEnds& ends = curveEnds[static_cast<std::size_t>(curveEntry.index)];
                const int leaveVertex = curveEntry.forward ? ends.v2 : ends.v1;
                const std::vector<PathCurveEntry> adjs = adjacencyAt(graph, leaveVertex, curveEntry.index);
                if (0 == adjs.size())
                {
                    break;
                }
                else if (1 == adjs.size())
                {
                    const PathCurveEntry& nextCurveEntry = adjs.front();
                    if (visited[static_cast<std::size_t>(nextCurveEntry.index)]) // 闭环
                    {
                        for (std::size_t i = 0; i < visited.size(); ++i)
                        {
                            if (!visited[i]) // 有没有访问的曲线
                            {
                                result.ret = FindRet::Error;
                                result.errorType = ErrorCode::PATH_MoreThanOneLoopIsNotAllowed;
                                return result;
                            }
                        }

                        // 刚好是一个完整的闭环
                        result.ret = FindRet::Finished;
                        return result;
                    }
                    else
                    {
                        visited[static_cast<std::size_t>(nextCurveEntry.index)] = true;
                        pathCurves.emplace_back(nextCurveEntry);
                    }
                }
                else // >1
                {
                    result.ret = FindRet::Error;
                    result.errorType = ErrorCode::SKETCH_MoreThanTwoCurvesAtOneEndPoint;
                    for (const PathCurveEntry& entry : adjs)
                    {
                        result.errorCurves.emplace_back(entry.index);
                    }
                    result.errorCurves.emplace_back(curveEntry.index);
                    return result;
                }
            }
        }

        // 至此访问到了一条连续的路径,需判断是结束还是继续
        for (std::size_t i = 0; i < visited.size(); ++i)
        {
            if (!visited[i]) // 有没有访问的曲线
            {
                result.ret = FindRet::Continue;
                return result;
            }
        }
        result.ret = FindRet::Finished;
        return result;
    }
}

Sketch3DPath::Sketch3DPath(const Sketch3D* pSketch3D, double tol)
    : _pSketch3D(pSketch3D), _tol(tol > 0.0 ? tol : 1e-5), _isValid(false)
{
    assert(_pSketch3D);
}

bool Sketch3DPath::check()
{
    assert(_pSketch3D);
    // Drop any previous result first: the path holds non-owning curve pointers,
    // which dangle once the sketch is edited
    _isValid = false;
    _pError = nullptr;
    _path.clear();
    if (_pSketch3D && this->init() && !_path.empty())
    {
        _isValid = true;
        return true;
    }
    else
    {
        return false;
    }
}

void Sketch3DPath::setError(ErrorCode type, const std::vector<wydb::ElementId>& ids)
{
    _pError = std::make_shared<SketchError>();
    _pError->type = type;
    _pError->ids = ids;
}

bool Sketch3DPath::init()
{
    assert(_pSketch3D);
    Sketch3DCurveGraph graph(_pSketch3D, _tol);
    if (!graph.isCollected())
    {
        const wydb::ElementId& rejectedId = graph.getRejectedCurveId();
        this->setError(ErrorCode::PATH_InvalidPath,
            rejectedId.isNull() ? std::vector<wydb::ElementId>{}
                                : std::vector<wydb::ElementId>{ rejectedId });
        return false;
    }

    const std::vector<const SketchCurve3D*>& curves = graph.getCurves();
    if (curves.empty())
    {
        this->setError(ErrorCode::PATH_NoCurves, {});
        return false;
    }

    // 每个顶点上以它为起点的曲线不超过1条、以它为终点的曲线不超过1条
    // (对齐2D SketchCurveGraph::buildGraph; 不是轮廓的"度必须为2")
    std::vector<int> offendingCurves;
    for (const Sketch3DCurveGraph::VertexInfo& info : graph.getVertexInfos())
    {
        if (info.outgoingCurves.size() > 1 || info.incomingCurves.size() > 1)
        {
            offendingCurves.insert(offendingCurves.cend(),
                info.outgoingCurves.cbegin(), info.outgoingCurves.cend());
            offendingCurves.insert(offendingCurves.cend(),
                info.incomingCurves.cbegin(), info.incomingCurves.cend());
        }
    }
    if (!offendingCurves.empty())
    {
        this->setError(ErrorCode::SKETCH_MoreThanTwoCurvesAtOneEndPoint, graph.idsOfCurves(offendingCurves));
        return false;
    }

    // 查找路径
    const std::size_t n = curves.size();
    std::vector<bool> visited(n, false);

    // 从第一条曲线正向查找
    std::vector<PathCurveEntry> pathCurves1st;
    FindResult findResult = findPathImpl(graph, visited, PathCurveEntry{ 0, true }, pathCurves1st);
    if (FindRet::Error == findResult.ret)
    {
        this->setError(findResult.errorType, graph.idsOfCurves(findResult.errorCurves));
        return false;
    }

    std::vector<PathCurveEntry> pathCurves;
    if (FindRet::Finished == findResult.ret)
    {
        pathCurves.swap(pathCurves1st);
    }
    else
    {
        // 从第一条曲线逆向查找 (复用同一个 visited)
        assert(FindRet::Continue == findResult.ret);
        std::vector<PathCurveEntry> pathCurves2nd;
        findResult = findPathImpl(graph, visited, PathCurveEntry{ 0, false }, pathCurves2nd);
        if (FindRet::Error == findResult.ret)
        {
            this->setError(findResult.errorType, graph.idsOfCurves(findResult.errorCurves));
            return false;
        }
        else if (FindRet::Continue == findResult.ret)
        {
            this->setError(ErrorCode::PATH_MoreThanOneLoopIsNotAllowed, {});
            return false;
        }
        assert(FindRet::Finished == findResult.ret);

        assert(pathCurves2nd.size() > 1);
        pathCurves.reserve(pathCurves2nd.size() - 1 + pathCurves1st.size());
        for (auto iter = pathCurves2nd.crbegin(); iter != pathCurves2nd.crend(); ++iter)
        {
            if (0 == iter->index)
            {
                assert(std::next(iter) == pathCurves2nd.crend());
                break;
            }
            pathCurves.emplace_back(PathCurveEntry{ iter->index, !iter->forward });
        }
        pathCurves.insert(pathCurves.cend(), pathCurves1st.cbegin(), pathCurves1st.cend());
    }

    // 路径结果
    assert(_path.empty());
    _path.reserve(pathCurves.size());
    for (const PathCurveEntry& entry : pathCurves)
    {
        assert(entry.index >= 0 && static_cast<std::size_t>(entry.index) < curves.size());
        _path.emplace_back(BiCurve3D(curves[static_cast<std::size_t>(entry.index)], entry.forward));
    }

    return true;
}

NS_WY3D_END
