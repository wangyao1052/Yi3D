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

#ifndef WY3DAPP_MAKE_CONE_GUI_CMD_H
#define WY3DAPP_MAKE_CONE_GUI_CMD_H

#include <cfloat>
#include <wyVector2.h>
#include <wyVector3.h>
#include <wy3dVector3.h>
#include <wy3dCone.h>
#include "commands/modeling/solid/primitives/MakePrimitiveGuiCmd.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class MakeCone;
class SketchCircleTransient;
class GuiCmdHoverInputPopup1;
class GuiCmdHoverInputPopup2;

class MakeConeGuiCmd : public MakePrimitiveGuiCmd
{
    WYRX_DECLARE_MEMBERS(MakeConeGuiCmd, wy3dApp::MakeConeGuiCmd, MakePrimitiveGuiCmd);
public:
    MakeConeGuiCmd();
    virtual ~MakeConeGuiCmd();

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

    // 创建圆锥体
    std::shared_ptr<MakeCone> _pMakeCone;
    std::shared_ptr<SketchCircleTransient> _pCircleTransient;
    std::unique_ptr<GuiCmdHoverInputPopup2> _pXYPopup;
    std::unique_ptr<GuiCmdHoverInputPopup1> _pRadiusPopup;
    std::unique_ptr<GuiCmdHoverInputPopup1> _pHeightPopup;
    HoverPopupState _hoverPopupState;
};

class MakeCone : public GuiCmdMakeElement
{
public:
    MakeCone(GuiCommand* pGuiCmd) : GuiCmdMakeElement(pGuiCmd), _pCone(nullptr) {}
    ~MakeCone() {}

    // 收集创建的元素ID
    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

    // 创建
    bool init(const wy3d::Plane& workPln, const wy::Vector2& pnt1, const wy::Vector2& pnt2);
    // 更新
    bool update(double height);

private:
    wy3d::Cone* _pCone;
    wy::Vector3 _rotation;
    wy::Vector3 _oppositeRot;
};

} // namespace wy3dApp

#endif // WY3DAPP_MAKE_CONE_GUI_CMD_H
