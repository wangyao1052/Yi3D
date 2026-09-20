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

#include <utils/wy3dSketch3DExtendGraph.h>
#include <utils/wy3dSketch3DCurveIntersectionUtil.h>

#include "Sketch3DCurveBox.h"

#include <cassert>
#include <vector>

#include <wy3dDatabase.h>
#include <wy3dMath.h>
#include <wy3dSketch3D.h>
#include <wy3dSketchCurve3D.h>
#include <wy3dSketchLine3D.h>
#include <wy3dSketchSpline3D.h>

NS_WY3D_BEG

namespace
{
using wy3d::detail::CurveBox3;
using wy3d::detail::computeCurveBox3;

// Whether the curve grows out along a straight support: a line's own support and a spline's end
// tangents. Returning true means the extension is a straight piece reaching up to `reach` past the
// ends, which is what the extended box below assumes.
bool growsStraight(const SketchCurve3D* pCurve)
{
    return SketchLine3D::cast(pCurve) != nullptr || SketchSpline3D::cast(pCurve) != nullptr;
}
} // namespace

Sketch3DExtendGraph::Sketch3DExtendGraph(const Sketch3D* pSketch3D, double tol)
    : _pSketch3D(pSketch3D), _tol(tol), _isValid(false)
{
    assert(_pSketch3D);
    if (_pSketch3D && this->init())
    {
        _isValid = true;
    }
}

wydb::Database* Sketch3DExtendGraph::getDatabase() const
{
    return _pSketch3D ? _pSketch3D->getDatabase() : nullptr;
}

bool Sketch3DExtendGraph::init()
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

    std::vector<CurveBox3> boxes(curves.size());
    CurveBox3 whole;
    for (std::size_t i = 0; i < curves.size(); ++i)
    {
        Sketch3DExtendNodeSPtr pNode = std::make_shared<Sketch3DExtendNode>(curves[i]->getId());
        if (curves[i]->isClosed())
        {
            pNode->setIsClosed(true);
        }
        _id2Node[curves[i]->getId()] = std::move(pNode);

        if (!computeCurveBox3(curves[i], boxes[i]) || boxes[i].isEmpty()) continue;
        whole.expand(wy::Vector3(boxes[i].min[0], boxes[i].min[1], boxes[i].min[2]));
        whole.expand(wy::Vector3(boxes[i].max[0], boxes[i].max[1], boxes[i].max[2]));
    }

    // How far an extension is allowed to travel. Every curve lies inside `whole`, and an extension
    // starts from a point inside it too, so the diagonal is enough to reach anything reachable -
    // and keeps the support curve's parameter range finite, which OCCT insists on.
    const double reach = whole.diagonal();

    std::vector<wy::Vector3> intPnts;
    intPnts.reserve(5);

    for (std::size_t i = 0; i < curves.size(); ++i)
    {
        // A closed curve has nowhere to grow, so it is only ever the thing being reached for.
        if (curves[i]->isClosed()) continue;
        if (boxes[i].isEmpty()) continue;

        Sketch3DExtendNodeSPtr pNode = _id2Node[curves[i]->getId()];
        assert(pNode);

        // A spline grows along an explicit tangent ray; everything else that can grow does so along
        // its own support curve, which widening is enough for.
        const bool straight = growsStraight(curves[i]);
        const Sketch3DCurveIntersectionUtil::Operand operand = Sketch3DCurveIntersectionUtil::extended(
            curves[i], reach, SketchSpline3D::cast(curves[i]) != nullptr, SketchSpline3D::cast(curves[i]) != nullptr);

        // The box that extension could cover. A straight one reaches `reach` past the ends in any
        // direction, so the curve's own box grown by `reach` contains it; an arc grows around the
        // circle it was cut from, which the box already covers.
        CurveBox3 extendedBox = boxes[i];
        if (straight)
        {
            extendedBox.inflate(reach);
        }

        for (std::size_t j = 0; j < curves.size(); ++j)
        {
            if (j == i) continue;
            if (boxes[j].isEmpty()) continue;
            if (!extendedBox.overlaps(boxes[j])) continue;

            intPnts.clear();
            const Sketch3DCurveIntersectionUtil::Result result = Sketch3DCurveIntersectionUtil::intersect(
                operand, Sketch3DCurveIntersectionUtil::bounded(curves[j]), intPnts, _tol);
            if (Sketch3DCurveIntersectionUtil::Result::Ok != result) continue;

            Sketch3DExtendNodeSPtr pNodeOther = _id2Node[curves[j]->getId()];
            assert(pNodeOther);
            for (const wy::Vector3& intPnt : intPnts)
            {
                pNode->appendKnot(intPnt, curves[j]->getId());
                if (!pNodeOther->isClosed())
                {
                    pNodeOther->appendKnot(intPnt, pNode->getId());
                }
            }
        }
    }

    for (auto& kvp : _id2Node)
    {
        kvp.second->refresh(pDb);
    }

    return true;
}

Sketch3DExtendNodeSPtr Sketch3DExtendGraph::getNode(const wydb::ElementId& id) const
{
    auto iter = _id2Node.find(id);
    if (iter == _id2Node.cend())
    {
        return nullptr;
    }
    return iter->second;
}

bool Sketch3DExtendGraph::addNode(Sketch3DExtendNodeSPtr pNode)
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

Sketch3DExtendSegment Sketch3DExtendGraph::pick(const wydb::ElementId& id, const wy::Vector3& pos)
{
    auto iter = _id2Node.find(id);
    if (iter == _id2Node.cend())
    {
        return Sketch3DExtendSegment();
    }

    return iter->second->pick(this, pos, _tol);
}

NS_WY3D_END
