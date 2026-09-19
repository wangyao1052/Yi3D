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

#include "snap3d/Sketch3DSnapResult.h"

#include <cassert>
#include <set>

#include <wydbDatabase.h>
#include <wy3dImpl.h>
#include <wy3dSketchCurve3D.h>

#include "application/Application.h"
#include "commands/transient/Sketch3DCurveTransient.h"

Sketch3DSnapResult::Sketch3DSnapResult(const wy::Vector3& position)
    : _items(), _position(position), _pMouseFollow(nullptr)
{
}

Sketch3DSnapResult::~Sketch3DSnapResult()
{
    this->hide();
}

void Sketch3DSnapResult::addItem(Item&& item)
{
    _items.emplace_back(std::move(item));
}

void Sketch3DSnapResult::show()
{
    if (_pMouseFollow)
    {
        return;
    }

    wydb::Database* pDb = Application::instance().getActiveDatabase();

    _pMouseFollow = std::make_shared<SketchSnapTipMouseFollow>();
    std::set<wydb::ElementId> highlightedIds; // 同一参考图元只高亮一次(角度+相等可同参考)
    for (Item& item : _items)
    {
        if (!item.pSnapObject)
        {
            continue;
        }

        // 参考图元高亮(平行/垂直/相等/切点携带参考id;水平/竖直无参考)
        const wydb::ElementId refId = item.pSnapObject->getRefId();
        if (!refId.isNull() && pDb && highlightedIds.insert(refId).second)
        {
            if (const wy3d::SketchCurve3D* pCurve = wy3d::SketchCurve3D::cast(pDb->getElement(refId)))
            {
                item.transients.emplace_back(std::make_shared<Sketch3DCurveTransient>(pCurve));
            }
        }

        SnapTipWidget* pTipWidget(nullptr);
        switch (item.pSnapObject->getType())
        {
        case Sketch3DSnapType::Horizontal:
            pTipWidget = SnapTipWidgetMgr::instance().getHorizontal();
            break;
        case Sketch3DSnapType::Vertical:
            pTipWidget = SnapTipWidgetMgr::instance().getVertical();
            break;
        case Sketch3DSnapType::Parallel:
            pTipWidget = SnapTipWidgetMgr::instance().getParallel();
            break;
        case Sketch3DSnapType::Perpendicular:
            pTipWidget = SnapTipWidgetMgr::instance().getPerpendicular();
            break;
        case Sketch3DSnapType::Equal:
            pTipWidget = SnapTipWidgetMgr::instance().getEqual();
            break;
        case Sketch3DSnapType::Tangent:
            pTipWidget = SnapTipWidgetMgr::instance().getTangent();
            break;
        case Sketch3DSnapType::EndPoint:
            pTipWidget = SnapTipWidgetMgr::instance().getEnd();
            break;
        case Sketch3DSnapType::MiddlePoint:
            pTipWidget = SnapTipWidgetMgr::instance().getMiddle();
            break;
        case Sketch3DSnapType::CenterPoint:
            pTipWidget = SnapTipWidgetMgr::instance().getCenter();
            break;
        default:
            assert(false);
            break;
        }
        item.pSnapTipWidget = pTipWidget;
        if (item.pSnapTipWidget)
        {
            _pMouseFollow->addTipWidget(item.pSnapTipWidget);
        }
    }
    _pMouseFollow->start();
}

void Sketch3DSnapResult::hide()
{
    // 先释放参考图元高亮(GuiCmdTransient析构自动从场景移除)
    for (Item& item : _items)
    {
        item.transients.clear();
    }

    if (_pMouseFollow)
    {
        _pMouseFollow->stop(); // stop内部隐藏全部图标
        _pMouseFollow = nullptr;
    }
}

bool Sketch3DSnapResult::isLooseEqual(const Sketch3DSnapResult& other) const
{
    if (_items.size() != other._items.size())
    {
        return false;
    }

    // 只比较捕捉到的对象(类型+参考id),不比较位置(与2D语义一致)
    for (size_t i = 0; i < _items.size(); ++i)
    {
        const Sketch3DSnapObject* pObjA = _items[i].pSnapObject.get();
        const Sketch3DSnapObject* pObjB = other._items[i].pSnapObject.get();
        if ((pObjA == nullptr) != (pObjB == nullptr))
        {
            return false;
        }
        if (!pObjA)
        {
            continue;
        }
        if (pObjA->getType() != pObjB->getType() || pObjA->getRefId() != pObjB->getRefId())
        {
            return false;
        }
    }
    return true;
}