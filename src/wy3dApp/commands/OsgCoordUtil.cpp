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

#include "commands/OsgCoordUtil.h"

#include <cassert>

#include <osg/LineSegment>
#include <osg/Matrix>
#include <osgViewer/View>

#include <wyapSelManager.h>

#include <wy3dApp/Application.h>
#include "snap/SnapContext.h"
#include "commands/GuiCmdMakeElement.h"
#include <wy3dApp/Snap2dSystem.h>
#include <wy3dApp/Snap2dContext.h>
#include "snap/SnapResult.h"
#include "snap/SnapObject.h"
#include <wy3dApp/Snap2dResult.h>
#include "snap/Snap2dSystemImpl.h"
#include "snap3d/Sketch3DSnapObject.h"
#include "snap3d/Sketch3DSnapContext.h"
#include "snap3d/Sketch3DSnapResult.h"
#include "snap3d/Sketch3DSnapSystem.h"
#include "utils/MathUtils.h"
#include <wy3dApp/wy3dAppDefs.h>

// ── 文件级辅助函数 ──────────────────────────────────────────────

namespace wy3dApp {

// 全局体系点捕捉结果→3D草图捕捉结果
static std::shared_ptr<Sketch3DSnapResult> convertSnapResult3D(
    const wyap::SnapResultSPtr& pSnapRet3d)
{
    assert(pSnapRet3d);

    const SnapResultPoint* pSnapRetPoint = dynamic_cast<const SnapResultPoint*>(pSnapRet3d.get());
    if (!pSnapRetPoint) return nullptr;
    wyap::SnapObjectSPtr pSnapObj = pSnapRetPoint->getSnapObject();
    if (!pSnapObj) return nullptr;
    SnapPoint* pSnapPoint = dynamic_cast<SnapPoint*>(pSnapObj.get());
    if (!pSnapPoint)
    {
        assert(false);
        return nullptr;
    }

    Sketch3DSnapType snapType;
    switch (pSnapPoint->getType())
    {
    case SnapPoint::Type::End:
        snapType = Sketch3DSnapType::EndPoint;
        break;
    case SnapPoint::Type::Middle:
        snapType = Sketch3DSnapType::MiddlePoint;
        break;
    case SnapPoint::Type::Center:
        snapType = Sketch3DSnapType::CenterPoint;
        break;
    case SnapPoint::Type::Origin:
        snapType = Sketch3DSnapType::Origin;
        break;
    case SnapPoint::Type::Quadrant:
        snapType = Sketch3DSnapType::QuadrantPoint;
        break;
    default:
        // A hit always yields a result; kinds without a marker
        // (plain points) yield a display-less one.
        return std::make_shared<Sketch3DSnapResult>(pSnapRetPoint->getPosition());
    }

    std::shared_ptr<Sketch3DSnapResult> pResult = std::make_shared<Sketch3DSnapResult>(pSnapRetPoint->getPosition());
    Sketch3DSnapResult::Item item;
    item.pSnapObject = std::make_shared<Sketch3DPointSnapObject>(snapType, pSnapObj->getId());
    pResult->addItem(std::move(item));
    return pResult;
}

static bool computeClosestPoints(const osg::LineSegment& l1, const osg::LineSegment& l2,
    osg::Vec3d& p1, osg::Vec3d& p2)
{
    osg::LineSegment::vec_type u = l1.end() - l1.start(); u.normalize();
    osg::LineSegment::vec_type v = l2.end() - l2.start(); v.normalize();
    osg::LineSegment::vec_type w0 = l1.start() - l2.start();

    double a = u * u;
    double b = u * v;
    double c = v * v;
    double d = u * w0;
    double e = v * w0;

    double denominator = a * c - b * b;
    if (denominator == 0.0) return false;

    double sc = (b * e - c * d) / denominator;
    double tc = (a * e - b * d) / denominator;

    p1 = l1.start() + u * sc;
    p2 = l2.start() + v * tc;

    return true;
}

// ── 公共函数 ──────────────────────────────────────────────────

std::pair<wy::Vector3, wyap::SnapResultSPtr> OsgCoordUtil::computePoint(
    osgViewer::View* pView,
    double x, double y,
    const wy3d::Plane& workPlane,
    const std::set<wydb::ElementId>& excludeIds,
    bool snap,
    bool isResultPublished)
{
    if (snap)
    {
        wyap::SnapContextSPtr pSnapContext = std::make_shared<LocateContext>();
        wyap::SnapResultSPtr pSnapResult = Application::instance().getSnapSystem()->snap(
            x, y, pSnapContext, excludeIds, isResultPublished);
        if (pSnapResult)
        {
            wy::Vector3 retPnt = workPlane.value(workPlane.uv(pSnapResult->getPosition()));
            return std::pair<wy::Vector3, wyap::SnapResultSPtr>(retPnt, pSnapResult);
        }
    }

    return std::pair<wy::Vector3, wyap::SnapResultSPtr>(
        MathUtils::projectToPlane(pView, x, y, workPlane), nullptr);
}

std::pair<wy::Vector3, bool> OsgCoordUtil::computePosition3dForSketch3D(
    osgViewer::View* pView,
    double x, double y,
    const wy3d::Plane& workPlane,
    const std::set<wydb::ElementId>& excludeIds,
    const Sketch3DSnapContext* pSnapContext,
    Sketch3DSnapSystem* pSketch3DSnapSys,
    const wydb::ElementId& sketch3dId)
{
    assert(pView);
    std::pair<wy::Vector3, wyap::SnapResultSPtr> ret = OsgCoordUtil::computePoint(
        pView, x, y, workPlane, excludeIds, true);
    if (ret.second)
    {
        if (pSketch3DSnapSys)
        {
            pSketch3DSnapSys->setSnapResult(convertSnapResult3D(ret.second));
        }
        return std::make_pair(ret.second->getPosition(), true);
    }
    else
    {
        if (pSketch3DSnapSys && pSnapContext)
        {
            Sketch3DSnapResultSPtr pSketch3DSnapRet = pSketch3DSnapSys->snap(
                pSnapContext, pView, ret.first, workPlane, excludeIds, sketch3dId);
            if (pSketch3DSnapRet)
            {
                return std::make_pair(pSketch3DSnapRet->getPosition(), false);
            }
        }
        return std::make_pair(ret.first, false);
    }
}

std::pair<wy::Vector2, Snap2dResultSPtr> OsgCoordUtil::computePoint2d(
    osgViewer::View* pView,
    double x, double y,
    const std::set<wydb::ElementId>& excludeIds,
    std::shared_ptr<Snap2dContext> pSnapContext,
    Snap2dSystem* pSnap2dSys)
{
    if (!pSnap2dSys || !pSnapContext)
    {
        return std::make_pair(wy::Vector2(0.0, 0.0), Snap2dResultSPtr());
    }

    Snap2dSystemImpl* pSnap2dSystemImpl = dynamic_cast<Snap2dSystemImpl*>(pSnap2dSys);
    assert(pSnap2dSystemImpl);
    std::pair<Snap2dResultSPtr, wy::Vector2> ret = pSnap2dSystemImpl->snap(
        x, y, pView, excludeIds, pSnapContext);
    return std::make_pair(ret.first ? ret.first->getPosition() : ret.second, ret.first);
}

std::pair<wy::Vector2, wyap::SnapResultSPtr> OsgCoordUtil::computePoint2d(
    osgViewer::View* pView,
    double x, double y,
    const wy3d::Plane& workPlane,
    const std::set<wydb::ElementId>& excludeIds,
    bool snap)
{
    std::pair<wy::Vector3, wyap::SnapResultSPtr> ret = OsgCoordUtil::computePoint(
        pView, x, y, workPlane, excludeIds, snap);
    return std::make_pair(workPlane.uv(ret.first), ret.second);
}

double OsgCoordUtil::computeHeight(
    osgViewer::View* pView,
    double x, double y,
    const wy::Vector3& basePnt,
    const GuiCmdMakeElement* pMakeElement)
{
    if (!pView) return 0.0;
    osg::Camera* pCamera = pView->getCamera();
    if (!pCamera) return 0.0;
    osg::Matrix MVPW = pCamera->getViewMatrix() * pCamera->getProjectionMatrix() * pCamera->getViewport()->computeWindowMatrix();
    osg::Matrix inverseMVPW = MVPW.inverse(MVPW);
    osg::Vec3d nearPnt = osg::Vec3d(x, y, 0.0) * inverseMVPW;
    osg::Vec3d farPnt = osg::Vec3d(x, y, 1.0) * inverseMVPW;

    wyap::SnapContextSPtr pSnapContext = std::make_shared<LocateContext>();
    std::set<wydb::ElementId> excludeIds;
    if (pMakeElement) pMakeElement->collectElements(excludeIds);
    wyap::SnapResultSPtr pSnapResult = Application::instance().getSnapSystem()->snap(x, y, pSnapContext, excludeIds);
    if (pSnapResult)
    {
        const wy::Vector3& pos = pSnapResult->getPosition();
        farPnt.set(pos.x(), pos.y(), pos.z());
    }

    osg::ref_ptr<osg::LineSegment> mouseLineSeg = new osg::LineSegment(nearPnt, farPnt);

    wy::Vector3 wokkPlnNormal(0.0, 0.0, 1.0);
    if (pMakeElement)
    {
        wokkPlnNormal = pMakeElement->getWorkingPlaneNormal();
    }
    const osg::Vec3d osgBasePnt = MathUtils::toVec3d(basePnt);
    osg::ref_ptr<osg::LineSegment> workPlnBaseLineSeg = new osg::LineSegment(osgBasePnt,
        osgBasePnt + osg::Vec3d(wokkPlnNormal.x(), wokkPlnNormal.y(), wokkPlnNormal.z()));

    osg::Vec3d closestPtLine, closestPtProjWorkingLine;
    if (!computeClosestPoints(*mouseLineSeg, *workPlnBaseLineSeg, closestPtLine, closestPtProjWorkingLine))
        return 0.0;

    return (closestPtProjWorkingLine - osgBasePnt) * osg::Vec3d(wokkPlnNormal.x(), wokkPlnNormal.y(), wokkPlnNormal.z());
}

double OsgCoordUtil::computeHeight2(
    osgViewer::View* pView,
    double x, double y,
    const wy3d::Plane& workPln,
    const wy::Vector2& baseUV,
    const std::set<wydb::ElementId>& excludeIds)
{
    if (!pView) return 0.0;
    osg::Camera* pCamera = pView->getCamera();
    if (!pCamera) return 0.0;
    osg::Matrix MVPW = pCamera->getViewMatrix() * pCamera->getProjectionMatrix() * pCamera->getViewport()->computeWindowMatrix();
    osg::Matrix inverseMVPW = MVPW.inverse(MVPW);
    osg::Vec3d nearPnt = osg::Vec3d(x, y, 0.0) * inverseMVPW;
    osg::Vec3d farPnt = osg::Vec3d(x, y, 1.0) * inverseMVPW;

    wyap::SnapContextSPtr pSnapContext = std::make_shared<LocateContext>();
    wyap::SnapResultSPtr pSnapResult = Application::instance().getSnapSystem()->snap(x, y, pSnapContext, excludeIds);
    if (pSnapResult)
    {
        const wy::Vector3& pos = pSnapResult->getPosition();
        return (pos - workPln.getOrigin()).dot(workPln.getNormal());
    }

    osg::ref_ptr<osg::LineSegment> mouseLineSeg = new osg::LineSegment(nearPnt, farPnt);

    wy::Vector3 wokkPlnNormal = workPln.getNormal();
    wy::Vector3 basePnt = workPln.value(baseUV);
    osg::ref_ptr<osg::LineSegment> workPlnBaseLineSeg = new osg::LineSegment(MathUtils::toVec3d(basePnt),
        MathUtils::toVec3d(basePnt) + osg::Vec3d(wokkPlnNormal.x(), wokkPlnNormal.y(), wokkPlnNormal.z()));

    osg::Vec3d closestPtLine, closestPtProjWorkingLine;
    if (!computeClosestPoints(*mouseLineSeg, *workPlnBaseLineSeg, closestPtLine, closestPtProjWorkingLine))
        return 0.0;

    return (MathUtils::toVector3(closestPtProjWorkingLine) - basePnt).dot(wokkPlnNormal);
}

bool OsgCoordUtil::computeRotationAngle(
    osgViewer::View* pView,
    double x, double y,
    const wy3d::Plane& workPln,
    const wy::Vector2& basis,
    const std::set<wydb::ElementId>& excludeIds,
    double& rotationAngle)
{
    rotationAngle = 0.0;
    wy::Vector2 uvPnt;

    wyap::SnapContextSPtr pSnapContext = std::make_shared<LocateContext>();
    wyap::SnapResultSPtr pSnapResult = Application::instance().getSnapSystem()->snap(x, y, pSnapContext, excludeIds);
    if (pSnapResult)
    {
        const wy::Vector3& pos = pSnapResult->getPosition();
        uvPnt = workPln.uv(pos);
        rotationAngle = wy::Vector2::rotationAngle(basis, uvPnt);
        return true;
    }
    else
    {
        if (!pView) return false;
        osg::Camera* pCamera = pView->getCamera();
        if (!pCamera) return false;
        osg::Matrix MVPW = pCamera->getViewMatrix() * pCamera->getProjectionMatrix() * pCamera->getViewport()->computeWindowMatrix();
        osg::Matrix inverseMVPW = MVPW.inverse(MVPW);
        osg::Vec3d world = osg::Vec3d(x, y, 0.0) * inverseMVPW;

        osg::Vec3d eye, center, up;
        pCamera->getViewMatrixAsLookAt(eye, center, up);
        osg::Vec3d projectDir(0.0, 0.0, -1.0);
        double left, right, bottom, top, zNear, zFar;
        if (pCamera->getProjectionMatrixAsOrtho(left, right, bottom, top, zNear, zFar))
        {
            projectDir = center - eye;
            projectDir.normalize();
        }
        else
        {
            projectDir = world - eye;
            projectDir.normalize();
        }

        wy::Vector3 workPlnNormal = workPln.getNormal();
        double D = (-workPln.getOrigin()).dot(workPlnNormal);
        double t = -workPlnNormal.dot(wy::Vector3(world.x(), world.y(), world.z())) - D;
        double s = workPlnNormal.dot(wy::Vector3(projectDir.x(), projectDir.y(), projectDir.z()));
        if (std::fabs(s) > wy3d::EPS)
        {
            t /= s;
        }
        else
        {
            rotationAngle = 0.0;
            return false;
        }
        osg::Vec3d projPnt = world + projectDir * t;
        uvPnt = workPln.uv(projPnt.x(), projPnt.y(), projPnt.z());

        rotationAngle = wy::Vector2::rotationAngle(basis, uvPnt);
        rotationAngle = wy3d::radiansToDegrees(rotationAngle);
        rotationAngle = std::round(rotationAngle * 10) / 10;
        rotationAngle = wy3d::degreesToRadians(rotationAngle);

        return true;
    }
}

// 世界坐标投影到窗口像素坐标
osg::Vec2d OsgCoordUtil::projectWorldToWindow(
    osgViewer::View* pView,
    const wy::Vector3& worldPnt)
{
    assert(pView);
    static const osg::Vec2d kNullRet(0.0, 0.0);
    if (!pView || !pView->getCamera() || !pView->getCamera()->getViewport())
    {
        return kNullRet;
    }

    osg::Camera* camera = pView->getCamera();
    osg::Matrix MVPW = camera->getViewMatrix() * camera->getProjectionMatrix() *
        camera->getViewport()->computeWindowMatrix();
    osg::Vec3d win = osg::Vec3d(worldPnt.x(), worldPnt.y(), worldPnt.z()) * MVPW;
    return osg::Vec2d(win.x(), win.y());
}

} // namespace wy3dApp
