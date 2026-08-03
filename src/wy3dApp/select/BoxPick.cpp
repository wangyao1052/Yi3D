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

#include "select/BoxPick.h"
#include <list>
#include "select/PickCommon.h"
#include "application/Application.h"
#include "scene/Scene.h"
#include "scene/nodes/ElementNode.h"
#include "scene/nodes/SolidElementNode.h"
#include "scene/nodes/SketchElementNode.h"
#include "OsgSelectUtils.h"

struct TraverseItem
{
    wydb::ElementId id;
    const osgUtil::PolytopeIntersector::Intersection* pIntersection;

    TraverseItem() : id(wydb::ElementId::kNull), pIntersection(nullptr) {}

    TraverseItem(const wydb::ElementId& argElemId,
        const osgUtil::PolytopeIntersector::Intersection* pArgIntersection)
        : id(argElemId), pIntersection(pArgIntersection) {}
};

std::list<TraverseItem> _traverseIntersections(
    const wydb::Database* pDb,
    const osgUtil::PolytopeIntersector::Intersections& intersections,
    const BoxPickOption& option,
    DrawMode allowedDrawMode = DrawMode::All)
{
    assert(pDb);
    std::list<TraverseItem> items;

    if (option.pSelPreFilter)
    {
        wydb::ElementId id = wydb::ElementId::kNull;
        for (auto iter = intersections.cbegin(); iter != intersections.cend(); ++iter)
        {
            const osgUtil::PolytopeIntersector::Intersection& intersection = *iter;
            osg::Geometry* geom = dynamic_cast<osg::Geometry*>(intersection.drawable.get());
            if (!geom) continue;

            // added by wangyao 2025.05.06 {
            // 在选择实体边时(wy3d::SelectionType::SolidEdge)
            // 如果拾取到的是GL_TRIANGLES则继续
            DrawMode curDrawMode = PickCommon::getDrawableModeOfGeometry(geom);
            if (!(static_cast<unsigned int>(curDrawMode) & static_cast<unsigned int>(allowedDrawMode)))
            {
                continue;
            }
            // }

            unsigned int idValue(0), idXData(0);
            if (!geom->getUserValue("ElementId", idValue))
            {
                id = wydb::ElementId::kNull;
            }
            else
            {
                id = wydb::ElementId(idValue);
            }

            switch ((*option.pSelPreFilter)(pDb, id, SelectAction::Crossing))
            {
            case SelectFilterStatus::Ok:
            {
                if (!id.isNull())
                {
                    items.emplace_back(TraverseItem(id, &intersection));
                }
            }
            break;

            case SelectFilterStatus::Continue:
            break;

            case SelectFilterStatus::Break:
            default:
            {
                return items;
            }
            break;
            }
        }

        return items;
    }
    else
    {
        for (auto iter = intersections.cbegin(); iter != intersections.cend(); ++iter)
        {
            const osgUtil::PolytopeIntersector::Intersection& intersection = *iter;
            osg::Geometry* geom = dynamic_cast<osg::Geometry*>(intersection.drawable.get());
            if (!geom) continue;

            // added by wangyao 2025.05.06 {
            // 在选择实体边时(wy3d::SelectionType::SolidEdge)
            // 如果拾取到的是GL_TRIANGLES则继续
            DrawMode curDrawMode = PickCommon::getDrawableModeOfGeometry(geom);
            if (!(static_cast<unsigned int>(curDrawMode) & static_cast<unsigned int>(allowedDrawMode)))
            {
                continue;
            }
            // }

            unsigned int idValue(0), idXData(0);
            if (!geom->getUserValue("ElementId", idValue))
            {
                continue;
            }

            wydb::ElementId id = wydb::ElementId(idValue);
            if (!id.isNull())
            {
                items.emplace_back(TraverseItem(id, &intersection));
            }
        }

        return items;
    }

    return items;
}

// 在_traverseIntersections的基础上仅仅加了一小段逻辑:循环中如果items不为空则返回
std::list<TraverseItem> _traverseIntersections_OneBreak(
    const wydb::Database* pDb,
    const osgUtil::PolytopeIntersector::Intersections& intersections,
    const BoxPickOption& option,
    DrawMode allowedDrawMode = DrawMode::All)
{
    assert(pDb);
    std::list<TraverseItem> items;

    if (option.pSelPreFilter)
    {
        wydb::ElementId id = wydb::ElementId::kNull;
        for (auto iter = intersections.cbegin(); iter != intersections.cend(); ++iter)
        {
            const osgUtil::PolytopeIntersector::Intersection& intersection = *iter;
            osg::Geometry* geom = dynamic_cast<osg::Geometry*>(intersection.drawable.get());
            if (!geom) continue;

            // added by wangyao 2025.05.06 {
            // 在选择实体边时(wy3d::SelectionType::SolidEdge)
            // 如果拾取到的是GL_TRIANGLES则继续
            DrawMode curDrawMode = PickCommon::getDrawableModeOfGeometry(geom);
            if (!(static_cast<unsigned int>(curDrawMode) & static_cast<unsigned int>(allowedDrawMode)))
            {
                continue;
            }
            // }

            unsigned int idValue(0), idXData(0);
            if (!geom->getUserValue("ElementId", idValue))
            {
                id = wydb::ElementId::kNull;
            }
            else
            {
                id = wydb::ElementId(idValue);
            }

            switch ((*option.pSelPreFilter)(pDb, id, SelectAction::Crossing))
            {
            case SelectFilterStatus::Ok:
            {
                if (!id.isNull())
                {
                    items.emplace_back(TraverseItem(id, &intersection));
                }
            }
            break;

            case SelectFilterStatus::Continue:
            break;

            case SelectFilterStatus::Break:
            default:
            {
                return items;
            }
            break;
            }

            if (!items.empty())
            {
                return items;
            }
        }

        return items;
    }
    else
    {
        for (auto iter = intersections.cbegin(); iter != intersections.cend(); ++iter)
        {
            const osgUtil::PolytopeIntersector::Intersection& intersection = *iter;
            osg::Geometry* geom = dynamic_cast<osg::Geometry*>(intersection.drawable.get());
            if (!geom) continue;

            // added by wangyao 2025.05.06 {
            // 在选择实体边时(wy3d::SelectionType::SolidEdge)
            // 如果拾取到的是GL_TRIANGLES则继续
            DrawMode curDrawMode = PickCommon::getDrawableModeOfGeometry(geom);
            if (!(static_cast<unsigned int>(curDrawMode) & static_cast<unsigned int>(allowedDrawMode)))
            {
                continue;
            }
            // }

            unsigned int idValue(0), idXData(0);
            if (!geom->getUserValue("ElementId", idValue))
            {
                continue;
            }

            wydb::ElementId id = wydb::ElementId(idValue);
            if (!id.isNull())
            {
                items.emplace_back(TraverseItem(id, &intersection));
            }

            if (!items.empty())
            {
                return items;
            }
        }

        return items;
    }

    return items;
}

wyap::SelectionSet BoxPick::pick(
    const wydb::Database* pDb,
    osgViewer::View* pView,
    double xMin, double yMin, double xMax, double yMax,
    const BoxPickOption& option)
{
    assert(pDb);
    assert(pView);
    if (!pDb || !pView || !pView->getCamera()) return wyap::SelectionSet();

    if (xMin > xMax) std::swap(xMin, xMax);
    if (yMin > yMax) std::swap(yMin, yMax);

    // 框选不支持同时选择多种类型,只支持一种类型
    // Element or SolidBody or SolidFace or SolidEdge or SolidVertex or SketchCurve
    // 不支持它们的组合
    assert(option.selType == wy3d::SelectionType::Element
        || option.selType == wy3d::SelectionType::SolidBody
        || option.selType == wy3d::SelectionType::SolidFace
        || option.selType == wy3d::SelectionType::SolidEdge
        || option.selType == wy3d::SelectionType::SolidVertex
        || option.selType == wy3d::SelectionType::SketchCurve);

    wyap::SelectionSet ss;
    if (option.selType == wy3d::SelectionType::Element)
    {
        // 使用OSG多面体求交器
        osg::ref_ptr<osgUtil::PolytopeIntersector> intersector = new osgUtil::PolytopeIntersector(
            osgUtil::Intersector::WINDOW, xMin, yMin, xMax, yMax);
        intersector->setIntersectionLimit(osgUtil::Intersector::IntersectionLimit::LIMIT_ONE_PER_DRAWABLE); // 优化性能
        osgUtil::IntersectionVisitor iv(intersector.get());
        iv.setTraversalMask(option.pickMask);
        pView->getCamera()->accept(iv);
        const osgUtil::PolytopeIntersector::Intersections& intersections = intersector->getIntersections();
        if (intersections.empty()) return ss;

        // 结果
        std::list<TraverseItem> items = _traverseIntersections(pDb, intersections, option, DrawMode::All);
        if (items.empty()) return ss;
        std::set<wydb::ElementId> ids;
        for (const TraverseItem& item : items)
        {
            assert(!item.id.isNull());
            ids.insert(item.id);
        }

        // 选择集结果
        for (const wydb::ElementId& id : ids)
        {
            ss.add(wyap::Selection(id));
        }
    }
    else if (option.selType == wy3d::SelectionType::SolidEdge 
        || option.selType == wy3d::SelectionType::SolidFace
        || option.selType == wy3d::SelectionType::SketchCurve)
    {
        // 框选实体边(面)时,先获取Pick到的第一个元素;再去Pick该元素的边(面);
        // 参照SolidWorks倒圆角时,框选只支持选择Pick到的第一个特征的边;

        // 使用OSG多面体求交器
        osg::ref_ptr<osgUtil::PolytopeIntersector> intersector = new osgUtil::PolytopeIntersector(
            osgUtil::Intersector::WINDOW, xMin, yMin, xMax, yMax);
        intersector->setIntersectionLimit(osgUtil::Intersector::IntersectionLimit::LIMIT_ONE_PER_DRAWABLE); // 优化性能
        osgUtil::IntersectionVisitor iv(intersector.get());
        iv.setTraversalMask(option.pickMask);
        pView->getCamera()->accept(iv);
        const osgUtil::PolytopeIntersector::Intersections& intersections = intersector->getIntersections();
        if (intersections.empty()) return ss;

        // 选择最先Pick到的元素
        std::list<TraverseItem> items = _traverseIntersections_OneBreak(pDb, intersections, option, DrawMode::All);
        if (items.empty()) return ss;
        assert(items.size() == 1);
        wydb::ElementId id = items.front().id;
        assert(!id.isNull());

        // 获取实体元素节点
        SolidElementNode* pSolidElemNode(nullptr);
        SketchElementNode* pSketchElemNode(nullptr);
        osg::Group* pOsgNode(nullptr);
        if (option.selType == wy3d::SelectionType::SolidEdge
            || option.selType == wy3d::SelectionType::SolidFace)
        {
            pSolidElemNode = dynamic_cast<SolidElementNode*>(
                Application::instance().getActiveScene()->getElementNode(id));
            if (pSolidElemNode) pOsgNode = pSolidElemNode->getOsgNode();
        }
        else if (option.selType == wy3d::SelectionType::SketchCurve)
        {
            pSketchElemNode = dynamic_cast<SketchElementNode*>(
                Application::instance().getActiveScene()->getElementNode(id));
            if (pSketchElemNode) pOsgNode = pSketchElemNode->getOsgNode();
        }
        if (!pOsgNode)
        {
            return ss;
        }
        if (!pSolidElemNode && !pSketchElemNode)
        {
            assert(false);
            return ss;
        }

        // 依据窗口矩形构造世界坐标系下的多面体
        osg::Polytope polytope;
        OsgSelectUtils::initPolytope(pView->getCamera(), xMin, yMin, xMax, yMax, polytope);

        // 对元素节点使用多面体求交器
        intersector = new osgUtil::PolytopeIntersector(osgUtil::Intersector::PROJECTION, polytope);
        osgUtil::IntersectionVisitor ivDetail(intersector.get());
        ivDetail.setTraversalMask(option.pickMask);
        pOsgNode->accept(ivDetail);
        const osgUtil::PolytopeIntersector::Intersections& detailIntersections = intersector->getIntersections();
        if (detailIntersections.empty()) return ss;

        // 遍历求交结果
        unsigned int idValue(0), idXData(0);
        DrawMode allowedDrawMode = DrawMode::Undefined;
        if (option.selType == wy3d::SelectionType::SolidEdge || option.selType == wy3d::SelectionType::SketchCurve)
        {
            allowedDrawMode = DrawMode::Edge;
        }
        else if (option.selType == wy3d::SelectionType::SolidFace)
        {
            allowedDrawMode = DrawMode::Face;
        }
        else
        {
            assert(false);
            allowedDrawMode = DrawMode::Undefined;
        }
        for (auto iter = detailIntersections.cbegin(); iter != detailIntersections.cend(); ++iter)
        {
            const osgUtil::PolytopeIntersector::Intersection& intersection = *iter;
            osg::Geometry* geom = dynamic_cast<osg::Geometry*>(intersection.drawable.get());
            if (!geom) continue;

            DrawMode curDrawMode = PickCommon::getDrawableModeOfGeometry(geom);
            if (!(static_cast<unsigned int>(curDrawMode) & static_cast<unsigned int>(allowedDrawMode)))
            {
                continue;
            }

            if (!geom->getUserValue("ElementId", idValue))
            {
                continue;
            }

            if (option.selType == wy3d::SelectionType::SolidEdge && pSolidElemNode)
            {
                unsigned int edgeIndex = pSolidElemNode->getEdgeIndex(intersection.primitiveIndex);
                if (-1 == edgeIndex)
                {
                    assert(false);
                    continue;
                }
                wyap::Selection edgeSel(static_cast<unsigned int>(wy3d::SelectionType::SolidEdge), id, std::to_string(edgeIndex));
                ss.add(edgeSel);
            }
            else if (option.selType == wy3d::SelectionType::SolidFace && pSolidElemNode)
            {
                unsigned int faceIndex = pSolidElemNode->getFaceIndex(intersection.primitiveIndex);
                if (-1 == faceIndex)
                {
                    assert(false);
                    continue;
                }
                wyap::Selection faceSel(static_cast<unsigned int>(wy3d::SelectionType::SolidFace), id, std::to_string(faceIndex));
                ss.add(faceSel);
            }
            else if (option.selType == wy3d::SelectionType::SketchCurve && pSketchElemNode)
            {
                unsigned int curveId(0);
                if (geom == pSketchElemNode->getCenterLinesGeom())
                {
                    curveId = pSketchElemNode->getCenterLineCurveId(intersection.primitiveIndex);
                }
                else
                {
                    curveId = pSketchElemNode->getCurveId(intersection.primitiveIndex);
                }
                if (0 == curveId)
                {
                    assert(false);
                    continue;
                }
                wyap::Selection sketchCurveSel(static_cast<unsigned int>(wy3d::SelectionType::SketchCurve), id, std::to_string(curveId));
                ss.add(sketchCurveSel);
            }
            else
            {
                assert(false);
                break;
            }
        }
    }
    else
    {
        assert(false);
    }

    // 用户自定义过滤
    if (option.pSelFilter)
    {
        wyap::SelectionSet ssFiltered;
        for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
        {
            switch ((*option.pSelFilter)(pDb, iter.current(), SelectAction::Crossing))
            {
            case SelectFilterStatus::Ok:
            {
                ssFiltered.add(iter.current());
            }
            break;

            case SelectFilterStatus::Continue:
            {
            }
            break;

            case SelectFilterStatus::Break:
            {
                return ssFiltered;
            }
            break;

            default:
            {
                assert(false);
            }
            break;
            }
        }
        ss.swap(ssFiltered);
    }

    return ss;
}