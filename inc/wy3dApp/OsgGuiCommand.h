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

#ifndef WY3DAPP_OSG_GUI_COMMAND_H
#define WY3DAPP_OSG_GUI_COMMAND_H

#include <osg/observer_ptr>
#include <osg/Vec3d>
#include <osgGA/GUIActionAdapter>
#include <osgViewer/View>

#include <wyVector2.h>
#include <wyVector3.h>
#include <wy3dPlane.h>
#include <wy3dApp/GuiCommand.h>
#include "snap/SketchSnapContext.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class WY3DAPP_EXPORT OsgGuiCommand : public GuiCommand
{
    WYRX_DECLARE_ABSTRACT_MEMBERS(OsgGuiCommand, wy3dApp::OsgGuiCommand, GuiCommand)
public:
    OsgGuiCommand();

    // 事件响应函数
    bool handle(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& aa);

protected:
    // 获取 OSG 视图
    osgViewer::View* getOsgView() const;

    // Converts screen coordinates to window coordinates.
    void screenToWindow(double screenX, double screenY,
                        double& windowX, double& windowY) const override;

    // 点选
    wyap::Selection pointPick(double x, double y,
        const PointPickOption& option) override;

    // 坐标计算
    std::pair<wy::Vector3, wyap::SnapResultSPtr> computePosition3d(
        double x, double y,
        const wy3d::Plane& sketchPlane,
        const std::set<wydb::ElementId>& excludeIds,
        bool snap = true) override;

    wy::Vector2 computePosition2d(
        double x, double y,
        const wy3d::Plane& sketchPlane,
        const std::set<wydb::ElementId>& excludeIds,
        std::shared_ptr<SketchSnapContext> pSnapContext = nullptr,
        SketchSnapSystem* pSketchSnapSys = nullptr,
        bool snap3d = true) override;

    wy::Vector2 computePosition2dWithoutSnap(
        double x, double y,
        const wy3d::Plane& sketchPlane) override;

    bool computeHeight(double x, double y,
        const wy::Vector3& basePnt, double& height,
        const GuiCmdMakeElement* pMakeElement = nullptr) override;
    bool computeHeight(double x, double y,
        const osg::Vec3d& basePnt, double& height,
        const GuiCmdMakeElement* pMakeElement = nullptr) override;
    bool computeHeight2(double x, double y,
        const wy3d::Plane& workPln,
        const wy::Vector2& basePnt,
        const std::set<wydb::ElementId>& excludeIds,
        double& height) override;

    bool computeRotationAngle(double x, double y,
        const wy3d::Plane& workPln,
        const wy::Vector2& basis,
        const std::set<wydb::ElementId>& excludeIds,
        double& rotationAngle) override;

protected:
    // 当前命令事件对应的视图(非拥有)
    osg::observer_ptr<osgViewer::View> _pOsgView;
};

} // namespace wy3dApp

#endif // WY3DAPP_OSG_GUI_COMMAND_H
