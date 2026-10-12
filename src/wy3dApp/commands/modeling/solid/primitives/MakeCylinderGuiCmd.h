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

#ifndef WY3DAPP_MAKE_CYLINDER_GUI_CMD_H
#define WY3DAPP_MAKE_CYLINDER_GUI_CMD_H

#include <cfloat>
#include <wyVector2.h>
#include <wyVector3.h>
#include <wy3dVector3.h>
#include <wy3dCylinder.h>
#include "commands/modeling/solid/primitives/MakePrimitiveGuiCmd.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class MakeCylinder;
class SketchCircleTransient;
class GuiCmdHoverInputPopup1;
class GuiCmdHoverInputPopup2;

class MakeCylinderGuiCmd : public MakePrimitiveGuiCmd
{
    WYRX_DECLARE_MEMBERS(MakeCylinderGuiCmd, wy3dApp::MakeCylinderGuiCmd, MakePrimitiveGuiCmd)
public:
    MakeCylinderGuiCmd();
    virtual ~MakeCylinderGuiCmd();

protected:
    virtual void reset();
    virtual void cleanup() override;
    virtual bool finishStep(unsigned int step) override;
    virtual void gotoStepImpl(unsigned int step) override;

    void onFrame(double time) override;
    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseDown(const MouseEvent& event) override;

private:
    void initializePopups();
    void showPopup();
    void hidePopup();
    void tryShowPopupOnHover(double time);
    void onPopupEnterKey();
    void onPopupEscapeKey();
    void simulateMouseMoveFromPopup();

private:
    struct HoverPopupState
    {
        double lastMouseX;
        double lastMouseY;
        double lastMouseMoveTime;
        wy::Vector2 point;
        double radius;
        double height;
        int heightSign;

        HoverPopupState()
            : lastMouseX(DBL_MAX)
            , lastMouseY(DBL_MAX)
            , lastMouseMoveTime(-1.0)
            , point(0.0, 0.0)
            , radius(0.0)
            , height(0.0)
            , heightSign(1)
        {}

        void resetValue()
        {
            point.set(0.0, 0.0);
            radius = 0.0;
            height = 0.0;
            heightSign = 1;
        }
    };

private:
    wy::Vector2 _uv1;
    wy::Vector2 _uv2;
    double _height;

    // 创建圆柱体
    std::shared_ptr<MakeCylinder> _pMakeCylinder;
    std::shared_ptr<SketchCircleTransient> _pCircleTransient;
    std::unique_ptr<GuiCmdHoverInputPopup2> _pXYPopup;
    std::unique_ptr<GuiCmdHoverInputPopup1> _pRadiusPopup;
    std::unique_ptr<GuiCmdHoverInputPopup1> _pHeightPopup;
    HoverPopupState _hoverPopupState;
};

class MakeCylinder : public GuiCmdMakeElement
{
public:
    MakeCylinder(GuiCommand* pGuiCmd) : GuiCmdMakeElement(pGuiCmd), _pCylinder(nullptr) {}
    ~MakeCylinder() {}

    // 收集创建的元素ID
    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

    // 创建
    bool init(const wy3d::Plane& workPln, const wy::Vector2& uv1, const wy::Vector2& uv2);
    // 更新
    bool update(double height);

private:
    wy3d::Cylinder* _pCylinder;
    wy::Vector3 _initOrigin;
    wy::Vector3 _zAxis;
};

}

#endif // WY3DAPP_MAKE_CYLINDER_GUI_CMD_H
