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

#ifndef WY3DAPP_OSG_COORD_UTIL_H
#define WY3DAPP_OSG_COORD_UTIL_H

#include <set>
#include <memory>

#include <osg/Vec3d>
#include <osg/LineSegment>
#include <osgViewer/View>

#include <wyVector2.h>
#include <wyVector3.h>
#include <wy3dSketchPlane.h>
#include <wydbElementId.h>
#include "snap/SnapSystemBase.h"

class GuiCmdMakeElement;
class SketchSnapSystem;
class SketchSnapContext;
class SketchSnapResult;
class SnapResultPoint;
class EndPointSnapObject;
class MiddlePointSnapObject;
class CenterPointSnapObject;
class Sketch3DSnapContext;
class Sketch3DSnapSystem;
class Sketch3DSnapResult;
namespace wydb { class Database; }

// OSG 坐标计算工具函数
namespace OsgCoordUtil {

    // 计算3D位置
    std::pair<wy::Vector3, wyap::SnapResultSPtr> computePosition3d(
        osgViewer::View* pView,
        double x, double y,
        const wy3d::SketchPlane& sketchPlane,
        const std::set<wydb::ElementId>& excludeIds,
        bool snap = true);

    std::pair<wy::Vector3, bool> computePosition3dForSketch3D(
        osgViewer::View* pView,
        double x, double y,
        const wy3d::SketchPlane& workPlane,
        const std::set<wydb::ElementId>& excludeIds,
        const Sketch3DSnapContext* pSnapContext,
        Sketch3DSnapSystem* pSketch3DSnapSys,
        const wydb::ElementId& sketch3dId);

    // 计算2D位置
    wy::Vector2 computePosition2d(
        osgViewer::View* pView,
        double x, double y,
        const wy3d::SketchPlane& sketchPlane,
        const std::set<wydb::ElementId>& excludeIds,
        std::shared_ptr<SketchSnapContext> pSnapContext = nullptr,
        SketchSnapSystem* pSketchSnapSys = nullptr,
        bool snap3d = true);

    // 计算2D位置(无捕捉)
    wy::Vector2 computePosition2dWithoutSnap(
        osgViewer::View* pView,
        double x, double y,
        const wy3d::SketchPlane& sketchPlane);

    // 计算高度
    bool computeHeight(
        osgViewer::View* pView,
        double x, double y,
        const wy::Vector3& basePnt,
        double& height,
        const GuiCmdMakeElement* pMakeElement = nullptr);
    bool computeHeight(
        osgViewer::View* pView,
        double x, double y,
        const osg::Vec3d& basePnt,
        double& height,
        const GuiCmdMakeElement* pMakeElement);  // no default, this is the main impl
    bool computeHeight2(
        osgViewer::View* pView,
        double x, double y,
        const wy3d::SketchPlane& workPln,
        const wy::Vector2& basePnt,
        const std::set<wydb::ElementId>& excludeIds,
        double& height);

    // 计算旋转角度
    bool computeRotationAngle(
        osgViewer::View* pView,
        double x, double y,
        const wy3d::SketchPlane& workPln,
        const wy::Vector2& basis,
        const std::set<wydb::ElementId>& excludeIds,
        double& rotationAngle);

    // 世界坐标投影到窗口像素坐标
    osg::Vec2d projectWorldToWindow(
        osgViewer::View* pView,
        const wy::Vector3& worldPnt);

} // namespace OsgCoordUtil

#endif // WY3DAPP_OSG_COORD_UTIL_H
