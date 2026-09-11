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

#ifndef WY3DAPP_SKETCH_DRAW_ARC_BY_3_POINTS3D_GUI_CMD_H
#define WY3DAPP_SKETCH_DRAW_ARC_BY_3_POINTS3D_GUI_CMD_H

#include "commands/sketch3d/Sketch3DDrawGuiCmd.h"
#include <cfloat>
#include <memory>
#include <set>
#include <utility>
#include <wyVector2.h>
#include <wyVector3.h>
#include <wydbElementId.h>
#include <wy3dSketchArc3D.h>
#include <wy3dSketchPlane.h>

class MakeSketchArcBy3Points3D;
class GuiCmdHoverInputPopup3;

class SketchDrawArcBy3Points3DGuiCmd : public Sketch3DDrawGuiCmd
{
    WYRX_DECLARE_MEMBERS(SketchDrawArcBy3Points3DGuiCmd, SketchDrawArcBy3Points3DGuiCmd, Sketch3DDrawGuiCmd)
public:
    SketchDrawArcBy3Points3DGuiCmd();
    virtual ~SketchDrawArcBy3Points3DGuiCmd();

protected:
    enum class Step
    {
        SpecifyPoint1st = 1,
        SpecifyPoint2nd = 2,
        SpecifyPoint3rd = 3,
    };

    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void cleanup() override;
    virtual void onEscapeKey() override;

    bool finishStep(unsigned int step);
    void gotoStep(unsigned int step);

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseDown(const MouseEvent& event) override;
    void onFrame(double time) override;

private:
    std::set<wydb::ElementId> getSnapExcludeIds() const;
    std::pair<wy::Vector3, bool> computePoint3d(double x, double y);

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

        HoverPopupState()
            : lastMouseX(DBL_MAX)
            , lastMouseY(DBL_MAX)
            , lastMouseMoveTime(-1.0)
            , point()
        {
            this->resetValue();
        }

        void resetValue()
        {
            point.set(0.0, 0.0, 0.0);
        }
    };

private:
    unsigned int _step;
    wy::Vector3 _pnt1st;
    wy::Vector3 _pnt2nd;
    wy::Vector3 _pnt3rd;
    std::shared_ptr<MakeSketchArcBy3Points3D> _pMakeSketchArc;

    std::unique_ptr<GuiCmdHoverInputPopup3> _pXYZPopup;
    HoverPopupState _hoverPopupState;
};

class MakeSketchArcBy3Points3D : public GuiCmdMakeElement
{
public:
    MakeSketchArcBy3Points3D(GuiCommand* pGuiCmd)
        : GuiCmdMakeElement(pGuiCmd)
        , _pSketchArc3D(nullptr)
    {}

    ~MakeSketchArcBy3Points3D() {}

    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

    wydb::ElementId getId() const
    {
        return _pSketchArc3D ? _pSketchArc3D->getId() : wydb::ElementId::kNull;
    }

    bool init(const wy::Vector3& pnt1, const wy3d::SketchPlane& plane, wydb::ElementId sketch3dId);
    // 弧平面由落点反算:两点预览(useThirdPoint=false)时法向取 fallbackNormal
    bool update(const wy::Vector3& pnt1, const wy::Vector3& pnt2, const wy::Vector3& pnt3,
        const wy::Vector3& fallbackNormal, bool useThirdPoint);

private:
    wy3d::SketchArc3D* _pSketchArc3D;
};

#endif // WY3DAPP_SKETCH_DRAW_ARC_BY_3_POINTS3D_GUI_CMD_H
