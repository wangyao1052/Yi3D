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

#include "SketchEllipseArcGizmo.h"
#include <cassert>
#include <wyVector2.h>
#include <wy3dSketchEllipseArc.h>
#include <wy3dApp/Application.h>
#include "utils/MathUtils.h"
#include "gizmo/renderer/SketchEllipseArcGhostOsgRenderer.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

EllipseArcGhostGizmo::EllipseArcGhostGizmo(const wy3d::SketchEllipseArc* pSketchEllipseArc)
    : GhostGizmo(pSketchEllipseArc)
{
    _pRenderer = std::make_unique<SketchEllipseArcGhostOsgRenderer>(this);
}

SketchEllipseArcGripGizmo::SketchEllipseArcGripGizmo(const wy3d::SketchEllipseArc* pSketchEllipseArc, Type type)
    : SketchEntityGripGizmo(pSketchEllipseArc), _type(type), _startMajorRadius(0.0), _startMinorRadius(0.0)
{
    assert(pSketchEllipseArc);
}

wy::Vector2 SketchEllipseArcGripGizmo::getGripPosition() const
{
    const wy3d::SketchEllipseArc* pEllipseArc = wy3d::SketchEllipseArc::cast(this->getSketchEntity());
    if (pEllipseArc)
    {        switch (_type)
        {
        case Type::Center:
            return pEllipseArc->getCenter();
        case Type::MajorPositive:
            return MathUtils::getPointAtEllipse(pEllipseArc->getCenter(), pEllipseArc->getMajorAxis(), pEllipseArc->getRadiusRatio(), 0.0);
        case Type::MajorNegtive:
            return MathUtils::getPointAtEllipse(pEllipseArc->getCenter(), pEllipseArc->getMajorAxis(), pEllipseArc->getRadiusRatio(), 0.5);
        case Type::MinorPositive:
            return MathUtils::getPointAtEllipse(pEllipseArc->getCenter(), pEllipseArc->getMajorAxis(), pEllipseArc->getRadiusRatio(), 0.25);
        case Type::MinorNegtive:
            return MathUtils::getPointAtEllipse(pEllipseArc->getCenter(), pEllipseArc->getMajorAxis(), pEllipseArc->getRadiusRatio(), 0.75);
        case Type::StartAngle:
            return pEllipseArc->getStartPoint();
        case Type::EndAngle:
            return pEllipseArc->getEndPoint();
        default:
            break;
        }
    }

    assert(false);
    return wy::Vector2(0.0, 0.0);
}

bool SketchEllipseArcGripGizmo::onBeginDrag(wydb::Database* pDb)
{
    assert(pDb);
    const wy3d::SketchEllipseArc* pEllipseArc = wy3d::SketchEllipseArc::cast(pDb->getElement(_id));
    if (!pEllipseArc)
    {
        assert(false);
        return false;
    }
    _startMajorRadius = pEllipseArc->getMajorRadius();
    _startMinorRadius = pEllipseArc->getMinorRadius();
    return true;
}

bool SketchEllipseArcGripGizmo::onDragging(wydb::Transaction* pTrans, const wy::Vector2& curPos)
{
    assert(pTrans);
    wy3d::SketchEllipseArc* pEllipseArc = wy3d::SketchEllipseArc::cast(pTrans->getElementForWrite(_id));
    if (pEllipseArc)
    {        switch (_type)
        {
        case Type::Center:
            return wy::ErrorStatus::Ok == pEllipseArc->setCenter(curPos);
        case Type::MajorPositive:
        case Type::MajorNegtive:
        {
            double majorRadius = (curPos - pEllipseArc->getCenter()).length();
            wy::ErrorStatus error = pEllipseArc->setMajorRadius(majorRadius);
            if (wy::ErrorStatus::Ok != error) return false; // 半径过小时会返回false
            error = pEllipseArc->setRadiusRatio(_startMinorRadius / majorRadius, true); // 半径比过小或过大时会返回false
            if (wy::ErrorStatus::Ok != error)
            {
                return false;
            }
            return true;
        }
        case Type::MinorPositive:
        case Type::MinorNegtive:
        {
            double minorRadius = (curPos - pEllipseArc->getCenter()).length();
            return wy::ErrorStatus::Ok == pEllipseArc->setRadiusRatio(minorRadius / _startMajorRadius, true);
        }
        case Type::StartAngle:
        {
            wy::Vector2 vec = curPos - pEllipseArc->getCenter();
            wy::Vector2 majorAxis = pEllipseArc->getMajorAxis();
            double startAngle = wy::Vector2::rotationAngle(majorAxis, vec); //[0,2PI)
            return wy::ErrorStatus::Ok == pEllipseArc->setStartAngle(startAngle);
        }
        case Type::EndAngle:
        {
            wy::Vector2 vec = curPos - pEllipseArc->getCenter();
            wy::Vector2 majorAxis = pEllipseArc->getMajorAxis();
            double endAngle = wy::Vector2::rotationAngle(majorAxis, vec); //[0,2PI)
            return wy::ErrorStatus::Ok == pEllipseArc->setEndAngle(endAngle);
        }
        default:
            break;
        }
    }

    assert(false);
    return false;
}

void SketchEllipseArcGripGizmo::onEndDrag(wydb::Database* pDb, bool commit)
{
    if (!commit) return; // 取消则不用处理

    assert(pDb);
    const wy3d::SketchEllipseArc* pConstEllipseArc = wy3d::SketchEllipseArc::cast(pDb->getElement(_id));
    if (!pConstEllipseArc)
    {
        assert(false);
        return;
    }

    // 如果短轴半径比长轴半径短则直接返回;否则需要互换长短轴
    double radiusRatio = pConstEllipseArc->getRadiusRatio();
    if (radiusRatio <= 1.0) return;
    wy::Vector2 startPnt = pConstEllipseArc->getStartPoint();
    wy::Vector2 endPnt = pConstEllipseArc->getEndPoint();

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
    wy3d::SketchEllipseArc* pEllipseArc = wy3d::SketchEllipseArc::cast(pTrans->getElementForWrite(_id));
    if (!pEllipseArc)
    {
        assert(false);
        pTransMgr->abortTransaction();
        return;
    }

    // 互换长短轴
    wy::Vector2 newMajorAxis = pEllipseArc->getMinorAxis();
    double newRadiusRatio = 1.0 / radiusRatio; // radiusRatio > 1.0
    assert(newRadiusRatio < 1.0);
    wy::ErrorStatus error = pEllipseArc->setMajorAxis(newMajorAxis);
    if (wy::ErrorStatus::Ok != error)
    {
        assert(false);
        pTransMgr->abortTransaction();
        return;
    }
    error = pEllipseArc->setRadiusRatio(newRadiusRatio);
    if (wy::ErrorStatus::Ok != error)
    {
        assert(false);
        pTransMgr->abortTransaction();
        return;
    }

    // 互换长短轴后,新的起始角度的值比旧值小90度.
    double newStartAngle = MathUtils::normalizeRadian(pEllipseArc->getStartAngle() - wy3d::PI_2);
    error = pEllipseArc->setStartAngle(newStartAngle);
    if (wy::ErrorStatus::Ok != error)
    {
        assert(false);
        pTransMgr->abortTransaction();
        return;
    }

    // 互换长短轴后,新的终止角度的值比旧值小90度.
    double newEndAngle = MathUtils::normalizeRadian(pEllipseArc->getEndAngle() - wy3d::PI_2);
    if (newEndAngle < newStartAngle)
    {
        newEndAngle += wy3d::PI * 2;
    }
    error = pEllipseArc->setEndAngle(newEndAngle);
    if (wy::ErrorStatus::Ok != error)
    {
        assert(false);
        pTransMgr->abortTransaction();
        return;
    }

    // 提交事务
    pTransMgr->endTransaction();

    return;
}

}
