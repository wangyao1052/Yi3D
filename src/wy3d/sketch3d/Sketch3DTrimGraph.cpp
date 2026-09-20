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

#include <utils/wy3dSketch3DTrimGraph.h>
#include <utils/wy3dSketch3DCurveIntersectionUtil.h>

#include "Sketch3DCurveBox.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <set>
#include <utility>
#include <vector>

#include <RTree/RTree.h>

#include <wy3dDatabase.h>
#include <wy3dMath.h>
#include <wy3dSketch3D.h>
#include <wy3dSketchCurve3D.h>

NS_WY3D_BEG

namespace
{
using wy3d::detail::CurveBox3;
using wy3d::detail::computeCurveBox3;

// A pair of curve indices, ordered so each pair is met exactly once.
std::pair<std::size_t, std::size_t> orderedPair(std::size_t a, std::size_t b)
{
    return a < b ? std::make_pair(a, b) : std::make_pair(b, a);
}
} // namespace

Sketch3DTrimGraph::Sketch3DTrimGraph(const Sketch3D* pSketch3D, double tol)
    : _pSketch3D(pSketch3D), _tol(tol), _isValid(false)
{
    assert(_pSketch3D);
    if (_pSketch3D && this->init())
    {
        _isValid = true;
    }
}

wydb::Database* Sketch3DTrimGraph::getDatabase() const
{
    return _pSketch3D ? _pSketch3D->getDatabase() : nullptr;
}

bool Sketch3DTrimGraph::init()
{
    assert(_pSketch3D);
    wydb::Database* pDb = _pSketch3D->getDatabase();
    if (!pDb)
    {
        assert(false);
        return false;
    }

    std::vector<const SketchCurve3D*> curves;
    curves.reserve(100);
    for (auto iter = _pSketch3D->createIterator(); !iter.isDone(); iter.moveNext())
    {
        const SketchCurve3D* pCurve = SketchCurve3D::cast(pDb->getElement(iter.current()));
        if (!pCurve) continue;
        curves.emplace_back(pCurve);
    }

    for (const SketchCurve3D* pCurve : curves)
    {
        Sketch3DTrimNodeSPtr pNode = std::make_shared<Sketch3DTrimNode>(pCurve->getId());
        if (!pCurve->isClosed())
        {
            pNode->appendKnot(pCurve->getStartPoint(), wydb::ElementId::kNull);
        }
        _id2Node[pCurve->getId()] = std::move(pNode);
    }

    // Bounding boxes, and with them the reach an extension is allowed to travel.
    struct RTreeNode
    {
        std::size_t index;

        explicit RTreeNode(std::size_t inIndex = 0) : index(inIndex) {}
    };

    std::vector<CurveBox3> boxes(curves.size());
    RTree<RTreeNode, double, 3> rtree;
    for (std::size_t i = 0; i < curves.size(); ++i)
    {
        if (!computeCurveBox3(curves[i], boxes[i]))
        {
            continue;
        }
        if (boxes[i].isEmpty())
        {
            continue;
        }
        boxes[i].inflate(wy3d::EPS);
        rtree.Insert(boxes[i].min, boxes[i].max, RTreeNode(i));
    }

    std::vector<std::size_t> candidates;
    candidates.reserve(curves.size());
    std::set<std::pair<std::size_t, std::size_t>> intersected;
    std::vector<wy::Vector3> intPnts;
    intPnts.reserve(5);

    for (std::size_t i = 0; i < curves.size(); ++i)
    {
        if (boxes[i].isEmpty())
        {
            continue;
        }

        candidates.clear();
        rtree.Search(boxes[i].min, boxes[i].max, [&candidates](const RTreeNode& rtreeNode) {
            candidates.emplace_back(rtreeNode.index);
            return true; });

        Sketch3DTrimNodeSPtr pNode = _id2Node[curves[i]->getId()];
        assert(pNode);

        for (const std::size_t index : candidates)
        {
            if (index == i) continue;
            if (!intersected.insert(orderedPair(i, index)).second) continue;

            intPnts.clear();
            const Sketch3DCurveIntersectionUtil::Result result = Sketch3DCurveIntersectionUtil::intersect(
                Sketch3DCurveIntersectionUtil::bounded(curves[i]),
                Sketch3DCurveIntersectionUtil::bounded(curves[index]), intPnts, _tol);
            if (Sketch3DCurveIntersectionUtil::Result::Ok != result) continue;

            Sketch3DTrimNodeSPtr pNodeOther = _id2Node[curves[index]->getId()];
            assert(pNodeOther);
            for (const wy::Vector3& intPnt : intPnts)
            {
                pNode->appendKnot(intPnt, curves[index]->getId());
                pNodeOther->appendKnot(intPnt, pNode->getId());
            }
        }
    }

    // An open curve ends where it ends; that is a knot like any other.
    for (const SketchCurve3D* pCurve : curves)
    {
        if (pCurve->isClosed()) continue;
        Sketch3DTrimNodeSPtr pNode = _id2Node[pCurve->getId()];
        assert(pNode);
        pNode->appendKnot(pCurve->getEndPoint(), wydb::ElementId::kNull);
    }

    for (auto& kvp : _id2Node)
    {
        kvp.second->refresh(pDb);
    }

    return true;
}

Sketch3DTrimNodeSPtr Sketch3DTrimGraph::getNode(const wydb::ElementId& id) const
{
    auto iter = _id2Node.find(id);
    if (iter == _id2Node.cend())
    {
        return nullptr;
    }
    return iter->second;
}

bool Sketch3DTrimGraph::addNode(Sketch3DTrimNodeSPtr pNode)
{
    if (!pNode) return false;
    const wydb::ElementId id = pNode->getId();
    if (_id2Node.find(id) != _id2Node.cend())
    {
        return false;
    }
    _id2Node[id] = pNode;
    return true;
}

Sketch3DTrimSegment Sketch3DTrimGraph::pick(const wydb::ElementId& id, const wy::Vector3& pos)
{
    auto iter = _id2Node.find(id);
    if (iter == _id2Node.cend())
    {
        return Sketch3DTrimSegment();
    }

    return iter->second->pick(this, pos, _tol);
}

NS_WY3D_END
