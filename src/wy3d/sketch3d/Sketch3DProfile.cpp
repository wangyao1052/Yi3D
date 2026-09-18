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
#include <cstdint>
#include <vector>

#include <wydbDatabase.h>
#include <wy3dErrorCode.h>
#include <wy3dImpl.h>
#include <wy3dSketch3DProfile.h>
#include <wy3dSketchCurve3D.h>

#include "Sketch3DCurveGraph.h"

NS_WY3D_BEG

Sketch3DProfile::Sketch3DProfile(const Sketch3D* pSketch3D, double tol)
    : _pSketch3D(pSketch3D), _tol(tol > 0.0 ? tol : 1e-5), _isValid(false)
{
    assert(_pSketch3D);
}

bool Sketch3DProfile::check()
{
    assert(_pSketch3D);
    // Drop any previous result first: the loop holds non-owning curve pointers,
    // which dangle once the sketch is edited
    _isValid = false;
    _pError = nullptr;
    _loopCurves.clear();
    if (_pSketch3D && this->init() && !_loopCurves.empty())
    {
        _isValid = true;
        return true;
    }
    else
    {
        return false;
    }
}

void Sketch3DProfile::setError(ErrorCode type, const std::vector<wydb::ElementId>& ids)
{
    _pError = std::make_shared<SketchError>();
    _pError->type = type;
    _pError->ids = ids;
}

bool Sketch3DProfile::init()
{
    assert(_pSketch3D);
    Sketch3DCurveGraph graph(_pSketch3D, _tol);
    if (!graph.isCollected())
    {
        const wydb::ElementId& rejectedId = graph.getRejectedCurveId();
        this->setError(ErrorCode::FILLEDSHEET_InvalidData,
            rejectedId.isNull() ? std::vector<wydb::ElementId>{}
                                : std::vector<wydb::ElementId>{ rejectedId });
        return false;
    }

    const std::vector<const SketchCurve3D*>& curves = graph.getCurves();
    if (curves.empty())
    {
        this->setError(ErrorCode::FILLEDSHEET_InvalidData, {});
        return false;
    }

    const std::vector<Sketch3DCurveGraph::CurveEnds>& curveEnds = graph.getCurveEnds();
    const std::vector<Sketch3DCurveGraph::VertexInfo>& vertexInfos = graph.getVertexInfos();
    assert(curveEnds.size() == curves.size());
    assert(!vertexInfos.empty());

    // Every vertex must have exactly 2 incident curves (open end / branch /
    // T-junction rejected)
    std::vector<int> offendingCurves;
    for (const Sketch3DCurveGraph::VertexInfo& info : vertexInfos)
    {
        if (info.adjacentCurves.size() != 2)
        {
            offendingCurves.insert(offendingCurves.cend(),
                info.adjacentCurves.cbegin(), info.adjacentCurves.cend());
        }
    }
    if (!offendingCurves.empty())
    {
        this->setError(ErrorCode::FILLEDSHEET_EdgesNotClosed, graph.idsOfCurves(offendingCurves));
        return false;
    }

    // Connectivity: all curves must belong to one loop
    const std::size_t n = curves.size();
    std::vector<bool> visited(n, false);
    {
        std::vector<std::size_t> stack{ 0 };
        visited[0] = true;
        while (!stack.empty())
        {
            const std::size_t curveIndex = stack.back();
            stack.pop_back();
            for (int vi : { curveEnds[curveIndex].v1, curveEnds[curveIndex].v2 })
            {
                for (int neighborCurve : vertexInfos[static_cast<std::size_t>(vi)].adjacentCurves)
                {
                    if (!visited[static_cast<std::size_t>(neighborCurve)])
                    {
                        visited[static_cast<std::size_t>(neighborCurve)] = true;
                        stack.emplace_back(static_cast<std::size_t>(neighborCurve));
                    }
                }
            }
        }
    }
    {
        std::vector<int> unreachable;
        for (std::size_t i = 0; i < n; ++i)
        {
            if (!visited[i])
            {
                unreachable.emplace_back(static_cast<int>(i));
            }
        }
        if (!unreachable.empty())
        {
            this->setError(ErrorCode::FILLEDSHEET_EdgesNotClosed, graph.idsOfCurves(unreachable));
            return false;
        }
    }

    // Walk the loop, orienting each curve so consecutive curves connect
    // end-to-start; the single closed curve (full circle) is the base case
    _loopCurves.clear();
    _loopCurves.reserve(n);
    std::vector<bool> used(n, false);
    int curEndVertex = curveEnds[0].v2;
    used[0] = true;
    _loopCurves.emplace_back(BiCurve3D(curves[0], true));
    for (std::size_t k = 1; k < n; ++k)
    {
        int nextCurve = -1;
        bool nextForward = false;
        for (int neighborCurve : vertexInfos[static_cast<std::size_t>(curEndVertex)].adjacentCurves)
        {
            if (used[static_cast<std::size_t>(neighborCurve)])
            {
                continue;
            }
            nextForward = (curveEnds[static_cast<std::size_t>(neighborCurve)].v1 == curEndVertex);
            nextCurve = neighborCurve;
            break;
        }
        if (nextCurve < 0)
        {
            std::vector<int> remaining;
            for (std::size_t i = 0; i < n; ++i)
            {
                if (!used[i])
                {
                    remaining.emplace_back(static_cast<int>(i));
                }
            }
            _loopCurves.clear();
            this->setError(ErrorCode::FILLEDSHEET_EdgesNotClosed, graph.idsOfCurves(remaining));
            return false;
        }

        used[static_cast<std::size_t>(nextCurve)] = true;
        const Sketch3DCurveGraph::CurveEnds& ends = curveEnds[static_cast<std::size_t>(nextCurve)];
        _loopCurves.emplace_back(BiCurve3D(curves[static_cast<std::size_t>(nextCurve)], nextForward));
        curEndVertex = nextForward ? ends.v2 : ends.v1;
    }

    // Closure: the walk must return to the first curve's start vertex
    if (curEndVertex != curveEnds[0].v1)
    {
        std::vector<int> remaining;
        for (std::size_t i = 0; i < n; ++i)
        {
            if (!used[i])
            {
                remaining.emplace_back(static_cast<int>(i));
            }
        }
        _loopCurves.clear();
        this->setError(ErrorCode::FILLEDSHEET_EdgesNotClosed, graph.idsOfCurves(remaining));
        return false;
    }

    return true;
}

NS_WY3D_END
