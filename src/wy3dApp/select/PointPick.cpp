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
#include <wy3dApp/Application.h>
#include "application/ApplicationImpl.h"
#include <wy3dApp/Scene.h>
#include <wy3dApp/ElementNode.h>
#include "utils/MathUtils.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

template<typename T>
bool _traverseIntersections(
    const wydb::Database* pDb,
    const typename T::Intersections& intersections,
    wydb::ElementId& pickedElemId,
    const typename T::Intersection*& pIntersection,
    const PointPick::Options& option,
    DrawMode allowedDrawMode = DrawMode::All)
{
    if (option.preFilter)
    {
        wydb::ElementId id = wydb::ElementId::kNull;
        for (auto iter = intersections.cbegin(); iter != intersections.cend(); ++iter)
        {
            const typename T::Intersection& intersection = *iter;
            osg::Geometry* geom = dynamic_cast<osg::Geometry*>(intersection.drawable.get());
            if (!geom) continue;

            // added by wangyao 2025.05.06 {
            // 在选择实体边时(wy3d::SelectionType::Edge)
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

            switch ((*option.preFilter)(pDb, id, SelectAction::Point))
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
            // 在选择实体边时(wy3d::SelectionType::Edge)
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
    wy3d::SelectionType selType)
{
    assert(!id.isNull());

    Scene* pActiveScene = ApplicationImpl::instance().getActiveScene();
    assert(pActiveScene);
    if (!pActiveScene) return wyap::Selection(wydb::ElementId::kNull);

    ElementNode* pElemNode = pActiveScene->getElementNode(id);
    assert(pElemNode);
    if (!pElemNode) return wyap::Selection(wydb::ElementId::kNull);

    return pElemNode->createSelection(pDrawable, primitiveIndex, selType);
}

inline wyap::Selection _userFilter(
    const wydb::Database* pDb,
    SelectPostFilterFunctorSPtr pPostFilter,
    const wyap::Selection& sel)
{
    assert(pDb);
    if (!pPostFilter) return sel;

    switch ((*pPostFilter)(pDb, sel, SelectAction::Point))
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
// 用于选择wy3d::SelectionType::Element or Edge or Vertex
wyap::Selection pointPick_PolytopeIntersector(
    const wydb::Database* pDb,
    osgViewer::View* pView,
    float x, float y,
    const PointPick::Options& option,
    DrawMode allowedDrawMode)
{
    assert(pDb);
    assert(pView);

    // 求交器
    osg::ref_ptr<osgUtil::PolytopeIntersector> intersector = new osgUtil::PolytopeIntersector(
        osgUtil::Intersector::WINDOW,
        x - option.pickHalfSize, y - option.pickHalfSize,
        x + option.pickHalfSize, y + option.pickHalfSize);

    unsigned int primitiveMask = osgUtil::PolytopeIntersector::ALL_PRIMITIVES;
    switch (allowedDrawMode)
    {
    case DrawMode::Vertex: primitiveMask = osgUtil::PolytopeIntersector::POINT_PRIMITIVES; break;
    case DrawMode::Edge:   primitiveMask = osgUtil::PolytopeIntersector::LINE_PRIMITIVES;  break;
    default: break;
    }
    intersector->setPrimitiveMask(primitiveMask);

    osgUtil::IntersectionVisitor iv(intersector.get());
    iv.setTraversalMask(option.allowedElemTypes);
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
        wy3d::SelectionTypeUtil::RemoveValues(option.selType, wy3d::SelectionType::Face));
    if (sel.getElementId().isNull()) return wyap::Selection(wydb::ElementId::kNull);
    sel.setPickPosition(MathUtils::toVector3(pIntersection->localIntersectionPoint));

    // 用户自定义过滤
    return _userFilter(pDb, option.postFilter, sel);
}

// 线段求交器
// 用于选择wy3d::SelectionType::Face
wyap::Selection pointPick_LineSegmentIntersector(
    const wydb::Database* pDb,
    osgViewer::View* pView, 
    float x, float y,
    const PointPick::Options& option,
    DrawMode allowedDrawMode)
{
    assert(pDb);
    assert(pView);

    // 求交器
    osg::ref_ptr<osgUtil::LineSegmentIntersector> intersector = new osgUtil::LineSegmentIntersector(
        osgUtil::Intersector::WINDOW, x, y);
    osgUtil::IntersectionVisitor iv(intersector.get());
    iv.setTraversalMask(option.allowedElemTypes);
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
    // The Element bit must survive the Face narrowing (element-only nodes are answered by this pass).
    wyap::Selection sel = _newSelection(pickedElemId,
        pIntersection->drawable.get(),
        pIntersection->primitiveIndex,
        option.selType & (wy3d::SelectionType::Face | wy3d::SelectionType::Element));
    if (sel.getElementId().isNull()) return wyap::Selection(wydb::ElementId::kNull);
    sel.setPickPosition(MathUtils::toVector3(pIntersection->localIntersectionPoint));

    // 用户自定义过滤
    return _userFilter(pDb, option.postFilter, sel);
}

wyap::Selection PointPick::pick(
    const wydb::Database* pDb,
    osgViewer::View* pView,
    float x, float y,
    const PointPick::Options& option)
{
    assert(pDb);
    assert(pView);
    if (!pDb || !pView) return wyap::Selection(wydb::ElementId::kNull);

    // 多面体求交器(用于选择元素&边&顶点)
    // /*正常情况下,这三种选择类型只能三选一*/在命令成角度基准面中,确定旋转轴线步骤时,要支持选择:Edge + Face + DatumPlane
    wyap::Selection selRet = wyap::Selection(wydb::ElementId::kNull);
    if (wy3d::SelectionTypeUtil::HasValue(option.selType, wy3d::SelectionType::Vertex))
    {
        selRet = pointPick_PolytopeIntersector(pDb, pView, x, y, option, DrawMode::Vertex);
        if (!selRet.getElementId().isNull()) return selRet;
    }
    if (wy3d::SelectionTypeUtil::HasValue(option.selType, wy3d::SelectionType::Edge) ||
        wy3d::SelectionTypeUtil::HasValue(option.selType, wy3d::SelectionType::SketchCurve) ||
        wy3d::SelectionTypeUtil::HasValue(option.selType, wy3d::SelectionType::SketchCurve3D))
    {
        selRet = pointPick_PolytopeIntersector(pDb, pView, x, y, option, DrawMode::Edge);
        if (!selRet.getElementId().isNull()) return selRet;
    }
    // The Element bit enables the whole-element fallback (nodes having no sub-element picks).
    if (wy3d::SelectionTypeUtil::HasValue(option.selType, wy3d::SelectionType::Element))
    {
        selRet = pointPick_PolytopeIntersector(pDb, pView, x, y, option, DrawMode::All);
        if (!selRet.getElementId().isNull()) return selRet;
    }
    if (!selRet.getElementId().isNull()) return selRet;

    // 线段求交器(用于选择面)
    if (wy3d::SelectionTypeUtil::HasValue(option.selType, wy3d::SelectionType::Face))
    {
        selRet = pointPick_LineSegmentIntersector(pDb, pView, x, y, option, DrawMode::Face);
    }
    return selRet;
}

} // namespace wy3dApp
