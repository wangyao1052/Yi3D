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

#include "SketchEllipseGripGizmo.h"
#include <wyVector2.h>
#include <wy3dSketchEllipse.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

SketchEllipseGripGizmo::SketchEllipseGripGizmo(const wy3d::SketchEllipse* pSketchEllipse, Type type)
    : SketchEntityGripGizmo(pSketchEllipse), _type(type), _startMajorRadius(0.0), _startMinorRadius(0.0)
{
    assert(pSketchEllipse);
}

wy::Vector2 SketchEllipseGripGizmo::getGripPosition() const
{
    const wy3d::SketchEllipse* pSketchEllipse = wy3d::SketchEllipse::cast(this->getSketchEntity());
    if (pSketchEllipse)
    {        switch (_type)
        {
        case Type::Center:
            return pSketchEllipse->getCenter();
        case Type::Quarter0:
            return pSketchEllipse->getPointAt(0.0);
        case Type::Quarter1:
            return pSketchEllipse->getPointAt(0.25);
        case Type::Quarter2:
            return pSketchEllipse->getPointAt(0.5);
        case Type::Quarter3:
            return pSketchEllipse->getPointAt(0.75);
        default:
            break;
        }
    }

    assert(false);
    return wy::Vector2(0.0, 0.0);
}

bool SketchEllipseGripGizmo::onBeginDrag(wydb::Database* pDb)
{
    assert(pDb);
    const wy3d::SketchEllipse* pEllipse = wy3d::SketchEllipse::cast(pDb->getElement(_id));
    if (!pEllipse)
    {
        assert(false);
        return false;
    }
    _startMajorRadius = pEllipse->getMajorRadius();
    _startMinorRadius = pEllipse->getMinorRadius();
    return true;
}

bool SketchEllipseGripGizmo::onDragging(wydb::Transaction* pTrans, const wy::Vector2& curPos)
{
    assert(pTrans);
    wy3d::SketchEllipse* pSketchEllipse = wy3d::SketchEllipse::cast(pTrans->getElementForWrite(_id));
    if (pSketchEllipse)
    {        switch (_type)
        {
        case Type::Center:
            return wy::ErrorStatus::Ok == pSketchEllipse->setCenter(curPos);
        case Type::Quarter0:
        case Type::Quarter2:
        {
            double majorRadius = (curPos - pSketchEllipse->getCenter()).length();
            wy::ErrorStatus error = pSketchEllipse->setMajorRadius(majorRadius);
            if (wy::ErrorStatus::Ok != error) return false; // 半径过小时会返回false
            error = pSketchEllipse->setRadiusRatio(_startMinorRadius / majorRadius, true); // 半径比过小或过大时会返回false
            if (wy::ErrorStatus::Ok != error)
            {
                return false;
            }
            return true;
        }
        case Type::Quarter1:
        case Type::Quarter3:
        {
            double minorRadius = (curPos - pSketchEllipse->getCenter()).length();
            return wy::ErrorStatus::Ok == pSketchEllipse->setRadiusRatio(minorRadius / _startMajorRadius, true);
        }
        default:
            break;
        }
    }

    assert(false);
    return false;
}

// 椭圆由三个参数表达:圆心&长半轴&短轴半径与长轴半径的比例(<=1.0);
// 椭圆的长轴半径必须大于等于短轴半径,在wy3d::SketchEllipse的API接口setRadiusRatio中做了限制;
// 在椭圆的夹点拖拽过程中,允许长轴半径小于短轴半径,在完成拖拽之后如果长轴半径小于短轴半径,需要互换长短轴;
// 注:目前草图Gizmo的架构系统中,Gizmo由id和类型唯一确定,如果允许拖拽夹点过程中长轴与短轴互换,则夹点在
// refresh时会转换类型,目前系统不支持,所以在拖拽完成之后针对长轴半径小于短轴半径的情况互换长短轴.
void SketchEllipseGripGizmo::onEndDrag(wydb::Database* pDb, bool commit)
{
    if (!commit) return; // 取消则不用处理

    assert(pDb);
    const wy3d::SketchEllipse* pConstEllipse = wy3d::SketchEllipse::cast(pDb->getElement(_id));
    if (!pConstEllipse)
    {
        assert(false);
        return;
    }

    // 如果短轴半径比长轴半径短则直接返回;否则需要互换长短轴
    double radiusRatio = pConstEllipse->getRadiusRatio();
    if (radiusRatio <= 1.0) return;

    // 互换长轴与短轴
    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();
    assert(pTransMgr);
    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = pTransMgr->startTransaction("", option);
    if (!pTrans)
    {
        assert(false);
        return;
    }
    wy3d::SketchEllipse* pEllipse = wy3d::SketchEllipse::cast(pTrans->getElementForWrite(_id));
    if (!pEllipse)
    {
        assert(false);
        pTransMgr->abortTransaction();
        return;
    }

    wy::Vector2 newMajorAxis = pEllipse->getMinorAxis();
    double newRadiusRatio = 1.0 / radiusRatio;
    assert(newRadiusRatio < 1.0);
    wy::ErrorStatus error = pEllipse->setMajorAxis(newMajorAxis);
    if (wy::ErrorStatus::Ok != error)
    {
        assert(false);
        pTransMgr->abortTransaction();
        return;
    }
    error = pEllipse->setRadiusRatio(newRadiusRatio);
    if (wy::ErrorStatus::Ok != error)
    {
        assert(false);
        pTransMgr->abortTransaction();
        return;
    }
    pTransMgr->endTransaction();
    return;
}

}
