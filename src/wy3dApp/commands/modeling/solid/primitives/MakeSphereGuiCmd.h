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

#ifndef WY3DAPP_MAKE_SPHERE_GUI_CMD_H
#define WY3DAPP_MAKE_SPHERE_GUI_CMD_H

#include <cfloat>
#include <wyVector2.h>
#include <wyVector3.h>
#include <wy3dVector3.h>
#include <wy3dSphere.h>
#include <wy3dPlane.h>
#include "commands/modeling/solid/primitives/MakePrimitiveGuiCmd.h"

class MakeSphere;
class RadiusTransient;
class GuiCmdHoverInputPopup1;
class GuiCmdHoverInputPopup2;

class MakeSphereGuiCmd : public MakePrimitiveGuiCmd
{
    WYRX_DECLARE_MEMBERS(MakeSphereGuiCmd, MakeSphereGuiCmd, MakePrimitiveGuiCmd)
public:
    MakeSphereGuiCmd();
    virtual ~MakeSphereGuiCmd();

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

        HoverPopupState()
            : lastMouseX(DBL_MAX)
            , lastMouseY(DBL_MAX)
            , lastMouseMoveTime(-1.0)
            , point(0.0, 0.0)
            , radius(0.0)
        {}

        void resetValue()
        {
            point.set(0.0, 0.0);
            radius = 0.0;
        }
    };

private:
    wy::Vector2 _uv1;
    double _radius;

    // 创建Sphere
    std::shared_ptr<MakeSphere> _pMakeSphere;
    std::shared_ptr<RadiusTransient> _pRadiusTransient;
    std::unique_ptr<GuiCmdHoverInputPopup2> _pXYPopup;
    std::unique_ptr<GuiCmdHoverInputPopup1> _pRadiusPopup;
    HoverPopupState _hoverPopupState;
};

class MakeSphere : public GuiCmdMakeElement
{
public:
    MakeSphere(GuiCommand* pGuiCmd) : GuiCmdMakeElement(pGuiCmd), _pSphere(nullptr) {}
    ~MakeSphere() {}

    // 收集创建的元素ID
    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

    // 创建
    bool init(const wy3d::Plane& workPln, const wy::Vector2& pnt);
    // 更新
    bool update(double radius);

private:
    wy3d::Sphere* _pSphere;
    wy::Vector3 _initOrigin;
};

class RadiusTransient : public GuiCmdTransient
{
public:
    RadiusTransient();
    ~RadiusTransient();

    //void show();
    //void hide();
    void update(const wy::Vector3& pnt1, const wy::Vector3& pnt2);

private:
    osg::ref_ptr<osg::Geometry> _geom;
    osg::ref_ptr<osg::Vec3Array> _vertices;
};

#endif // WY3DAPP_MAKE_SPHERE_GUI_CMD_H
