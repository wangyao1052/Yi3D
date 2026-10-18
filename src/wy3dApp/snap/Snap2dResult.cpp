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

namespace {

Snap2dResultInfo makeResultInfo(const SketchSnapObject& snapObject)
{
    Snap2dResultInfo info;
    info.type = snapObject.getType();
    info.highlightId = snapObject.getId();
    info.extra = snapObject.getExtra();

    if (const PointSnapObject* pPointSnapObject = dynamic_cast<const PointSnapObject*>(&snapObject))
        info.point = pPointSnapObject->getValue();
    if (const AlignXSnapObject* pAlignXSnapObject = dynamic_cast<const AlignXSnapObject*>(&snapObject))
        info.refPoint = pAlignXSnapObject->getPosition();
    else if (const AlignYSnapObject* pAlignYSnapObject = dynamic_cast<const AlignYSnapObject*>(&snapObject))
        info.refPoint = pAlignYSnapObject->getPosition();
    else if (const KeyPointSnapObject* pKeyPointSnapObject = dynamic_cast<const KeyPointSnapObject*>(&snapObject))
        info.refPoint = pKeyPointSnapObject->getRefPoint();

    return info;
}

} // namespace

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
    _items.emplace_back(Snap2dResultItem(makeResultInfo(*pSnapObject)));
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
    _items.emplace_back(Snap2dResultItem(makeResultInfo(*pSnapObject)));
    _items.emplace_back(Snap2dResultItem(makeResultInfo(*pOtherSnapObject)));
}

Snap2dResult::Snap2dResult(
    const wy3d::Plane& sketchPlane,
    const wy::Vector2& position,
    Snap2dContextSPtr pContext,
    const Snap2dResultInfo& snapInfo,
    bool drawPoint)
    : _sketchPlane(sketchPlane), _position(position), _pContext(pContext),
      _isPointToDraw(drawPoint)
{
    assert(pContext);

    _items.reserve(3);
    _items.emplace_back(Snap2dResultItem(snapInfo));
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
        const Snap2dResultInfo& info = _items[i].info;
        const Snap2dResultInfo& infoRhs = rhs._items[i].info;

        // id
        if (info.highlightId != infoRhs.highlightId)
        {
            return false;
        }
        // 类型
        if (info.type != infoRhs.type)
        {
            return false;
        }
        // 额外信息
        if (info.extra != infoRhs.extra)
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
        const Snap2dResultInfo& info = item.info;
        auto iter = _highlightTransients.find(info.highlightId);
        if (iter != _highlightTransients.cend())
        {
            assert(iter->second);
            item.transients.push_back(iter->second);
        }

        switch (info.type)
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

        // 对齐X / 对齐Y
        case Snap2dType::AlignX:
        case Snap2dType::AlignY:
        {
            // 点划线
            item.transients.push_back(std::make_shared<SketchDotLineTransient>(
                _sketchPlane, info.refPoint, _position));
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
            item.pSnapTipWidget = SnapTipWidgetMgr::instance().getTangent();
            if (_isPointToDraw)
            {
                item.transients.push_back(std::make_shared<SketchPointTransient>(_sketchPlane, _position,
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
            item.transients.push_back(std::make_shared<SketchDotLineTransient>(
                _sketchPlane, info.refPoint, _position));
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
            if (_isPointToDraw)
            {
                item.transients.push_back(std::make_shared<SketchPointTransient>(_sketchPlane, _position,
                    SnapConsts::PointSize, SnapConsts::PointColor));
            }
        }
        break;

        // 中心点(默认捕捉系统)
        case Snap2dType::CenterPoint:
        {
            item.pSnapTipWidget = SnapTipWidgetMgr::instance().getCenter();
            if (_isPointToDraw)
            {
                item.transients.push_back(std::make_shared<SketchPointTransient>(_sketchPlane, _position,
                    SnapConsts::PointSize, SnapConsts::PointColor));
            }
        }
        break;

        // 象限点(默认捕捉系统)
        case Snap2dType::QuadrantPoint:
        {
            item.pSnapTipWidget = SnapTipWidgetMgr::instance().getQuadrant();
            if (_isPointToDraw)
            {
                item.transients.push_back(std::make_shared<SketchPointTransient>(_sketchPlane, _position,
                    SnapConsts::PointSize, SnapConsts::PointColor));
            }
        }
        break;

        // 原点(默认捕捉系统)
        case Snap2dType::Origin:
        {
            item.pSnapTipWidget = SnapTipWidgetMgr::instance().getOrigin();
            if (_isPointToDraw)
            {
                item.transients.push_back(std::make_shared<SketchPointTransient>(_sketchPlane, _position,
                    SnapConsts::PointSize, SnapConsts::PointColor));
            }
        }
        break;

        default:
        {
            if (_isPointToDraw)
            {
                item.transients.push_back(std::make_shared<SketchPointTransient>(_sketchPlane, _position,
                    SnapConsts::PointSizeOnSketchCurve, SnapConsts::PointColorOnSketchCurve));
            }
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
        const Snap2dResultInfo& info = item.info;

        // Point markers always follow the snap position.
        for (GuiCmdTransientSPtr& pTransient : item.transients)
        {
            if (SketchPointTransient* pPointTransient = dynamic_cast<SketchPointTransient*>(pTransient.get()))
            {
                pPointTransient->update(_position);
            }
        }

        switch (info.type)
        {
        // 对齐X / 对齐Y / 关键点
        case Snap2dType::AlignX:
        case Snap2dType::AlignY:
        case Snap2dType::KeyPoint:
        {
            for (GuiCmdTransientSPtr& pTransient : item.transients)
            {
                if (SketchDotLineTransient* pDotLineTransient = dynamic_cast<SketchDotLineTransient*>(pTransient.get()))
                {
                    pDotLineTransient->update(info.refPoint, _position);
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
