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
#include <cmath>
#include <map>
#include <algorithm>

#include <wydbDatabase.h>
#include <wy3dImpl.h>
#include <wy3dSketch3D.h>
#include <wy3dSketchCurve3D.h>
#include <wy3dSketchCircle3D.h>
#include <wy3dSketchArc3D.h>
#include <wy3dSketchEllipse3D.h>
#include <wy3dSketchEllipseArc3D.h>

#include "Sketch3DCurveGraph.h"

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

    // 平面无效只可能来自反序列化出的畸形文件,按精确类取法向校验
    bool hasUsableNormal(const wy3d::SketchCurve3D* pCurve)
    {
        assert(pCurve);
        const wyrx::ClassInfo* pCurveClassInfo = pCurve->getClassInfo();
        if (wy3d::SketchCircle3D::classInfo() == pCurveClassInfo)
        {
            return static_cast<const wy3d::SketchCircle3D*>(pCurve)->getNormal().length() >= 0.5;
        }
        else if (wy3d::SketchArc3D::classInfo() == pCurveClassInfo)
        {
            return static_cast<const wy3d::SketchArc3D*>(pCurve)->getNormal().length() >= 0.5;
        }
        else if (wy3d::SketchEllipse3D::classInfo() == pCurveClassInfo)
        {
            return static_cast<const wy3d::SketchEllipse3D*>(pCurve)->getNormal().length() >= 0.5;
        }
        else if (wy3d::SketchEllipseArc3D::classInfo() == pCurveClassInfo)
        {
            return static_cast<const wy3d::SketchEllipseArc3D*>(pCurve)->getNormal().length() >= 0.5;
        }
        return true;
    }
}

Sketch3DCurveGraph::Sketch3DCurveGraph(const Sketch3D* pSketch3D, double tol)
    : _pSketch3D(pSketch3D), _tol(tol > 0.0 ? tol : 1e-5), _isCollected(false),
      _rejectedCurveId(wydb::ElementId::kNull)
{
    assert(_pSketch3D);
    _isCollected = this->collect() && this->fuseVertices();
}

bool Sketch3DCurveGraph::collect()
{
    assert(_pSketch3D);
    const wydb::Database* pDb = _pSketch3D->getDatabase();
    if (!pDb)
    {
        assert(false);
        return false;
    }

    // Reject degenerated curves the same way Sketch3DTopoBuilder::makeEdge does
    for (wy::Iterator<wydb::ElementId> iter = _pSketch3D->createIterator(); !iter.isDone(); iter.moveNext())
    {
        const wydb::ElementId id = iter.current();
        if (id.isNull())
        {
            assert(false);
            return false;
        }
        const wydb::Element* pElem = pDb->getElement(id);
        const wy3d::SketchCurve3D* pCurve = wy3d::SketchCurve3D::cast(pElem);
        if (!pCurve)
        {
            assert(false);
            return false;
        }
        if (pCurve->isDegenerate(wy3d::TOL))
        {
            _rejectedCurveId = id;
            return false;
        }
        if (!hasUsableNormal(pCurve))
        {
            assert(false);
            _rejectedCurveId = id;
            return false;
        }

        _curves.emplace_back(pCurve);
        _curveIds.emplace_back(id);
    }

    return true;
}

bool Sketch3DCurveGraph::fuseVertices()
{
    // Fuse endpoints by quantized coordinates and build adjacency
    std::map<IntVector3, int> vertexIndexMap;
    auto getOrAddVertex = [this, &vertexIndexMap](const wy::Vector3& pnt) -> int
    {
        const IntVector3 key = quantize(pnt, this->_tol);
        auto iter = vertexIndexMap.find(key);
        if (iter != vertexIndexMap.end())
        {
            return iter->second;
        }
        const int index = static_cast<int>(_vertexInfos.size());
        vertexIndexMap.emplace(key, index);
        _vertexInfos.emplace_back();
        return index;
    };

    const std::size_t n = _curves.size();
    _curveEnds.resize(n);
    for (std::size_t i = 0; i < n; ++i)
    {
        const SketchCurve3D* pCurve = _curves[i];
        assert(pCurve);
        const int curveIndex = static_cast<int>(i);
        if (pCurve->isClosed())
        {
            // A self-closed curve contributes degree 2 to its single vertex
            const int idx = getOrAddVertex(pCurve->getStartPoint());
            _curveEnds[i] = CurveEnds{ idx, idx };
            _vertexInfos[idx].adjacentCurves.emplace_back(curveIndex);
            _vertexInfos[idx].adjacentCurves.emplace_back(curveIndex);
            _vertexInfos[idx].outgoingCurves.emplace_back(curveIndex);
            _vertexInfos[idx].incomingCurves.emplace_back(curveIndex);
        }
        else
        {
            const int idx1 = getOrAddVertex(pCurve->getStartPoint());
            const int idx2 = getOrAddVertex(pCurve->getEndPoint());
            _curveEnds[i] = CurveEnds{ idx1, idx2 };
            _vertexInfos[idx1].adjacentCurves.emplace_back(curveIndex);
            _vertexInfos[idx2].adjacentCurves.emplace_back(curveIndex);
            _vertexInfos[idx1].outgoingCurves.emplace_back(curveIndex);
            _vertexInfos[idx2].incomingCurves.emplace_back(curveIndex);
        }
    }

    return true;
}

std::vector<wydb::ElementId> Sketch3DCurveGraph::idsOfCurves(const std::vector<int>& indices) const
{
    std::vector<int> sorted(indices);
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());

    std::vector<wydb::ElementId> ids;
    ids.reserve(sorted.size());
    for (int index : sorted)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= _curveIds.size())
        {
            assert(false);
            continue;
        }
        ids.emplace_back(_curveIds[static_cast<std::size_t>(index)]);
    }
    return ids;
}

NS_WY3D_END
