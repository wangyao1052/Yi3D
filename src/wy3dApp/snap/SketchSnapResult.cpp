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

#include "SketchSnapResult.h"
#include <wyVector2.h>
#include <wy3dSketchPlane.h>
#include "application/Application.h"
#include "scene/RenderConst.h"
#include "commands/transient/SketchPointTransient.h"
#include "SnapConsts.h"

SketchSnapResult::SketchSnapResult(
    const wy3d::SketchPlane& sketchPlane,
    const wy::Vector2& position,
    SketchSnapContextSPtr pContext,
    SketchSnapObjectSPtr pSnapObject,
    bool drawPoint)
    : _sketchPlane(sketchPlane), _position(position), _pContext(pContext),
      _isPointToDraw(drawPoint)
{
    assert(pContext);
    assert(pSnapObject);

    _items.reserve(3);
    _items.emplace_back(std::move(SketchSnapResultItem(pSnapObject)));
}

SketchSnapResult::SketchSnapResult(
    const wy3d::SketchPlane& sketchPlane,
    const wy::Vector2& position,
    SketchSnapContextSPtr pContext,
    SketchSnapObjectSPtr pSnapObject, SketchSnapObjectSPtr pOtherSnapObject,
    bool drawPoint)
    : _sketchPlane(sketchPlane), _position(position), _pContext(pContext),
      _isPointToDraw(drawPoint)
{
    assert(pContext);
    assert(pSnapObject);
    assert(pOtherSnapObject);

    _items.reserve(3);
    _items.emplace_back(std::move(SketchSnapResultItem(pSnapObject)));
    _items.emplace_back(std::move(SketchSnapResultItem(pOtherSnapObject)));
}

SketchSnapResult::~SketchSnapResult()
{
}

bool SketchSnapResult::isLooseEqual(const SketchSnapResult& rhs) const
{
    // 上下文是否相等
    if (_pContext.get() != rhs._pContext.get())
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

std::map<wydb::ElementId, SketchCurveTransientSPtr> SketchSnapResult::getSketchCurveTransients() const
{
    std::map<wydb::ElementId, SketchCurveTransientSPtr> rets;
    for (const SketchSnapResultItem& item : _items)
    {
        const SketchSnapObject* pSnapObj = item.pSnapObject.get();
        if (!pSnapObj)
        {
            assert(false);
            continue;
        }

        wydb::ElementId id = pSnapObj->getId();
        if (id.isNull()) continue; // id可以为空比如:水平or竖直or坐标系原点

        for (GuiCmdTransientSPtr pTransient : item.transients)
        {
            if (!pTransient)
            {
                assert(false);
                continue;
            }

            // 如果是草图曲线临时绘制对象
            if (SketchCurveTransientSPtr pSketchCurve = std::dynamic_pointer_cast<SketchCurveTransient>(pTransient))
            {
                rets[id] = pSketchCurve;
            }
        }
    }
    return rets;
}

void SketchSnapResult::show(const std::map<wydb::ElementId, SketchCurveTransientSPtr>* pCache)
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return;

    for (SketchSnapResultItem& item : _items)
    {
        const SketchSnapObject* pSnapObject = item.pSnapObject.get();
        if (!pSnapObject)
        {
            assert(false);
            continue;
        }
        wydb::ElementId id = pSnapObject->getId();
        if (pCache)
        {
            auto iter = pCache->find(id);
            if (iter != pCache->cend())
            {
                assert(iter->second);
                item.transients.push_back(iter->second);
            }
            else if (const wy3d::SketchCurve* pSketchCurve = wy3d::SketchCurve::cast(pDb->getElement(id)))
            {
                item.transients.emplace_back(std::move(
                    std::make_shared<SketchCurveTransient>(pSketchCurve)));
            }
        }
        else
        {
            if (const wy3d::SketchCurve* pSketchCurve = wy3d::SketchCurve::cast(pDb->getElement(id)))
            {
                item.transients.emplace_back(std::move(
                    std::make_shared<SketchCurveTransient>(pSketchCurve)));
            }
        }
        
        switch (pSnapObject->getType())
        {
        // 水平
        case SketchSnapType::Horizontal:
        {
            item.pSnapTipWidget = SnapTipWidgetMgr::instance().getHorizontal();
        }
        break;

        // 竖直
        case SketchSnapType::Vertical:
        {
            item.pSnapTipWidget = SnapTipWidgetMgr::instance().getVertical();
        }
        break;

        // 平行
        case SketchSnapType::Parallel:
        {
            item.pSnapTipWidget = SnapTipWidgetMgr::instance().getParallel();
        }
        break;

        // 垂直
        case SketchSnapType::Perpendicular:
        {
            item.pSnapTipWidget = SnapTipWidgetMgr::instance().getPerpendicular();
        }
        break;

        // 对齐X
        case SketchSnapType::AlignX:
        {
            // 点划线
            const AlignXSnapObject* pAlignX = dynamic_cast<const AlignXSnapObject*>(pSnapObject);
            assert(pAlignX);
            item.transients.push_back(std::make_shared<SketchDotLineTransient>(
                _sketchPlane, pAlignX->getPosition(), _position));
            item.transients.push_back(std::make_shared<SketchPointTransient>(_sketchPlane, _position,
                SnapConsts::PointSizeOnSketchCurve, SnapConsts::PointColorOnSketchCurve));
        }
        break;

        // 对齐Y
        case SketchSnapType::AlignY:
        {
            // 点划线
            const AlignYSnapObject* pAlignY = dynamic_cast<const AlignYSnapObject*>(pSnapObject);
            assert(pAlignY);
            item.transients.push_back(std::make_shared<SketchDotLineTransient>(
                _sketchPlane, pAlignY->getPosition(), _position));
            item.transients.push_back(std::make_shared<SketchPointTransient>(_sketchPlane, _position,
                SnapConsts::PointSizeOnSketchCurve, SnapConsts::PointColorOnSketchCurve));
        }
        break;

        // 相等
        case SketchSnapType::Equal:
        {
            item.pSnapTipWidget = SnapTipWidgetMgr::instance().getEqual();
        }
        break;

        // 点在线上
        case SketchSnapType::PointOnCurve:
        {
            const PointOnCurveSnapObject* pPointOnCurve = dynamic_cast<const PointOnCurveSnapObject*>(pSnapObject);
            assert(pPointOnCurve);
            item.transients.push_back(std::make_shared<SketchPointTransient>(_sketchPlane, _position,
                SnapConsts::PointSizeOnSketchCurve, SnapConsts::PointColorOnSketchCurve));
        }
        break;

        // 垂足
        case SketchSnapType::PerpendicularPoint:
        {
            item.pSnapTipWidget = SnapTipWidgetMgr::instance().getPerpendicular();
            item.transients.push_back(std::make_shared<SketchPointTransient>(_sketchPlane, _position,
                SnapConsts::PointSize, SnapConsts::PointColor));
        }
        break;

        // 切点
        case SketchSnapType::TangetPoint:
        {
            wy::Vector2 pos = _position;
            const TangentPointSnapObject* pTangentPointSnapObj = dynamic_cast<const TangentPointSnapObject*>(pSnapObject);
            if (pTangentPointSnapObj)
            {
                pos = pTangentPointSnapObj->getValue();
            }
            item.pSnapTipWidget = SnapTipWidgetMgr::instance().getTangent();
            item.transients.push_back(std::make_shared<SketchPointTransient>(_sketchPlane, pos,
                SnapConsts::PointSize, SnapConsts::PointColor));
        }
        break;

        // 交点
        case SketchSnapType::IntersectionPoint:
        {
            item.pSnapTipWidget = SnapTipWidgetMgr::instance().getIntersection();
            item.transients.push_back(std::make_shared<SketchPointTransient>(_sketchPlane, _position,
                SnapConsts::PointSize, SnapConsts::PointColor));
        }
        break;

        // 关键点
        case SketchSnapType::KeyPoint:
        {
            // 点
            item.transients.push_back(std::make_shared<SketchPointTransient>(_sketchPlane, _position,
                SnapConsts::PointSize, SnapConsts::PointColor));
            // 点划线
            const KeyPointSnapObject* pKeyPoint = dynamic_cast<const KeyPointSnapObject*>(pSnapObject);
            assert(pKeyPoint);
            item.transients.push_back(std::make_shared<SketchDotLineTransient>(
                _sketchPlane, pKeyPoint->getRefPoint(), _position));
        }
        break;

        // 端点(默认捕捉系统)
        case SketchSnapType::EndPoint:
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
        case SketchSnapType::MiddlePoint:
        {
            item.pSnapTipWidget = SnapTipWidgetMgr::instance().getMiddle();
        }
        break;

        // 中心点(默认捕捉系统)
        case SketchSnapType::CenterPoint:
        {
            item.pSnapTipWidget = SnapTipWidgetMgr::instance().getCenter();
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
    _pSnapTipMouseFollow = std::make_shared<SketchSnapTipMouseFollow>();
    for (const SketchSnapResultItem& item : _items)
    {
        if (item.pSnapTipWidget)
            _pSnapTipMouseFollow->addTipWidget(item.pSnapTipWidget);
    }
    _pSnapTipMouseFollow->start();
}

void SketchSnapResult::update()
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return;

    for (SketchSnapResultItem& item : _items)
    {
        SketchSnapObject* pSnapObject = item.pSnapObject.get();
        if (!pSnapObject) continue;
        switch (pSnapObject->getType())
        {
        // 对齐X
        case SketchSnapType::AlignX:
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
        case SketchSnapType::AlignY:
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
        case SketchSnapType::PointOnCurve:
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