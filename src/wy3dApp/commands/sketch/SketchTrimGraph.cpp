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

#include "SketchTrimGraph.h"
#include <set>
#include <algorithm>
#include <cassert>
#include <wyVector2.h>
#include <wydbDatabase.h>
#include <wy3dSketchCurve.h>
#include <wy3dSketchCurve.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>
#include <wy3dSketchEllipse.h>
#include <wy3dSketchEllipseArc.h>

#include "application/Application.h"

SketchTrimGraph::SketchTrimGraph(const wy3d::Sketch* pSketch, double tol) : _pSketch(pSketch), _tol(tol), _isValid(false)
{
    assert(_pSketch);
    if (_pSketch && this->init())
    {
        _isValid = true;
    }
}

bool SketchTrimGraph::init()
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
    std::map<wydb::ElementId, const wy3d::SketchCurve*> id2Curve;
    for (auto iter = _pSketch->createIterator(); !iter.isDone(); iter.moveNext())
    {
        const wy3d::SketchCurve* pSketchCurve = wy3d::SketchCurve::cast(pDb->getElement(iter.current()));
        if (!pSketchCurve) continue;
        curves.emplace_back(pSketchCurve);
        id2Curve[iter.current()] = pSketchCurve;
    }

    // 初始化结点
    for (const wy3d::SketchCurve* pSketchCurve : curves)
    {
        SketchTrimNodeSPtr pNode = std::make_shared<SketchTrimNode>(pSketchCurve->getId());
        if (!pSketchCurve->isClosed())
        {
            // 非闭合曲线添加起点
            pNode->appendKnot(SketchTrimKnot(pSketchCurve->getStartPoint(), 0.0));
        }
        _id2Node[pSketchCurve->getId()] = std::move(pNode);
    }

    // 构建区域树
    struct RTreeNode
    {
        wydb::ElementId id;
        size_t index;

        RTreeNode(wydb::ElementId inId = wydb::ElementId::kNull, size_t inIndex = -1) : id(inId), index(inIndex) {}
    };
    RTree<RTreeNode, double, 2> rtree;
    std::vector<wy3d::geom::BoundingBox2> bboxs;
    bboxs.resize(curves.size());
    for (size_t i = 0; i < curves.size(); ++i)
    {
        const wy3d::SketchCurve* pSketchCurve = curves[i];
        wy3d::geom::BoundingBox2 bbox = pSketchCurve->getBoundingBox();
        if (bbox.isEmpty()) continue;
        // 向外扩张一点点(用于处理水平竖直直线的情形)
        bbox.set(wy::Vector2(bbox.min().x() - wy3d::EPS, bbox.min().y() - wy3d::EPS),
                 wy::Vector2(bbox.max().x() + wy3d::EPS, bbox.max().y() + wy3d::EPS));
        bboxs[i] = bbox;
        double min[2] = { bbox.min().x(), bbox.min().y() };
        double max[2] = { bbox.max().x(), bbox.max().y() };
        rtree.Insert(min, max, RTreeNode(pSketchCurve->getId(), i));
    }
    
    // 遍历求出所有曲线的交点信息
    std::vector<size_t> candidates;
    candidates.reserve(curves.size());
    std::set<std::pair<size_t, size_t>> intersected;
    for (size_t i = 0; i < curves.size(); ++i)
    {
        // 空间搜索快速找出候选曲线
        candidates.clear();
        const wy3d::geom::BoundingBox2& bbox = bboxs[i];
        if (bbox.isEmpty()) continue;
        double min[2] = { bbox.min().x(), bbox.min().y() };
        double max[2] = { bbox.max().x(), bbox.max().y() };
        rtree.Search(min, max, [&candidates](const RTreeNode& rtreeNode) {
            candidates.emplace_back(rtreeNode.index);
            return true;}
        );

        // 当前曲线
        const wy3d::SketchCurve* pSketchCurve = curves[i];
        assert(pSketchCurve);        SketchTrimNodeSPtr pNode = _id2Node[pSketchCurve->getId()];
        assert(pNode);

        // 求出曲线上的所有交点
        std::vector<wy::Vector2> intPnts;
        intPnts.reserve(5);
        for (size_t index : candidates)
        {
            // 排除本身
            if (index == i) continue;
            // 之前已经求过交点了
            if (intersected.find(std::pair<size_t, size_t>(i, index)) != intersected.cend())
            {
                continue;
            }

            // 交点
            const wy3d::SketchCurve* pSketchCurveOther = curves[index];
            assert(pSketchCurveOther);            intPnts.clear();
            unsigned int num = pSketchCurve->intersectWith(*pSketchCurveOther, intPnts);
            intersected.insert(std::pair<size_t, size_t>(index, i)); // 标记已经求过交点了
            if (0 == num) continue;
            SketchTrimNodeSPtr pNodeOther = _id2Node[pSketchCurveOther->getId()];
            assert(pNodeOther);
            for (const wy::Vector2& intPnt : intPnts)
            {
                pNode->appendKnot(intPnt, pNodeOther->getId());
                pNodeOther->appendKnot(intPnt, pNode->getId());
            }
        }
    }

    // 对于非闭合曲线,添加终点Knot
    for (auto& kvp : _id2Node)
    {
        SketchTrimNodeSPtr pNode = kvp.second;
        assert(pNode);
        auto iter = id2Curve.find(kvp.first);
        if (iter == id2Curve.cend())
        {
            assert(false);
            continue;
        }
        if (!iter->second->isClosed())
        {
            pNode->appendKnot(SketchTrimKnot(iter->second->getEndPoint(), 1.0));
        }
    }

    // 刷新标记点参数
    for (auto& kvp : _id2Node)
    {
        kvp.second->refresh(pDb);
    }

    return true;
}

SketchTrimNodeSPtr SketchTrimGraph::getNode(const wydb::ElementId& id) const
{
    auto iter = _id2Node.find(id);
    if (iter == _id2Node.cend())
    {
        return nullptr;
    }
    return iter->second;
}

bool SketchTrimGraph::addNode(SketchTrimNodeSPtr pNode)
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

SketchTrimSegment SketchTrimGraph::pick(const wydb::ElementId& id, const wy::Vector2& position)
{
    auto iter = _id2Node.find(id);
    if (iter == _id2Node.cend())
    {
        // 返回无效的修剪段
        SketchTrimSegment segment;
        return segment;
    }

    return iter->second->pick(this, position);
}