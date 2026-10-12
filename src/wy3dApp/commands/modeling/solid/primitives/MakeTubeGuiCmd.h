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

#ifndef WY3DAPP_MAKE_TUBE_GUI_CMD_H
#define WY3DAPP_MAKE_TUBE_GUI_CMD_H

#include <cfloat>
#include <wyVector2.h>
#include <wyVector3.h>
#include <wy3dVector3.h>
#include <wy3dTube.h>
#include "commands/modeling/solid/primitives/MakePrimitiveGuiCmd.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class MakeTube;
class SketchCircleTransient;
class GuiCmdHoverInputPopup1;
class GuiCmdHoverInputPopup2;

class MakeTubeGuiCmd : public MakePrimitiveGuiCmd
{
    WYRX_DECLARE_MEMBERS(MakeTubeGuiCmd, wy3dApp::MakeTubeGuiCmd, MakePrimitiveGuiCmd)
public:
    MakeTubeGuiCmd();
    virtual ~MakeTubeGuiCmd();

protected:
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;

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
        double outerRadius;
        double innerRadius;
        double height;
        int heightSign;

        HoverPopupState()
            : lastMouseX(DBL_MAX)
            , lastMouseY(DBL_MAX)
            , lastMouseMoveTime(-1.0)
            , point(0.0, 0.0)
            , outerRadius(0.0)
            , innerRadius(0.0)
            , height(0.0)
            , heightSign(1)
        {}

        void resetValue()
        {
            point.set(0.0, 0.0);
            outerRadius = 0.0;
            innerRadius = 0.0;
            height = 0.0;
            heightSign = 1;
        }
    };

private:
    wy::Vector2 _uv1;
    wy::Vector2 _uv2;
    wy::Vector2 _uv3;
    double _height;

    // 创建圆管
    std::shared_ptr<MakeTube> _pMakeTube;
    std::shared_ptr<SketchCircleTransient> _pCircleTransientOuter;
    std::shared_ptr<SketchCircleTransient> _pCircleTransientInner;
    std::unique_ptr<GuiCmdHoverInputPopup2> _pXYPopup;
    std::unique_ptr<GuiCmdHoverInputPopup1> _pOuterRadiusPopup;
    std::unique_ptr<GuiCmdHoverInputPopup1> _pInnerRadiusPopup;
    std::unique_ptr<GuiCmdHoverInputPopup1> _pHeightPopup;
    HoverPopupState _hoverPopupState;
};

class MakeTube : public GuiCmdMakeElement
{
public:
    MakeTube(GuiCommand* pGuiCmd) : GuiCmdMakeElement(pGuiCmd), _pTube(nullptr) {}
    ~MakeTube() {}

    // 收集创建的元素ID
    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

    // 创建
    bool init(const wy3d::Plane& workPln,
        const wy::Vector2& pnt1,
        const wy::Vector2& pnt2,
        const wy::Vector2& pnt3);
    // 更新
    bool update(double height);

private:
    wy3d::Tube* _pTube;
    wy::Vector3 _initOrigin;
    wy::Vector3 _zAxis;
};

} // namespace wy3dApp

#endif // WY3DAPP_MAKE_TUBE_GUI_CMD_H
