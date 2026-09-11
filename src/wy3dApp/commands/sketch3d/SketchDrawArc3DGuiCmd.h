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

#ifndef WY3DAPP_SKETCH_DRAW_ARC3D_GUI_CMD_H
#define WY3DAPP_SKETCH_DRAW_ARC3D_GUI_CMD_H

#include "commands/sketch3d/Sketch3DDrawGuiCmd.h"
#include <cfloat>
#include <memory>
#include <set>
#include <utility>
#include <wyVector3.h>
#include <wydbElementId.h>
#include <wy3dSketchArc3D.h>

class MakeSketchArc3D;
class GuiCmdHoverInputPopup1;
class GuiCmdHoverInputPopup2;
class GuiCmdHoverInputPopup3;

class SketchDrawArc3DGuiCmd : public Sketch3DDrawGuiCmd
{
    WYRX_DECLARE_MEMBERS(SketchDrawArc3DGuiCmd, SketchDrawArc3DGuiCmd, Sketch3DDrawGuiCmd)
public:
    SketchDrawArc3DGuiCmd();
    virtual ~SketchDrawArc3DGuiCmd();

protected:
    enum class Step
    {
        SpecifyCenterPnt = 1,
        SpecifyStartPoint = 2,
        SpecifyEndPoint = 3,
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
    // 计算点相对圆心在工作平面内的半径与起始角(弧度)
    bool computeRadiusAndAngle(const wy::Vector3& pnt, double& outRadius, double& outAngle) const;

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
        double startAngleDeg;
        double totalAngleDeg;

        HoverPopupState()
            : lastMouseX(DBL_MAX)
            , lastMouseY(DBL_MAX)
            , lastMouseMoveTime(-1.0)
            , point()
            , radius(0.0)
            , startAngleDeg(0.0)
            , totalAngleDeg(0.0)
        {
            this->resetValue();
        }

        void resetValue()
        {
            point.set(0.0, 0.0, 0.0);
            radius = 0.0;
            startAngleDeg = 0.0;
            totalAngleDeg = 0.0;
        }
    };

private:
    unsigned int _step;
    wy::Vector3 _centerPnt;
    double _radius;
    double _startAngle;
    double _totalAngle;
    std::shared_ptr<MakeSketchArc3D> _pMakeSketchArc3D;

    std::unique_ptr<GuiCmdHoverInputPopup3> _pXYZPopup;
    std::unique_ptr<GuiCmdHoverInputPopup2> _pRadiusStartAnglePopup;
    std::unique_ptr<GuiCmdHoverInputPopup1> _pSweepAnglePopup;
    HoverPopupState _hoverPopupState;
};

class MakeSketchArc3D : public GuiCmdMakeElement
{
public:
    MakeSketchArc3D(GuiCommand* pGuiCmd)
        : GuiCmdMakeElement(pGuiCmd)
        , _pSketchArc3D(nullptr)
    {}

    ~MakeSketchArc3D() {}

    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

    wydb::ElementId getId() const
    {
        return _pSketchArc3D ? _pSketchArc3D->getId() : wydb::ElementId::kNull;
    }

    bool init(const wy::Vector3& centerPnt, const wy::Vector3& normal, const wy::Vector3& xDir, wydb::ElementId sketch3dId);
    bool updateRadiusAndStartAngle(double radius, double startAngle);
    bool updateTotalAngle(double totalAngle);
    // 切换工作平面:圆心/半径/扫角不变,起点方向取原起点在新平面上的投影
    bool updatePlane(const wy::Vector3& normal, const wy::Vector3& xDir);

    double getRadius() const;
    double getStartAngle() const;

private:
    wy3d::SketchArc3D* _pSketchArc3D;
};

#endif // WY3DAPP_SKETCH_DRAW_ARC3D_GUI_CMD_H
