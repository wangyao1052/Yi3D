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

#ifndef WY3DAPP_SKETCH_DRAW_ELLIPSE3D_GUI_CMD_H
#define WY3DAPP_SKETCH_DRAW_ELLIPSE3D_GUI_CMD_H

#include "commands/sketch3d/Sketch3DDrawGuiCmd.h"
#include "commands/transient/BasicTransient.h"
#include <cfloat>
#include <memory>
#include <set>
#include <utility>
#include <wyVector3.h>
#include <wy3dSketchPlane.h>
#include <wydbElementId.h>
#include <wy3dSketchEllipse3D.h>

class GuiCmdHoverInputPopup1;
class GuiCmdHoverInputPopup2;
class GuiCmdHoverInputPopup3;

class MakeSketchEllipse3D : public GuiCmdMakeElement
{
public:
    MakeSketchEllipse3D(GuiCommand* pGuiCmd) : GuiCmdMakeElement(pGuiCmd), _pSketchEllipse3D(nullptr) {}
    ~MakeSketchEllipse3D() {}

    // 收集创建的元素ID
    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override;

    wydb::ElementId getId() const
    {
        return _pSketchEllipse3D ? _pSketchEllipse3D->getId() : wydb::ElementId::kNull;
    }

    // 创建
    bool init(const wy::Vector3& centerPnt, const wy::Vector3& normal, const wy::Vector3& xDir,
        double majorRadius, double radiusRatio, wydb::ElementId sketch3dId);
    // 写入平面与长短半轴(任一项失败即整次放弃)
    bool update(const wy::Vector3& normal, const wy::Vector3& xDir, double majorRadius, double radiusRatio);
    // 切换工作平面:长短半轴取实体现值,只换平面
    bool updatePlane(const wy::Vector3& normal, const wy::Vector3& xDir);

private:
    wy3d::SketchEllipse3D* _pSketchEllipse3D;
};

// 3D草图椭圆:中心 -> 长轴端点 -> 另一半轴
class SketchDrawEllipse3DGuiCmd : public Sketch3DDrawGuiCmd
{
    WYRX_DECLARE_MEMBERS(SketchDrawEllipse3DGuiCmd, SketchDrawEllipse3DGuiCmd, Sketch3DDrawGuiCmd)
public:
    SketchDrawEllipse3DGuiCmd();
    virtual ~SketchDrawEllipse3DGuiCmd();

protected:
    enum class Step
    {
        SpecifyCenterPnt = 1,
        SpecifyAxisEndPoint = 2,
        SpecifyOtherRadius = 3,
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
    // 平面内坐标 -> 世界方向(长轴方向按角度取)
    wy::Vector3 planeDirFromAngle(const wy3d::SketchPlane& plane, double angle) const;
    // 由世界长轴方向取它在工作平面内的角度
    double angleFromPlaneDir(const wy3d::SketchPlane& plane, const wy::Vector3& dir) const;
    double currentRatio() const;
    void updateAxisTransient(const wy::Vector3& endPnt);
    void destroyAxisTransient();
    // 用当前成员值刷新实体(切平面/预览共用)
    bool applyToEntity(const wy3d::SketchPlane& plane);

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
        double majorLength;
        double majorAngleDeg;
        double otherRadius;

        HoverPopupState()
            : lastMouseX(DBL_MAX)
            , lastMouseY(DBL_MAX)
            , lastMouseMoveTime(-1.0)
            , point()
            , majorLength(0.0)
            , majorAngleDeg(0.0)
            , otherRadius(0.0)
        {
            this->resetValue();
        }

        void resetValue()
        {
            point.set(0.0, 0.0, 0.0);
            majorLength = 0.0;
            majorAngleDeg = 0.0;
            otherRadius = 0.0;
        }
    };

private:
    unsigned int _step;
    wy::Vector3 _centerPnt;
    wy::Vector3 _majorAxisDir;
    double _majorRadius;
    double _otherRadius;
    std::shared_ptr<MakeSketchEllipse3D> _pMakeSketchEllipse3D;

    std::unique_ptr<GuiCmdHoverInputPopup3> _pXYZPopup;
    std::unique_ptr<GuiCmdHoverInputPopup2> _pLengthAnglePopup;
    std::unique_ptr<GuiCmdHoverInputPopup1> _pRadiusPopup;
    HoverPopupState _hoverPopupState;

    // 中心 -> 当前点 的辅助线
    LineTransientSPtr _pAxisTransient;
};

#endif // WY3DAPP_SKETCH_DRAW_ELLIPSE3D_GUI_CMD_H
