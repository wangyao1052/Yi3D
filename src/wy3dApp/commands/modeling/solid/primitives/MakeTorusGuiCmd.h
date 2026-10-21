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

#ifndef WY3DAPP_MAKE_TORUS_GUI_CMD_H
#define WY3DAPP_MAKE_TORUS_GUI_CMD_H

#include <cfloat>
#include "commands/GuiCmdMakeElement.h"
#include <wyVector2.h>
#include <wyVector3.h>
#include <wy3dVector3.h>
#include <wy3dTorus.h>
#include "commands/modeling/solid/primitives/MakePrimitiveGuiCmd.h"
#include "commands/modeling/solid/primitives/MakeSphereGuiCmd.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class MakeTorus;
class SketchCircleTransient;
class GuiCmdHoverInputPopup1;
class GuiCmdHoverInputPopup2;

class MakeTorusGuiCmd : public MakePrimitiveGuiCmd
{
    WYRX_DECLARE_MEMBERS(MakeTorusGuiCmd, wy3dApp::MakeTorusGuiCmd, MakePrimitiveGuiCmd)
public:
    MakeTorusGuiCmd();
    virtual ~MakeTorusGuiCmd();

protected:
    virtual void reset();
    virtual void cleanUp() override;
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
        double majorRadius;
        double minorRadius;

        HoverPopupState()
            : lastMouseX(DBL_MAX)
            , lastMouseY(DBL_MAX)
            , lastMouseMoveTime(-1.0)
            , point(0.0, 0.0)
            , majorRadius(0.0)
            , minorRadius(0.0)
        {}

        void resetValue()
        {
            point.set(0.0, 0.0);
            majorRadius = 0.0;
            minorRadius = 0.0;
        }
    };

private:
    wy::Vector2 _uv1;
    wy::Vector2 _uv2;
    double _minorRadius;

    // 创建圆环
    std::shared_ptr<MakeTorus> _pMakeTorus;
    std::shared_ptr<SketchCircleTransient> _pCircleTransient;
    std::shared_ptr<RadiusTransient> _pRadiusTransient;
    std::unique_ptr<GuiCmdHoverInputPopup2> _pXYPopup;
    std::unique_ptr<GuiCmdHoverInputPopup1> _pMajorRadiusPopup;
    std::unique_ptr<GuiCmdHoverInputPopup1> _pMinorRadiusPopup;
    HoverPopupState _hoverPopupState;
};

class MakeTorus : public GuiCmdMakeElement
{
public:
    MakeTorus(GuiCommand* pGuiCmd) : GuiCmdMakeElement(pGuiCmd), _pTorus(nullptr), _majorRadius(0.0) {}
    ~MakeTorus() {}

    // 收集创建的元素ID
    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

    // 创建
    bool init(const wy3d::Plane& workPln, const wy::Vector2& pnt1, const wy::Vector2& pnt2);
    // 更新
    bool update(double minorRadius);
    // 获取主径
    double getMajorRadius() const
    {
        return _majorRadius;
    }

private:
    wy3d::Torus* _pTorus;
    wy::Vector3 _initOrigin;
    double _majorRadius;
};

} // namespace wy3dApp

#endif // WY3DAPP_MAKE_TORUS_GUI_CMD_H
