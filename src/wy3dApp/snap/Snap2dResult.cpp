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

#include "Snap2dResult.h"
#include <wyVector2.h>
#include <wy3dPlane.h>
#include <wy3dApp/Application.h>
#include "scene/RenderConst.h"
#include "commands/transient/SketchPointTransient.h"
#include "commands/transient/SketchCurveTransient.h"
#include "SnapConsts.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

Snap2dResult::Snap2dResult(
    const wy3d::Plane& sketchPlane,
    const wy::Vector2& position,
    Snap2dContextSPtr pContext,
    SketchSnapObjectSPtr pSnapObject,
    bool drawPoint)
    : _sketchPlane(sketchPlane), _position(position), _pContext(pContext),
      _isPointToDraw(drawPoint)
{
    assert(pContext);
    assert(pSnapObject);

    _items.reserve(3);
    _items.emplace_back(std::move(Snap2dResultItem(pSnapObject)));
}

Snap2dResult::Snap2dResult(
    const wy3d::Plane& sketchPlane,
    const wy::Vector2& position,
    Snap2dContextSPtr pContext,
    SketchSnapObjectSPtr pSnapObject, SketchSnapObjectSPtr pOtherSnapObject,
    bool drawPoint)
    : _sketchPlane(sketchPlane), _position(position), _pContext(pContext),
      _isPointToDraw(drawPoint)
{
    assert(pContext);
    assert(pSnapObject);
    assert(pOtherSnapObject);

    _items.reserve(3);
    _items.emplace_back(std::move(Snap2dResultItem(pSnapObject)));
    _items.emplace_back(std::move(Snap2dResultItem(pOtherSnapObject)));
}

Snap2dResult::Snap2dResult(
    const wy3d::Plane& sketchPlane,
    const wy::Vector2& position,
    Snap2dContextSPtr pContext)
    : _sketchPlane(sketchPlane), _position(position), _pContext(pContext),
      _isPointToDraw(false)
{
    assert(pContext);
}

Snap2dResult::~Snap2dResult()
{
}

bool Snap2dResult::isLooseEqual(const Snap2dResult& rhs) const
{
    // 上下文是否相等
    if (_pContext.get() != rhs._pContext.get())
    {
        return false;
    }

    // Whether the point marker is drawn is part of the result identity.
    if (_isPointToDraw != rhs._isPointToDraw)
    {
        return false;
    }

    // 捕捉对象的数量是否相等
    if (_items.size() != rhs._items.size())
    {
        return false;
    }

    // 遍历比较捕捉的对象是否一致
    size_t num = _items.size();
    for (size_t i = 0; i < num; ++i)
    {
        const SketchSnapObjectSPtr& pSnapObj = _items[i].pSnapObject;
        assert(pSnapObj);
        const SketchSnapObjectSPtr& pSnapObjRhs = rhs._items[i].pSnapObject;
        assert(pSnapObjRhs);

        // id
        if (pSnapObj->getId() != pSnapObjRhs->getId())
        {
            return false;
        }
        // 类型
        if (pSnapObj->getType() != pSnapObjRhs->getType())
        {
            return false;
        }
        // 额外信息
        if (pSnapObj->getExtra() != pSnapObjRhs->getExtra())
        {
            return false;
        }
    }

    return true;
}

void Snap2dResult::show(std::map<wydb::ElementId, GuiCmdTransientSPtr> highlights)
{
    _highlightTransients = std::move(highlights);

    for (Snap2dResultItem& item : _items)
    {
        const SketchSnapObject* pSnapObject = item.pSnapObject.get();
        if (!pSnapObject)
        {
            assert(false);
            continue;
        }
        auto iter = _highlightTransients.find(pSnapObject->getId());
        if (iter != _highlightTransients.cend())
        {
            assert(iter->second);
            item.transients.push_back(iter->second);
        }

        // External rule hits carry no auxiliary geometry: the kind only selects
        // the tip icon and the point marker follows _isPointToDraw.
        if (dynamic_cast<const RuleHitSnapObject*>(pSnapObject))
        {
            switch (pSnapObject->getType())
            {
            case Snap2dType::Horizontal:
                item.pSnapTipWidget = SnapTipWidgetMgr::instance().getHorizontal();
                break;
            case Snap2dType::Vertical:
                item.pSnapTipWidget = SnapTipWidgetMgr::instance().getVertical();
                break;
            case Snap2dType::Parallel:
                item.pSnapTipWidget = SnapTipWidgetMgr::instance().getParallel();
                break;
            case Snap2dType::Perpendicular:
            case Snap2dType::PerpendicularPoint:
                item.pSnapTipWidget = SnapTipWidgetMgr::instance().getPerpendicular();
                break;
            case Snap2dType::Equal:
                item.pSnapTipWidget = SnapTipWidgetMgr::instance().getEqual();
                break;
            case Snap2dType::TangentPoint:
                item.pSnapTipWidget = SnapTipWidgetMgr::instance().getTangent();
                break;
            case Snap2dType::IntersectionPoint:
                item.pSnapTipWidget = SnapTipWidgetMgr::instance().getIntersection();
                break;
            case Snap2dType::EndPoint:
                item.pSnapTipWidget = SnapTipWidgetMgr::instance().getEnd();
                break;
            case Snap2dType::MiddlePoint:
                item.pSnapTipWidget = SnapTipWidgetMgr::instance().getMiddle();
                break;
            case Snap2dType::CenterPoint:
                item.pSnapTipWidget = SnapTipWidgetMgr::instance().getCenter();
                break;
            case Snap2dType::Origin:
                item.pSnapTipWidget = SnapTipWidgetMgr::instance().getOrigin();
                break;
            default:
                // Undefined/AlignX/AlignY/PointOnCurve/KeyPoint: point only.
                break;
            }

            // Point-like kinds use the default point style, the rest the on-curve style.
            float pointSize = SnapConsts::PointSizeOnSketchCurve;
            osg::Vec4 pointColor = SnapConsts::PointColorOnSketchCurve;
            switch (pSnapObject->getType())
            {
            case Snap2dType::PerpendicularPoint:
            case Snap2dType::TangentPoint:
            case Snap2dType::IntersectionPoint:
            case Snap2dType::KeyPoint:
            case Snap2dType::EndPoint:
            case Snap2dType::MiddlePoint:
            case Snap2dType::CenterPoint:
            case Snap2dType::Origin:
                pointSize = SnapConsts::PointSize;
                pointColor = SnapConsts::PointColor;
                break;
            default:
                break;
            }

            if (_isPointToDraw)
            {
                item.transients.push_back(std::make_shared<SketchPointTransient>(_sketchPlane, _position,
                    pointSize, pointColor));
            }
            continue;
        }

        switch (pSnapObject->getType())
        {
        // 水平
        case Snap2dType::Horizontal:
        {
            item.pSnapTipWidget = SnapTipWidgetMgr::instance().getHorizontal();
        }
        break;

        // 竖直
        case Snap2dType::Vertical:
        {
            item.pSnapTipWidget = SnapTipWidgetMgr::instance().getVertical();
        }
        break;

        // 平行
        case Snap2dType::Parallel:
        {
            item.pSnapTipWidget = SnapTipWidgetMgr::instance().getParallel();
        }
        break;

        // 垂直
        case Snap2dType::Perpendicular:
        {
            item.pSnapTipWidget = SnapTipWidgetMgr::instance().getPerpendicular();
        }
        break;

        // 对齐X
        case Snap2dType::AlignX:
        {
            // 点划线
            const AlignXSnapObject* pAlignX = dynamic_cast<const AlignXSnapObject*>(pSnapObject);
            assert(pAlignX);
            item.transients.push_back(std::make_shared<SketchDotLineTransient>(
                _sketchPlane, pAlignX->getPosition(), _position));
            if (_isPointToDraw)
            {
                item.transients.push_back(std::make_shared<SketchPointTransient>(_sketchPlane, _position,
                    SnapConsts::PointSizeOnSketchCurve, SnapConsts::PointColorOnSketchCurve));
            }
        }
        break;

        // 对齐Y
        case Snap2dType::AlignY:
        {
            // 点划线
            const AlignYSnapObject* pAlignY = dynamic_cast<const AlignYSnapObject*>(pSnapObject);
            assert(pAlignY);
            item.transients.push_back(std::make_shared<SketchDotLineTransient>(
                _sketchPlane, pAlignY->getPosition(), _position));
            if (_isPointToDraw)
            {
                item.transients.push_back(std::make_shared<SketchPointTransient>(_sketchPlane, _position,
                    SnapConsts::PointSizeOnSketchCurve, SnapConsts::PointColorOnSketchCurve));
            }
        }
        break;

        // 相等
        case Snap2dType::Equal:
        {
            item.pSnapTipWidget = SnapTipWidgetMgr::instance().getEqual();
        }
        break;

        // 点在线上
        case Snap2dType::PointOnCurve:
        {
            const PointOnCurveSnapObject* pPointOnCurve = dynamic_cast<const PointOnCurveSnapObject*>(pSnapObject);
            assert(pPointOnCurve);
            if (_isPointToDraw)
            {
                item.transients.push_back(std::make_shared<SketchPointTransient>(_sketchPlane, _position,
                    SnapConsts::PointSizeOnSketchCurve, SnapConsts::PointColorOnSketchCurve));
            }
        }
        break;

        // 垂足
        case Snap2dType::PerpendicularPoint:
        {
            item.pSnapTipWidget = SnapTipWidgetMgr::instance().getPerpendicular();
            if (_isPointToDraw)
            {
                item.transients.push_back(std::make_shared<SketchPointTransient>(_sketchPlane, _position,
                    SnapConsts::PointSize, SnapConsts::PointColor));
            }
        }
        break;

        // 切点
        case Snap2dType::TangentPoint:
        {
            wy::Vector2 pos = _position;
            const TangentPointSnapObject* pTangentPointSnapObj = dynamic_cast<const TangentPointSnapObject*>(pSnapObject);
            if (pTangentPointSnapObj)
            {
                pos = pTangentPointSnapObj->getValue();
            }
            item.pSnapTipWidget = SnapTipWidgetMgr::instance().getTangent();
            if (_isPointToDraw)
            {
                item.transients.push_back(std::make_shared<SketchPointTransient>(_sketchPlane, pos,
                    SnapConsts::PointSize, SnapConsts::PointColor));
            }
        }
        break;

        // 交点
        case Snap2dType::IntersectionPoint:
        {
            item.pSnapTipWidget = SnapTipWidgetMgr::instance().getIntersection();
            if (_isPointToDraw)
            {
                item.transients.push_back(std::make_shared<SketchPointTransient>(_sketchPlane, _position,
                    SnapConsts::PointSize, SnapConsts::PointColor));
            }
        }
        break;

        // 关键点
        case Snap2dType::KeyPoint:
        {
            // 点
            if (_isPointToDraw)
            {
                item.transients.push_back(std::make_shared<SketchPointTransient>(_sketchPlane, _position,
                    SnapConsts::PointSize, SnapConsts::PointColor));
            }
            // 点划线
            const KeyPointSnapObject* pKeyPoint = dynamic_cast<const KeyPointSnapObject*>(pSnapObject);
            assert(pKeyPoint);
            item.transients.push_back(std::make_shared<SketchDotLineTransient>(
                _sketchPlane, pKeyPoint->getRefPoint(), _position));
        }
        break;

        // 端点(默认捕捉系统)
        case Snap2dType::EndPoint:
        {
            item.pSnapTipWidget = SnapTipWidgetMgr::instance().getEnd();
            if (_isPointToDraw)
            {
                item.transients.push_back(std::make_shared<SketchPointTransient>(_sketchPlane, _position,
                    SnapConsts::PointSize, SnapConsts::PointColor));
            }
        }
        break;

        // 中点(默认捕捉系统)
        case Snap2dType::MiddlePoint:
        {
            item.pSnapTipWidget = SnapTipWidgetMgr::instance().getMiddle();
        }
        break;

        // 中心点(默认捕捉系统)
        case Snap2dType::CenterPoint:
        {
            item.pSnapTipWidget = SnapTipWidgetMgr::instance().getCenter();
        }
        break;

        // 原点(默认捕捉系统)
        case Snap2dType::Origin:
        {
            item.pSnapTipWidget = SnapTipWidgetMgr::instance().getOrigin();
        }
        break;

        default:
        {
            assert(false);
        }
        break;
        }
    }

    // 跟随鼠标提示
    _pSnapTipMouseFollow = std::make_shared<SnapTipMouseFollow>();
    for (const Snap2dResultItem& item : _items)
    {
        if (item.pSnapTipWidget)
            _pSnapTipMouseFollow->addTipWidget(item.pSnapTipWidget);
    }
    _pSnapTipMouseFollow->start();
}

void Snap2dResult::update()
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return;

    for (Snap2dResultItem& item : _items)
    {
        SketchSnapObject* pSnapObject = item.pSnapObject.get();
        if (!pSnapObject) continue;

        // External rule hits only move their point marker.
        if (dynamic_cast<const RuleHitSnapObject*>(pSnapObject))
        {
            for (GuiCmdTransientSPtr& pTransient : item.transients)
            {
                if (SketchPointTransient* pPointTransient = dynamic_cast<SketchPointTransient*>(pTransient.get()))
                {
                    pPointTransient->update(_position);
                }
            }
            continue;
        }

        switch (pSnapObject->getType())
        {
        // 对齐X
        case Snap2dType::AlignX:
        {
            const AlignXSnapObject* pAlignX = dynamic_cast<const AlignXSnapObject*>(pSnapObject);
            assert(pAlignX);
            for (GuiCmdTransientSPtr& pTransient : item.transients)
            {
                if (SketchDotLineTransient* pDotLineTransient = dynamic_cast<SketchDotLineTransient*>(pTransient.get()))
                {
                    pDotLineTransient->update(pAlignX->getPosition(), _position);
                }
                else if (SketchPointTransient* pPointTransient = dynamic_cast<SketchPointTransient*>(pTransient.get()))
                {
                    pPointTransient->update(_position);
                }
            }
        }
        break;

        // Y 对齐
        case Snap2dType::AlignY:
        {
            const AlignYSnapObject* pAlignY = dynamic_cast<const AlignYSnapObject*>(pSnapObject);
            assert(pAlignY);
            for (GuiCmdTransientSPtr& pTransient : item.transients)
            {
                if (SketchDotLineTransient* pDotLineTransient = dynamic_cast<SketchDotLineTransient*>(pTransient.get()))
                {
                    pDotLineTransient->update(pAlignY->getPosition(), _position);
                }
                else if (SketchPointTransient* pPointTransient = dynamic_cast<SketchPointTransient*>(pTransient.get()))
                {
                    pPointTransient->update(_position);
                }
            }
        }
        break;

        // 点在线上
        case Snap2dType::PointOnCurve:
        {
            const PointOnCurveSnapObject* pPointOnCurve = dynamic_cast<const PointOnCurveSnapObject*>(pSnapObject);
            assert(pPointOnCurve);
            for (GuiCmdTransientSPtr& pTransient : item.transients)
            {
                if (SketchPointTransient* pPointTransient = dynamic_cast<SketchPointTransient*>(pTransient.get()))
                {
                    pPointTransient->update(_position);
                }
            }
        }
        break;

        default:
        {
        }
        break;
        }
    }
}

} // namespace wy3dApp
