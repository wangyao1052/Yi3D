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

#ifndef WY3DAPP_SKETCH_DRAW_CIRCLE3D_GUI_CMD_H
#define WY3DAPP_SKETCH_DRAW_CIRCLE3D_GUI_CMD_H

#include "commands/sketch3d/Sketch3DDrawGuiCmd.h"
#include <cfloat>
#include <memory>
#include <set>
#include <utility>
#include <wyVector3.h>
#include <wydbElementId.h>
#include <wy3dSketchCircle3D.h>

class Sketch3DSnapContext;

class MakeSketchCircle3D;
class GuiCmdHoverInputPopup1;
class GuiCmdHoverInputPopup3;

class SketchDrawCircle3DGuiCmd : public Sketch3DDrawGuiCmd
{
    WYRX_DECLARE_MEMBERS(SketchDrawCircle3DGuiCmd, SketchDrawCircle3DGuiCmd, Sketch3DDrawGuiCmd)
public:
    SketchDrawCircle3DGuiCmd();
    virtual ~SketchDrawCircle3DGuiCmd();

protected:
    enum class Step
    {
        SpecifyCenterPnt = 1,
        SpecifyRadius = 2,
    };

    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void cleanup() override;
    virtual void onEscapeKey() override;

    bool finishStep(unsigned int step);
    void gotoStep(unsigned int step);

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseDown(const MouseEvent& event) override;
    void onFrame(double time) override;
    virtual void onSpaceKey() override;

private:
    std::set<wydb::ElementId> getSnapExcludeIds() const;
    std::pair<wy::Vector3, bool> computePoint3d(double x, double y);
    bool computeCirclePlane(const wy::Vector3& pnt, wy::Vector3& outNormal, wy::Vector3& outXDir) const;

    void initializePopups();
    void showPopup();
    void hidePopup();
    void tryShowPopupOnHover(double time);
    void onPopupEnterKey();
    void onPopupEscapeKey();
    void onPopupSpaceKey();
    void simulateMouseMoveFromPopup();

private:
    struct HoverPopupState
    {
        double lastMouseX;
        double lastMouseY;
        double lastMouseMoveTime;

        wy::Vector3 point;
        double radius;

        HoverPopupState()
            : lastMouseX(DBL_MAX)
            , lastMouseY(DBL_MAX)
            , lastMouseMoveTime(-1.0)
            , point()
            , radius(0.0)
        {
            this->resetValue();
        }

        void resetValue()
        {
            point.set(0.0, 0.0, 0.0);
            radius = 0.0;
        }
    };

    struct SnapPlaneState
    {
        bool snapped;
        wy::Vector3 normal;
        wy::Vector3 xDir;

        SnapPlaneState()
            : snapped(false)
            , normal()
            , xDir()
        {
            this->resetValue();
        }

        void resetValue()
        {
            snapped = false;
            normal.set(0.0, 0.0, 0.0);
            xDir.set(0.0, 0.0, 0.0);
        }
    };

private:
    unsigned int _step;
    wy::Vector3 _centerPnt;
    double _radius;
    SnapPlaneState _snapPlaneState;
    std::shared_ptr<MakeSketchCircle3D> _pMakeSketchCircle3D;
    // 捕捉上下文(圆心步=Locate,半径步=DrawCircle,仿2D命令的_pSnapContext)
    std::shared_ptr<Sketch3DSnapContext> _pSnapContext;

    std::unique_ptr<GuiCmdHoverInputPopup3> _pXYZPopup;
    std::unique_ptr<GuiCmdHoverInputPopup1> _pRadiusPopup;
    HoverPopupState _hoverPopupState;
};

class MakeSketchCircle3D : public GuiCmdMakeElement
{
public:
    MakeSketchCircle3D(GuiCommand* pGuiCmd)
        : GuiCmdMakeElement(pGuiCmd)
        , _pSketchCircle3D(nullptr)
    {}

    ~MakeSketchCircle3D() {}

    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

    wydb::ElementId getId() const
    {
        return _pSketchCircle3D ? _pSketchCircle3D->getId() : wydb::ElementId::kNull;
    }

    bool init(const wy::Vector3& centerPnt, const wy::Vector3& normal, const wy::Vector3& xDir, wydb::ElementId sketch3dId);
    bool update(double radius);
    bool updateRadiusAndPlane(double radius, const wy::Vector3& normal, const wy::Vector3& xDir);
    bool updatePlane(const wy::Vector3& normal, const wy::Vector3& xDir);

private:
    wy3d::SketchCircle3D* _pSketchCircle3D;
};

#endif // WY3DAPP_SKETCH_DRAW_CIRCLE3D_GUI_CMD_H
