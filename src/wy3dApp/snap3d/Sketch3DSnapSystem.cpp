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

#include "snap3d/Sketch3DSnapSystem.h"

#include <cassert>
#include <cmath>

#include <wydbDatabase.h>
#include <wy3dImpl.h>
#include <wy3dSketch3D.h>
#include <wy3dSketchLine3D.h>
#include <wy3dSketchCircle3D.h>
#include <wy3dApp/application/Application.h>

#include "commands/OsgCoordUtil.h"

namespace
{
    // 两向量夹角[0,PI]
    double angleBetween(const wy::Vector3& a, const wy::Vector3& b)
    {
        const double lenA = a.length();
        const double lenB = b.length();
        if (lenA < wy3d::TOL || lenB < wy3d::TOL)
        {
            return wy3d::PI;
        }
        double cosv = a.dot(b) / (lenA * lenB);
        cosv = std::max(-1.0, std::min(1.0, cosv));
        return std::acos(cosv);
    }

    // 两直线方向夹角[0,PI/2]:方向不分正反(90度与270度、0度与180度视为同向)
    double lineAngleBetween(const wy::Vector3& a, const wy::Vector3& b)
    {
        const double ang = angleBetween(a, b);
        return std::min(ang, wy3d::PI - ang);
    }

    // 参考线/参考圆与工作平面平行的判定容差(方向单位向量点积)
    const double kPlaneParallelTol = 1e-6;
}

Sketch3DSnapSystem::Sketch3DSnapSystem()
    : _pSnapResult(nullptr)
{
}

Sketch3DSnapSystem::~Sketch3DSnapSystem()
{
    this->clearSnapResult();
}

Sketch3DSnapResultSPtr Sketch3DSnapSystem::snap(
    const Sketch3DSnapContext* pContext,
    osgViewer::View* pView,
    const wy::Vector3& rawPnt,
    const wy3d::SketchPlane& workPlane,
    const std::set<wydb::ElementId>& excludeIds,
    const wydb::ElementId& sketch3dId)
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    assert(pDb);
    Sketch3DSnapResultSPtr pResult(nullptr);
    if (pDb && pContext && pView)
    {
        switch (pContext->getType())
        {
        case Sketch3DSnapContextType::DrawLine:
            pResult = this->snapDrawLine(
                pDb, dynamic_cast<const Sketch3DDrawLineContext*>(pContext),
                pView, rawPnt, workPlane, excludeIds, sketch3dId);
            break;
        case Sketch3DSnapContextType::DrawCircle:
            pResult = this->snapDrawCircle(
                pDb, dynamic_cast<const Sketch3DDrawCircleContext*>(pContext),
                pView, rawPnt, workPlane, excludeIds, sketch3dId);
            break;
        case Sketch3DSnapContextType::Locate:
        default:
            break;
        }
    }

    this->setSnapResult(pResult);
    return pResult;
}

void Sketch3DSnapSystem::clearSnapResult()
{
    if (_pSnapResult)
    {
        _pSnapResult->hide();
        _pSnapResult = nullptr;
    }
}

Sketch3DSnapResultSPtr Sketch3DSnapSystem::snapDrawLine(
    wydb::Database* pDb,
    const Sketch3DDrawLineContext* pContext,
    osgViewer::View* pView,
    const wy::Vector3& rawPnt,
    const wy3d::SketchPlane& workPlane,
    const std::set<wydb::ElementId>& excludeIds,
    const wydb::ElementId& sketch3dId)
{
    assert(pContext && pView && pDb);
    if (!pContext || !pView || !pDb)
    {
        return nullptr;
    }

    const wy::Vector3 startPnt = pContext->getStartPoint();
    const wy::Vector3 delta = rawPnt - startPnt;
    const double drawLength = delta.length();
    if (drawLength <= wy3d::TOL)
    {
        return nullptr;
    }

    wy::Vector3 xDir = workPlane.getXDir();
    xDir.normalize();
    wy::Vector3 yDir = workPlane.getYDir();
    yDir.normalize();
    wy::Vector3 normal = workPlane.getNormal();
    normal.normalize();

    // 捕捉角度容差:15像素,上限10度(与2D草图一致)
    const double scale = this->computePixelScale(pView, workPlane, startPnt);
    double angleTol = std::atan2(15.0 * scale, drawLength);
    static const double kRadian10 = wy3d::degreesToRadians(10.0);
    if (angleTol > kRadian10)
    {
        angleTol = kRadian10;
    }

    // 平面坐标系下的绘制角度,折叠到[0,PI)
    const double u = delta.dot(xDir);
    const double v = delta.dot(yDir);
    if (std::sqrt(u * u + v * v) <= wy3d::TOL)
    {
        return nullptr; // 面内分量退化(如起点离面且delta近乎纯法向),无有效角度
    }
    double drawAngle = std::atan2(v, u); // [-PI, PI],含两端
    if (drawAngle >= wy3d::PI)
    {
        drawAngle -= wy3d::PI; // +PI折回0(方向按PI为周期)
    }
    else if (drawAngle < 0.0)
    {
        drawAngle += wy3d::PI; // 负角折回[0,PI)
    }
    assert(drawAngle >= 0.0 && drawAngle < wy3d::PI);

    // 候选方向(按优先级:切点相切>水平>竖直>平行>垂直),命中后按精确方向投影重建端点
    struct Candidate
    {
        Sketch3DSnapType type;
        wydb::ElementId refId;
        double refLength;   // 参考线长度(平行/垂直命中时有效,-1表示无)
        wy::Vector3 dir;
        wy::Vector3 exactPnt;  // 切点捕捉:精确端点(切点本身)
    };
    std::shared_ptr<Candidate> pCandidate; // 空=未命中任何候选(无哨兵值歧义)

    {
        const wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pDb->getElement(sketch3dId));
        if (pSketch3D)
        {
            const osg::Vec2d cursor2d = OsgCoordUtil::projectWorldToWindow(pView, rawPnt);
            const double tangentSnapPx = 10.0; // 像素

            for (const wydb::ElementId& id : pSketch3D->getChildren())
            {
                if (excludeIds.count(id) != 0)
                {
                    continue;
                }
                const wy3d::SketchCircle3D* pCircle = wy3d::SketchCircle3D::cast(pDb->getElement(id));
                if (!pCircle)
                {
                    continue;
                }

                // 参考圆必须落在当前工作平面上(法向平行且圆心在平面上),切点才会在平面内
                wy::Vector3 circleNormal = pCircle->getNormal();
                circleNormal.normalize();
                if (std::fabs(circleNormal.dot(normal)) < 1.0 - kPlaneParallelTol)
                {
                    continue;
                }
                const wy::Vector3 centerToOrigin = workPlane.getOrigin() - pCircle->getCenter();
                if (std::fabs(centerToOrigin.dot(normal)) > kPlaneParallelTol)
                {
                    continue; // 圆心不在工作平面上(平行偏移),跳过
                }

                // 起点投影到圆平面,取圆平面2D坐标
                wy::Vector3 circleXDir = pCircle->getXDir();
                circleXDir.normalize();
                const wy::Vector3 circleYDir = circleNormal.cross(circleXDir);
                const wy::Vector3 relRaw = startPnt - pCircle->getCenter();
                const wy::Vector3 rel = relRaw - circleNormal * relRaw.dot(circleNormal);
                const double ux = rel.dot(circleXDir);
                const double uy = rel.dot(circleYDir);
                const double d = std::sqrt(ux * ux + uy * uy);
                const double radius = pCircle->getRadius();
                if (d <= radius + wy3d::TOL)
                {
                    continue; // 起点在圆内或圆上,无外切线
                }

                // 切点:C + R*(cos(phi±theta), sin(phi±theta)), theta=acos(R/d)
                double cosTheta = radius / d;
                cosTheta = std::max(-1.0, std::min(1.0, cosTheta));
                const double theta = std::acos(cosTheta);
                const double phi = std::atan2(uy, ux);
                wy::Vector3 tangentPnts[2];
                for (int i = 0; i < 2; ++i)
                {
                    const double angle = phi + (0 == i ? theta : -theta);
                    tangentPnts[i] = pCircle->getCenter()
                        + circleXDir * (radius * std::cos(angle))
                        + circleYDir * (radius * std::sin(angle));
                }

                // 取屏幕距离更近且在10像素内的切点
                int best(-1);
                double bestPx(tangentSnapPx);
                for (int i = 0; i < 2; ++i)
                {
                    const osg::Vec2d pnt2d = OsgCoordUtil::projectWorldToWindow(pView, tangentPnts[i]);
                    const double distPx = (pnt2d - cursor2d).length();
                    if (distPx < bestPx)
                    {
                        bestPx = distPx;
                        best = i;
                    }
                }
                if (best < 0)
                {
                    continue;
                }

                pCandidate = std::make_shared<Candidate>();
                pCandidate->type = Sketch3DSnapType::Tangent;
                pCandidate->refId = id;
                pCandidate->refLength = -1.0;
                pCandidate->dir = tangentPnts[best] - startPnt;
                pCandidate->dir.normalize();
                pCandidate->exactPnt = tangentPnts[best];
                break; // 命中切点,不再看其他圆
            }
        }
    }

    // 1.水平:与平面XDir平行
    if (!pCandidate)
    {
        const double tolH = std::min(drawAngle, wy3d::PI - drawAngle);
        if (tolH <= angleTol)
        {
            pCandidate = std::make_shared<Candidate>();
            pCandidate->type = Sketch3DSnapType::Horizontal;
            pCandidate->refId = wydb::ElementId::kNull; // 水平/竖直无参考图元
            pCandidate->refLength = -1.0;
            pCandidate->dir = xDir;
        }
        else if (std::fabs(drawAngle - wy3d::PI_2) <= angleTol)
        {
            // 2.竖直:与平面YDir平行
            pCandidate = std::make_shared<Candidate>();
            pCandidate->type = Sketch3DSnapType::Vertical;
            pCandidate->refId = wydb::ElementId::kNull;
            pCandidate->refLength = -1.0;
            pCandidate->dir = yDir;
        }
        else
        {
            // 3/4.平行/垂直已有线:仅参考方向平行于工作平面的线(两遍扫描保持优先级)
            const wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pDb->getElement(sketch3dId));
            if (!pSketch3D)
            {
                return nullptr;
            }

            auto tryLine = [&](const wydb::ElementId& id, bool parallelPass) -> bool
            {
                if (excludeIds.count(id) != 0)
                {
                    return false;
                }
                const wy3d::SketchLine3D* pLine = wy3d::SketchLine3D::cast(pDb->getElement(id));
                if (!pLine)
                {
                    return false;
                }
                wy::Vector3 d = pLine->getEndPoint() - pLine->getStartPoint();
                if (d.length() <= wy3d::TOL)
                {
                    return false;
                }
                d.normalize();
                if (std::fabs(d.dot(normal)) > kPlaneParallelTol)
                {
                    return false; // 参考线方向不平行于工作平面,跳过
                }

            if (parallelPass)
            {
                if (lineAngleBetween(delta, d) <= angleTol)
                {
                    pCandidate = std::make_shared<Candidate>();
                    pCandidate->type = Sketch3DSnapType::Parallel;
                    pCandidate->refId = id;
                    pCandidate->refLength = pLine->getLength();
                    pCandidate->dir = d;
                    return true;
                }
            }
            else
            {
                // 面内垂直方向:既在平面内又垂直于参考方向
                const wy::Vector3 dirPerp = normal.cross(d);
                if (lineAngleBetween(delta, dirPerp) <= angleTol)
                {
                    pCandidate = std::make_shared<Candidate>();
                    pCandidate->type = Sketch3DSnapType::Perpendicular;
                    pCandidate->refId = id;
                    pCandidate->refLength = pLine->getLength();
                    pCandidate->dir = dirPerp;
                    return true;
                }
            }
                return false;
            };

            for (const wydb::ElementId& id : pSketch3D->getChildren())
            {
                if (tryLine(id, true)) { break; }
            }
            if (!pCandidate)
            {
                for (const wydb::ElementId& id : pSketch3D->getChildren())
                {
                    if (tryLine(id, false)) { break; }
                }
            }
            if (!pCandidate)
            {
                return nullptr;
            }
        }
    }
    if (!pCandidate)
    {
        return nullptr;
    }

    wy::Vector3 snappedPnt;
    bool equalSnapped(false);
    if (pCandidate->type == Sketch3DSnapType::Tangent)
    {
        // 切点捕捉:端点精确取切点(落在圆上,不经过投影重建)
        snappedPnt = pCandidate->exactPnt;
    }
    else
    {
        // 按精确方向投影重建端点(负投影翻转方向)
        double projectLen = delta.dot(pCandidate->dir);
        if (projectLen < 0.0)
        {
            projectLen = -projectLen;
            pCandidate->dir = -pCandidate->dir;
        }

        // 平行/垂直命中的前提下,长度接近参考线长度时再吸附为相等(水平/竖直不做长度相等)
        if (pCandidate->refLength > wy3d::TOL)
        {
            const double valueTol = 10.0 * scale; // 10像素,与画圆相等捕捉一致
            if (std::fabs(projectLen - pCandidate->refLength) <= valueTol)
            {
                projectLen = pCandidate->refLength;
                equalSnapped = true;
            }
        }
        snappedPnt = startPnt + pCandidate->dir * projectLen;
    }

    Sketch3DSnapResultSPtr pResult = std::make_shared<Sketch3DSnapResult>(snappedPnt);
    Sketch3DSnapResult::Item item;
    item.pSnapObject = std::make_shared<Sketch3DAngleSnapObject>(pCandidate->type, pCandidate->refId);
    pResult->addItem(std::move(item));

    if (equalSnapped)
    {
        // 追加相等捕捉项(与角度项同一参考图元,数值在对象上)
        Sketch3DSnapResult::Item equalItem;
        equalItem.pSnapObject = std::make_shared<Sketch3DEqualSnapObject>(pCandidate->refId, pCandidate->refLength);
        pResult->addItem(std::move(equalItem));
    }
    return pResult;
}

Sketch3DSnapResultSPtr Sketch3DSnapSystem::snapDrawCircle(
    wydb::Database* pDb,
    const Sketch3DDrawCircleContext* pContext,
    osgViewer::View* pView,
    const wy::Vector3& rawPnt,
    const wy3d::SketchPlane& workPlane,
    const std::set<wydb::ElementId>& excludeIds,
    const wydb::ElementId& sketch3dId)
{
    assert(pContext && pView && pDb);
    const double rawRadius = (rawPnt - pContext->getCenterPoint()).length();
    if (!pContext || !pView || !pDb || rawRadius <= wy3d::TOL)
    {
        return nullptr;
    }

    const wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pDb->getElement(sketch3dId));
    if (!pSketch3D)
    {
        return nullptr;
    }

    // 半径容差:10像素(与2D草图相等捕捉一致)
    const double scale = this->computePixelScale(pView, workPlane, pContext->getCenterPoint());
    const double valueTol = 10.0 * scale;

    wy::Vector3 normal = workPlane.getNormal();
    normal.normalize();

    for (const wydb::ElementId& id : pSketch3D->getChildren())
    {
        if (excludeIds.count(id) != 0)
        {
            continue;
        }
        const wy3d::SketchCircle3D* pCircle = wy3d::SketchCircle3D::cast(pDb->getElement(id));
        if (!pCircle)
        {
            continue;
        }

        // 参考圆仅限平面平行于工作平面(法向平行)
        wy::Vector3 circleNormal = pCircle->getNormal();
        circleNormal.normalize();
        if (std::fabs(circleNormal.dot(normal)) < 1.0 - kPlaneParallelTol)
        {
            continue;
        }

        const double refRadius = pCircle->getRadius();
        if (std::fabs(rawRadius - refRadius) < valueTol)
        {
            // 结果携带"吸附后半径对应的圆周上的点"(与2D语义一致,命令用|点-圆心|取半径)
            const wy::Vector3 dir = rawPnt - pContext->getCenterPoint();
            if (dir.length() <= wy3d::TOL)
            {
                return nullptr;
            }
            const wy::Vector3 snappedPnt = pContext->getCenterPoint() + dir * (refRadius / dir.length());

            Sketch3DSnapResultSPtr pResult = std::make_shared<Sketch3DSnapResult>(snappedPnt);
            Sketch3DSnapResult::Item item;
            item.pSnapObject = std::make_shared<Sketch3DEqualSnapObject>(id, refRadius);
            pResult->addItem(std::move(item));
            return pResult;
        }
    }
    return nullptr;
}

void Sketch3DSnapSystem::setSnapResult(Sketch3DSnapResultSPtr pResult)
{
    if (_pSnapResult && pResult && _pSnapResult->isLooseEqual(*pResult))
    {
        return; // 捕捉结果未变化,保持显示避免闪烁
    }

    this->clearSnapResult();
    _pSnapResult = pResult;
    if (_pSnapResult)
    {
        _pSnapResult->show();
    }
}

double Sketch3DSnapSystem::computePixelScale(
    osgViewer::View* pView,
    const wy3d::SketchPlane& workPlane,
    const wy::Vector3& originPnt) const
{
    // 返回"每像素对应的世界单位数"(与2D草图scaleScreen同语义):
    // 平面X/Y轴单位长度投影到屏幕的像素数,像素数越多说明每像素代表的单位越少
    wy::Vector3 xDir = workPlane.getXDir();
    xDir.normalize();
    wy::Vector3 yDir = workPlane.getYDir();
    yDir.normalize();

    const osg::Vec2d origin2d = OsgCoordUtil::projectWorldToWindow(pView, originPnt);
    const osg::Vec2d x2d = OsgCoordUtil::projectWorldToWindow(pView, originPnt + xDir);
    const osg::Vec2d y2d = OsgCoordUtil::projectWorldToWindow(pView, originPnt + yDir);

    const double pxX = (x2d - origin2d).length();
    const double pxY = (y2d - origin2d).length();
    if (pxX <= 0.0 || pxY <= 0.0)
    {
        return 0.0; // 视图退化(轴投影为零像素),不捕捉
    }
    // 1世界单位=pxX/pxY像素 → 1像素=(pxX+pxY)/2取倒数的世界单位
    return 2.0 / (pxX + pxY);
}