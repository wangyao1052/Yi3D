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

#include "SolidMoveGizmo.h"
#include <cassert>

#include <wy3dApp/GuiCommand.h> // MouseEvent

#include <osg/Camera>
#include <osgViewer/View>

#include <gp_Quaternion.hxx>

#include <wyVector3.h>

#include <wy3dApp/OsgGizmoNode.h>
#include <wy3dApp/Application.h>
#include "application/ApplicationImpl.h"
#include <wy3dApp/Scene.h>
#include "gizmo/renderer/SolidMoveOsgRenderer.h"
#include <wy3dApp/OsgGizmoRenderer.h>
#include "utils/MathUtils.h"
#include <wy3dApp/wy3dAppDefs.h>

#define RADIUS 2.0
#define LEN1 50.0
#define LEN2 20.0

// ── 辅助函数 ──

namespace wy3dApp {

static bool computeClosestPoints(const osg::LineSegment& l1, const osg::LineSegment& l2,
    osg::Vec3d& p1, osg::Vec3d& p2)
{
    // Computes the closest points (p1 and p2 on line l1 and l2 respectively) between the two lines
    // An explanation of the algorithm can be found at
    // http://www.geometryalgorithms.com/Archive/algorithm_0106/algorithm_0106.htm

    osg::LineSegment::vec_type u = l1.end() - l1.start(); u.normalize();
    osg::LineSegment::vec_type v = l2.end() - l2.start(); v.normalize();

    osg::LineSegment::vec_type w0 = l1.start() - l2.start();

    double a = u * u;
    double b = u * v;
    double c = v * v;
    double d = u * w0;
    double e = v * w0;

    double denominator = a * c - b * b;

    // Test if lines are parallel
    if (denominator < 1e-12)
    {
        return false;
    }

    double sc = (b * e - c * d) / denominator;
    double tc = (a * e - b * d) / denominator;

    p1 = l1.start() + u * sc;
    p2 = l2.start() + v * tc;

    return true;
}

// ── SolidMoveGizmo ──

SolidMoveGizmo::SolidMoveGizmo(const wy3d::Primitive* pPrimitive, const osg::Vec4& color, const osg::Quat& axisRot)
    : DraggableGizmo(pPrimitive ? pPrimitive->getId() : wydb::ElementId{})
{
    assert(pPrimitive);
    if (!pPrimitive) return;

    // 创建渲染器（处理几何、高亮、拖拽视觉、初始位姿刷新）
    _pRenderer = std::make_unique<SolidMoveOsgRenderer>(this, color, axisRot);
}

SolidMoveGizmo::~SolidMoveGizmo()
{
}

void SolidMoveGizmo::setSiblings(SolidMoveGizmo* pSiblingA, SolidMoveGizmo* pSiblingB)
{
    _siblings[0] = pSiblingA;
    _siblings[1] = pSiblingB;
}

void SolidMoveGizmo::syncPosition()
{
    auto* pR = static_cast<SolidMoveOsgRenderer*>(_pRenderer.get());
    for (SolidMoveGizmo* s : _siblings)
        pR->syncPosition(static_cast<SolidMoveOsgRenderer*>(s ? s->_pRenderer.get() : nullptr));
}

DraggableGizmo::StepResult SolidMoveGizmo::onStartDrag(wydb::Database* pDb, const MouseEvent& event)
{
    osg::Vec3d closestPnt;
    if (!this->computeClosestPoint(static_cast<OsgGizmoRenderer*>(_pRenderer.get())->getOsgView()->getCamera(), event.x, event.y, closestPnt))
    {
        return { false, wy::Vector3() };
    }

    Scene* pActiveScene = ApplicationImpl::instance().getActiveScene();
    if (!pDb || !pActiveScene)
    {
        assert(false);
        return { false, wy::Vector3() };
    }

    _startPos = MathUtils::toVector3(closestPnt);
    return { true, _startPos };
}

DraggableGizmo::StepResult SolidMoveGizmo::onDragging(wydb::Database*, const MouseEvent& event)
{
    osg::Vec3d dragPnt;
    if (!this->computeClosestPoint(static_cast<OsgGizmoRenderer*>(_pRenderer.get())->getOsgView()->getCamera(), event.x, event.y, dragPnt))
    {
        return { false, wy::Vector3() };
    }

    return { true, MathUtils::toVector3(dragPnt) };
}

DraggableGizmo::StepResult SolidMoveGizmo::onEndDrag(wydb::Database* pDb, const MouseEvent& event)
{
    StepResult result = this->onDragging(pDb, event);
    if (!result.result) return result;

    wydb::TransactionManager* pTransMgr = pDb ? pDb->getTransactionManager() : nullptr;
    if (!pTransMgr) return { false, result.position };

    wydb::Transaction* pTrans = pTransMgr->startTransaction();
    if (!pTrans) return { false, result.position };

    bool ok = false;
    wy3d::Primitive* pPrimitive =
        wy3d::Primitive::cast(pTrans->getElementForWrite(this->getFirstElementId()));
    if (pPrimitive)
    {
        const wy::Vector3 delta = result.position - _startPos;
        pPrimitive->setPosition(pPrimitive->getPosition() + delta);
        ok = true;
    }

    if (ok)
    {
        if (wy::ErrorStatus::Ok != pTransMgr->endTransaction()) ok = false;
    }
    else
    {
        pTransMgr->abortTransaction();
    }

    return { ok, result.position };
}

void SolidMoveGizmo::onDragMoved()
{
    this->syncPosition();
}

void SolidMoveGizmo::onDragCancelled()
{
    this->syncPosition();
}

bool SolidMoveGizmo::computeClosestPoint(osg::Camera* camera, float winX, float winY, osg::Vec3d& outPnt)
{
    if (!camera) return false;
    osg::Matrix MVPW = camera->getViewMatrix() * camera->getProjectionMatrix() * camera->getViewport()->computeWindowMatrix();
    osg::Matrix inverseMVPW = MVPW.inverse(MVPW);
    osg::Vec3d nearPnt = osg::Vec3d(winX, winY, 0.0) * inverseMVPW;
    osg::Vec3d farPnt = osg::Vec3d(winX, winY, 1.0) * inverseMVPW;
    osg::ref_ptr<osg::LineSegment> pPickLineSeg = new osg::LineSegment(nearPnt, farPnt);
    osg::ref_ptr<osg::LineSegment> pAxisLineSeg = this->getAxisLineSegment();
    if (!pAxisLineSeg)
    {
        return false;
    }
    osg::Vec3d closestPtLine, closestPtProjWorkingLine;
    if (!computeClosestPoints(*pPickLineSeg, *pAxisLineSeg, closestPtLine, closestPtProjWorkingLine))
    {
        return false;
    }

    outPnt = closestPtProjWorkingLine;
    return true;
}

osg::ref_ptr<osg::LineSegment> SolidMoveGizmo::getAxisLineSegment() const
{
    return static_cast<SolidMoveOsgRenderer*>(_pRenderer.get())->getAxisLineSegment();
}

// ── SolidMoveXGizmo / SolidMoveYGizmo / SolidMoveZGizmo ──

SolidMoveXGizmo::SolidMoveXGizmo(const wy3d::Primitive* pPrimitive)
    : SolidMoveGizmo(pPrimitive,
        OsgGizmoNode::RED_COLOR,
        osg::Quat(osg::PI_2, osg::Vec3(0, 1, 0)))
{
}

SolidMoveYGizmo::SolidMoveYGizmo(const wy3d::Primitive* pPrimitive)
    : SolidMoveGizmo(pPrimitive,
        OsgGizmoNode::GREEN_COLOR,
        osg::Quat(-osg::PI_2, osg::Vec3(1, 0, 0)))
{
}

SolidMoveZGizmo::SolidMoveZGizmo(const wy3d::Primitive* pPrimitive)
    : SolidMoveGizmo(pPrimitive,
        OsgGizmoNode::BLUE_COLOR,
        osg::Quat())
{
}

} // namespace wy3dApp
