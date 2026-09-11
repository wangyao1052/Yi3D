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
#include <cmath>
#include <cstdint>
#include <map>
#include <algorithm>

#include <wydbDatabase.h>
#include <wy3dErrorCode.h>
#include <wy3dImpl.h>
#include <wy3dSketch3DProfile.h>
#include <wy3dSketchCurve3D.h>
#include <wy3dSketchCircle3D.h>
#include <wy3dSketchArc3D.h>
#include <wy3dSketchEllipse3D.h>
#include <wy3dSketchEllipseArc3D.h>

NS_WY3D_BEG

namespace
{
    // Quantized 3D point key for vertex fusion (same idea as the 2D
    // SketchCurveGraph::convertToIntVec2)
    struct IntVector3
    {
        std::int64_t x;
        std::int64_t y;
        std::int64_t z;

        bool operator<(const IntVector3& other) const
        {
            if (x != other.x) return x < other.x;
            if (y != other.y) return y < other.y;
            return z < other.z;
        }
    };

    IntVector3 quantize(const wy::Vector3& pnt, double tol)
    {
        return IntVector3{
            static_cast<std::int64_t>(std::llround(pnt.x() / tol)),
            static_cast<std::int64_t>(std::llround(pnt.y() / tol)),
            static_cast<std::int64_t>(std::llround(pnt.z() / tol)) };
    }

    struct VertexInfo
    {
        std::vector<int> adjacentCurves;
    };

    struct CurveEnds
    {
        int v1;
        int v2;
    };
}

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
    const wydb::Database* pDb = _pSketch3D->getDatabase();
    if (!pDb)
    {
        assert(false);
        this->setError(ErrorCode::FILLEDSHEET_InvalidData, {});
        return false;
    }

    // Collect the curves; reject degenerated ones the same way
    // Sketch3DTopoBuilder::makeEdge does
    std::vector<const SketchCurve3D*> curves;
    std::vector<wydb::ElementId> curveIds;
    for (wy::Iterator<wydb::ElementId> iter = _pSketch3D->createIterator(); !iter.isDone(); iter.moveNext())
    {
        const wydb::ElementId id = iter.current();
        if (id.isNull())
        {
            assert(false);
            this->setError(ErrorCode::FILLEDSHEET_InvalidData, {});
            return false;
        }
        const wydb::Element* pElem = pDb->getElement(id);
        const wy3d::SketchCurve3D* pCurve = wy3d::SketchCurve3D::cast(pElem);
        if (!pCurve)
        {
            assert(false);
            this->setError(ErrorCode::FILLEDSHEET_InvalidData, {});
            return false;
        }
        if (pCurve->isDegenerate(wy3d::TOL))
        {
            this->setError(ErrorCode::FILLEDSHEET_InvalidData, { id });
            return false;
        }

        // 平面无效只可能来自反序列化出的畸形文件,按精确类取法向校验
        const wyrx::ClassInfo* pCurveClassInfo = pCurve->getClassInfo();
        if (wy3d::SketchCircle3D::classInfo() == pCurveClassInfo)
        {
            if (static_cast<const wy3d::SketchCircle3D*>(pCurve)->getNormal().length() < 0.5)
            {
                assert(false);
                this->setError(ErrorCode::FILLEDSHEET_InvalidData, { id });
                return false;
            }
        }
        else if (wy3d::SketchArc3D::classInfo() == pCurveClassInfo)
        {
            if (static_cast<const wy3d::SketchArc3D*>(pCurve)->getNormal().length() < 0.5)
            {
                assert(false);
                this->setError(ErrorCode::FILLEDSHEET_InvalidData, { id });
                return false;
            }
        }
        else if (wy3d::SketchEllipse3D::classInfo() == pCurveClassInfo)
        {
            if (static_cast<const wy3d::SketchEllipse3D*>(pCurve)->getNormal().length() < 0.5)
            {
                assert(false);
                this->setError(ErrorCode::FILLEDSHEET_InvalidData, { id });
                return false;
            }
        }
        else if (wy3d::SketchEllipseArc3D::classInfo() == pCurveClassInfo)
        {
            if (static_cast<const wy3d::SketchEllipseArc3D*>(pCurve)->getNormal().length() < 0.5)
            {
                assert(false);
                this->setError(ErrorCode::FILLEDSHEET_InvalidData, { id });
                return false;
            }
        }
        curves.emplace_back(pCurve);
        curveIds.emplace_back(id);
    }
    if (curves.empty())
    {
        this->setError(ErrorCode::FILLEDSHEET_InvalidData, {});
        return false;
    }

    auto idsOf = [&curveIds](const std::vector<int>& indices) -> std::vector<wydb::ElementId>
    {
        std::vector<int> sorted(indices);
        std::sort(sorted.begin(), sorted.end());
        sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
        std::vector<wydb::ElementId> ids;
        ids.reserve(sorted.size());
        for (int index : sorted)
        {
            ids.emplace_back(curveIds[static_cast<std::size_t>(index)]);
        }
        return ids;
    };

    // Fuse endpoints by quantized coordinates and build adjacency
    std::map<IntVector3, int> vertexIndexMap;
    std::vector<VertexInfo> vertexInfos;
    auto getOrAddVertex = [this, &vertexIndexMap, &vertexInfos](const wy::Vector3& pnt) -> int
    {
        const IntVector3 key = quantize(pnt, this->_tol);
        auto iter = vertexIndexMap.find(key);
        if (iter != vertexIndexMap.end())
        {
            return iter->second;
        }
        const int index = static_cast<int>(vertexInfos.size());
        vertexIndexMap.emplace(key, index);
        vertexInfos.emplace_back();
        return index;
    };

    const std::size_t n = curves.size();
    std::vector<CurveEnds> curveEnds(n);
    for (std::size_t i = 0; i < n; ++i)
    {
        const SketchCurve3D* pCurve = curves[i];
        assert(pCurve);
        if (pCurve->isClosed())
        {
            // A self-closed curve contributes degree 2 to its single vertex
            const int idx = getOrAddVertex(pCurve->getStartPoint());
            curveEnds[i] = CurveEnds{ idx, idx };
            vertexInfos[idx].adjacentCurves.emplace_back(static_cast<int>(i));
            vertexInfos[idx].adjacentCurves.emplace_back(static_cast<int>(i));
        }
        else
        {
            const int idx1 = getOrAddVertex(pCurve->getStartPoint());
            const int idx2 = getOrAddVertex(pCurve->getEndPoint());
            curveEnds[i] = CurveEnds{ idx1, idx2 };
            vertexInfos[idx1].adjacentCurves.emplace_back(static_cast<int>(i));
            vertexInfos[idx2].adjacentCurves.emplace_back(static_cast<int>(i));
        }
    }

    // Every vertex must have exactly 2 incident curves (open end / branch /
    // T-junction rejected)
    std::vector<int> offendingCurves;
    for (const VertexInfo& info : vertexInfos)
    {
        if (info.adjacentCurves.size() != 2)
        {
            offendingCurves.insert(offendingCurves.cend(),
                info.adjacentCurves.cbegin(), info.adjacentCurves.cend());
        }
    }
    if (!offendingCurves.empty())
    {
        this->setError(ErrorCode::FILLEDSHEET_EdgesNotClosed, idsOf(offendingCurves));
        return false;
    }

    // Connectivity: all curves must belong to one loop
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
                for (int neighborCurve : vertexInfos[vi].adjacentCurves)
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
            this->setError(ErrorCode::FILLEDSHEET_EdgesNotClosed, idsOf(unreachable));
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
        for (int neighborCurve : vertexInfos[curEndVertex].adjacentCurves)
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
            this->setError(ErrorCode::FILLEDSHEET_EdgesNotClosed, idsOf(remaining));
            return false;
        }

        used[static_cast<std::size_t>(nextCurve)] = true;
        const CurveEnds& ends = curveEnds[static_cast<std::size_t>(nextCurve)];
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
        this->setError(ErrorCode::FILLEDSHEET_EdgesNotClosed, idsOf(remaining));
        return false;
    }

    return true;
}

NS_WY3D_END
