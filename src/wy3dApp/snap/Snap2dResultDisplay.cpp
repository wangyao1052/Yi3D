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

#include "Snap2dResultDisplay.h"

#include <cassert>
#include <wyVector2.h>
#include <wy3dPlane.h>

#include "SnapConsts.h"
#include "commands/transient/SketchPointTransient.h"
#include "commands/transient/SketchCurveTransient.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

Snap2dResultDisplay::Snap2dResultDisplay(HighlightTransientCreator createHighlight)
    : _createHighlight(std::move(createHighlight))
{
    assert(_createHighlight);
}

Snap2dResultDisplay::~Snap2dResultDisplay()
{
    this->clear();
}

void Snap2dResultDisplay::apply(const Snap2dResult* pResult)
{
    if (!pResult)
    {
        this->clear();
        return;
    }

    this->refreshElementHighlights(*pResult);

    const std::vector<Snap2dHit>& items = pResult->getHits();
    std::vector<ItemDisplay> newItemDisplays;
    const size_t numItems = items.size();
    newItemDisplays.reserve(numItems);
    bool tipsChanged = (numItems != _itemDisplays.size());
    for (size_t i = 0; i < numItems; ++i)
    {
        bool isNew(true);
        if (i >= _itemDisplays.size())
        {
            newItemDisplays.emplace_back();
        }
        else if (items[i].type == _itemDisplays[i].type)
        {
            isNew = false;
            newItemDisplays.emplace_back(std::move(_itemDisplays[i]));
        }
        else
        {
            newItemDisplays.emplace_back();
        }

        if (isNew)
        {
            tipsChanged = true;
            newItemDisplays[i].type = items[i].type;
            this->buildItemDisplay(*pResult, items[i], newItemDisplays[i]);
        }
        this->updateItemDisplay(*pResult, items[i], newItemDisplays[i]);
    }

    _itemDisplays = std::move(newItemDisplays);
    if (tipsChanged) this->rebuildTipMouseFollow();
}

void Snap2dResultDisplay::buildItemDisplay(const Snap2dResult& result,
    const Snap2dHit& item, ItemDisplay& display)
{
    const wy3d::Plane& sketchPlane = result.getWorkPlane();
    const wy::Vector2& position = result.getPosition();

    switch (item.type)
    {
    // 水平
    case Snap2dType::Horizontal:
    {
        display.pSnapTipWidget = SnapTipWidgetMgr::instance().getHorizontal();
    }
    break;

    // 竖直
    case Snap2dType::Vertical:
    {
        display.pSnapTipWidget = SnapTipWidgetMgr::instance().getVertical();
    }
    break;

    // 平行
    case Snap2dType::Parallel:
    {
        display.pSnapTipWidget = SnapTipWidgetMgr::instance().getParallel();
    }
    break;

    // 垂直
    case Snap2dType::Perpendicular:
    {
        display.pSnapTipWidget = SnapTipWidgetMgr::instance().getPerpendicular();
    }
    break;

    // 对齐X / 对齐Y
    case Snap2dType::AlignX:
    case Snap2dType::AlignY:
    {
        // 点划线
        display.transients.push_back(std::make_shared<SketchDotLineTransient>(
            sketchPlane, item.point, position));
        display.transients.push_back(std::make_shared<SketchPointTransient>(sketchPlane, position,
            SnapConsts::PointSizeOnSketchCurve, SnapConsts::PointColorOnSketchCurve));
    }
    break;

    // 相等
    case Snap2dType::EqualLength:
    case Snap2dType::EqualRadius:
    {
        display.pSnapTipWidget = SnapTipWidgetMgr::instance().getEqual();
    }
    break;

    // 最近点
    case Snap2dType::Nearest:
    {
        display.transients.push_back(std::make_shared<SketchPointTransient>(sketchPlane, position,
            SnapConsts::PointSizeOnSketchCurve, SnapConsts::PointColorOnSketchCurve));
    }
    break;

    // 垂足
    case Snap2dType::PerpendicularPoint:
    {
        display.pSnapTipWidget = SnapTipWidgetMgr::instance().getPerpendicular();
        display.transients.push_back(std::make_shared<SketchPointTransient>(sketchPlane, position,
            SnapConsts::PointSize, SnapConsts::PointColor));
    }
    break;

    // 切点
    case Snap2dType::TangentPoint:
    {
        display.pSnapTipWidget = SnapTipWidgetMgr::instance().getTangent();
        display.transients.push_back(std::make_shared<SketchPointTransient>(sketchPlane, position,
            SnapConsts::PointSize, SnapConsts::PointColor));
    }
    break;

    // 交点
    case Snap2dType::IntersectionPoint:
    {
        display.pSnapTipWidget = SnapTipWidgetMgr::instance().getIntersection();
        display.transients.push_back(std::make_shared<SketchPointTransient>(sketchPlane, position,
            SnapConsts::PointSize, SnapConsts::PointColor));
    }
    break;

    // 端点(默认捕捉系统)
    case Snap2dType::EndPoint:
    {
        display.pSnapTipWidget = SnapTipWidgetMgr::instance().getEnd();
        display.transients.push_back(std::make_shared<SketchPointTransient>(sketchPlane, position,
            SnapConsts::PointSize, SnapConsts::PointColor));
    }
    break;

    // 中点(默认捕捉系统)
    case Snap2dType::MiddlePoint:
    {
        display.pSnapTipWidget = SnapTipWidgetMgr::instance().getMiddle();
        display.transients.push_back(std::make_shared<SketchPointTransient>(sketchPlane, position,
            SnapConsts::PointSize, SnapConsts::PointColor));
    }
    break;

    // 中心点(默认捕捉系统)
    case Snap2dType::CenterPoint:
    {
        display.pSnapTipWidget = SnapTipWidgetMgr::instance().getCenter();
        display.transients.push_back(std::make_shared<SketchPointTransient>(sketchPlane, position,
            SnapConsts::PointSize, SnapConsts::PointColor));
    }
    break;

    // 象限点(默认捕捉系统)
    case Snap2dType::QuadrantPoint:
    {
        display.pSnapTipWidget = SnapTipWidgetMgr::instance().getQuadrant();
        display.transients.push_back(std::make_shared<SketchPointTransient>(sketchPlane, position,
            SnapConsts::PointSize, SnapConsts::PointColor));
    }
    break;

    // 原点(默认捕捉系统)
    case Snap2dType::Origin:
    {
        display.pSnapTipWidget = SnapTipWidgetMgr::instance().getOrigin();
        display.transients.push_back(std::make_shared<SketchPointTransient>(sketchPlane, position,
            SnapConsts::PointSize, SnapConsts::PointColor));
    }
    break;

    // 一般点(样条首点)
    case Snap2dType::GenericPoint:
    {
        display.transients.push_back(std::make_shared<SketchPointTransient>(sketchPlane, position,
            SnapConsts::PointSize, SnapConsts::PointColor));
    }
    break;

    default:
    {
        display.transients.push_back(std::make_shared<SketchPointTransient>(sketchPlane, position,
            SnapConsts::PointSizeOnSketchCurve, SnapConsts::PointColorOnSketchCurve));
    }
    break;
    }
}

void Snap2dResultDisplay::updateItemDisplay(const Snap2dResult& result,
    const Snap2dHit& item, ItemDisplay& display)
{
    const wy::Vector2& position = result.getPosition();

    // Point markers always follow the snap position.
    for (GuiCmdTransientSPtr& pTransient : display.transients)
    {
        if (SketchPointTransient* pPointTransient = dynamic_cast<SketchPointTransient*>(pTransient.get()))
        {
            pPointTransient->update(position);
        }
    }

    switch (item.type)
    {
    // 对齐X / 对齐Y
    case Snap2dType::AlignX:
    case Snap2dType::AlignY:
    {
        for (GuiCmdTransientSPtr& pTransient : display.transients)
        {
            if (SketchDotLineTransient* pDotLineTransient = dynamic_cast<SketchDotLineTransient*>(pTransient.get()))
            {
                pDotLineTransient->update(item.point, position);
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

void Snap2dResultDisplay::clear()
{
    _itemDisplays.clear();
    _elementHighlights.clear();
    // Stops the timer and hides the tip widgets.
    _pSnapTipMouseFollow = nullptr;
}

void Snap2dResultDisplay::refreshElementHighlights(const Snap2dResult& result)
{
    std::map<wydb::ElementId, GuiCmdTransientSPtr> newElemHighlights;
    const std::vector<Snap2dHit>& items = result.getHits();
    for (const Snap2dHit& item : items)
    {
        const wydb::ElementId& id = item.element;
        if (id.isNull() || newElemHighlights.find(id) != newElemHighlights.cend())
        {
            continue;
        }

        auto iter = _elementHighlights.find(id);
        if (iter != _elementHighlights.cend())
        {
            newElemHighlights.emplace(id, std::move(iter->second));
            continue;
        }

        GuiCmdTransientSPtr pTransient = _createHighlight(id);
        if (pTransient)
        {
            newElemHighlights.emplace(id, std::move(pTransient));
        }
    }

    _elementHighlights = std::move(newElemHighlights);
}

void Snap2dResultDisplay::rebuildTipMouseFollow()
{
    // The follow object depends only on the tip-widget set.
    _pSnapTipMouseFollow = std::make_shared<SnapTipMouseFollow>();
    for (const ItemDisplay& display : _itemDisplays)
    {
        if (display.pSnapTipWidget)
            _pSnapTipMouseFollow->addTipWidget(display.pSnapTipWidget);
    }
    _pSnapTipMouseFollow->start();
}

} // namespace wy3dApp
