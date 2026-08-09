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

#include "PointPick.h"
#include "select/PickCommon.h"
#include "application/Application.h"
#include "scene/Scene.h"
#include "scene/nodes/ElementNode.h"
#include "scene/nodes/SolidElementNode.h"
#include "scene/nodes/SheetElementNode.h"
#include "scene/nodes/SketchElementNode.h"
#include "utils/MathUtils.h"

template<typename T>
bool _traverseIntersections(
    const wydb::Database* pDb,
    const typename T::Intersections& intersections,
    wydb::ElementId& pickedElemId,
    const typename T::Intersection*& pIntersection,
    const PointPickOption& option,
    DrawMode allowedDrawMode = DrawMode::All)
{
    if (option.pSelPreFilter)
    {
        wydb::ElementId id = wydb::ElementId::kNull;
        for (auto iter = intersections.cbegin(); iter != intersections.cend(); ++iter)
        {
            const typename T::Intersection& intersection = *iter;
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

            switch ((*option.pSelPreFilter)(pDb, id, SelectAction::Point))
            {
            case SelectFilterStatus::Ok:
            {
                pickedElemId = wydb::ElementId(idValue);
                pIntersection = &intersection;
                return true;
            }
            break;

            case SelectFilterStatus::Continue:
            {
                continue;
            }
            break;

            case SelectFilterStatus::Break:
            default:
            {
                return false;
            }
            break;
            }
        }

        return false;
    }
    else
    {
        for (auto iter = intersections.cbegin(); iter != intersections.cend(); ++iter)
        {
            const typename T::Intersection& intersection = *iter;
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
                return false;
            }

            pickedElemId = wydb::ElementId(idValue);
            pIntersection = &intersection;
            return true;
        }

        return false;
    }

    return false;
}

wyap::Selection _newSelection(
    const wydb::ElementId& id,
    osg::Drawable* pDrawable,
    unsigned int primitiveIndex,
    wy3d::SelectionType selType,
    bool acceptElement)
{
    assert(!id.isNull());

    Scene* pActiveScene = Application::instance().getActiveScene();
    assert(pActiveScene);
    if (!pActiveScene) return wyap::Selection(wydb::ElementId::kNull);
    ElementNode* pElemNode = pActiveScene->getElementNode(id);
    assert(pElemNode);
    if (!pElemNode) return wyap::Selection(wydb::ElementId::kNull);

    switch (pElemNode->getNodeType())
    {
    case ElementNodeType::Solid:
    {
        SolidElementNode* pSolidNode = static_cast<SolidElementNode*>(pElemNode);
        assert(pSolidNode);
        switch (PickCommon::getDrawableMode(pDrawable))
        {
        case DrawMode::Face:
        {
            if (wy3d::SelectionTypeUtil::HasValue(selType, wy3d::SelectionType::SolidFace))
            {
                unsigned int faceIndex = pSolidNode->getFaceIndex(primitiveIndex);
                if (-1 == faceIndex)
                {
                    assert(false);
                    return wyap::Selection(wydb::ElementId::kNull);
                }
                return wyap::Selection(static_cast<unsigned int>(wy3d::SelectionType::SolidFace), id, std::to_string(faceIndex));
            }
            else if (acceptElement)
            {
                return wyap::Selection(id);
            }
            return wyap::Selection(wydb::ElementId::kNull);
        }
        break;

        case DrawMode::Edge:
        {
            if (wy3d::SelectionTypeUtil::HasValue(selType, wy3d::SelectionType::SolidEdge))
            {
                unsigned int edgeIndex = pSolidNode->getEdgeIndex(primitiveIndex);
                if (-1 == edgeIndex)
                {
                    assert(false);
                    return wyap::Selection(wydb::ElementId::kNull);
                }
                return wyap::Selection(static_cast<unsigned int>(wy3d::SelectionType::SolidEdge), id, std::to_string(edgeIndex));
            }
            else if (acceptElement)
            {
                return wyap::Selection(id);
            }
            return wyap::Selection(wydb::ElementId::kNull);
        }
        break;

        case DrawMode::Vertex:
        {
            if (wy3d::SelectionTypeUtil::HasValue(selType, wy3d::SelectionType::SolidVertex))
            {
                // TODO 后续支持
                assert(false);
                return wyap::Selection(wydb::ElementId::kNull);
            }
            else if (acceptElement)
            {
                return wyap::Selection(id);
            }
            return wyap::Selection(wydb::ElementId::kNull);
        }
        break;

        default:
        {
            assert(false);
            return wyap::Selection(wydb::ElementId::kNull);
        }
        break;
        }
    }
    break;

    case ElementNodeType::Sheet:
    {
        SheetElementNode* pSheetNode = static_cast<SheetElementNode*>(pElemNode);
        assert(pSheetNode);
        switch (PickCommon::getDrawableMode(pDrawable))
        {
        case DrawMode::Face:
        {
            if (wy3d::SelectionTypeUtil::HasValue(selType, wy3d::SelectionType::SolidFace))
            {
                unsigned int faceIndex = pSheetNode->getFaceIndex(primitiveIndex);
                if (-1 == faceIndex)
                {
                    assert(false);
                    return wyap::Selection(wydb::ElementId::kNull);
                }
                return wyap::Selection(static_cast<unsigned int>(wy3d::SelectionType::SolidFace), id, std::to_string(faceIndex));
            }
            else if (acceptElement)
            {
                return wyap::Selection(id);
            }
            return wyap::Selection(wydb::ElementId::kNull);
        }
        break;

        case DrawMode::Edge:
        {
            if (wy3d::SelectionTypeUtil::HasValue(selType, wy3d::SelectionType::SolidEdge))
            {
                unsigned int edgeIndex = pSheetNode->getEdgeIndex(primitiveIndex);
                if (-1 == edgeIndex)
                {
                    assert(false);
                    return wyap::Selection(wydb::ElementId::kNull);
                }
                return wyap::Selection(static_cast<unsigned int>(wy3d::SelectionType::SolidEdge), id, std::to_string(edgeIndex));
            }
            else if (acceptElement)
            {
                return wyap::Selection(id);
            }
            return wyap::Selection(wydb::ElementId::kNull);
        }
        break;

        case DrawMode::Vertex:
        {
            if (acceptElement)
            {
                return wyap::Selection(id);
            }
            return wyap::Selection(wydb::ElementId::kNull);
        }
        break;

        default:
        {
            assert(false);
            return wyap::Selection(wydb::ElementId::kNull);
        }
        break;
        }
    }
    break;

    case ElementNodeType::Sketch:
    {
        SketchElementNode* pSketchElemNode = static_cast<SketchElementNode*>(pElemNode);
        if (!pSketchElemNode)
        {
            assert(false);
            return wyap::Selection(wydb::ElementId::kNull);
        }

        if (wy3d::SelectionTypeUtil::HasValue(selType, wy3d::SelectionType::SketchCurve))
        {
            unsigned int curveId(0);
            if (pDrawable == pSketchElemNode->getCenterLinesGeom())
            {
                curveId = pSketchElemNode->getCenterLineCurveId(primitiveIndex);
            }
            else
            {
                curveId = pSketchElemNode->getCurveId(primitiveIndex);
            }
            if (0 == curveId)
            {
                assert(false);
                return wyap::Selection(wydb::ElementId::kNull);
            }

            return wyap::Selection(static_cast<unsigned int>(wy3d::SelectionType::SketchCurve), id, std::to_string(curveId));
        }
        else if (acceptElement)
        {
            return wyap::Selection(id);
        }
        else
        {
            assert(false);
            return wyap::Selection(wydb::ElementId::kNull);
        }
    }
    break;

    case ElementNodeType::SketchEntity:
    {
        if (acceptElement)
        {
            return wyap::Selection(id);
        }
        else
        {
            assert(false);
            return wyap::Selection(wydb::ElementId::kNull);
        }
    }
    break;

    case ElementNodeType::DatumPlane:
    {
        if (acceptElement)
        {
            return wyap::Selection(id);
        }
        else
        {
            assert(false);
            return wyap::Selection(wydb::ElementId::kNull);
        }
    }
    break;

    case ElementNodeType::Curve:
    {
        if (acceptElement)
        {
            return wyap::Selection(id);
        }
        else
        {
            assert(false);
            return wyap::Selection(wydb::ElementId::kNull);
        }
    }
    break;

    default:
    {
        assert(false);
        return wyap::Selection(wydb::ElementId::kNull);
    }
    break;
    }

    return wyap::Selection(wydb::ElementId::kNull);
}

inline wyap::Selection _userFilter(
    const wydb::Database* pDb,
    SelectFilterFunctorSPtr pSelFilter,
    const wyap::Selection& sel)
{
    assert(pDb);
    if (!pSelFilter) return sel;

    switch ((*pSelFilter)(pDb, sel, SelectAction::Point))
    {
    case SelectFilterStatus::Ok:
        return sel;

    case SelectFilterStatus::Continue:
    default:
        return wyap::Selection(wydb::ElementId::kNull);
    }

    return wyap::Selection(wydb::ElementId::kNull);
}

// 多面体求交器
// 用于选择wy3d::SelectionType::Element or SolidEdge or SolidVertex
wyap::Selection pointPick_PolytopeIntersector(
    const wydb::Database* pDb,
    osgViewer::View* pView,
    float x, float y,
    const PointPickOption& option,
    DrawMode allowedDrawMode)
{
    assert(pDb);
    assert(pView);

    // 求交器
    osg::ref_ptr<osgUtil::PolytopeIntersector> intersector = new osgUtil::PolytopeIntersector(
        osgUtil::Intersector::WINDOW,
        x - option.pickHalfSize, y - option.pickHalfSize,
        x + option.pickHalfSize, y + option.pickHalfSize);
    osgUtil::IntersectionVisitor iv(intersector.get());
    iv.setTraversalMask(option.pickMask);
    pView->getCamera()->accept(iv);
    const osgUtil::PolytopeIntersector::Intersections& intersections = intersector->getIntersections();
    if (intersections.empty()) return wyap::Selection(wydb::ElementId::kNull);

    // 遍历求交结果
    wydb::ElementId pickedElemId = wydb::ElementId::kNull;
    const osgUtil::PolytopeIntersector::Intersection* pIntersection(nullptr);
    if (!_traverseIntersections<osgUtil::PolytopeIntersector>(pDb, intersections, pickedElemId, pIntersection, option, allowedDrawMode))
    {
        return wyap::Selection(wydb::ElementId::kNull);
    }

    // 新建选择对象
    assert(pIntersection);
    wyap::Selection sel = _newSelection(
        pickedElemId,
        pIntersection->drawable.get(),
        pIntersection->primitiveIndex,
        wy3d::SelectionTypeUtil::RemoveValues(option.selType, wy3d::SelectionType::SolidFace | wy3d::SelectionType::SolidBody),
        option.acceptElement);
    if (sel.getElementId().isNull()) return wyap::Selection(wydb::ElementId::kNull);
    sel.setPickPosition(MathUtils::toVector3(pIntersection->localIntersectionPoint));

    // 用户自定义过滤
    return _userFilter(pDb, option.pSelFilter, sel);
}

// 线段求交器
// 用于选择wy3d::SelectionType::SolidFace or SolidBody
wyap::Selection pointPick_LineSegmentIntersector(
    const wydb::Database* pDb,
    osgViewer::View* pView, 
    float x, float y,
    const PointPickOption& option,
    DrawMode allowedDrawMode)
{
    assert(pDb);
    assert(pView);

    // 求交器
    osg::ref_ptr<osgUtil::LineSegmentIntersector> intersector = new osgUtil::LineSegmentIntersector(
        osgUtil::Intersector::WINDOW, x, y);
    osgUtil::IntersectionVisitor iv(intersector.get());
    iv.setTraversalMask(option.pickMask);
    pView->getCamera()->accept(iv);
    const osgUtil::LineSegmentIntersector::Intersections& intersections = intersector->getIntersections();
    if (intersections.empty()) return wyap::Selection(wydb::ElementId::kNull);

    // 遍历求交结果
    wydb::ElementId pickedElemId = wydb::ElementId::kNull;
    const osgUtil::LineSegmentIntersector::Intersection* pIntersection(nullptr);
    if (!_traverseIntersections<osgUtil::LineSegmentIntersector>(pDb, intersections, pickedElemId, pIntersection, option, allowedDrawMode))
    {
        return wyap::Selection(wydb::ElementId::kNull);
    }

    // 新建选择对象
    assert(pIntersection);
    wyap::Selection sel = _newSelection(pickedElemId,
        pIntersection->drawable.get(),
        pIntersection->primitiveIndex,
        option.selType & (wy3d::SelectionType::SolidFace | wy3d::SelectionType::SolidBody),
        option.acceptElement);
    if (sel.getElementId().isNull()) return wyap::Selection(wydb::ElementId::kNull);
    sel.setPickPosition(MathUtils::toVector3(pIntersection->localIntersectionPoint));

    // 用户自定义过滤
    return _userFilter(pDb, option.pSelFilter, sel);
}

wyap::Selection PointPick::pick(
    const wydb::Database* pDb,
    osgViewer::View* pView,
    float x, float y,
    const PointPickOption& option)
{
    assert(pDb);
    assert(pView);
    if (!pDb || !pView) return wyap::Selection(wydb::ElementId::kNull);

    // 多面体求交器(用于选择元素&边&顶点)
    // /*正常情况下,这三种选择类型只能三选一*/在命令成角度基准面中,确定旋转轴线步骤时,要支持选择:SolidEdge + SolidFace + DatumPlane
    wyap::Selection selRet = wyap::Selection(wydb::ElementId::kNull);
    if (wy3d::SelectionTypeUtil::HasValue(option.selType, wy3d::SelectionType::SolidVertex))
    {
        selRet = pointPick_PolytopeIntersector(pDb, pView, x, y, option, DrawMode::Vertex);
        if (!selRet.getElementId().isNull()) return selRet;
    }
    if (wy3d::SelectionTypeUtil::HasValue(option.selType, wy3d::SelectionType::SolidEdge) ||
        wy3d::SelectionTypeUtil::HasValue(option.selType, wy3d::SelectionType::SketchCurve))
    {
        selRet = pointPick_PolytopeIntersector(pDb, pView, x, y, option, DrawMode::Edge);
        if (!selRet.getElementId().isNull()) return selRet;
    }
    if (option.acceptElement)
    {
        selRet = pointPick_PolytopeIntersector(pDb, pView, x, y, option, DrawMode::All);
        if (!selRet.getElementId().isNull()) return selRet;
    }
    if (!selRet.getElementId().isNull()) return selRet;

    // 线段求交器(用于选择面&体)
    // 正常情况下,这两种选择类型只能三选一
    if (wy3d::SelectionTypeUtil::HasValue(option.selType, wy3d::SelectionType::SolidFace))
    {
        assert(!wy3d::SelectionTypeUtil::HasValue(option.selType, wy3d::SelectionType::SolidBody));
        selRet = pointPick_LineSegmentIntersector(pDb, pView, x, y, option, DrawMode::Face);
    }
    else if (wy3d::SelectionTypeUtil::HasValue(option.selType, wy3d::SelectionType::SolidBody))
    {
        assert(!wy3d::SelectionTypeUtil::HasValue(option.selType, wy3d::SelectionType::SolidFace));
        selRet = pointPick_LineSegmentIntersector(pDb, pView, x, y, option, DrawMode::Face);
    }
    return selRet;
}