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
#include <wy3dApp/Snap2dContext.h>
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
        const PointPickOptions& option) override;

    // 坐标计算
    std::pair<wy::Vector3, wyap::SnapResultSPtr> computePoint(
        double x, double y,
        const wy3d::Plane& workPlane,
        bool snap = true,
        const std::set<wydb::ElementId>& excludeIds = {}) override;

    std::pair<wy::Vector2, Snap2dResultSPtr> computePoint2d(
        double x, double y,
        Snap2dSystem* pSnap2dSystem,
        std::shared_ptr<Snap2dContext> pSnapContext,
        const std::set<wydb::ElementId>& snapExcludeIds = {}) override;

    std::pair<wy::Vector2, wyap::SnapResultSPtr> computePoint2d(
        double x, double y,
        const wy3d::Plane& workPlane,
        bool snap = true,
        const std::set<wydb::ElementId>& excludeIds = {}) override;

    double computeHeight(double x, double y,
        const wy::Vector3& basePnt,
        const GuiCmdMakeElement* pMakeElement = nullptr) override;
    double computeHeight(double x, double y,
        const osg::Vec3d& basePnt,
        const GuiCmdMakeElement* pMakeElement = nullptr) override;
    double computeHeight2(double x, double y,
        const wy3d::Plane& workPln,
        const wy::Vector2& basePnt,
        const std::set<wydb::ElementId>& excludeIds) override;

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
