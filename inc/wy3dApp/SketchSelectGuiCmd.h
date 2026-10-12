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

#ifndef WY3DAPP_SKETCH_SELECT_GUI_CMD_H
#define WY3DAPP_SKETCH_SELECT_GUI_CMD_H

#include <wy3dApp/SelectGuiCmd.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class WY3DAPP_EXPORT SketchSelectGuiCmd : public SelectGuiCmd
{
    WYRX_DECLARE_MEMBERS(SketchSelectGuiCmd, wy3dApp::SketchSelectGuiCmd, SelectGuiCmd)
public:
    SketchSelectGuiCmd();
    virtual ~SketchSelectGuiCmd();

protected:
    GuiCmdSketchInfo _sketchInfo;
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void configureSelectOptions(GuiCmdSelectOptions& options) override;
    virtual void onStart_EnvSpecific() override;
    virtual void selectAll_Impl(wyap::SelectionSet& ss) override;
    virtual bool tryAddPositionGizmo_Impl(const wyap::SelectionSet& sels, std::list<wyap::GizmoSPtr>& gizmos) override;
    virtual wy::Vector3 computePastePosition(double x, double y,
        const std::set<wydb::ElementId>& excludeIds) override;
    SketchSnapSystem* getSketchSnapSys() const override { return _sketchInfo.pSketchSnapSys; }
    wydb::ElementId getSketchId() const override { return _sketchInfo.sketchId; }
    const wy3d::Plane& getSketchPlane() const override { return _sketchInfo.sketchPlane; }
};

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_SELECT_GUI_CMD_H
